// Proba do motor novo (chips) fronte ao anterior (msfa): afinación, niveis, envolventes e espectros.
// Compilar: ver tools/enginetest.sh
#include "Synth7.h"
#include "Synth7Msfa.h"
#include "Cartuchos.h"
#include <cstdio>
#include <vector>
#include <complex>
#include <chrono>

static const double SR = 48000;

template <class S>
static std::vector<float> play (S& s, const b7::Patch& p, int note, int vel, double holdSec, double totalSec)
{
    s.prepare (SR);
    s.setPatch (p);
    std::vector<float> out ((size_t) (totalSec * SR));
    const int on = 0, off = (int) (holdSec * SR);
    int pos = 0;
    s.noteOn (note, vel);
    const int blk = 64;
    while (pos < (int) out.size())
    {
        int n = std::min (blk, (int) out.size() - pos);
        if (pos < off && pos + n > off) n = off - pos;
        s.render (out.data() + pos, n);
        pos += n;
        if (pos == off) s.noteOff (note);
    }
    (void) on;
    return out;
}

static double freqOf (const std::vector<float>& x, double t0, double t1)
{
    // cruces por cero con interpolación
    int a = (int) (t0 * SR), b = (int) (t1 * SR);
    double first = -1, last = -1; int n = 0;
    for (int i = a + 1; i < b; ++i)
        if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0)
        {
            const double t = (i - 1) + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
            if (first < 0) first = t; else { last = t; ++n; }
        }
    return n > 0 ? n * SR / (last - first) : 0;
}

static std::vector<double> spectrum (const std::vector<float>& x, double t0, int N = 8192)
{
    std::vector<std::complex<double>> X ((size_t) N);
    const int a = (int) (t0 * SR);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / (N - 1));
        X[(size_t) i] = (a + i < (int) x.size() ? x[(size_t) (a + i)] : 0.0) * w;
    }
    // FFT simple
    for (size_t i = 1, j = 0; i < (size_t) N; ++i) { size_t bit = (size_t) N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (X[i], X[j]); }
    for (size_t len = 2; len <= (size_t) N; len <<= 1)
    {
        const double ang = -2 * M_PI / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < (size_t) N; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = X[i + j], v = X[i + j + len / 2] * w; X[i + j] = u + v; X[i + j + len / 2] = u - v; w *= wl; } }
    }
    std::vector<double> m ((size_t) N / 2);
    for (int i = 0; i < N / 2; ++i) m[(size_t) i] = std::abs (X[(size_t) i]);
    return m;
}

// centroide espectral (Hz) e nivel RMS en dB
static void describe (const std::vector<float>& x, double t0, double& centroid, double& rmsDb)
{
    auto m = spectrum (x, t0);
    double s = 0, sw = 0;
    for (size_t i = 1; i < m.size(); ++i) { const double f = i * SR / 8192.0; s += m[i] * m[i]; sw += m[i] * m[i] * f; }
    centroid = s > 0 ? sw / s : 0;
    double e = 0; const int a = (int) (t0 * SR); for (int i = 0; i < 8192; ++i) e += (double) x[(size_t) (a + i)] * x[(size_t) (a + i)];
    rmsDb = 10 * std::log10 (e / 8192 + 1e-20);
}

int main()
{
    b7::Synth7 a; b7::Synth7Msfa b;
    // 1. afinación: son inicial (OP1 senoidal)
    auto init = b7::initPatch();
    auto xa = play (a, init, 69, 100, 1.0, 1.2), xb = play (b, init, 69, 100, 1.0, 1.2);
    std::printf ("La4: novo %.3f Hz, msfa %.3f Hz\n", freqOf (xa, 0.2, 0.9), freqOf (xb, 0.2, 0.9));
    float pa = 0, pb = 0; for (auto v : xa) pa = std::max (pa, std::abs (v)); for (auto v : xb) pb = std::max (pb, std::abs (v));
    std::printf ("pico son inicial: novo %.4f  msfa %.4f\n", pa, pb);
    // fixo: OP1 en frecuencia fixa 100 Hz
    auto fx = init; fx[5 * 21 + 17] = 1; fx[5 * 21 + 18] = 2; fx[5 * 21 + 19] = 0;
    xa = play (a, fx, 60, 100, 1.0, 1.2);
    std::printf ("fixa 100 Hz: %.3f Hz\n", freqOf (xa, 0.2, 0.9));
    for (int n : { 21, 36, 60, 84, 108 })
    {
        xa = play (a, init, n, 100, 1.0, 1.2);
        const double f = freqOf (xa, 0.2, 0.9), want = 440.0 * std::pow (2.0, (n - 69) / 12.0);
        std::printf ("  nota %3d: %9.3f Hz (%+.2f cents)\n", n, f, 1200 * std::log2 (f / want));
    }

    // 2. ROM1A: centroide e nivel aos 0,3 s, dos dous motores
    std::printf ("\nROM1A, Do4 vel 100 (centroide Hz / nivel dB aos 0,3 s):\n");
    for (int k = 0; k < 32; ++k)
    {
        auto p = b7::unpackVmem (b7::CARTRIDGES[0].vmem + k * 128);
        auto ya = play (a, p, 60, 100, 1.0, 1.5), yb = play (b, p, 60, 100, 1.0, 1.5);
        double ca, ra, cb, rb; describe (ya, 0.3, ca, ra); describe (yb, 0.3, cb, rb);
        std::printf ("  %-10s  novo %6.0f Hz %6.1f dB   msfa %6.0f Hz %6.1f dB   %s\n", b7::patchName (p).c_str(), ca, ra, cb, rb,
                     std::abs (std::log2 (ca / std::max (cb, 1.0))) > 0.5 ? "<<" : "");
    }

    // 3. CPU: 16 voces durante 10 s
    {
        auto p = b7::unpackVmem (b7::CARTRIDGES[0].vmem + 10 * 128);   // E.PIANO 1
        a.prepare (SR); a.setPatch (p);
        for (int i = 0; i < 16; ++i) a.noteOn (48 + i * 2, 100);
        std::vector<float> buf (512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < (int) (10 * SR / 512); ++i) a.render (buf.data(), 512);
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("\nCPU, 16 voces: %.3f s por 10 s (%.0fx tempo real)\n", s, 10 / s);
    }
}
