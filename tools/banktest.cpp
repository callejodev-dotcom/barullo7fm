#include "../Source/Synth7.h"
#include "../Source/Banco.h"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace b7;
int main (int argc, char** argv)
{
    static Synth7 s; s.prepare (48000);
    std::vector<float> all;
    for (int k = 0; k < 32; ++k)
    {
        Patch p {}; std::copy (FACTORY[k].data, FACTORY[k].data + 155, p.begin()); p[155] = 0x3f;
        s.setPatch (p);
        const int SR = 48000; std::vector<float> x ((size_t) SR * 3);
        // acorde curto e despois unha frase de catro notas
        int chord[3] = { 48, 55, 64 }; if (k >= 20 && k <= 22) { chord[0] = 36; chord[1] = 43; chord[2] = 48; }
        for (int n : chord) s.noteOn (n, 100);
        s.render (x.data(), SR); for (int n : chord) s.noteOff (n);
        s.render (x.data() + SR, SR / 2);
        const int mel[4] = { 60, 64, 67, 72 };
        for (int i = 0; i < 4; ++i) { s.noteOn (mel[i] - (k >= 20 && k <= 22 ? 24 : 0), 70 + i * 15); s.render (x.data() + SR * 3 / 2 + i * SR / 4, SR / 4); s.noteOff (mel[i] - (k >= 20 && k <= 22 ? 24 : 0)); }
        s.render (x.data() + SR * 5 / 2, SR / 2);
        double pk = 0, e1 = 0, e2 = 0; for (size_t i = 0; i < x.size(); ++i) pk = std::max (pk, (double) std::fabs (x[i]));
        for (int i = 2400; i < 24000; ++i) e1 += x[(size_t) i] * x[(size_t) i];
        for (int i = SR + SR / 4; i < SR + SR / 2; ++i) e2 += x[(size_t) i] * x[(size_t) i];
        std::printf ("%2d %-10s pico %5.2f  nivel %6.1f dB  cola tras soltar %6.1f dB\n", k + 1, patchName (p).c_str(), pk,
                     10 * std::log10 (e1 / 21600 + 1e-12), 10 * std::log10 (e2 / (SR / 4) + 1e-12) - 10 * std::log10 (e1 / 21600 + 1e-12));
        all.insert (all.end(), x.begin(), x.end());
        s.allSoundOff();
    }
    if (argc > 1) { FILE* f = std::fopen (argv[1], "wb"); std::fwrite (all.data(), 4, all.size(), f); std::fclose (f); }
}
