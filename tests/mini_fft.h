// =============================================================================
//  mini_fft.h  --  Tiny dependency-free radix-2 FFT for the test harness.
//
//  Only used by the offline unit tests to inspect the spectrum of processed
//  audio (harmonic content, aliasing, crossover flatness). Not part of the
//  shipping DSP.
// =============================================================================
#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <cstddef>

namespace mfft
{

using cd = std::complex<double>;

// In-place iterative Cooley-Tukey FFT. a.size() must be a power of two.
inline void fft (std::vector<cd>& a, bool invert)
{
    const int n = static_cast<int> (a.size());

    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[static_cast<size_t> (i)], a[static_cast<size_t> (j)]);
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * M_PI / len * (invert ? 1.0 : -1.0);
        const cd wlen (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            cd w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k)
            {
                const cd u = a[static_cast<size_t> (i + k)];
                const cd v = a[static_cast<size_t> (i + k + len / 2)] * w;
                a[static_cast<size_t> (i + k)]           = u + v;
                a[static_cast<size_t> (i + k + len / 2)] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert)
        for (auto& z : a) z /= n;
}

// Magnitude spectrum of a real signal (length must be a power of two).
// Returns N/2 bins. No window: choose bin-aligned test frequencies to avoid
// spectral leakage.
inline std::vector<double> magnitudeSpectrum (const std::vector<float>& x)
{
    std::vector<cd> a (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        a[i] = cd (static_cast<double> (x[i]), 0.0);

    fft (a, false);

    const size_t half = x.size() / 2;
    std::vector<double> mag (half);
    for (size_t i = 0; i < half; ++i)
        mag[i] = std::abs (a[i]);
    return mag;
}

// Nearest FFT bin for a frequency, given block size N and sample rate.
inline int binFor (double freqHz, int N, double sampleRate)
{
    return static_cast<int> (std::lround (freqHz * N / sampleRate));
}

// Snap a frequency onto an exact FFT bin centre (makes it periodic in N).
inline double snapToBin (double freqHz, int N, double sampleRate)
{
    return binFor (freqHz, N, sampleRate) * sampleRate / N;
}

} // namespace mfft
