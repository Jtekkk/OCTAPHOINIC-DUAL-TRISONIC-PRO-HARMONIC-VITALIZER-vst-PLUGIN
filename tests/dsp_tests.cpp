// =============================================================================
//  dsp_tests.cpp  --  Offline verification of the OCTAPHONIC DSP core.
//
//  These tests run with a plain C++17 compiler (no JUCE) and exercise the real
//  audio path: polyphase oversampling, the dual harmonic shaper, the LR4
//  crossover and the full engine. Spectral assertions use the bundled mini FFT.
//
//  Exit code 0 = all passed, 1 = at least one failure.
// =============================================================================
#include <cstdio>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

#include "../src/dsp/OctaphonicEngine.h"
#include "../src/dsp/Oversampler.h"
#include "../src/dsp/Crossover.h"
#include "../src/dsp/HarmonicGenerator.h"
#include "mini_fft.h"

using namespace octa;

// ----------------------------------------------------------------- framework
static int g_pass = 0, g_fail = 0;

static void check (bool cond, const std::string& msg)
{
    if (cond) { ++g_pass; std::printf ("  [ ok ] %s\n", msg.c_str()); }
    else      { ++g_fail; std::printf ("  [FAIL] %s\n", msg.c_str()); }
}

// ----------------------------------------------------------------- helpers
static std::vector<float> genSine (double freq, double sr, int N, double amp = 1.0, double phase = 0.0)
{
    std::vector<float> x (static_cast<size_t> (N));
    for (int n = 0; n < N; ++n)
        x[static_cast<size_t> (n)] = static_cast<float> (amp * std::sin (2.0 * M_PI * freq * n / sr + phase));
    return x;
}

static double rms (const std::vector<float>& x, int start = 0, int end = -1)
{
    if (end < 0) end = static_cast<int> (x.size());
    double acc = 0.0; int cnt = 0;
    for (int n = start; n < end; ++n) { acc += static_cast<double> (x[static_cast<size_t>(n)]) * x[static_cast<size_t>(n)]; ++cnt; }
    return cnt ? std::sqrt (acc / cnt) : 0.0;
}

static double maxAbs (const std::vector<float>& x)
{
    double m = 0.0;
    for (float v : x) m = std::max (m, std::fabs (static_cast<double> (v)));
    return m;
}

static bool allFinite (const std::vector<float>& x)
{
    for (float v : x) if (! std::isfinite (v)) return false;
    return true;
}

static double db (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-12)); }

// ============================================================================
//  1. Oversampler in-band reconstruction (identity nonlinearity ~= pure delay)
// ============================================================================
static void testOversamplerReconstruction()
{
    std::printf ("Oversampler reconstruction (identity -> delayed copy):\n");
    const double sr = 48000.0;
    const int    N  = 8192;

    for (int factor : { 2, 4, 8 })
    {
        Oversampler os;
        os.prepare (factor, 16);
        const int lat = os.latencySamples();

        auto in = genSine (1000.0, sr, N + lat + 64, 0.7);
        std::vector<float> out (in.size());
        for (size_t i = 0; i < in.size(); ++i)
            out[i] = os.processSample (in[i], [] (float v) noexcept { return v; }); // identity

        // Compare out[n] against in[n - lat], past the start-up transient.
        const int settle = lat + 256;
        double errAcc = 0.0, sigAcc = 0.0; int cnt = 0;
        for (int n = settle; n < N; ++n)
        {
            const double ref = in[static_cast<size_t> (n - lat)];
            const double e   = out[static_cast<size_t> (n)] - ref;
            errAcc += e * e; sigAcc += ref * ref; ++cnt;
        }
        const double snr = db (std::sqrt (sigAcc / cnt) / std::sqrt (errAcc / cnt));
        check (snr > 45.0,
               "factor " + std::to_string (factor) + ": reconstruction SNR " +
               std::to_string (static_cast<int> (snr)) + " dB > 45 dB");
    }
}

