// Engine.h — Barullo 7FM: os tres chips do sintetizador, emulados por separado e en orde.
//
//   CPU (firmware)  →  EGS (envolventes e frecuencias)  →  OPS (operadores)  →  saída (Output.h)
//
// · CPU: o que fai o programa do teclado (v1.8), copiado rutina a rutina do desensamblado:
//   reparto de voces, escalas de teclado e velocidade, envolvente de ton, LFO, rodas, bend.
//   Corre no seu temporizador (~372 veces por segundo), coas súas táboas e redondeos de 8 bits.
// · EGS: recibe os rexistros que escribe a CPU e calcula, mostra a mostra, a envolvente de cada
//   operador e o logaritmo da súa frecuencia. A súa envolvente segue o modelo medido de msfa
//   (Raph Levien), que coincide coa cuantización do firmware.
// · OPS: o chip de son, segundo a análise do seu silicio (Ken Shirriff): fase de 23 bits,
//   ROM de log-seno de 1024 entradas (14 bits), ROM exponencial de 1024 (12 bits), saída de 15 bits,
//   atenuación dos portadores segundo cantos hai (COM), realimentación coa media das dúas
//   últimas saídas. Todo en enteiros, a 49 096 Hz.
#pragma once
#include "Rom.h"
#include "Algorithms.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace dx7 {

constexpr double CHIP_RATE = 9426500.0 / 192.0;   // 49 096,35 Hz
constexpr double TICK_RATE = 372.4;               // temporizador da CPU (medido polas velocidades do LFO)
constexpr int NV = 16;                            // voces

using Patch = std::array<uint8_t, 156>;

// ============================================================================ ROMs do OPS
struct OpsRom
{
    uint16_t logsin[1024];   // −log2(sen) × 1024, cuarto de onda
    uint16_t exp[1024];      // round(2^(n/1024) × 2048), con o 1 de diante (12 bits)
    OpsRom()
    {
        const double PI = 3.14159265358979323846;
        for (int n = 0; n < 1024; ++n)
        {
            const double w = (n + 0.5) / 1024.0 * PI / 2.0;
            logsin[n] = (uint16_t) (int) (-std::log2 (std::sin (w)) * 1024.0 + 0.5002);
            exp[n] = (uint16_t) std::lround (std::pow (2.0, n / 1024.0) * 2048.0);
        }
    }
};
inline const OpsRom& opsRom() { static const OpsRom r; return r; }

// atenuación (log2 × 1024, 14 bits) → amplitude de 14 bits
inline int opsExp (int att)
{
    if (att >= 0x4000) return 0;
    if (att < 0) att = 0;
    const int x = 0x3FFF - att;
    return ((int) opsRom().exp[x & 1023] << (x >> 10)) >> 13;
}

// ============================================================================ envolvente do EGS (modelo msfa)
struct Eg
{
    // nivel en log2 × 2^24 (sonoridade, como en msfa) e 16 bits máis de fracción
    int64_t level = 0, target = 0, inc = 0;
    int stage = 4;               // 0..3 tramos; 4 = parada
    int staticCount = 0;
    bool rising = false, down = false;
};

static constexpr int EG_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250,
    882000, 705600, 705600, 584325, 507150, 502740, 441000, 418950,
    352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530,
    145530, 125685, 110250, 110250, 88200, 88200, 74970, 61740,
    61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562,
    22050, 18522, 17640, 15435, 14112, 13230, 11025, 9261, 9261, 7717,
    6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705,
    573, 573, 529, 441, 441 };

// ============================================================================ a máquina enteira
class Engine
{
public:
    Engine() { opsRom(); reset(); }

    // ------------------------------------------------------------ parámetros de función
    int bendRange = 2;           // 0..12
    struct Source { uint8_t range = 0, flags = 0, input = 0, scaled = 0; };   // flags: 1 ton, 2 amp, 4 EG bias
    std::array<Source, 4> src;   // roda, pedal, sopro, aftertouch
    uint16_t masterTune = 0x100;

    void reset()
    {
        for (auto& v : voice) v = Voice {};
        lfo = LfoState {};
        pitchModReg = 0; ampModReg = 0; bendValue = 0;
        pegToggle = 0; portaHalf = 0; currVoice = 0;
        tickAcc = 0;
        processSources();
        parseBend (128);
    }

