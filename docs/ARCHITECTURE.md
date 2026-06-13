# Architecture & DSP design

This document explains how the engine works and the reasoning behind the design
choices. All of the audio code is in `src/dsp/` and is independent of JUCE.

## 1. Layering

```
src/plugin/  (JUCE)      thin: parameters <-> octa::Settings, UI, state I/O
src/dsp/     (C++17)     all signal processing, no framework dependencies
tests/                   links src/dsp directly, asserts on real spectra
```

The plugin never does DSP itself. Once per `processBlock`, `PluginProcessor`
reads the atomic parameter values, fills an `octa::Settings` struct and calls
`engine.process(...)`. Keeping the engine framework-free is what makes the
offline test harness possible.

## 2. Per-sample signal flow

For each channel, every sample travels:

1. **Input gain** (smoothed).
2. **3-band crossover** (`ThreeBandCrossover`) → Low / Mid / High.
3. For each band:
   - **Drive** pre-gain into the shaper.
   - **Oversampled dual shaper** (`Oversampler` + `HarmonicGenerator`): the
     nonlinearity runs at L× the sample rate.
   - **DC blocker** (the even shaper adds DC).
   - **Band gain** × mute/solo mask.
4. Sum the three bands → *wet*.
5. **Air** high-shelf on the wet sum.
6. **Mix**: `dry·(1−mix) + wet·mix`, where *dry* is delayed to match the
   oversampler latency.
7. **Output gain**, final **DC blocker**, NaN/Inf guard.

Processing is **sample-major** (the outer loop is over samples, the inner over
channels) so the shared parameter smoothers advance exactly once per sample and
every channel sees an identical ramp — no inter-channel drift.

## 3. Crossover (`Crossover.h`)

Each split is a 4th-order **Linkwitz-Riley** filter, i.e. two cascaded
Butterworth (Q = 1/√2) biquads. LR4 low-pass and high-pass at the same corner
are in phase and sum to an all-pass, so no polarity inversion is needed.

A naïve three-way tree is **not** magnitude-flat near the upper corner, because
the low band is split off before the second crossover and therefore misses its
phase shift. The fix is to pass the low band through an all-pass equivalent of
the second split, realised as `LP_f2 + HP_f2`:

```
low  = AP_f2( LR4_LP_f1(x) )           // = LP_f2(low1) + HP_f2(low1)
mid  = LR4_LP_f2( LR4_HP_f1(x) )
high = LR4_HP_f2( LR4_HP_f1(x) )
```

Now `low + mid + high = AP_f2(AP_f1(x))`, a pure all-pass → **flat magnitude**.
The test `testCrossoverFlatness` confirms the recombined response is within
1e-6 dB of 0 dB across the audio band.

## 4. Dual harmonic generator (`HarmonicGenerator.h`)

Two memory-less shapers, blended by `character ∈ [0,1]`:

```
odd(x)  = tanh(D·x)            // symmetric  -> odd harmonics, keeps fundamental
even(x) = 2·x·tanh(D·x)        // even-sym.  -> even harmonics (+DC)
out     = (character·odd + (1−character)·even) / D
```

- `even(x)` is an even function (`f(x)=f(−x)`), so for a sinusoid it produces
  only DC and even harmonics; the DC is stripped downstream.
- Dividing by the drive keeps the odd path near unity gain for small signals
  and lets it saturate gracefully as drive increases.

`testHarmonicBalance` verifies that `character=1` makes the 3rd harmonic
dominate the 2nd (and vice-versa for `character=0`).

## 5. Oversampling (`Oversampler.h`)

Saturation creates harmonics above Nyquist that alias on the way back down. The
oversampler runs the shaper at L× (1/2/4/8):

```
1 input ─poly-phase interpolation→ L hi-rate samples
        ─apply nonlinearity to each→
        ─FIR + decimate→ 1 output
```

Key points:

- One **Type-I linear-phase** prototype low-pass (Blackman-windowed sinc, odd
  length `taps·L + 1`, cutoff at base-rate Nyquist) is shared by both the
  interpolation phases and the decimator.
- Because the prototype is odd-length and shared, and the decimator is centred
  on high-rate phase `nL` (not the newest sample), the **combined group delay
  is exactly `taps` base-rate samples** — an integer. That lets the dry path be
  aligned with a plain integer delay line, so the dry/wet mix never comb-filters.
- `testOversamplerReconstruction` measures > 80 dB in-band SNR for an identity
  nonlinearity; `testAntialiasing` shows > 70 dB alias rejection at 8× vs 1×.

## 6. Latency

`engine.latencySamples()` equals the oversampler's `taps` (0 when oversampling
is off). The plugin reports this to the host via `setLatencySamples`, and the
dry path is delayed by the same amount internally. **Bypass preserves the
reported latency** so that toggling it never shifts timing — verified by
`testEngine` (bypass SNR > 80 dB against the delayed input).

## 7. Real-time safety

- `processBlock` allocates nothing and takes no locks.
- Denormals are disabled (`ScopedNoDenormals`); a final `std::isfinite` guard
  prevents any NaN/Inf escaping to the host.
- The **one** non-RT-safe operation is changing the oversampling factor, which
  re-allocates the polyphase state. It is intentionally an occasional,
  user-initiated event (and documented as best changed while stopped) rather
  than something that happens per block.

## 8. Parameter smoothing

Gains and the dry/wet mix use an exponential one-pole smoother (~15 ms) to avoid
zipper noise on automation. Filter coefficients (crossover frequencies, air
shelf) are recomputed only when their source values actually change.
