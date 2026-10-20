// Synth7Msfa.h — O motor anterior (msfa), só para comparar nas probas
// (reparto de voces, pedal, rodas, aftertouch, LFO común e edición en vivo).
// O motor calcula en bloques de 64 mostras; os eventos MIDI aplícanse ao comezo
// do bloque no que caen (como mínimo 0, como máximo 1,3 ms antes): sen latencia engadida.
#pragma once
#include "MsfaInclude.h"
#include "Dx7Patch.h"
#include <memory>
#include <array>
#include <atomic>
#include <mutex>

namespace b7 {

class Synth7Msfa
{
public:
    static constexpr int MAXV = 16;   // o DX7 ten 16 voces

    Synth7Msfa()
    {
        tuning = createStandardTuning();
        for (auto& v : voices) v.note = std::make_unique<Dx7Note> (tuning, nullptr);
        patch = initPatch();
        ctrls.core = &core;
        ctrls.masterTune = 0;
        ctrls.pitch_cc = 0x2000;
        ctrls.pitch_range_up = ctrls.pitch_range_dn = 2;
        ctrls.pitch_step = 0;
        ctrls.modwheel_cc = ctrls.foot_cc = ctrls.breath_cc = ctrls.aftertouch_cc = 0;
        ctrls.portamento_enable_cc = false; ctrls.portamento_cc = 0; ctrls.portamento_gliss_cc = false;
        ctrls.mpeEnabled = false;
        ctrls.wheel.range = 99; ctrls.wheel.pitch = true;       // roda de modulación → vibrato do LFO, como no DX7
        ctrls.at.range = 50;    ctrls.at.pitch = true;
        ctrls.breath.range = 50; ctrls.breath.amp = true;
        ctrls.foot.range = 50;  ctrls.foot.amp = true;
        ctrls.refresh();
    }

    // As táboas do motor son globais: todas as instancias usan a mesma frecuencia de mostraxe
    void prepare (double sampleRate)
    {
        static std::mutex m;
        {
            std::lock_guard<std::mutex> lk (m);
            static bool tables = false;
            if (! tables) { Exp2::init(); Tanh::init(); Sin::init(); tables = true; }
            Freqlut::init (sampleRate);
            Lfo::init (sampleRate);
            PitchEnv::init (sampleRate);
            Env::init_sr (sampleRate);
            Porta::init_sr (sampleRate);
        }
        lfo.reset (&patch[137]);
        allSoundOff();
        bufPos = N;
    }

    // ---- son ----
    const Patch& getPatch() const { return patch; }
    void setPatch (const Patch& p)            // cambio de son: as notas que soan apáganse
    {
        allSoundOff();
        patch = p; patch[155] = 0x3f;
        lfo.reset (&patch[137]);
    }
    void editPatch (int index, uint8_t value)  // edición en vivo: as notas que soan cambian xa
    {
        if (index < 0 || index >= 155) return;
        patch[(size_t) index] = std::min (value, patchMax()[(size_t) index]);
        if (index >= 137 && index <= 142) lfo.reset (&patch[137]);
        for (auto& v : voices)
            if (v.live) v.note->update (patch.data(), v.midinote, v.velocity, 1);
    }
    void setOpSwitch (uint8_t mask) { patch[155] = mask & 0x3f; for (auto& v : voices) if (v.live) v.note->update (patch.data(), v.midinote, v.velocity, 1); }