    // ------------------------------------------------------------ o son (activación, como PATCH_ACTIVATE)
    void setPatch (const Patch& p, bool silence)
    {
        patch = p;
        if (silence)   // VOICE_RESET_EGS: todos os niveis a 0xFF e as envolventes reiniciadas
            for (auto& v : voice)
            {
                v.opLevel.fill (0xFF);
                for (auto& e : v.eg) { e = Eg {}; }
                v.status = 0; v.pegStep = 5;
            }
        activate();
    }
    const Patch& getPatch() const { return patch; }
    void editParam (int index, uint8_t value) { patch[(size_t) index] = value; activate(); }
    void setOpMask (uint8_t m) { patch[155] = m & 0x3f; }

    // ------------------------------------------------------------ MIDI → CPU
    void noteOn (int note, int midiVel)
    {
        if (midiVel <= 0) { noteOff (note); return; }
        const uint8_t intVel = MIDI_VELOCITY[(midiVel & 127) >> 2];
        // VOICE_ADD: transposición (con a volta dos 8 bits do orixinal) e frecuencia logarítmica
        uint8_t key = (uint8_t) (note + patch[144] - 24);
        if (key > 127) key = 127;
        const uint16_t keyFreq = keyToLogF (key);
        // VOICE_ADD_POLY: busca en rolda unha voz sen tecla premida; se as 16 están premidas, non soa
        int found = -1;
        for (int k = 0; k < NV; ++k)
        {
            const int v = (currVoice + k) & 15;
            if (! (voice[(size_t) v].status & ACTIVE)) { found = v; currVoice = (v + 1) & 15; break; }
        }
        if (found < 0) return;
        auto& V = voice[(size_t) found];
        egsKey (found, false);
        V.target = keyFreq & 0xFFFC;
        V.key = (uint8_t) note;
        V.status = ACTIVE;
        V.pegLevel = (uint16_t) (pegLevels[3] << 7);
        V.pegStep = 0;
        if (patch[138] != 0) { lfo.delayAcc = 0; lfo.fade = 0; }
        if (patch[141]) lfo.phase = 0x7FFF;
        // VOICE_ADD_LOAD_OPERATOR_DATA_TO_EGS
        const uint8_t vs = VELOCITY_SCALE[intVel >> 2];
        const int keyIdx = (keyFreq >> 8) >> 2;
        for (int s = 0; s < 6; ++s)
        {
            int a = sensHi[s] + ((sensLo[s] * vs) >> 8);
            if (a > 255) a = 255;
            if ((patch[155] >> s) & 1)
            {
                a += kbdCurve[s][keyIdx];
                if (a > 255) a = 255;
            }
            else a = 255;
            if (a <= 3) a = 4;
            V.opLevel[(size_t) s] = (uint8_t) a;
        }
        V.keyMsb = (uint8_t) (keyFreq >> 8);
        loadVoiceFreq (found);
        egsKey (found, true);
        for (int s = 0; s < 6; ++s) V.inc[(size_t) s] = opIncrement (V, s);
    }

    void noteOff (int note)
    {
        for (int v = 0; v < NV; ++v)
        {
            auto& V = voice[(size_t) v];
            if (V.key != (uint8_t) note || ! (V.status & ACTIVE)) continue;
            if (sustain) { V.status = (uint8_t) ((V.status | SUSTAIN) & ~ACTIVE); }
            else { V.status &= (uint8_t) ~ACTIVE; V.pegStep = 4; egsKey (v, false); }
            return;
        }
    }

    void setSustain (bool on)
    {
        sustain = on;
        if (on) return;
        for (int v = 0; v < NV; ++v)
        {
            auto& V = voice[(size_t) v];
            if (V.status & SUSTAIN) { V.status &= (uint8_t) ~SUSTAIN; egsKey (v, false); V.pegStep = 4; }
        }
    }

    void allNotesOff()
    {
        sustain = false;
        for (int v = 0; v < NV; ++v)
            if (voice[(size_t) v].status & (ACTIVE | SUSTAIN)) { voice[(size_t) v].status = 0; voice[(size_t) v].pegStep = 4; egsKey (v, false); }
    }
    void allSoundOff()
    {
        sustain = false;
        for (auto& v : voice) { v.status = 0; v.pegStep = 5; for (auto& e : v.eg) e = Eg {}; v.opLevel.fill (0xFF); }
    }

