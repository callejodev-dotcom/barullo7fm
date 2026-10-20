// Afinación estándar (temperamento igual, la4 = 440 Hz) para o motor msfa.
// Substitúe o tuning.h de Dexed, que dependía doutra biblioteca (afinacións microtonais).
#ifndef __SYNTH_TUNING_H
#define __SYNTH_TUNING_H

#include "synth.h"
#include <memory>

class TuningState {
public:
    virtual ~TuningState() {}
    // frecuencia en escala logarítmica Q24 (1 << 24 = unha oitava)
    virtual int32_t midinote_to_logfreq (int midinote)
    {
        const int base = 50857777;   // (1 << 24) * (log2 (440) - 69 / 12)
        const int step = (1 << 24) / 12;
        return base + step * midinote;
    }
    virtual bool is_standard_tuning() { return true; }
    virtual int scale_length() { return 12; }
};

inline std::shared_ptr<TuningState> createStandardTuning() { return std::make_shared<TuningState>(); }

#endif
