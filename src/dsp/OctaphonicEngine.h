// =============================================================================
//  OctaphonicEngine.h  --  Top-level DSP engine.
//
//  OCTAPHONIC DUAL-TRISONIC PRO-HARMONIC VITALIZER signal flow (per channel):
//
//      in -> input gain -> 3-band LR4 crossover
//                              |-- Low  --> [Lx oversampled dual-shaper] -> DC -> gain --|
//                              |-- Mid  --> [Lx oversampled dual-shaper] -> DC -> gain --+--> wet sum
//                              |-- High --> [Lx oversampled dual-shaper] -> DC -> gain --|
//                                                                                         |
//                                                              air high-shelf  <----------+
//                                                                     |
//      out <- output gain <- DC block <- ( dry(delayed)*(1-mix) + wet*mix )
//
//  The engine is completely independent of JUCE so it can be unit tested with a
//  bare C++17 compiler (see tests/).
// =============================================================================
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

#include "Parameters.h"
#include "Biquad.h"
#include "DCBlocker.h"
#include "Crossover.h"
#include "HarmonicGenerator.h"
#include "Oversampler.h"

namespace octa
{

// Exponential one-pole parameter smoother (de-zippering for per-sample reads).
struct Smoother
{
    void  prepare (double sr, double ms)
    {
        const double tau = std::max (ms, 0.01) * 0.001;
        a = static_cast<float> (std::exp (-1.0 / (sr * tau)));
    }
    void  snap (float v) noexcept { cur = target = v; }
    void  setTarget (float t) noexcept { target = t; }
    inline float next() noexcept { cur = target + a * (cur - target); return cur; }

    float cur = 0.0f, target = 0.0f, a = 0.0f;
};

// Simple integer delay line used to time-align the dry path with the
// oversampler's group delay.
struct DelayLine
{
    void prepare (int maxLatency)
    {
        buf.assign (static_cast<size_t> (std::max (1, maxLatency + 1)), 0.0f);
        write = 0;
        latency = 0;
    }
    void setLatency (int l) noexcept
    {
        latency = std::max (0, std::min (l, static_cast<int> (buf.size()) - 1));
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); write = 0; }

    inline float process (float x) noexcept
    {
        if (latency == 0) return x;
        buf[static_cast<size_t> (write)] = x;
        const int read = (write - latency + static_cast<int> (buf.size())) % static_cast<int> (buf.size());
        write = (write + 1) % static_cast<int> (buf.size());
        return buf[static_cast<size_t> (read)];
    }

    std::vector<float> buf;
    int write = 0, latency = 0;
};

class OctaphonicEngine
{
public:
    OctaphonicEngine() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Push a new parameter snapshot (cheap; only recomputes filter coefficients
    // when the relevant values actually change).
    void setSettings (const Settings& s);

    // Change oversampling ratio. NOTE: re-allocates filter state, so it is not
    // hard real-time safe -- prefer to call it when transport is stopped.
    void setOversamplingFactor (int factor);

    int  latencySamples() const noexcept { return currentLatency; }

    // In-place processing. channels is an array of numCh pointers, each
    // numSamples long.
    void process (float* const* channels, int numCh, int numSamples);

private:
    struct ChannelState
    {
        ThreeBandCrossover crossover;
        Oversampler        os[kNumBands];
        DCBlocker          bandDC[kNumBands];
        DCBlocker          outDC;
        Biquad             air;
        DelayLine          dryDelay;
    };

    void rebuildOversamplers();
    void updateDerived();      // recompute things that depend on the latest Settings

    double sr = 44100.0;
    int    maxBlock = 512;
    int    numChannels = 2;
    int    osFactor = 4;
    int    osTapsPerPhase = 16;
    int    currentLatency = 0;

    Settings settings;       // latest snapshot
    Settings applied;        // last snapshot whose coefficients we committed

    // Per-block derived values (constant across the block).
    float driveLin[kNumBands]   = { 1, 1, 1 };
    float charVal[kNumBands]    = { 0.6f, 0.6f, 0.6f };
    float bandActive[kNumBands] = { 1, 1, 1 };

    // Smoothers (per-sample, shared across channels).
    Smoother inGain, outGain, mixWet, bandGain[kNumBands];

    std::vector<ChannelState> ch;
    bool prepared = false;
};

} // namespace octa