    // o DX7 só usa o byte alto do bend MIDI (×2)
    void pitchBend14 (int v14) { parseBend ((uint8_t) (((v14 >> 7) & 127) << 1)); }
    void setController (int i, int value127) { src[(size_t) i].input = (uint8_t) ((value127 & 127) << 1); processSources(); }
    void setSourceSetup (int i, int range, int flags) { src[(size_t) i].range = (uint8_t) std::clamp (range, 0, 99); src[(size_t) i].flags = (uint8_t) flags; processSources(); }
    void setBendRange (int r) { bendRange = std::clamp (r, 0, 12); parseBend (lastBendInput); }

    // ------------------------------------------------------------ unha mostra do chip (49 096 Hz)
    // devolve a suma analóxica das 16 voces, cada unha xa pasada polo conversor (en unidades de 15 bits)
    int sampleOut()
    {
        tickAcc += TICK_RATE;
        if (tickAcc >= CHIP_RATE) { tickAcc -= CHIP_RATE; firmwareTick(); }

        int sum = 0;
        const auto& alg = ALGORITHMS[patch[134] & 31];
        const int fbShift = patch[135] ? 8 - patch[135] : 16;
        for (int v = 0; v < NV; ++v)
        {
            auto& V = voice[(size_t) v];
            if (! V.awake) continue;
            const int s15 = computeVoice (V, alg, fbShift);
            sum += dac (s15);
        }
        return sum;
    }

    // ------------------------------------------------------------ información para a interface
    int activeVoices() const { int n = 0; for (auto& v : voice) n += v.awake ? 1 : 0; return n; }
    void opLevels (float out[6]) const   // out[0] = OP1
    {
        for (int i = 0; i < 6; ++i) out[i] = 0;
        int best = -1; uint32_t age = 0;
        for (int v = 0; v < NV; ++v) if (voice[(size_t) v].awake && (best < 0 || voice[(size_t) v].age > age)) { best = v; age = voice[(size_t) v].age; }
        if (best < 0) return;
        for (int s = 0; s < 6; ++s) out[5 - s] = (float) std::pow (2.0, -voice[(size_t) best].lastAtt[(size_t) s] / 1024.0);
    }

private:
    static constexpr uint8_t SUSTAIN = 1, ACTIVE = 2;

    struct Voice
    {
        // CPU
        uint8_t key = 0, status = 0, keyMsb = 0;
        uint16_t target = 0, pegLevel = 0;
        int pegStep = 5;
        // EGS
        uint16_t freqReg = 0;                 // rexistro de frecuencia da voz (1/4096 de oitava)
        std::array<uint8_t, 6> opLevel { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        std::array<Eg, 6> eg {};
        int detuneUnit = 0;                   // desafinación por paso, en 1/16384 de oitava
        int rateKey = 0;                      // para a escala de taxa
        // OPS
        std::array<uint32_t, 6> phase {};
        std::array<uint32_t, 6> inc {};       // incremento de fase (recalculado cando cambia algo)
        std::array<int, 6> out {};
        int fb0 = 0, fb1 = 0;
        // estado
        bool awake = false;
        uint32_t age = 0;
        std::array<int, 6> lastAtt {};
    };
    std::array<Voice, NV> voice {};
    uint32_t ageClock = 0;

    Patch patch {};
    bool sustain = false;
    int currVoice = 0;
    double tickAcc = 0;

    // rexistros derivados do son (PATCH_ACTIVATE)
    uint8_t egRate[6][4] {}, egLevel[6][4] {};
    uint8_t kbdCurve[6][43] {};
    uint8_t sensHi[6] {}, sensLo[6] {};
    uint16_t opFreq[6] {};                    // con o bit 0 = frecuencia fixa
    int8_t detune[6] {};
    uint8_t rateScale[6] {}, ams[6] {};
    uint8_t pegRates[4] {}, pegLevels[4] {};

    // LFO e modulación (CPU)
    struct LfoState
    {
        uint16_t phase = 0, delayAcc = 0, phaseInc = 11, delayInc = 0;
        uint8_t fade = 0, amp = 0, shAcc = 0, wave = 0, pmd = 0, amd = 0, pms = 0;
        bool shReset = false;
    } lfo;
    uint8_t ampFactor = 0, egBiasTotal = 0;
    uint16_t bendValue = 0;
    uint8_t lastBendInput = 128;
    int16_t pitchModReg = 0;
    uint8_t ampModReg = 0;
    uint8_t pegToggle = 0, portaHalf = 0;

