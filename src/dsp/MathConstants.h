// =============================================================================
//  MathConstants.h  --  Portable math constants.
//
//  M_PI is a POSIX extension and is NOT guaranteed by the C++ standard; MSVC and
//  MinGW only expose it when _USE_MATH_DEFINES is set before <cmath>. To keep
//  the DSP core portable across Linux / macOS / Windows we use our own constant.
// =============================================================================
#pragma once

namespace octa
{
    inline constexpr double kPi    = 3.14159265358979323846;
    inline constexpr double kTwoPi = 6.28318530717958647692;
}
