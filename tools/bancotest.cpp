// Analiza un banco .syx co motor: pico, brillo (centroide) e caída de cada son.
#include "Synth7.h"
#include <cstdio>
#include <fstream>
#include <vector>
#include <complex>
static const double SR = 48000;
static std::vector<float> play (b7::Synth7& s, const b7::Patch& p, std::vector<int> notes, int vel, double hold, double tot)
{
    s.prepare (SR); s.setPatch (p);
    std::vector<float> o ((size_t) (tot * SR));
    for (int n : notes) s.noteOn (n, vel);
    const int off = (int) (hold * SR);
    for (int pos = 0; pos < (int) o.size(); pos += 64)
    {
        s.render (o.data() + pos, std::min (64, (int) o.size() - pos));
        if (pos <= off && pos + 64 > off) for (int n : notes) s.noteOff (n);
    }
    return o;
}
static double centroid (const std::vector<float>& x, double t0)
{
    const int N = 4096, a = (int) (t0 * SR);
    std::vector<std::complex<double>> X (N);
    for (int i = 0; i < N; ++i) X[(size_t) i] = (a + i < (int) x.size() ? x[(size_t) (a + i)] : 0.0) * (0.5 - 0.5 * std::cos (2 * M_PI * i / (N - 1)));
    // DFT lenta pero suficiente para 4096 puntos con FFT radix 2
    for (size_t i = 1, j = 0; i < (size_t) N; ++i) { size_t bit = (size_t) N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (X[i], X[j]); }
    for (size_t len = 2; len <= (size_t) N; len <<= 1) { const double ang = -2 * M_PI / (double) len; std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < (size_t) N; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = X[i + j], v = X[i + j + len / 2] * w; X[i + j] = u + v; X[i + j + len / 2] = u - v; w *= wl; } } }
    double s = 0, sw = 0; for (int i = 1; i < N / 2; ++i) { const double m = std::norm (X[(size_t) i]); s += m; sw += m * i * SR / N; }
    return s > 0 ? sw / s : 0;
}
static double rmsDb (const std::vector<float>& x, double t0, double t1)
{ double e = 0; int a = (int) (t0 * SR), b = std::min ((int) x.size(), (int) (t1 * SR)); for (int i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i]; return 10 * std::log10 (e / std::max (1, b - a) + 1e-20); }
int main (int argc, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary);
    std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), {});
    auto v = b7::parseSysex (d);
    b7::Synth7 s;
    std::printf ("%-10s %6s %7s %7s %7s | %7s %7s %7s | %5s %6s %6s\n", "son", "pico", "c50ms", "c300", "c1s", "rms0.1", "rms1", "rms2.5", "vel40", "Do2", "Do6");
    for (size_t k = 0; k < v.size(); ++k)
    {
        auto x = play (s, v[k], { 60 }, 100, 2.0, 3.0);
        float pk = 0; for (auto y : x) pk = std::max (pk, std::abs (y));
        auto q = play (s, v[k], { 60 }, 40, 2.0, 2.2);
        auto lo = play (s, v[k], { 36 }, 100, 1.0, 1.2);
        auto hi = play (s, v[k], { 84 }, 100, 1.0, 1.2);
        std::printf ("%-10s %6.3f %7.0f %7.0f %7.0f | %7.1f %7.1f %7.1f | %5.1f %6.1f %6.1f\n", b7::patchName (v[k]).c_str(), pk,
                     centroid (x, 0.05), centroid (x, 0.3), centroid (x, 1.0),
                     rmsDb (x, 0.05, 0.15), rmsDb (x, 0.95, 1.05), rmsDb (x, 2.45, 2.55),
                     rmsDb (q, 0.05, 0.15) - rmsDb (x, 0.05, 0.15), rmsDb (lo, 0.05, 0.3), rmsDb (hi, 0.05, 0.3));
    }
}