    static uint8_t scaleMsb (uint8_t v) { return (uint8_t) ((((unsigned) v * 2u * 165u) << 1) >> 8); }

    static uint16_t keyToLogF (uint8_t key)
    {
        const uint8_t a = KEY_TO_LOG_F[key & 127];
        const uint8_t lo = a & 3;
        uint8_t A = lo;
        for (int i = 0; i < 3; ++i) A = (uint8_t) ((A | lo) << 2);
        return (uint16_t) ((a << 8) | (A & 0xFC));
    }

    // a memoria tal como está no orixinal: a curva exponencial, a lineal e a táboa de niveis seguidas
    // (as curvas léense ás veces máis aló das súas 36 entradas)
    static uint8_t curveMem (int base, int idx)
    {
        static const auto mem = [] {
            std::array<uint8_t, 36 + 36 + 100> m {};
            for (int i = 0; i < 36; ++i) { m[(size_t) i] = KBD_CURVE_EXP[i]; m[(size_t) (36 + i)] = KBD_CURVE_LIN[i]; }
            for (int i = 0; i < 100; ++i) m[(size_t) (72 + i)] = LOG_LEVEL[i];
            return m;
        }();
        const int i = base + idx;
        return i >= 0 && i < (int) mem.size() ? mem[(size_t) i] : 0;
    }

    void activate()
    {
        for (int s = 0; s < 6; ++s)
        {
            const uint8_t* op = &patch[(size_t) (s * 21)];
            for (int i = 0; i < 4; ++i)
            {
                egRate[s][i] = (uint8_t) ((op[i] * 164) >> 8);
                egLevel[s][i] = (uint8_t) (LOG_LEVEL[std::min<int> (op[4 + i], 99)] >> 1);
            }
            // PATCH_ACTIVATE_OPERATOR_KBD_SCALING_LEVEL
            const int bp = KEY_TO_LOG_F[std::min (op[8] + 20, 127)] >> 2;
            const uint8_t dl = scaleMsb (op[9]), dr = scaleMsb (op[10]);
            const int lc = op[11] & 3, rc = op[12] & 3;
            const int baseL = (lc == 0 || lc == 3) ? 36 : 0, baseR = (rc == 0 || rc == 3) ? 36 : 0;
            const bool posL = (lc & 2) != 0, posR = (rc & 2) != 0;
            const int ol = LOG_LEVEL[std::min<int> (op[16], 99)];
            for (int idx = 43; idx >= 1; --idx)
            {
                int A; bool pos;
                if (idx > bp) { A = (curveMem (baseR, idx - bp) * dr) >> 8; pos = posR; }
                else { A = (curveMem (baseL, bp - idx) * dl) >> 8; pos = posL; }
                if (A & 0x80) A = 127;
                if (pos) { A = ol - A; if (A < 0) A = 0; }
                else { A = ol + A; if (A & 0x80) A = 127; }
                kbdCurve[s][idx - 1] = (uint8_t) (A << 1);
            }
            // velocidade
            const int kvs = op[15] & 7;
            sensLo[s] = (uint8_t) ((kvs * 32) & 0xFF);
            sensHi[s] = (uint8_t) (~((kvs << 1) | 0xF0) & 0xFF);
            // frecuencia
            if (op[17] == 0)
            {
                const int c = op[18] & 31;
                const int coarse = c == 0 ? -0x1000 : (int) FREQ_COARSE[c];
                opFreq[s] = (uint16_t) (((coarse + FREQ_FINE[std::min<int> (op[19], 99)] + 0x232C) & 0xFFFF) & ~1);
            }
            else
                opFreq[s] = (uint16_t) (((0x16AC + FREQ_FIXED[op[18] & 3] + op[19] * 136) & 0xFFFF) | 1);
            detune[s] = (int8_t) (std::min<int> (op[20], 14) - 7);
            rateScale[s] = op[13] & 7;
            ams[s] = op[14] & 3;
        }
        for (int i = 0; i < 4; ++i)
        {
            pegRates[i] = PITCH_EG_RATE[std::min<int> (patch[(size_t) (126 + i)], 99)];
            pegLevels[i] = PITCH_EG_LEVEL[std::min<int> (patch[(size_t) (130 + i)], 99)];
        }
        // LFO (PATCH_ACTIVATE_LFO)
        {
            const uint8_t sp = patch[137];
            if (sp == 0) lfo.phaseInc = 11;
            else
            {
                const uint8_t a = scaleMsb (sp);
                lfo.phaseInc = a < 160 ? (uint16_t) (a * 11) : (uint16_t) (a * (((a - 160) >> 2) + 11));
            }
            // retardo
            const uint8_t d = (uint8_t) (99 - std::min<int> (patch[138], 99));
            const int shifts = 7 - (((d & 0x70) * 16) >> 8);
            uint16_t D = (uint16_t) ((((d & 15) | 16) << 1) << 8);
            for (int i = 0; i < shifts; ++i) D >>= 1;
            lfo.delayInc = D;
            lfo.pmd = scaleMsb (patch[139]);
            lfo.amd = scaleMsb (patch[140]);
            lfo.wave = patch[142] % 6;
            lfo.pms = PITCH_MOD_SENS[patch[143] & 7];
        }
        // as voces que soan colleen os novos tempos, niveis e frecuencias
        for (auto& V : voice)
            for (int s = 0; s < 6; ++s)
                if (V.eg[(size_t) s].stage < 4) refreshEgTarget (V, s);
        updateIncrements();
    }

