// =============================================================================
//  Biquad.h  --  Single second-order IIR filter section (RBJ cookbook).
//
//  Part of the OCTAPHONIC DUAL-TRISONIC PRO-HARMONIC VITALIZER DSP core.
//  This file is intentionally free of any JUCE / framework dependency so the
//  whole DSP engine can be unit-tested with a plain C++17 compiler.
// =============================================================================
#pragma once

#include <cmath>

namespace octa
{

// A classic Robert Bristow-Johnson biquad implemented as a Transposed
// Direct-Form II structure (good numerical behaviour for audio, only two
// state variables). Coefficients are stored already normalised by a0.
//
// The design methods below name their working coefficients with a leading 'c'
// (cb0, ca1, ...) so they never shadow the stored members (b0, a1, ...).
class Biquad
{
public:
    Biquad() = default;

    // ---- Coefficient setters (design equations from the RBJ cookbook) -------

    void setLowpass (double sampleRate, double freqHz, double q)
    {
        const double w0 = omega (sampleRate, freqHz);
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double cb0 = (1.0 - cs) * 0.5;
        const double cb1 =  1.0 - cs;
        const double cb2 = (1.0 - cs) * 0.5;
        const double ca0 =  1.0 + alpha;
        const double ca1 = -2.0 * cs;
        const double ca2 =  1.0 - alpha;
        normalise (cb0, cb1, cb2, ca0, ca1, ca2);
    }

    void setHighpass (double sampleRate, double freqHz, double q)
    {
        const double w0 = omega (sampleRate, freqHz);
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double cb0 =  (1.0 + cs) * 0.5;
        const double cb1 = -(1.0 + cs);
        const double cb2 =  (1.0 + cs) * 0.5;
        const double ca0 =   1.0 + alpha;
        const double ca1 =  -2.0 * cs;
        const double ca2 =   1.0 - alpha;
        normalise (cb0, cb1, cb2, ca0, ca1, ca2);
    }

    // A second-order all-pass: flat magnitude, frequency dependent phase.
    // Used to keep band phases coherent across a multi-way crossover.
    void setAllpass (double sampleRate, double freqHz, double q)
    {
        const double w0 = omega (sampleRate, freqHz);
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double cb0 =  1.0 - alpha;
        const double cb1 = -2.0 * cs;
        const double cb2 =  1.0 + alpha;
        const double ca0 =  1.0 + alpha;
        const double ca1 = -2.0 * cs;
        const double ca2 =  1.0 - alpha;
        normalise (cb0, cb1, cb2, ca0, ca1, ca2);
    }

    // High-shelf used by the "AIR" band of the vitalizer.
    void setHighShelf (double sampleRate, double freqHz, double q, double gainDb)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = omega (sampleRate, freqHz);
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double twoSqrtAalpha = 2.0 * std::sqrt (A) * alpha;

        const double cb0 =      A * ((A + 1.0) + (A - 1.0) * cs + twoSqrtAalpha);
        const double cb1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
        const double cb2 =      A * ((A + 1.0) + (A - 1.0) * cs - twoSqrtAalpha);
        const double ca0 =           (A + 1.0) - (A - 1.0) * cs + twoSqrtAalpha;
        const double ca1 =    2.0 * ((A - 1.0) - (A + 1.0) * cs);
        const double ca2 =           (A + 1.0) - (A - 1.0) * cs - twoSqrtAalpha;
        normalise (cb0, cb1, cb2, ca0, ca1, ca2);
    }

    // ---- Runtime --------------------------------------------------------------

    inline float processSample (float x) noexcept
    {
        const double in = static_cast<double> (x);
        const double y  = b0 * in + s1;
        s1 = b1 * in - a1 * y + s2;
        s2 = b2 * in - a2 * y;
        return static_cast<float> (y);
    }

    void reset() noexcept { s1 = s2 = 0.0; }

private:
    static double omega (double sampleRate, double freqHz)
    {
        // Clamp to a sane range so we never evaluate at or past Nyquist.
        if (freqHz < 1.0)                 freqHz = 1.0;
        if (freqHz > 0.49 * sampleRate)   freqHz = 0.49 * sampleRate;
        return 2.0 * M_PI * freqHz / sampleRate;
    }

    void normalise (double cb0, double cb1, double cb2,
                    double ca0, double ca1, double ca2)
    {
        const double inv = 1.0 / ca0;
        b0 = cb0 * inv; b1 = cb1 * inv; b2 = cb2 * inv;
        a1 = ca1 * inv; a2 = ca2 * inv;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;   // feed-forward (normalised)
    double a1 = 0.0, a2 = 0.0;             // feed-back    (normalised, a0 == 1)
    double s1 = 0.0, s2 = 0.0;             // state
};

} // namespace octa
