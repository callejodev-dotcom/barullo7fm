#include "../Source/Algo.h"
#include <cstdio>
int main() {
  for (int a = 0; a < 32; ++a) {
    auto g = b7::algoGraph (a);
    std::printf ("%2d port:", a + 1); for (int o = 1; o <= 6; ++o) if (g.carrier[o]) std::printf (" %d", o);
    std::printf ("  |"); for (auto& e : g.edges) std::printf (" %d>%d", e.first, e.second);
    std::printf ("  | fb %d>%d  | x:", g.fbFrom, g.fbTo); for (int o = 1; o <= 6; ++o) std::printf (" %.1f/%g", g.x[o], g.y[o]); std::printf ("\n");
  }
}