    // ------------------------------------------------------------ EGS
    void egsKey (int v, bool on)
    {
        auto& V = voice[(size_t) v];
        if (on)
        {
            V.awake = true; V.age = ++ageClock;
            if (patch[136]) { V.phase.fill (0); }          // sincronía dos osciladores (OPS)
            for (int s = 0; s < 6; ++s) { V.eg[(size_t) s].down = true; egAdvance (V, s, 0); }
        }
        else
            for (int s = 0; s < 6; ++s)
                if (V.eg[(size_t) s].down) { V.eg[(size_t) s].down = false; egAdvance (V, s, 3); }
    }

    // nivel do tramo en unidades de msfa (log2 × 256)
    int egTargetQ8 (const Voice& V, int s, int ix) const
    {
        const int outlevel = 4064 - V.opLevel[(size_t) s] * 16;
        int actual = ((63 - egLevel[s][ix]) << 6) + outlevel - 4256;
        return actual < 16 ? 16 : actual;
    }
    void refreshEgTarget (Voice& V, int s)
    {
        auto& e = V.eg[(size_t) s];
        e.target = (int64_t) egTargetQ8 (V, s, e.stage) << 32;
        e.rising = e.target > e.level;
        e.inc = egIncrement (V, s, e.stage);
    }

    int64_t egIncrement (const Voice& V, int s, int ix) const
    {
        // escala de taxa (ScaleRate de msfa, coa tecla do EGS)
        const int x = std::clamp ((V.keyMsb >> 2) - 6, 0, 31);
        int q = egRate[s][ix] + ((rateScale[s] * x) >> 3);
        if (q > 63) q = 63;
        // msfa avanza (4 + q&3) << (2 + 6 + q>>2) cada 64 mostras a 44,1 kHz; aquí, por mostra
        const int64_t perBlock = (int64_t) (4 + (q & 3)) << (2 + 6 + (q >> 2));
        return (int64_t) ((double) perBlock * 65536.0 * (44100.0 / CHIP_RATE) / 64.0);
    }

    void egAdvance (Voice& V, int s, int ix)
    {
        auto& e = V.eg[(size_t) s];
        e.stage = ix;
        if (ix >= 4) return;
        const int rawLevel = patch[(size_t) (s * 21 + 4 + ix)];
        e.target = (int64_t) egTargetQ8 (V, s, ix) << 32;
        e.rising = e.target > e.level;
        const int x = std::clamp ((V.keyMsb >> 2) - 6, 0, 31);
        const int rs = (rateScale[s] * x) >> 3;
        e.inc = egIncrement (V, s, ix);
        // tramos sen cambio de nivel: o orixinal espera igual
        e.staticCount = 0;
        if (e.target == e.level || (ix == 0 && rawLevel == 0))
        {
            int sr = patch[(size_t) (s * 21 + ix)] + rs;
            sr = std::min (sr, 99);
            double cnt = sr < 77 ? EG_STATICS[sr] : 20.0 * (99 - sr);
            if (sr < 77 && ix == 0 && rawLevel == 0) cnt /= 20.0;
            e.staticCount = (int) (cnt * CHIP_RATE / 44100.0);
        }
    }