    // ---- MIDI ----
    void noteOn (int note, int vel)
    {
        if (vel <= 0) { noteOff (note); return; }
        const int n = std::clamp (note + (int) patch[144] - 24, 0, 127);   // transposición do son (C3 = 24)
        // se xa soa esa tecla (pedal), reutilízase a voz
        int slot = -1;
        for (int i = 0; i < MAXV; ++i) if (voices[(size_t) i].live && voices[(size_t) i].key == note) { slot = i; break; }
        if (slot < 0) slot = pickVoice();
        bool anyDown = false;
        for (auto& o : voices) anyDown = anyDown || (o.live && o.down);
        auto& v = voices[(size_t) slot];
        const bool steal = v.live && v.note->isPlaying();
        v.note->init (patch.data(), n, vel, 1, &ctrls);
        if (patch[136])
        {
            if (! steal) v.note->oscSync();   // OSC KEY SYNC: as fases comezan en cero (sen roubo, para non dar clic)
        }
        else
        {
            // sen sincronía: se esa mesma nota xa soa noutra voz, cóllese a súa fase (evita anulacións ao chou)
            for (auto& o : voices)
                if (&o != &v && o.live && o.midinote == n && o.note->isPlaying()) { v.note->transferPhase (*o.note); break; }
        }
        v.key = note; v.midinote = n; v.velocity = vel;
        v.live = true; v.down = true; v.sustained = false; v.age = ++clock;
        if (! anyDown) lfo.keydown();        // o LFO arrinca (retardo e, con LFO KEY SYNC, fase) ao premer tras soltar todo
    }
    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.live && v.down && v.key == note)
            {
                v.down = false;
                if (sustain) v.sustained = true; else v.note->keyup();
            }
    }
    void setSustain (bool on)
    {
        sustain = on;
        if (! on) for (auto& v : voices) if (v.live && v.sustained) { v.sustained = false; v.note->keyup(); }
    }
    void pitchBend (int value14) { ctrls.pitch_cc = std::clamp (value14, 0, 16383); }
    void setBendRange (int semis) { ctrls.pitch_range_up = ctrls.pitch_range_dn = std::clamp (semis, 0, 12); }
    void setWheel (int range, bool pitch, bool amp, bool eg)
    {
        ctrls.wheel.range = std::clamp (range, 0, 99); ctrls.wheel.pitch = pitch; ctrls.wheel.amp = amp; ctrls.wheel.eg = eg;
        ctrls.refresh();
    }
    void modWheel (int v) { ctrls.modwheel_cc = std::clamp (v, 0, 127); ctrls.refresh(); }
    void breath (int v) { ctrls.breath_cc = std::clamp (v, 0, 127); ctrls.refresh(); }
    void foot (int v) { ctrls.foot_cc = std::clamp (v, 0, 127); ctrls.refresh(); }
    void aftertouch (int v) { ctrls.aftertouch_cc = std::clamp (v, 0, 127); ctrls.refresh(); }
    void allNotesOff() { sustain = false; for (auto& v : voices) if (v.live) { v.down = v.sustained = false; v.note->keyup(); } }
    void allSoundOff() { sustain = false; for (auto& v : voices) { v.live = v.down = v.sustained = false; } }

    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.live ? 1 : 0; return n; }
    uint32_t activeMask() const { uint32_t m = 0; for (int i = 0; i < MAXV; ++i) if (voices[(size_t) i].live) m |= 1u << i; return m; }

    // nivel de cada operador da voz máis recente (para os medidores da interface)
    void opLevels (float out[6]) const
    {
        for (int i = 0; i < 6; ++i) out[i] = 0;
        const Voice* best = nullptr;
        for (auto& v : voices) if (v.live && (! best || v.age > best->age)) best = &v;
        if (! best) return;
        VoiceStatus st; best->note->peekVoiceStatus (st);
        for (int i = 0; i < 6; ++i) out[i] = (float) st.amp[i] / (float) (1 << 24);
    }

    // Xera n mostras (mono: o DX7 é mono)
    void render (float* out, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (bufPos >= N) { computeBlock(); bufPos = 0; }
            out[i] = blockOut[(size_t) bufPos++];
        }
    }
    // mostras que quedan no bloque actual (para aplicar eventos no comezo do seguinte)
    int samplesLeftInBlock() const { return N - bufPos; }

    float outputGain = 0.04f / (float) (1 << 24);   // a metade dos sons, cun acorde de tres notas, queda por baixo de −12 dBFS; o limitador coida dos picos

private:
    struct Voice
    {
        std::unique_ptr<Dx7Note> note;
        int key = -1, midinote = 60, velocity = 100;
        bool live = false, down = false, sustained = false;
        uint32_t age = 0;
    };

    int pickVoice()
    {
        // 1) unha libre; 2) a soltada máis vella; 3) a máis vella
        for (int i = 0; i < MAXV; ++i) if (! voices[(size_t) i].live) return i;
        int best = -1;
        for (int i = 0; i < MAXV; ++i)
        {
            const auto& v = voices[(size_t) i];
            if (! v.down && ! v.sustained && (best < 0 || v.age < voices[(size_t) best].age)) best = i;
        }
        if (best >= 0) return best;
        best = 0;
        for (int i = 1; i < MAXV; ++i) if (voices[(size_t) i].age < voices[(size_t) best].age) best = i;
        return best;
    }

    void computeBlock()
    {
        std::array<int32_t, N> buf {};
        const int32_t lfoVal = lfo.getsample(), lfoDelay = lfo.getdelay();
        for (auto& v : voices)
        {
            if (! v.live) continue;
            v.note->compute (buf.data(), lfoVal, lfoDelay, &ctrls);
            if (! v.down && ! v.sustained && ! v.note->isPlaying()) v.live = false;
        }
        for (int i = 0; i < N; ++i) blockOut[(size_t) i] = (float) buf[(size_t) i] * outputGain;
    }

    std::shared_ptr<TuningState> tuning;
    std::array<Voice, MAXV> voices;
    FmCore core;
    Controllers ctrls {};
    Lfo lfo {};
    Patch patch {};
    bool sustain = false;
    uint32_t clock = 0;
    std::array<float, N> blockOut {};
    int bufPos = N;
};

} // namespace b7
