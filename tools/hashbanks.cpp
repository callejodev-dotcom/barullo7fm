// Calcula a pegada (bankHash) de cada .syx de 32 sons, para a táboa de bancos coñecidos.
#include "Dx7Patch.h"
#include <cstdio>
#include <fstream>
int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::ifstream f (argv[i], std::ios::binary);
        std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), {});
        auto v = b7::parseSysex (d);
        if (v.size() < 32) continue;
        std::printf ("%s 0x%016llxull\n", argv[i], (unsigned long long) b7::bankHash (v.data()));
    }
}