    inline int egStep (Voice& V, int s)
    {
        auto& e = V.eg[(size_t) s];
        if (e.stage < 3 || (e.stage == 3 && ! e.down))
        {
            if (e.staticCount > 0)
            {
                if (--e.staticCount == 0) egAdvance (V, s, e.stage + 1);
            }
            else if (e.rising)
            {
                const int64_t jump = (int64_t) 1716 << 32;
                if (e.level < jump) e.level = jump;
                e.level += ((((int64_t) 17 << 40) - e.level) >> 40) * e.inc;
                if (e.level >= e.target) { e.level = e.target; egAdvance (V, s, e.stage + 1); }
            }
            else
            {
                e.level -= e.inc;
                if (e.level <= e.target) { e.level = e.target; egAdvance (V, s, e.stage + 1); }
            }
        }
        // atenuación cara ao OPS: 12 bits en 1/256 de oitava, máis a modulación de amplitude
        int att = 3840 - (int) (e.level >> 32);
        if (ams[s]) { static const int AMS[4] = { 0, 66, 109, 255 }; att += (ampModReg * 16 * AMS[ams[s]]) / 255; }
        return std::clamp (att, 0, 4095);
    }

    // logaritmo da frecuencia de cada operador → incremento de fase de 22 bits
    inline uint32_t opIncrement (const Voice& V, int s) const
    {
        int L;   // en 1/16384 de oitava
        if (opFreq[s] & 1)
            L = (opFreq[s] & 0xFFFE) * 4 + (detune[s] > 0 ? (13 * detune[s]) : 0);
        else
            L = (V.freqReg + opFreq[s]) * 4 + pitchModReg + V.detuneUnit * detune[s];
        int l14 = L >> 4;
        l14 = std::clamp (l14, 0, 0x3FFF);
        return ((uint32_t) opsRom().exp[l14 & 1023] << (l14 >> 10)) >> 5;
    }

    // ------------------------------------------------------------ OPS: unha voz, os 6 operadores (OP6 primeiro)
    inline int computeVoice (Voice& V, const Algorithm& alg, int fbShift)
    {
        static const int COM[7] = { 0, 0, 1024, 1664, 2048, 2432, 2688 };
        const int com = COM[alg.nCarriers];
        const auto& rom = opsRom();
        int outSum = 0;
        bool anyAlive = false;
        int outs[7] = { 0 };
        for (int s = 0; s < 6; ++s)
        {
            const int opn = 6 - s;
            const int env = egStep (V, s);
            V.phase[(size_t) s] = (V.phase[(size_t) s] + V.inc[(size_t) s]) & 0x7FFFFF;
            // modulación: saídas dos operadores que a modulan (xa calculados nesta mostra)
            int mod = 0;
            const uint8_t m = alg.mods[opn];
            if (m) for (int k = opn + 1; k <= 6; ++k) if ((m >> k) & 1) mod += outs[k];
            // a saída entra na fase co seu bit máis baixo no do índice da ROM (1/4096 de ciclo):
            // un modulador a todo volume move a fase ±4 ciclos
            // (× 2048 e non << 11: as saídas son negativas a metade do tempo e en C++17 desprazar
            // un negativo cara á esquerda non está definido; o resultado é o mesmo)
            int phaseMod = mod * 2048;
            if (alg.fbTo == opn && fbShift < 16) phaseMod += ((V.fb0 + V.fb1) * 2048) >> (fbShift + 1);
            const uint32_t idx = ((V.phase[(size_t) s] + (uint32_t) phaseMod) >> 11) & 0xFFF;
            int a = (int) (idx & 0x3FF);
            if (idx & 0x400) a ^= 0x3FF;
            const bool carrier = (alg.carriers >> opn) & 1;
            const int att = rom.logsin[a] + (env << 2) + (carrier ? com : 0);
            int o = opsExp (att);
            if (idx & 0x800) o = -o;
            if (! ((patch[155] >> s) & 1)) o = 0;
            outs[opn] = o;
            V.lastAtt[(size_t) s] = (env << 2) + (carrier ? com : 0);
            if (opn == alg.fbFrom) { V.fb1 = V.fb0; V.fb0 = o; }
            if (carrier) outSum += o;
            if (V.eg[(size_t) s].stage < 4 || env < 3700) anyAlive = true;
        }
        if (! anyAlive && ! (V.status & (ACTIVE | SUSTAIN))) V.awake = false;
        return std::clamp (outSum, -16384, 16383);
    }

