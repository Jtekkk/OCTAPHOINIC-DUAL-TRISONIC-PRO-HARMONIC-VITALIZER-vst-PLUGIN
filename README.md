# OCTAPHONIC DUAL-TRISONIC PRO-HARMONIC VITALIZER

A **3-band harmonic exciter / vitalizer** audio plugin, built from scratch in
C++17 with a JUCE front-end. It splits the signal into three bands, runs each
band through a pair of complementary waveshapers (even + odd harmonic
generators) at up to 8× oversampling, and blends the harmonically-enhanced
result back with the dry signal to add presence, warmth and "air".

The flamboyant name maps onto real, measurable DSP:

| Marketing term      | What it actually is                                                  |
|---------------------|----------------------------------------------------------------------|
| **TRISONIC**        | 3-band Linkwitz-Riley (LR4) crossover (Low / Mid / High)             |
| **DUAL**            | Dual harmonic generators per band — even *and* odd, blended          |
| **OCTAPHONIC**      | Up to **8×** polyphase oversampling for alias-free saturation        |
| **PRO-HARMONIC**    | Chebyshev-style harmonic synthesis with per-band drive & character   |
| **VITALIZER**       | Parallel dry/wet "Vitalize" blend + high-shelf "Air"                 |

Formats: **VST3**, **Standalone**, and **AU** (on macOS).

---

## Why this architecture

The interesting, novel code — the actual audio processing — lives in
[`src/dsp/`](src/dsp) and has **zero dependency on JUCE** or any framework. That
means the entire engine can be compiled and **unit-tested with a bare C++17
compiler**, which is exactly what [`tests/dsp_tests.cpp`](tests/dsp_tests.cpp)
does: it drives real signals through the engine and verifies the spectrum with a
small built-in FFT (harmonic generation, alias suppression, crossover flatness,
stability, latency, bypass transparency, DC rejection).

The JUCE layer in [`src/plugin/`](src/plugin) is a thin wrapper: it owns the
parameter tree and, each block, hands a plain `octa::Settings` snapshot to the
engine.

```
            ┌──────────────── src/plugin (JUCE) ────────────────┐
 host  ───► │ PluginProcessor ─► octa::Settings ─► PluginEditor │
            └───────────────────────┬───────────────────────────┘
                                     ▼
            ┌──────────────── src/dsp (pure C++17) ─────────────┐
            │ OctaphonicEngine: crossover → dual shapers (OS)   │
            │                 → DC block → band gain → air → mix│
            └───────────────────────────────────────────────────┘
                                     ▲
                          tests/ link the SAME core
```

## Signal flow (per channel)

```
in ─► input gain ─► 3-band LR4 crossover
                        │
      ┌─────────────────┼──────────────────┐
      ▼ Low             ▼ Mid              ▼ High
  [drive→OS dual    [drive→OS dual     [drive→OS dual
   shaper→DC→gain]   shaper→DC→gain]    shaper→DC→gain]
      └─────────────────┼──────────────────┘
                        ▼  Σ (wet)
                   high-shelf "AIR"
                        ▼
   out ◄ output gain ◄ DC block ◄ ( dry·(1−mix) + wet·mix )
```

Each oversampled shaper blends two nonlinearities:

- **Odd** path: `tanh(D·x)` → odd harmonics (3rd, 5th …), preserves the fundamental.
- **Even** path: `2·x·tanh(D·x)` → even harmonics (2nd, 4th …); its DC is removed
  by a per-band DC blocker.

The per-band **Character** knob morphs between the two (0 = even, 1 = odd).

---

## Building

### Prerequisites

- CMake ≥ 3.21 and a C++17 compiler (GCC 11+, Clang 13+, MSVC 2022).
- For the **plugin** on Linux, the usual JUCE dev packages:
  ```
  sudo apt install libasound2-dev libfreetype6-dev libfontconfig1-dev \
       libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
       libxcomposite-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
  ```
- JUCE itself is fetched automatically (pinned to `8.0.13`); no manual install.

### Build everything (plugin + tests)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts land in `build/OctaphonicVitalizer_artefacts/Release/`
(`VST3/`, `Standalone/`, and `AU/` on macOS).

### Build & run only the DSP tests (no JUCE, no network)

```bash
cmake -S . -B build-test -DOCTA_BUILD_PLUGIN=OFF
cmake --build build-test -j
cd build-test && ctest --output-on-failure
```

### Useful CMake options

| Option                 | Default   | Meaning                                              |
|------------------------|-----------|------------------------------------------------------|
| `OCTA_BUILD_PLUGIN`    | `ON`      | Build the JUCE plugin (fetches JUCE)                  |
| `OCTA_BUILD_TESTS`     | `ON`      | Build the framework-independent DSP tests            |
| `OCTA_JUCE_TAG`        | `8.0.13`  | JUCE git tag to fetch                                |
| `JUCE_PATH`            | *(unset)* | Use a local JUCE checkout instead of fetching        |
| `OCTA_USE_SYSTEM_JUCE` | `OFF`     | Use an installed JUCE (`find_package`) vs. fetching  |

### Windows / macOS / CI

Pushing the branch builds the plugin on **Windows (MSVC)**, **macOS** and
**Linux** via [`.github/workflows/build.yml`](.github/workflows/build.yml) and
uploads the `.vst3` / `.exe` / `.component` artifacts — this is **verified
green** (Windows VST3 + Standalone, macOS VST3 + AU, Linux VST3). The DSP core +
tests additionally cross-compile to a native Windows `.exe` from Linux with
MinGW-w64. Full details — including why the JUCE plugin needs MSVC on Windows —
are in [`docs/BUILD_WINDOWS.md`](docs/BUILD_WINDOWS.md).

---

## Parameters

See [`docs/PARAMETERS.md`](docs/PARAMETERS.md) for the full list. In short:

- **Global:** Input, Vitalize (dry/wet), Air, Output, Oversampling (Off/2×/4×/8×), Bypass
- **Crossover:** Low/Mid frequency, Mid/High frequency
- **Per band (Low/Mid/High):** Drive, Character (Even↔Odd), Gain, Mute, Solo

All parameters are host-automatable and saved/restored with the session.

## Project layout

```
src/dsp/       Framework-independent DSP core (the real audio code)
  Biquad.h            RBJ biquad (TDF-II)
  DCBlocker.h         one-pole DC remover
  Crossover.h         3-band LR4 crossover with all-pass compensation
  HarmonicGenerator.h dual even/odd waveshaper
  Oversampler.h       polyphase FIR up/down-sampler (integer latency)
  OctaphonicEngine.*  the engine that ties it together
  Parameters.h        single source of truth for IDs / ranges / defaults
src/plugin/    JUCE wrapper (PluginProcessor / PluginEditor)
tests/         dsp_tests.cpp + mini_fft.h  (ctest target)
docs/          ARCHITECTURE.md, PARAMETERS.md
```

## Testing

The DSP suite (`ctest`) verifies, on real processed audio:

- polyphase oversampling reconstructs in-band signal to **> 80 dB SNR**;
- 8× oversampling suppresses saturation aliasing by **> 70 dB** vs. no OS;
- the dual shaper produces the expected **odd vs. even** harmonic balance;
- the 3-band crossover sums back to a **flat magnitude** response;
- the full engine is **stable**, finite (no NaN/Inf), DC-free, and **bypass is
  bit-transparent** apart from the reported latency.

## License

MIT (see [`LICENSE`](LICENSE)). Note that building the plugin links JUCE, which
carries its own licensing terms; the `src/dsp` core is MIT and JUCE-free.
