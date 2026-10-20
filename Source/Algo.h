// Algo.h — O debuxo dos 32 algoritmos, deducido da mesma táboa que usa o motor (dx7/Algorithms.h):
// así o diagrama é sempre exactamente o que se calcula.
#pragma once
#include <cstdint>
#include <cstddef>
#include "dx7/Algorithms.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

namespace b7 {

struct AlgoGraph
{
    // operadores numerados 1..6 como no panel
    std::vector<std::pair<int, int>> edges;   // (modulador, destino)
    std::array<bool, 7> carrier {};
    int fbFrom = 0, fbTo = 0;                  // realimentación: de fbFrom a fbTo (normalmente o mesmo)
    std::array<float, 7> x {}, y {};          // posición: x en columnas, y = nivel (0 = portadores, abaixo)
};

inline AlgoGraph algoGraph (int alg)
{
    AlgoGraph g;
    const auto& A = dx7::ALGORITHMS[std::clamp (alg, 0, 31)];
    for (int opn = 6; opn >= 1; --opn)
    {
        for (int k = opn + 1; k <= 6; ++k) if ((A.mods[opn] >> k) & 1) g.edges.push_back ({ k, opn });
        g.carrier[(size_t) opn] = ((A.carriers >> opn) & 1) != 0;
    }
    g.fbFrom = A.fbFrom; g.fbTo = A.fbTo;
    // niveis: distancia máis longa ata un portador
    std::array<int, 7> lvl {};
    for (int it = 0; it < 6; ++it)
        for (auto& e : g.edges) lvl[(size_t) e.first] = std::max (lvl[(size_t) e.first], lvl[(size_t) e.second] + 1);
    // columnas: portadores en orde 1..6; cada modulador sobre a media dos seus destinos
    float col = 0;
    for (int o = 1; o <= 6; ++o) if (g.carrier[(size_t) o]) { g.x[(size_t) o] = col; col += 1; }
    const int maxL = *std::max_element (lvl.begin() + 1, lvl.end());
    for (int L = 1; L <= maxL; ++L)
    {
        std::vector<int> row;
        for (int o = 1; o <= 6; ++o) if (lvl[(size_t) o] == L) row.push_back (o);
        for (int o : row)
        {
            float s = 0; int n = 0;
            for (auto& e : g.edges) if (e.first == o && lvl[(size_t) e.second] == L - 1) { s += g.x[(size_t) e.second]; ++n; }
            if (n == 0) for (auto& e : g.edges) if (e.first == o) { s += g.x[(size_t) e.second]; ++n; }
            g.x[(size_t) o] = n ? s / (float) n : 0;
        }
        // varios moduladores sobre o mesmo sitio: abrilos
        std::sort (row.begin(), row.end(), [&] (int a, int b) { return g.x[(size_t) a] < g.x[(size_t) b] || (std::abs (g.x[(size_t) a] - g.x[(size_t) b]) < 1.0e-6f && a < b); });
        for (size_t i = 1; i < row.size(); ++i)
            if (g.x[(size_t) row[i]] < g.x[(size_t) row[i - 1]] + 1.0f) g.x[(size_t) row[i]] = g.x[(size_t) row[i - 1]] + 1.0f;
        // recentrar o grupo arredor dos seus destinos se se abriu
        if (row.size() > 1)
        {
            float want = 0, have = 0;
            for (int o : row) { have += g.x[(size_t) o]; float s = 0; int n = 0; for (auto& e : g.edges) if (e.first == o) { s += g.x[(size_t) e.second]; ++n; } want += n ? s / (float) n : g.x[(size_t) o]; }
            const float shift = (want - have) / (float) row.size();
            for (int o : row) g.x[(size_t) o] += shift;
        }
    }
    for (int o = 1; o <= 6; ++o) g.y[(size_t) o] = (float) lvl[(size_t) o];
    // normalizar x para que empece en 0
    float mn = 1e9f; for (int o = 1; o <= 6; ++o) mn = std::min (mn, g.x[(size_t) o]);
    for (int o = 1; o <= 6; ++o) g.x[(size_t) o] -= mn;
    return g;
}

inline bool isCarrier (int alg, int opn) { return algoGraph (alg).carrier[(size_t) std::clamp (opn, 1, 6)]; }

} // namespace b7
