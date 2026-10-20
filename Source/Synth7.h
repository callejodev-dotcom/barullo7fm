// Synth7.h — Barullo 7FM visto desde o plugin: a máquina (dx7::Engine) a 49 096 Hz,
// a súa saída analóxica e o paso á frecuencia do DAW. Os eventos MIDI entran na mostra exacta.
#pragma once
#include "dx7/Engine.h"
#include "dx7/Output.h"
#include "Dx7Patch.h"
#include <memory>

namespace b7 {

class Synth7
{
public:
    static constexpr int MAXV = dx7::NV;

    Synth7()
    {
        engine = std::make_unique<dx7::Engine>();
        patch = initPatch();
        engine->setPatch (patch, true);
        // controis: roda → ton, aftertouch → ton, sopro e pedal → amplitude (como viñan antes)
        setWheel (99, true, false, false);
        engine->setSourceSetup (1, 50, 2);
        engine->setSourceSetup (2, 50, 2);
        engine->setSourceSetup (3, 50, 1);
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        step = dx7::CHIP_RATE / sampleRate;
        out.prepare (sampleRate);
        out.gain = outputGain;
        engine->allSoundOff();
        chipPos = 0;
    }

    const Patch& getPatch() const { return patch; }
    void setPatch (const Patch& p)
    {
        patch = p; patch[155] = 0x3f;
        engine->setPatch (patch, true);
    }
    void editPatch (int index, uint8_t value)
    {
        if (index < 0 || index >= 155) return;
        patch[(size_t) index] = std::min (value, patchMax()[(size_t) index]);
        engine->editParam (index, patch[(size_t) index]);
    }
    void setOpSwitch (uint8_t mask) { patch[155] = mask & 0x3f; engine->setOpMask (patch[155]); }

    void noteOn (int note, int vel) { engine->noteOn (note, vel); }
    void noteOff (int note) { engine->noteOff (note); }
    void setSustain (bool on) { engine->setSustain (on); }
    void pitchBend (int value14) { engine->pitchBend14 (std::clamp (value14, 0, 16383)); }
    void setBendRange (int semis) { engine->setBendRange (semis); }
    void setWheel (int range, bool pitch, bool amp, bool eg) { engine->setSourceSetup (0, range, (pitch ? 1 : 0) | (amp ? 2 : 0) | (eg ? 4 : 0)); }
    void modWheel (int v) { engine->setController (0, v); }
    void foot (int v) { engine->setController (1, v); }
    void breath (int v) { engine->setController (2, v); }
    void aftertouch (int v) { engine->setController (3, v); }
    void allNotesOff() { engine->allNotesOff(); }
    void allSoundOff() { engine->allSoundOff(); }

    int activeVoices() const { return engine->activeVoices(); }
    void opLevels (float o[6]) const   // o[0] = OP6 … o[5] = OP1 (a orde do son)
    {
        float l[6]; engine->opLevels (l);  // l[0] = OP1
        for (int i = 0; i < 6; ++i) o[i] = l[5 - i];
    }

    // n mostras á frecuencia do DAW (mono: o orixinal é mono)
    void render (float* dst, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            chipPos += step;
            while (chipPos >= 1.0) { chipPos -= 1.0; out.pushChip (engine->sampleOut()); }
            dst[i] = (float) out.out (chipPos);
        }
    }

    // unha voz a todo volume (±16384) → 0,3
    double outputGain = 0.3 / 16384.0;

private:
    std::unique_ptr<dx7::Engine> engine;
    dx7::AnalogOut out;
    Patch patch {};
    double sr = 48000, step = 1, chipPos = 0;
};

} // namespace b7
