#include "../Source/Synth7.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <fstream>
#include <iterator>
using namespace b7;
int main (int argc, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary);
    std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), {});
    int sum = 0; for (size_t i = 6; i < 6 + 4096 && i < d.size(); ++i) sum += d[i];
    std::printf ("%zu bytes, cabeceira %02X %02X %02X %02X %02X %02X, checksum %s\n", d.size(), d[0], d[1], d[2], d[3], d[4], d[5],
                 ((sum + d[4102]) & 127) == 0 ? "correcto" : "non cadra");
    auto bank = parseSysex (d);
    std::printf ("%zu sons:\n", bank.size());
    static Synth7 s; s.prepare (48000);
    std::vector<float> all;
    for (size_t k = 0; k < bank.size(); ++k)
    {
        s.setPatch (bank[k]);
        const int SR = 48000; std::vector<float> x ((size_t) SR * 3);
        bool low = std::string (patchName (bank[k])).find ("BASS") != std::string::npos;
        int b = low ? -24 : 0;
        for (int n : { 48, 55, 64 }) s.noteOn (n + b, 100);
        s.render (x.data(), SR); for (int n : { 48, 55, 64 }) s.noteOff (n + b);
        s.render (x.data() + SR, SR / 2);
        const int mel[4] = { 60, 64, 67, 72 };
        for (int i = 0; i < 4; ++i) { s.noteOn (mel[i] + b, 70 + i * 15); s.render (x.data() + SR * 3 / 2 + i * SR / 4, SR / 4); s.noteOff (mel[i] + b); }
        s.render (x.data() + SR * 5 / 2, SR / 2);
        double pk = 0; for (float v : x) pk = std::max (pk, (double) std::fabs (v));
        std::printf ("  %2zu %-10s  alg %2d  fb %d  pico %.2f\n", k + 1, patchName (bank[k]).c_str(), bank[k][134] + 1, bank[k][135], pk);
        all.insert (all.end(), x.begin(), x.end()); s.allSoundOff();
    }
    if (argc > 2) { FILE* o = std::fopen (argv[2], "wb"); std::fwrite (all.data(), 4, all.size(), o); std::fclose (o); }
}
