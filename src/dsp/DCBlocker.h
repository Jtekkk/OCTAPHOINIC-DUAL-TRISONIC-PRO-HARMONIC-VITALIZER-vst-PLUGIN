// =============================================================================
//  DCBlocker.h  --  First-order DC-removal high-pass.
//
//  The even-order harmonic generator is an asymmetric / even-symmetric shaper
//  and therefore injects a DC offset. We remove it with the classic one-pole
//  differentiator-leaky-integrator:  y[n] = x[n] - x[n-1] + R * y[n-1].
// =============================================================================
#pragma once

#include <cmath>
#include "MathConstants.h"

namespace octa
{

class DCBlocker
{
public:
    // fc is the -3 dB corner in Hz (≈5-20 Hz is typical).
    void prepare (double sampleRate, double fc = 12.0)
    {
        // R places the pole near z = 1; derived from the bilinear approximation
        // of a one-pole high-pass corner frequency.
        R = 1.0 - (2.0 * octa::kPi * fc / sampleRate);
        if (R < 0.0)  R = 0.0;
        if (R > 0.9999) R = 0.9999;
        reset();
    }

    inline float processSample (float x) noexcept
    {
        const double in = static_cast<double> (x);
        const double y  = in - x1 + R * y1;
        x1 = in;
        y1 = y;
        return static_cast<float> (y);
    }

    void reset() noexcept { x1 = y1 = 0.0; }

private:
    double R  = 0.999;
    double x1 = 0.0, y1 = 0.0;
};

} // namespace octa