    // conversor: 12 bits cun desprazamento de 0 a 3 segundo os ceros de diante (coma flotante de 2 bits)
    static inline int dac (int v)
    {
        const int u = v < 0 ? ~v : v;          // ceros (ou uns, se é negativo) de diante, en 15 bits
        int lz = 0;
        for (int b = 14; b >= 0 && ! ((u >> b) & 1); --b) ++lz;
        const int sh = lz >= 5 ? 3 : lz == 4 ? 2 : lz == 3 ? 1 : 0;
        const int drop = 3 - sh;               // bits que se perden por abaixo
        return (v >> drop) * (1 << drop);   // (v pode ser negativo: multiplicar, non desprazar)
    }

    // ------------------------------------------------------------ CPU: temporizador (HANDLER_OCF)
    void firmwareTick()
    {
        pegToggle ^= 0xFF;
        lfoTick();
        ampModLoad();
        portaProcess();
        if (pegToggle & 0x80) pitchEgProcess();
        pitchModLoad();
        updateIncrements();
    }

    void updateIncrements()
    {
        for (auto& V : voice)
            if (V.awake) for (int s = 0; s < 6; ++s) V.inc[(size_t) s] = opIncrement (V, s);
    }

    void lfoTick()
    {
        auto& L = lfo;
        uint32_t d = (uint32_t) L.delayAcc + L.delayInc;
        if (d > 0xFFFF)
        {
            uint8_t b = (uint8_t) (L.delayInc >> 8);
            if (b == 0) b = 1;
            const int f = L.fade + b;
            L.fade = (uint8_t) (f > 255 ? 255 : f);
            d = 0xFFFF;
        }
        L.delayAcc = (uint16_t) d;
        const uint16_t sum = (uint16_t) (L.phase + L.phaseInc);
        const bool ovf = ((L.phase ^ sum) & (L.phaseInc ^ sum) & 0x8000) != 0;
        L.shReset = ovf;
        L.phase = sum;
        const uint8_t A = (uint8_t) (L.phase >> 8), B = (uint8_t) (L.phase & 0xFF);
        switch (L.wave)
        {
            case 0: { uint8_t a = (uint8_t) ((A << 1) | (B >> 7)); if (A & 0x80) a = (uint8_t) ~a; L.amp = (uint8_t) (a + 128); break; }
            case 4: { uint8_t b = A & 63; if (A & 64) b ^= 63; b = LFO_SIN[b]; if (A & 0x80) b = (uint8_t) ~b; L.amp = b; break; }
            case 1: L.amp = (uint8_t) ~A; break;
            case 2: L.amp = A; break;
            case 3: L.amp = (A & 0x80) ? 0x80 : 0x7F; break;
            default: if (L.shReset) { L.shAcc = (uint8_t) ((L.shAcc * 179 + 0x11) & 0xFF); L.amp = L.shAcc; } break;
        }
    }

    void processSources()
    {
        int egRange = 0;
        for (auto& S : src)
        {
            const uint8_t r = scaleMsb (S.range);
            if (S.flags & 4) egRange = std::min (255, egRange + r);
            S.scaled = (uint8_t) ((r * S.input) >> 8);
        }
        egRange = (~egRange) & 0xFF;
        int b = 0;
        for (auto& S : src) if (S.flags & 2) { b += S.scaled; if (b > 255) { b = 255; break; } }
        ampFactor = (uint8_t) b;
        b = 0;
        for (auto& S : src) if (S.flags & 4) { b += S.scaled; if (b > 255) { b = 255; break; } }
        b += egRange; if (b > 255) b = 255;
        egBiasTotal = (uint8_t) (~b & 0xFF);
    }

