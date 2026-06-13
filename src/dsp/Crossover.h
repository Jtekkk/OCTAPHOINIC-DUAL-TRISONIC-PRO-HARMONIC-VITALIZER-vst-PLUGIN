// =============================================================================
//  Crossover.h  --  Three-band Linkwitz-Riley (LR4) crossover.  ["TRISONIC"]
//
//  An LR4 section is two cascaded Butterworth (Q = 1/sqrt2) biquads. The LR4
//  low-pass and high-pass at the same corner are in phase and sum to an
//  all-pass, so no polarity inversion is required.
//
//  For a three-way split the LOW band (split off at f1) must additionally be
//  passed through an all-pass equivalent of the f2 split, otherwise the
//  recombined output is not magnitude-flat around f2. We realise that all-pass
//  as LP_f2 + HP_f2 of the low band, giving overall flat-magnitude
//  reconstruction (the full-band sum is a pure all-pass).
// =============================================================================
#pragma once

#include "Biquad.h"

namespace octa
{

// One 4th-order Linkwitz-Riley filter = two identical Butterworth biquads.
struct LR4
{
    enum class Type { lowpass, highpass };

    void set (Type t, double sampleRate, double freqHz)
    {
        constexpr double butterQ = 0.70710678118654752; // 1/sqrt(2)
        if (t == Type::lowpass)
        {
            a.setLowpass (sampleRate, freqHz, butterQ);
            b.setLowpass (sampleRate, freqHz, butterQ);
        }
        else
        {
            a.setHighpass (sampleRate, freqHz, butterQ);
            b.setHighpass (sampleRate, freqHz, butterQ);
        }
    }

    inline float processSample (float x) noexcept { return b.processSample (a.processSample (x)); }
    void reset() noexcept { a.reset(); b.reset(); }

    Biquad a, b;
};

class ThreeBandCrossover
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateCoefficients();
        reset();
    }

    // f1 = low/mid corner, f2 = mid/high corner. f2 is kept above f1.
    void setFrequencies (double lowMidHz, double midHighHz)
    {
        lowMid  = lowMidHz;
        midHigh = (midHighHz > lowMidHz * 1.05) ? midHighHz : lowMidHz * 1.05;
        updateCoefficients();
    }

    void reset() noexcept
    {
        lp1.reset(); hp1.reset();
        lp2.reset(); hp2.reset();
        lowCompLp.reset(); lowCompHp.reset();
    }

    // Split one input sample into three magnitude-complementary bands.
    inline void processSample (float x, float& low, float& mid, float& high) noexcept
    {
        const float low1  = lp1.processSample (x);   // everything below f1
        const float high1 = hp1.processSample (x);   // everything above f1

        mid  = lp2.processSample (high1);             // f1 .. f2
        high = hp2.processSample (high1);             // above f2

        // Phase-compensate the low band against the f2 split (LP+HP = all-pass).
        low  = lowCompLp.processSample (low1) + lowCompHp.processSample (low1);
    }

private:
    void updateCoefficients()
    {
        if (sampleRate <= 0.0) return;
        lp1.set (LR4::Type::lowpass,  sampleRate, lowMid);
        hp1.set (LR4::Type::highpass, sampleRate, lowMid);
        lp2.set (LR4::Type::lowpass,  sampleRate, midHigh);
        hp2.set (LR4::Type::highpass, sampleRate, midHigh);
        lowCompLp.set (LR4::Type::lowpass,  sampleRate, midHigh);
        lowCompHp.set (LR4::Type::highpass, sampleRate, midHigh);
    }

    double sampleRate = 0.0;
    double lowMid  = 200.0;
    double midHigh = 3000.0;

    LR4 lp1, hp1;                 // first split (at f1)
    LR4 lp2, hp2;                 // second split of the upper half (at f2)
    LR4 lowCompLp, lowCompHp;     // all-pass compensation of the low band (at f2)
};

} // namespace octa
