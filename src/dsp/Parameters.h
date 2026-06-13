// =============================================================================
//  Parameters.h  --  Single source of truth for every control.
//
//  Both the JUCE-independent engine and the plugin layer include this file so
//  parameter IDs, ranges and defaults can never drift apart. The Settings POD
//  is the contract the host/UI fills in and the engine consumes once per block.
// =============================================================================
#pragma once

namespace octa
{

static constexpr int kNumBands = 3;          // Low / Mid / High  ["TRISONIC"]

enum Band { kLow = 0, kMid = 1, kHigh = 2 };

// ---- Parameter identifiers (stable strings used for host automation) --------
namespace pid
{
    inline constexpr const char* inputGain    = "input_gain";
    inline constexpr const char* outputGain   = "output_gain";
    inline constexpr const char* mix          = "vitalize_mix";
    inline constexpr const char* air          = "air";
    inline constexpr const char* oversampling = "oversampling";
    inline constexpr const char* bypass       = "bypass";

    inline constexpr const char* xLowMid      = "xover_low_mid";
    inline constexpr const char* xMidHigh     = "xover_mid_high";

    // Per-band IDs, indexed [band].
    inline constexpr const char* drive[kNumBands]     = { "drive_low",  "drive_mid",  "drive_high"  };
    inline constexpr const char* character[kNumBands] = { "char_low",   "char_mid",   "char_high"   };
    inline constexpr const char* bandGain[kNumBands]  = { "gain_low",   "gain_mid",   "gain_high"   };
    inline constexpr const char* mute[kNumBands]      = { "mute_low",   "mute_mid",   "mute_high"   };
    inline constexpr const char* solo[kNumBands]      = { "solo_low",   "solo_mid",   "solo_high"   };
}

// ---- Ranges / defaults ------------------------------------------------------
namespace range
{
    // Global
    inline constexpr float gainMinDb   = -24.0f, gainMaxDb = 24.0f, gainDefDb = 0.0f;
    inline constexpr float mixMin       = 0.0f,  mixMax    = 1.0f,  mixDef    = 0.35f;
    inline constexpr float airMinDb     = 0.0f,  airMaxDb  = 12.0f, airDefDb  = 3.0f;
    inline constexpr float airFreqHz    = 8000.0f;   // fixed high-shelf corner

    // Crossover
    inline constexpr float lowMidMinHz  = 40.0f,  lowMidMaxHz  = 1000.0f,  lowMidDefHz  = 250.0f;
    inline constexpr float midHighMinHz = 800.0f, midHighMaxHz = 16000.0f, midHighDefHz = 4000.0f;

    // Per band
    inline constexpr float driveMinDb   = 0.0f,   driveMaxDb   = 36.0f;
    inline constexpr float charMin       = 0.0f,  charMax      = 1.0f;

    // Musical defaults: low band warm/even, high band bright/odd.
    inline constexpr float driveDefDb[kNumBands] = { 6.0f, 7.0f, 9.0f };
    inline constexpr float charDef[kNumBands]    = { 0.35f, 0.50f, 0.70f };
}

// Oversampling choice index -> factor.
inline int oversampleFactorForIndex (int index) noexcept
{
    switch (index)
    {
        case 0:  return 1;
        case 1:  return 2;
        case 2:  return 4;
        case 3:  return 8;
        default: return 2;
    }
}
inline constexpr int kOversamplingDefaultIndex = 2;   // 4x

// ---- Settings POD: the per-block snapshot the engine reads ------------------
struct BandSettings
{
    float driveDb   = 6.0f;     // pre-shaper gain
    float character = 0.6f;     // 0 = even, 1 = odd
    float gainDb    = 0.0f;     // band makeup
    bool  mute      = false;
    bool  solo      = false;
};

struct Settings
{
    float inputGainDb  = 0.0f;
    float outputGainDb = 0.0f;
    float mix          = 0.35f; // dry/wet (the "VITALIZE" amount)
    float airDb        = 3.0f;
    bool  bypass       = false;

    float lowMidHz     = 250.0f;
    float midHighHz    = 4000.0f;

    BandSettings band[kNumBands];
};

} // namespace octa