// ============================================================================
//  2. Oversampling suppresses aliasing from a hard nonlinearity
// ============================================================================
static void testAntialiasing()
{
    std::printf ("Oversampling reduces aliasing of a saturated high tone:\n");
    const double sr = 48000.0;
    const int    N  = 32768;
    const double f0 = mfft::snapToBin (7000.0, N, sr);

    // 5th harmonic of 7 kHz = 35 kHz, which folds to 48-35 = 13 kHz when run at
    // base rate. Heavy odd-harmonic drive makes that alias clearly visible.
    auto shaper = [] (float v) noexcept { return HarmonicGenerator::shape (v, 12.0f, 1.0f); };

    auto runAt = [&] (int factor) -> double
    {
        Oversampler os; os.prepare (factor, 16);
        auto in = genSine (f0, sr, N, 0.9);
        std::vector<float> out (static_cast<size_t> (N));
        for (int n = 0; n < N; ++n) out[static_cast<size_t> (n)] = os.processSample (in[static_cast<size_t> (n)], shaper);
        auto mag = mfft::magnitudeSpectrum (out);
        const int aliasBin = mfft::binFor (13000.0, N, sr);
        // Peak around the alias frequency (allow +-2 bins).
        double peak = 0.0;
        for (int b = aliasBin - 2; b <= aliasBin + 2; ++b) peak = std::max (peak, mag[static_cast<size_t> (b)]);
        return peak;
    };

    const double alias1x = runAt (1);
    const double alias8x = runAt (8);
    check (alias8x < alias1x * 0.25,
           "alias at 13 kHz: 8x is >12 dB below 1x (" +
           std::to_string (static_cast<int> (db (alias8x / std::max (alias1x, 1e-12)))) + " dB)");
}

// ============================================================================
//  3. Dual shaper produces the expected odd / even harmonic balance
// ============================================================================
static void testHarmonicBalance()
{
    std::printf ("Dual shaper: character morphs odd <-> even harmonics:\n");
    const double sr = 48000.0;
    const int    N  = 32768;
    const double f0 = mfft::snapToBin (900.0, N, sr);
    const int b1 = mfft::binFor (1.0 * f0, N, sr);
    const int b2 = mfft::binFor (2.0 * f0, N, sr);
    const int b3 = mfft::binFor (3.0 * f0, N, sr);

    auto runChar = [&] (float character)
    {
        Oversampler os; os.prepare (8, 16);
        auto in = genSine (f0, sr, N, 0.6);
        std::vector<float> out (static_cast<size_t> (N));
        for (int n = 0; n < N; ++n)
            out[static_cast<size_t> (n)] = os.processSample (in[static_cast<size_t> (n)],
                [character] (float v) noexcept { return HarmonicGenerator::shape (v, 8.0f, character); });
        return mfft::magnitudeSpectrum (out);
    };

    auto magOdd  = runChar (1.0f);   // expect 3rd >> 2nd, fundamental preserved
    auto magEven = runChar (0.0f);   // expect 2nd >> 3rd, fundamental suppressed

    check (magOdd[static_cast<size_t>(b3)] > 4.0 * magOdd[static_cast<size_t>(b2)],
           "character=1: 3rd harmonic dominates 2nd (odd)");
    check (magEven[static_cast<size_t>(b2)] > 4.0 * magEven[static_cast<size_t>(b3)],
           "character=0: 2nd harmonic dominates 3rd (even)");
    check (magOdd[static_cast<size_t>(b1)] > 3.0 * magEven[static_cast<size_t>(b1)],
           "fundamental stronger for odd than even shaping");
}

