// =============================================================================
//  OctaphonicEngine.cpp  --  Implementation of the top-level DSP engine.
// =============================================================================
#include "OctaphonicEngine.h"

namespace octa
{

static inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * (1.0f / 20.0f)); }

void OctaphonicEngine::prepare (double sampleRate, int maxBlockSize, int channels)
{
    sr          = (sampleRate > 0.0) ? sampleRate : 44100.0;
    maxBlock    = std::max (1, maxBlockSize);
    numChannels = std::max (1, channels);

    ch.assign (static_cast<size_t> (numChannels), ChannelState{});

    // Smoothers: ~15 ms is fast enough to track automation yet kills zipper noise.
    const double smoothMs = 15.0;
    inGain.prepare (sr, smoothMs);
    outGain.prepare (sr, smoothMs);
    mixWet.prepare (sr, smoothMs);
    for (auto& g : bandGain) g.prepare (sr, smoothMs);

    inGain.snap (dbToLin (settings.inputGainDb));
    outGain.snap (dbToLin (settings.outputGainDb));
    mixWet.snap (settings.mix);
    for (int b = 0; b < kNumBands; ++b)
        bandGain[b].snap (dbToLin (settings.band[b].gainDb));

    for (auto& c : ch)
    {
        c.crossover.prepare (sr);
        c.crossover.setFrequencies (settings.lowMidHz, settings.midHighHz);
        c.outDC.prepare (sr);
        for (int b = 0; b < kNumBands; ++b)
            c.bandDC[b].prepare (sr);
        c.air.setHighShelf (sr, range::airFreqHz, 0.70710678, settings.airDb);
    }

    rebuildOversamplers();
    prepared = true;

    applied = settings;
    updateDerived();
}

void OctaphonicEngine::rebuildOversamplers()
{
    for (auto& c : ch)
    {
        c.dryDelay.prepare (osTapsPerPhase + 4);   // headroom over max latency
        for (int b = 0; b < kNumBands; ++b)
            c.os[b].prepare (osFactor, osTapsPerPhase);
    }

    currentLatency = ch.empty() ? 0 : ch[0].os[0].latencySamples();
    for (auto& c : ch)
        c.dryDelay.setLatency (currentLatency);
}

void OctaphonicEngine::setOversamplingFactor (int factor)
{
    if (factor != osFactor)
    {
        osFactor = factor;
        if (prepared)
            rebuildOversamplers();
    }
}

void OctaphonicEngine::reset()
{
    for (auto& c : ch)
    {
        c.crossover.reset();
        c.outDC.reset();
        c.air.reset();
        c.dryDelay.reset();
        for (int b = 0; b < kNumBands; ++b)
        {
            c.os[b].reset();
            c.bandDC[b].reset();
        }
    }
}

void OctaphonicEngine::setSettings (const Settings& s)
{
    settings = s;
    if (prepared)
        updateDerived();
}

void OctaphonicEngine::updateDerived()
{
    // Targets for smoothed values.
    inGain.setTarget (dbToLin (settings.inputGainDb));
    outGain.setTarget (dbToLin (settings.outputGainDb));
    mixWet.setTarget (settings.mix);

    for (int b = 0; b < kNumBands; ++b)
    {
        driveLin[b] = dbToLin (settings.band[b].driveDb);
        charVal[b]  = settings.band[b].character;
        bandGain[b].setTarget (dbToLin (settings.band[b].gainDb));
    }

    // Solo overrides mute: if any band is soloed, only soloed bands are heard.
    const bool anySolo = settings.band[0].solo || settings.band[1].solo || settings.band[2].solo;
    for (int b = 0; b < kNumBands; ++b)
    {
        const bool on = anySolo ? settings.band[b].solo : ! settings.band[b].mute;
        bandActive[b] = on ? 1.0f : 0.0f;
    }

    // Only rebuild filter coefficients when their inputs actually changed.
    if (settings.lowMidHz != applied.lowMidHz || settings.midHighHz != applied.midHighHz)
        for (auto& c : ch)
            c.crossover.setFrequencies (settings.lowMidHz, settings.midHighHz);

    if (settings.airDb != applied.airDb)
        for (auto& c : ch)
            c.air.setHighShelf (sr, range::airFreqHz, 0.70710678, settings.airDb);

    applied = settings;
}

void OctaphonicEngine::process (float* const* channels, int numCh, int numSamples)
{
    if (! prepared) return;

    const int chN = std::min (numCh, static_cast<int> (ch.size()));
    const bool bypass = settings.bypass;

    // Sample-major: advance the shared smoothers once per sample and apply the
    // same value to every channel so all channels stay phase/level aligned.
    for (int n = 0; n < numSamples; ++n)
    {
        const float inG  = inGain.next();
        const float outG = outGain.next();
        const float mix  = mixWet.next();
        float bg[kNumBands];
        for (int b = 0; b < kNumBands; ++b)
            bg[b] = bandGain[b].next() * bandActive[b];

        for (int c = 0; c < chN; ++c)
        {
            ChannelState& st = ch[static_cast<size_t> (c)];
            float* x = channels[c];

            const float rawIn   = x[n];
            const float drySamp = st.dryDelay.process (rawIn);   // latency-aligned dry

            if (bypass)
            {
                // Transparent, but keep the reported latency so toggling bypass
                // does not shift timing in the host.
                x[n] = drySamp;
                continue;
            }

            const float xin = rawIn * inG;

            float lo, mid, hi;
            st.crossover.processSample (xin, lo, mid, hi);
            const float bandIn[kNumBands] = { lo, mid, hi };

            float wet = 0.0f;
            for (int b = 0; b < kNumBands; ++b)
            {
                const float d   = driveLin[b];
                const float chr = charVal[b];

                float shaped = st.os[b].processSample (bandIn[b],
                    [d, chr] (float v) noexcept { return HarmonicGenerator::shape (v, d, chr); });

                shaped = st.bandDC[b].processSample (shaped);
                shaped *= bg[b];
                wet += shaped;
            }

            wet = st.air.processSample (wet);   // "AIR" sheen on the harmonic sum

            float out = drySamp * inG * (1.0f - mix) + wet * mix;
            out *= outG;
            out = st.outDC.processSample (out);

            // Safety: never let a NaN/Inf escape to the host.
            if (! std::isfinite (out)) out = 0.0f;
            x[n] = out;
        }
    }
}

} // namespace octa
