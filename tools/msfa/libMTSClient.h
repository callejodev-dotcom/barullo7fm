// Sen MTS-ESP: só afinación estándar.
#pragma once
#include <cmath>
struct MTSClient;
inline bool MTS_HasMaster (const MTSClient*) { return false; }
inline double MTS_NoteToFrequency (const MTSClient*, int midinote, int) { return 440.0 * std::pow (2.0, (midinote - 69) / 12.0); }