    void ampModLoad()
    {
        int a = (lfo.fade * lfo.amd) >> 8;
        a += ampFactor; if (a > 255) a = 255;
        a += egBiasTotal; if (a > 255) a = 255;
        a -= egBiasTotal;
        const uint8_t b = (uint8_t) (((uint8_t) ~lfo.amp) ^ 0x80);
        a = (a * b) >> 8;
        a += egBiasTotal; if (a > 255) a = 255;
        ampModReg = AMP_MOD[(~a) & 0xFF];
    }

    void pitchModLoad()
    {
        int b = 0;
        for (auto& S : src) if (S.flags & 1) { b += S.scaled; if (b > 255) { b = 255; break; } }
        int total = ((lfo.fade * lfo.pmd) >> 8) + b;
        if (total > 255) total = 255;
        const int8_t amp = (int8_t) lfo.amp;
        int a;   // byte con signo
        if (amp >= 0) a = (amp * lfo.pms) >> 8;
        else { const int na = (uint8_t) (-amp); a = -(int) (uint8_t) ((na * lfo.pms) >> 8); }
        int d;
        if (a >= 0) d = a * total;
        else d = -((-a) * total);
        d = (int16_t) (uint16_t) d;
        d >>= 1;
        pitchModReg = (int16_t) (uint16_t) (d + (int16_t) bendValue);
    }

    void parseBend (uint8_t input)
    {
        lastBendInput = input;
        uint8_t A = PITCH_BEND[(input >> 1) & 127];
        A = (uint8_t) ((A << 1) ^ 0x80);
        const bool neg = (A & 0x80) != 0;
        uint16_t D;
        if (bendRange == 12)
        {
            if (! neg)
            {
                D = (uint16_t) (A << 8);
                if (A == 126) D = 0x7FFF;
                D = (uint16_t) (D << 1);
            }
            else
            {
                uint8_t a = (uint8_t) ~A;
                a = (uint8_t) ((a << 1) | 1);
                a = (uint8_t) ~a;
                D = (uint16_t) (a << 8);
            }
        }
        else
        {
            const uint8_t c = PITCH_BEND_CENTS[bendRange];
            if (! neg) D = (uint16_t) (((uint8_t) (A << 1)) * c);
            else { const uint8_t a = (uint8_t) (((uint8_t) ~A) << 1); D = (uint16_t) ~(uint16_t) (a * c); }
        }
        D >>= 2;
        if (neg) D |= 0xC000;
        bendValue = D;
    }

    void pitchEgProcess()
    {
        for (auto& V : voice)
        {
            if (V.pegStep == 3 || V.pegStep >= 5) continue;
            const int ix = std::min (V.pegStep, 3);
            const uint16_t inc = pegRates[ix];
            const uint16_t next = (uint16_t) (pegLevels[ix] << 7);
            const uint16_t cur = V.pegLevel;
            bool finished = false;
            if (cur == next) finished = true;
            else if (cur < next)
            {
                const uint16_t d = (uint16_t) (cur + inc);
                if ((d >> 8) < (next >> 8)) V.pegLevel = d; else finished = true;
            }
            else
            {
                const uint16_t d = (uint16_t) (cur - inc);
                if (d & 0x8000) finished = true;
                else if ((d >> 8) > (next >> 8)) V.pegLevel = d;
                else finished = true;
            }
            if (finished) { V.pegLevel = next; if (++V.pegStep == 6) V.pegStep = 0; }
        }
    }

    // PORTA_PROCESS sen portamento: cada interrupción actualiza a frecuencia de 8 voces
    void portaProcess()
    {
        const int first = portaHalf ? 8 : 0;
        portaHalf ^= 1;
        for (int v = first; v < first + 8; ++v) loadVoiceFreq (v);
    }

    void loadVoiceFreq (int v)
    {
        auto& V = voice[(size_t) v];
        const uint16_t t = (uint16_t) (V.target + V.pegLevel);
        const uint16_t f = (uint16_t) ((t < 0x1BA8 ? 0 : t - 0x1BA8) + masterTune);
        if (f != V.freqReg || V.detuneUnit == 0)
        {
            V.freqReg = f;
            // desafinación (medida en msfa): máis grande, en cents, nas notas graves
            const double L = f / 4096.0 + 0x232C / 4096.0 - 1.41665;   // log2 da frecuencia da nota
            const double ratio = 0.0209 * std::exp (-0.396 * L) / 7.0;
            V.detuneUnit = std::max (1, (int) std::lround (ratio * L * 16384.0));
        }
    }
};

} // namespace dx7
