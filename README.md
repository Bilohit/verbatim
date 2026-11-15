# Verbatim

A VST3 audio effect for Windows: type text, and the text deterministically generates an effects chain. Same text, same chain, bit for bit.

Design documents live in [`docs/`](docs/). Licence: [AGPLv3](LICENSE).

## Build (Windows)

Requirements: CMake 3.24 or newer, Visual Studio 2022 or newer with the C++ workload. JUCE (tag `8.0.17-security`) is fetched by CMake at configure time.

```
cmake -S . -B build -A x64
cmake --build build --config Release --target Verbatim_VST3
```

The plugin is written to `build/Verbatim_artefacts/Release/VST3/Verbatim.vst3`.

## Validate

Download [pluginval](https://github.com/Tracktion/pluginval/releases) (v1.0.4) into `tools/bin/`, then:

```
tools/bin/pluginval.exe --strictness-level 5 --skip-gui-tests --seed 1 --validate build/Verbatim_artefacts/Release/VST3/Verbatim.vst3
```
