// =============================================================================
//  Oversampler.h  --  Integer-ratio polyphase FIR oversampler.  ["OCTAPHONIC"]
//
//  Waveshaping a signal generates harmonics above the original Nyquist limit;
//  at the base sample rate those fold back as aliasing. To suppress it we run
//  the nonlinearity at L x the sample rate (L in {1,2,4,8}), band-limiting on
//  the way up (anti-imaging) and on the way down (anti-aliasing).
//
//  The processSample() call is the whole multirate round-trip for a single base
//  rate input sample:
//      1 input  --(polyphase interpolation)-->  L high-rate samples
//      apply the user nonlinearity to each high-rate sample
//      L high-rate samples --(FIR + decimate)-->  1 output
//
//  The prototype low-pass is a Type-I linear-phase FIR of *odd* length
//  N = tapsPerPhase * L + 1. With both the interpolation and decimation filters
//  sharing it, the combined group delay is exactly tapsPerPhase base-rate
//  samples -- an integer -- so the dry path can be sample-accurately aligned.
//  L = 1 is a transparent bypass.
// =============================================================================
#pragma once

#include <vector>
#include <cmath>
#include "MathConstants.h"
#include <cstddef>
#include <algorithm>

namespace octa
{

class Oversampler
{
public:
    // factor must be a power of two in {1,2,4,8}; tapsPerPhase controls the
    // steepness/quality of the band-limiting filters.
    void prepare (int factor, int tapsPerPhase = 16)
    {
        L    = (factor < 1) ? 1 : factor;
        taps = (tapsPerPhase < 4) ? 4 : tapsPerPhase;
        N    = taps * L + 1;            // odd -> Type-I linear phase, integer delay
        cpp  = taps + 1;               // max coefficients in any polyphase branch

        designPrototype();

        HR = N + L;                     // high-rate ring: N taps + the L-1 decimation offset

        inHist.assign (static_cast<size_t> (cpp), 0.0f);
        hrHist.assign (static_cast<size_t> (HR), 0.0f);
        pIn = 0;
        pHr = 0;
    }

    void reset() noexcept
    {
        std::fill (inHist.begin(), inHist.end(), 0.0f);
        std::fill (hrHist.begin(), hrHist.end(), 0.0f);
        pIn = 0;
        pHr = 0;
    }

    int factor() const noexcept { return L; }

    // Base-rate latency contributed by the up + down linear-phase filters.
    int latencySamples() const noexcept { return (L <= 1) ? 0 : taps; }

    // fn must be a memory-less map float -> float (the waveshaper).
    template <class Fn>
    inline float processSample (float x, Fn&& fn) noexcept
    {
        if (L == 1)
            return fn (x);

        // ---- push the new base-rate input into the interpolator history ----
        inHist[static_cast<size_t> (pIn)] = x;
        pIn = (pIn + 1) % cpp;

        // ---- produce L high-rate samples, shape each, feed the decimator ----
        for (int p = 0; p < L; ++p)
        {
            float acc = 0.0f;
            const float* phase = upCoeffs.data() + static_cast<size_t> (p) * cpp;
            for (int k = 0; k < cpp; ++k)
            {
                const int idx = (pIn - 1 - k + cpp) % cpp;   // newest input first
                acc += phase[k] * inHist[static_cast<size_t> (idx)];
            }

            const float shaped = fn (acc);

            hrHist[static_cast<size_t> (pHr)] = shaped;
            pHr = (pHr + 1) % HR;
        }

        // ---- decimate ------------------------------------------------------
        // Centre the decimation FIR on high-rate phase nL (i.e. (L-1) samples
        // back from the newest), which makes the combined up+down group delay
        // an exact integer (= taps base-rate samples).
        float out = 0.0f;
        const int base = pHr - L;               // newest is pHr-1 -> step back (L-1)
        for (int j = 0; j < N; ++j)
        {
            const int idx = (base - j + 2 * HR) % HR;
            out += proto[static_cast<size_t> (j)] * hrHist[static_cast<size_t> (idx)];
        }
        return out;
    }

private:
    // Windowed-sinc low-pass prototype (Blackman window) normalised to unit DC
    // gain. The interpolation phases carry an extra factor of L to compensate
    // the energy lost to zero-stuffing.
    void designPrototype()
    {
        proto.assign (static_cast<size_t> (N), 0.0f);

        const double fc = 0.5 / static_cast<double> (L) * 0.90; // cutoff (cycles/sample @ high rate)
        const double M  = static_cast<double> (N - 1);          // even -> integer centre
        double sum = 0.0;

        for (int n = 0; n < N; ++n)
        {
            const double m = static_cast<double> (n) - M * 0.5;   // centre at 0
            double sinc;
            if (std::abs (m) < 1.0e-9)
                sinc = 2.0 * fc;
            else
                sinc = std::sin (2.0 * octa::kPi * fc * m) / (octa::kPi * m);

            // Blackman window
            const double w = 0.42
                           - 0.5  * std::cos (2.0 * octa::kPi * n / M)
                           + 0.08 * std::cos (4.0 * octa::kPi * n / M);

            const double h = sinc * w;
            proto[static_cast<size_t> (n)] = static_cast<float> (h);
            sum += h;
        }

        // Normalise to unit DC gain.
        const float norm = (sum != 0.0) ? static_cast<float> (1.0 / sum) : 1.0f;
        for (auto& c : proto) c *= norm;

        // Build the L interpolation polyphase branches (gain L each), zero-padded
        // to cpp so every branch can be iterated uniformly.
        upCoeffs.assign (static_cast<size_t> (cpp) * L, 0.0f);
        for (int p = 0; p < L; ++p)
            for (int k = 0; k < cpp; ++k)
            {
                const int idx = k * L + p;
                if (idx < N)
                    upCoeffs[static_cast<size_t> (p) * cpp + k] = proto[static_cast<size_t> (idx)] * static_cast<float> (L);
            }
    }

    int L = 1, taps = 16, N = 17, cpp = 17, HR = 18;
    std::vector<float> proto;      // decimation prototype (length N)
    std::vector<float> upCoeffs;   // interpolation phases  (L * cpp)

    std::vector<float> inHist;     // base-rate input ring  (length cpp)
    std::vector<float> hrHist;     // high-rate output ring (length HR)
    int pIn = 0, pHr = 0;
};

} // namespace octa
