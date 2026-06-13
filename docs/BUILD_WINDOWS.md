# Building for Windows

> **Compiler note:** the **JUCE plugin (VST3/Standalone) must be built with
> MSVC** — JUCE 8 explicitly rejects MinGW (`#error "MinGW is not supported"` in
> `juce_TargetPlatform.h`). Use option **A** (CI) or **B** (local Visual Studio)
> for the plugin. The **framework-independent DSP core + tests** have no such
> restriction and *do* cross-compile from Linux with MinGW-w64 (option **C**).

---

## A. GitHub Actions (MSVC) — recommended ✓ verified green

The repo ships a CI workflow at [`.github/workflows/build.yml`](../.github/workflows/build.yml)
that builds the plugin on **Windows (MSVC)**, macOS and Linux, runs the DSP
tests on all three, and uploads the artifacts. It is confirmed passing and
produces three downloadable bundles:

| Artifact                            | Contents                                   |
|-------------------------------------|--------------------------------------------|
| `OctaphonicVitalizer-Windows-VST3`  | `OCTAPHONIC Vitalizer.vst3` + Standalone `.exe` |
| `OctaphonicVitalizer-macOS-VST3-AU` | `.vst3` + `.component` (AU)                 |
| `OctaphonicVitalizer-Linux-VST3`    | `.vst3` + Standalone                        |

To get the Windows plugin:

1. Push the branch (already wired to `on: push`). Open the run under the repo's
   **Actions** tab.
2. Download **`OctaphonicVitalizer-Windows-VST3`** from the run's *Artifacts*
   section (or `gh run download <run-id> -n OctaphonicVitalizer-Windows-VST3`).

No local Windows machine required.

> On Windows the build uses the **Ninja** generator with the MSVC environment
> (`msvc-dev-cmd`) and skips JUCE's optional VST3 `moduleinfo.json` helper —
> both work around fragilities of the hosted Windows image's preview toolchain.
> The resulting `.vst3` is fully functional.

## B. Native MSVC on Windows

Prerequisites: Visual Studio 2022 (Desktop C++ workload) and CMake ≥ 3.21.

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Output:

```
build\OctaphonicVitalizer_artefacts\Release\VST3\OCTAPHONIC Vitalizer.vst3
build\OctaphonicVitalizer_artefacts\Release\Standalone\OCTAPHONIC Vitalizer.exe
```

Copy the `.vst3` to `C:\Program Files\Common Files\VST3\` to install it.

## C. Cross-compile the DSP core + tests from Linux (MinGW-w64)

This produces a native Windows `dsp_tests.exe` (PE32+) that exercises the whole
audio engine — useful as a portability/CI gate. It does **not** build the JUCE
plugin (JUCE 8 rejects MinGW; see the note at the top).

### Toolchain

```bash
sudo apt-get install g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix mingw-w64-tools
# select the POSIX-threads variant (needed for std::thread)
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
```

### Build

```bash
cmake -S . -B build-win-test \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake \
      -DOCTA_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-win-test -j
# -> build-win-test/tests/dsp_tests.exe  (statically linked, run on Windows)
```

Or compile the engine + tests directly, without CMake:

```bash
x86_64-w64-mingw32-g++ -std=c++17 -O2 -I src/dsp \
    -static -static-libgcc -static-libstdc++ \
    tests/dsp_tests.cpp src/dsp/OctaphonicEngine.cpp -o dsp_tests.exe
```

> Want an actual Windows **VST3** built on Linux (no MSVC machine, no CI)? It is
> possible with `clang-cl` + the Windows SDK fetched via
> [`xwin`](https://github.com/Jake-Shadle/xwin), since clang-cl is
> MSVC-compatible and JUCE accepts it. That path is heavier (it downloads the
> MSVC CRT/SDK) and is left as an optional exercise; for most users the CI in
> option A is simpler and is the validated configuration.

---

## Installing the VST3 on Windows

Copy the built `OCTAPHONIC Vitalizer.vst3` (a folder) to:

```
C:\Program Files\Common Files\VST3\
```

Then rescan plugins in your DAW.
