// =============================================================================
//  HarmonicGenerator.h  --  Dual (even + odd) waveshaping core.  ["DUAL"]
//
//  Two complementary memory-less nonlinearities are blended by a "character"
//  control:
//
//    * ODD  path :  tanh(D * x)              -> symmetric  -> odd  harmonics
//                                                (3rd, 5th, ...) and a
//                                                fundamental that is preserved
//                                                in the low-signal region.
//
//    * EVEN path :  2 * x * tanh(D * x)       -> even-symmetric -> even
//                                                harmonics (2nd, 4th, ...) plus
//                                                a DC term that is removed by a
//                                                DC blocker downstream.
//
//  character = 1 -> pure odd, character = 0 -> pure even, in between -> a blend.
//  The result is divided by the drive so the odd path stays near unity gain for
//  quiet signals while progressively saturating as the drive is raised.
//
//  The shaper is intentionally state-free: it is evaluated once per
//  *oversampled* sample by the Oversampler, which is where alias suppression
//  happens.
// =============================================================================
#pragma once

#include <cmath>
#include <algorithm>

namespace octa
{

struct HarmonicGenerator
{
    // drive     : linear pre-gain into the shaper (>= 1 typical, never < ~0.001)
    // character : 0 = even-dominant, 1 = odd-dominant
    static inline float shape (float x, float drive, float character) noexcept
    {
        const float D = std::max (drive, 1.0e-3f);
        const float t = std::tanh (D * x);          // shared term, computed once

        const float odd  = t;                       // tanh(D x)        -> odd
        const float even = 2.0f * x * t;            // 2x*tanh(D x)     -> even (+DC)

        const float blended = character * odd + (1.0f - character) * even;
        return blended / D;                         // rough level normalisation
    }
};

} // namespace octa