// ============================================================================
//  4. LR4 three-band crossover reconstructs to a flat magnitude (all-pass sum)
// ============================================================================
static void testCrossoverFlatness()
{
    std::printf ("Three-band crossover sums to flat magnitude:\n");
    const double sr = 48000.0;
    const int    N  = 32768;

    ThreeBandCrossover xo;
    xo.prepare (sr);
    xo.setFrequencies (250.0, 4000.0);

    std::vector<float> recon (static_cast<size_t> (N), 0.0f);
    // Unit impulse -> summed band impulse response.
    for (int n = 0; n < N; ++n)
    {
        const float x = (n == 0) ? 1.0f : 0.0f;
        float lo, mid, hi;
        xo.processSample (x, lo, mid, hi);
        recon[static_cast<size_t> (n)] = lo + mid + hi;
    }

    auto mag = mfft::magnitudeSpectrum (recon);
    // Check magnitude is ~0 dB across the audible band.
    double worst = 0.0;
    for (double f = 50.0; f < 20000.0; f *= 1.05)
    {
        const int bin = mfft::binFor (f, N, sr);
        worst = std::max (worst, std::fabs (db (mag[static_cast<size_t> (bin)])));
    }
    check (worst < 0.5, "max deviation from 0 dB is " + std::to_string (worst) + " dB (< 0.5)");
}

// ============================================================================
//  5. Full engine: stability, finiteness, latency, bypass, DC rejection
// ============================================================================
static void processStereo (OctaphonicEngine& eng, std::vector<float>& L, std::vector<float>& R, int block)
{
    const int n = static_cast<int> (L.size());
    for (int i = 0; i < n; i += block)
    {
        const int len = std::min (block, n - i);
        float* chans[2] = { L.data() + i, R.data() + i };
        eng.process (chans, 2, len);
    }
}

static void testEngine()
{
    std::printf ("Full engine: stability / finiteness / bypass / DC:\n");
    const double sr = 48000.0;
    const int block = 256;

    Settings s;                       // sensible defaults from Parameters.h
    s.mix = 0.6f;
    for (int b = 0; b < kNumBands; ++b) s.band[b].driveDb = 18.0f;

    OctaphonicEngine eng;
    eng.prepare (sr, block, 2);
    eng.setOversamplingFactor (4);
    eng.setSettings (s);

    // (a) silence in -> (near) silence out, no self-oscillation.
    {
        std::vector<float> L (8192, 0.0f), R (8192, 0.0f);
        processStereo (eng, L, R, block);
        check (allFinite (L) && allFinite (R), "silence: output finite");
        check (maxAbs (L) < 1e-5 && maxAbs (R) < 1e-5, "silence: output stays silent");
    }

    // (b) hot noise in -> finite, bounded out.
    {
        eng.reset();
        std::vector<float> L (1 << 15), R (1 << 15);
        unsigned int seed = 12345u;
        auto noise = [&seed]() { seed = seed * 1664525u + 1013904223u; return (static_cast<float> (seed >> 9) / 8388608.0f) * 2.0f - 1.0f; };
        for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.9f * noise(); R[i] = 0.9f * noise(); }
        processStereo (eng, L, R, block);
        check (allFinite (L) && allFinite (R), "noise: output finite (no NaN/Inf)");
        check (maxAbs (L) < 8.0 && maxAbs (R) < 8.0, "noise: output bounded");
    }

    // (c) impulse -> decays to silence (IIR stability).
    {
        eng.reset();
        std::vector<float> L (1 << 15, 0.0f), R (1 << 15, 0.0f);
        L[0] = R[0] = 1.0f;
        processStereo (eng, L, R, block);
        const double tail = rms (L, static_cast<int> (L.size()) - 2048, static_cast<int> (L.size()));
        check (allFinite (L), "impulse: output finite");
        check (tail < 1e-4, "impulse: tail decays (stable)");
    }

    // (d) bypass is transparent except for the reported latency.
    {
        Settings sb = s; sb.bypass = true;
        eng.setSettings (sb);
        eng.reset();
        const int lat = eng.latencySamples();
        auto base = genSine (440.0, sr, 8192 + lat + 64, 0.5);
        std::vector<float> L = base, R = base;
        processStereo (eng, L, R, block);

        double errAcc = 0.0, sigAcc = 0.0; int cnt = 0;
        for (int n = lat + 256; n < 8192; ++n)
        {
            const double ref = base[static_cast<size_t> (n - lat)];
            const double e   = L[static_cast<size_t> (n)] - ref;
            errAcc += e * e; sigAcc += ref * ref; ++cnt;
        }
        const double snr = db (std::sqrt (sigAcc / cnt) / std::sqrt (std::max (errAcc / cnt, 1e-30)));
        check (snr > 80.0, "bypass: transparent within latency (SNR " + std::to_string (static_cast<int> (snr)) + " dB)");
    }

    // (e) latency is reported and positive for oversampled processing.
    {
        check (eng.latencySamples() > 0, "latency reported > 0 for oversampled mode");
    }

    // (f) DC offset on the input is removed.
    {
        Settings sd = s; sd.bypass = false; sd.mix = 1.0f;
        eng.setSettings (sd);
        eng.reset();
        auto tone = genSine (300.0, sr, 1 << 15, 0.3);
        std::vector<float> L (tone.size()), R (tone.size());
        for (size_t i = 0; i < tone.size(); ++i) { L[i] = tone[i] + 0.5f; R[i] = tone[i] + 0.5f; } // +0.5 DC
        processStereo (eng, L, R, block);
        // Mean of the settled region should be ~0.
        double mean = 0.0; int cnt = 0;
        for (size_t i = L.size() / 2; i < L.size(); ++i) { mean += L[i]; ++cnt; }
        mean /= cnt;
        check (allFinite (L) && std::fabs (mean) < 1e-3, "input DC removed from output (residual mean " + std::to_string (mean) + ")");
    }
}

