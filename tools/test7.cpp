// Proba do motor sen interface: frecuencia, nivel, algoritmos e lectura/escritura de bancos.
#include "../Source/Synth7.h"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace b7;
static std::vector<float> play (Synth7& s, int note, int vel, double secs, double hold)
{
    std::vector<float> o ((size_t) (48000 * secs));
    s.noteOn (note, vel);
    const size_t h = (size_t) (48000 * hold);
    s.render (o.data(), (int) h);
    s.noteOff (note);
    s.render (o.data() + h, (int) (o.size() - h));
    return o;
}
static double freqOf (const std::vector<float>& x, size_t a, size_t n)
{   // cruces por cero con interpolación
    double first = -1, last = -1; int cnt = 0;
    for (size_t i = a + 1; i < a + n; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]); if (first < 0) first = t; last = t; ++cnt; }
    return cnt > 1 ? (cnt - 1) * 48000.0 / (last - first) : 0;
}
int main()
{
    static Synth7 s; s.prepare (48000);
    auto x = play (s, 69, 127, 1.0, 0.6);
    double pk = 0; for (float v : x) pk = std::max (pk, (double) std::fabs (v));
    std::printf ("INIT VOICE la4: %.2f Hz, pico %.3f (%.1f dBFS)\n", freqOf (x, 4800, 19200), pk, 20 * std::log10 (pk));
    // nivel tras soltar
    double tail = 0; for (size_t i = 46000; i < 48000; ++i) tail = std::max (tail, (double) std::fabs (x[i]));
    std::printf ("cola ao soltar (0,95 s): %.4f   voces activas: %d\n", tail, s.activeVoices());
    // banco: empaquetar e desempaquetar
    std::vector<Patch> bank; for (int k = 0; k < 32; ++k) { Patch p = initPatch(); p[134] = (uint8_t) k; p[(size_t) (k % 6) * 21 + 18] = (uint8_t) (k % 31); setPatchName (p, "PROBA " + std::to_string (k)); bank.push_back (p); }
    auto syx = bankToSysex (bank);
    auto back = parseSysex (syx);
    bool same = back.size() == 32;
    for (size_t k = 0; same && k < 32; ++k) same = std::equal (bank[k].begin(), bank[k].begin() + 155, back[k].begin());
    std::printf ("banco SysEx: %zu bytes, %zu sons, ida e volta %s\n", syx.size(), back.size(), same ? "idéntica" : "DISTINTA");
    // os 32 algoritmos cos 6 operadores ao máximo: que ningún rompa
    double worst = 0;
    for (int alg = 0; alg < 32; ++alg)
    {
        Patch p = initPatch(); p[134] = (uint8_t) alg; p[135] = 7;
        for (int o = 0; o < 6; ++o) p[(size_t) (o * 21 + 16)] = 99;
        s.setPatch (p);
        auto y = play (s, 60, 127, 0.3, 0.2);
        for (float v : y) { if (! std::isfinite (v)) { std::printf ("alg %d: NaN\n", alg + 1); return 1; } worst = std::max (worst, (double) std::fabs (v)); }
    }
    std::printf ("32 algoritmos con todo ao máximo e realimentación 7: pico %.2f\n", worst);
    // 16 voces + roubo
    s.setPatch (initPatch());
    std::vector<float> b (4800);
    for (int k = 0; k < 20; ++k) { s.noteOn (40 + k, 100); s.render (b.data(), 64); }
    std::printf ("20 notas premidas: %d voces soando (máximo 16)\n", s.activeVoices());
}
