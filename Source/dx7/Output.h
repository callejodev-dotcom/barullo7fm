// Output.h — A parte analóxica da saída e o paso á frecuencia do DAW.
//
// No orixinal: cada voz pasa polo conversor de 12 bits por separado (Engine::dac), as 16 súmanse
// en analóxico, dous circuítos de mostraxe e retención (S/H) alternos súmanse, o conversor mantén
// cada valor ata o seguinte (retención de orde cero) e un filtro Sallen-Key de ~16 kHz suaviza.
//
// Aquí: S/H (media das dúas últimas mostras), filtro de 4 polos a 16 kHz e un remostraxe sen
// aliasing (Kaiser) cuxo núcleo leva dentro a retención de orde cero, así que a caída dos agudos
// do conversor é a mesma. O filtro do remostraxe é de fase mínima: ningunha latencia que declarar.
#pragma once
#include "Engine.h"
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <mutex>

namespace dx7 {

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fc, double fs, double q)
    {
        const double PI = 3.14159265358979323846;
        const double k = std::tan (PI * fc / fs), k2 = k * k, n = 1.0 / (1.0 + k / q + k2);
        b0 = k2 * n; b1 = 2 * b0; b2 = b0;
        a1 = 2 * (k2 - 1) * n; a2 = (1 - k / q + k2) * n;
    }
    inline double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

class Resampler
{
public:
    static constexpr int L = 256;   // fases por mostra do chip

    void design (double chipRate, double outRate)
    {
        static std::mutex m;
        static double cRate = -1, oRate = -1;
        static std::vector<float> cTable;
        static int cTaps = 0;
        {
            std::lock_guard<std::mutex> lk (m);
            if (std::abs (cRate - chipRate) < 1e-9 && std::abs (oRate - outRate) < 1e-9) { table = cTable; taps = cTaps; alloc(); return; }
        }
        designNow (chipRate, outRate);
        std::lock_guard<std::mutex> lk (m);
        cRate = chipRate; oRate = outRate; cTable = table; cTaps = taps;
    }

    void reset() { std::fill (hist.begin(), hist.end(), 0.0); head = 0; }
    inline void push (double x) { head = (head + 1) & mask; hist[head] = x; }

    // saída no instante (última mostra do chip + frac), frac en [0, 1)
    inline double out (double frac) const
    {
        const double p = frac * L;
        const int ip = (int) p; const float a = (float) (p - ip);
        double s = 0;
        size_t idx = head;
        const float* t = table.data() + ip;
        for (int k = 0; k < taps; ++k)
        {
            s += hist[idx] * (t[0] + (t[1] - t[0]) * a);
            idx = (idx - 1) & mask; t += L;
        }
        return s;
    }

private:
    std::vector<float> table;
    std::vector<double> hist;
    size_t mask = 0, head = 0;
    int taps = 0;

    void alloc()
    {
        size_t H = 1; while ((int) H < taps + 1) H <<= 1;
        hist.assign (H, 0.0); mask = H - 1; head = 0;
    }

    void designNow (double chipRate, double outRate)
    {
        const double PI = 3.14159265358979323846;
        const double fpass = std::min (20000.0, 0.45 * std::min (chipRate, outRate));
        const double fstop = std::min (chipRate, outRate) - fpass;
        const double A = 90.0, beta = 0.1102 * (A - 8.7);
        const double dw = 2 * PI * (fstop - fpass) / chipRate;
        const int N = (int) std::ceil ((A - 8) / (2.285 * dw)) + 1;
        const double cut = (fpass + fstop) / 2 / chipRate;
        const int P = N * L;
        std::vector<double> h ((size_t) P);
        const double i0b = besselI0 (beta);
        for (int i = 0; i < P; ++i)
        {
            const double t = (i - (P - 1) / 2.0) / L;
            const double x = 2 * cut * t;
            const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (PI * x) / (PI * x);
            const double r = 2.0 * i / (P - 1) - 1.0;
            h[(size_t) i] = 2 * cut * sinc * besselI0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / i0b;
        }
        // retención de orde cero do conversor: unha caixa dunha mostra de ancho
        std::vector<double> g (h.size() + L - 1, 0.0);
        for (size_t i = 0; i < h.size(); ++i)
            for (int j = 0; j < L; ++j) g[i + (size_t) j] += h[i] / L;

        // a fase mínima polo cepstro real
        size_t M = 1; while (M < g.size() * 8) M <<= 1;
        std::vector<std::complex<double>> X (M);
        for (size_t i = 0; i < g.size(); ++i) X[i] = g[i];
        fft (X, false);
        for (auto& v : X) v = std::log (std::max (std::abs (v), 1e-9));
        fft (X, true);
        for (size_t i = 1; i < M / 2; ++i) X[i] *= 2.0;
        for (size_t i = M / 2 + 1; i < M; ++i) X[i] = 0.0;
        for (auto& v : X) v = std::complex<double> (v.real(), 0.0);
        fft (X, false);
        for (auto& v : X) v = std::exp (v);
        fft (X, true);

        taps = (int) (g.size() + L - 1) / L + 1;
        table.assign ((size_t) (taps * L + 2), 0.0f);
        for (size_t i = 0; i < g.size() && i < table.size(); ++i) table[i] = (float) X[i].real();
        for (int ph = 0; ph < L; ++ph)
        {
            double s = 0; for (int k = 0; k < taps; ++k) s += table[(size_t) (ph + k * L)];
            if (std::abs (s) > 1e-12) for (int k = 0; k < taps; ++k) table[(size_t) (ph + k * L)] = (float) (table[(size_t) (ph + k * L)] / s);
        }
        alloc();
    }

    static double besselI0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 60; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-14 * s) break; }
        return s;
    }

    static void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        const double PI = 3.14159265358979323846;
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2 * PI / (double) len * (inverse ? 1 : -1);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
        if (inverse) for (auto& v : a) v /= (double) n;
    }
};

// a saída enteira: S/H, filtro analóxico e remostraxe
class AnalogOut
{
public:
    double gain = 1.0;            // escala final (unidades de 15 bits → ±1)
    bool shAverage = true;        // os dous S/H sumados

    void prepare (double outRate)
    {
        f1.lowpass (16000.0, CHIP_RATE, 0.54119610);
        f2.lowpass (16000.0, CHIP_RATE, 1.30656296);
        rs.design (CHIP_RATE, outRate);
        reset();
    }
    void reset() { f1.reset(); f2.reset(); rs.reset(); prev = 0; }

    inline void pushChip (int dacSum)
    {
        const double x = (double) dacSum;
        const double sh = shAverage ? 0.5 * (x + prev) : x;
        prev = x;
        rs.push (f2.run (f1.run (sh)) * gain);
    }
    inline double out (double frac) const { return rs.out (frac); }

private:
    Biquad f1, f2;
    Resampler rs;
    double prev = 0;
};

} // namespace dx7