// ============================================================================
//  6. Engine end-to-end: a single soloed band actually adds harmonics
// ============================================================================
static void testEngineHarmonics()
{
    std::printf ("Engine end-to-end: soloed band adds harmonics to a clean tone:\n");
    const double sr = 48000.0;
    const int    N  = 32768;
    const int block = 512;
    const double f0 = mfft::snapToBin (1000.0, N, sr);

    Settings s;
    s.mix = 1.0f;                       // fully wet
    s.lowMidHz = 300.0f; s.midHighHz = 4000.0f;
    s.band[kMid].solo = true;           // isolate the mid band (contains 1 kHz)
    s.band[kMid].driveDb = 24.0f;
    s.band[kMid].character = 1.0f;      // odd harmonics

    OctaphonicEngine eng;
    eng.prepare (sr, block, 1);
    eng.setOversamplingFactor (8);
    eng.setSettings (s);

    auto tone = genSine (f0, sr, N, 0.5);
    std::vector<float> x = tone;
    for (int i = 0; i < N; i += block)
    {
        const int len = std::min (block, N - i);
        float* chans[1] = { x.data() + i };
        eng.process (chans, 1, len);
    }

    auto mag = mfft::magnitudeSpectrum (x);
    const double h1 = mag[static_cast<size_t> (mfft::binFor (1.0 * f0, N, sr))];
    const double h3 = mag[static_cast<size_t> (mfft::binFor (3.0 * f0, N, sr))];

    check (allFinite (x), "engine harmonic test: output finite");
    check (h3 > 0.02 * h1, "3rd harmonic generated (h3/h1 = " + std::to_string (h3 / std::max (h1, 1e-9)) + ")");
}

// ============================================================================
int main()
{
    std::printf ("=============================================================\n");
    std::printf (" OCTAPHONIC DUAL-TRISONIC PRO-HARMONIC VITALIZER -- DSP tests\n");
    std::printf ("=============================================================\n");

    testOversamplerReconstruction();
    testAntialiasing();
    testHarmonicBalance();
    testCrossoverFlatness();
    testEngine();
    testEngineHarmonics();

    std::printf ("-------------------------------------------------------------\n");
    std::printf (" Result: %d passed, %d failed\n", g_pass, g_fail);
    std::printf ("-------------------------------------------------------------\n");
    return g_fail == 0 ? 0 : 1;
}
