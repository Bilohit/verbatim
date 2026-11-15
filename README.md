# Verbatim

A VST3 audio effect for Windows: type text, and the text deterministically generates an effects chain. Same text, same chain, bit for bit.

Design documents live in [`docs/`](docs/). Licence: [AGPLv3](LICENSE).

## Build (Windows)

Requirements: CMake 3.24 or newer, Visual Studio 2022 or newer with the C++ workload, Node.js 20 or newer with npm. At configure time CMake fetches JUCE (tag `8.0.17-security`), Catch2 and the WebView2 SDK NuGet package (1.0.3485.44, into `build/nuget/`, no system install). The build runs `npm ci` and `npm run build` in `gui/` and embeds the result in the plugin.

The plugin UI needs the Microsoft WebView2 runtime (preinstalled on Windows 11).

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
