# 07 Build plan

Each task is one PR-sized commit set. Order is binding. Test-first: write the listed tests, see them fail, implement, see them pass. Test IDs refer to `06-test-plan.md`. "Done" = acceptance met, listed tests green on all CI jobs that run them, nothing earlier broken.

Layout (created by the tasks): `core/` `dsp/` `plugin/` `gui/` `tools/` `tests/{core,dsp,plugin,golden,manual}` `.github/workflows/ci.yml`.

## M0 Skeleton

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T01 | Repo and empty plugin. `git init`, AGPLv3 `LICENSE`, `README.md`, CMake project, JUCE 8 pinned by tag, VST3 target "Verbatim" that passes audio through. Ask the owner for manufacturer name and plugin IDs (A10). CI `win-msvc` builds it. | `CMakeLists.txt`, `plugin/VerbatimProcessor.{h,cpp}`, `.github/workflows/ci.yml`, `LICENSE`, `.gitignore` | `.vst3` builds in Release; loads in REAPER; input = output | V1 at strictness 5 in CI |
| T02 | `core` library with primitives. Vendor `xxhash.h`. `SplitMix64`, `bias`. Catch2 wired. Add CI jobs `win-clangcl`, `linux-clang`, `linux-gcc`, `macos-clang` for `tests/core`. Compiler flags from `02-spec.md` C7. | `core/CMakeLists.txt`, `core/Hash.h`, `core/Rng.h`, `core/Bias.h`, `third_party/xxhash.h`, `tests/core/primitives.test.cpp` | Library has no JUCE include and no float type | G1 on 5 jobs |

## M1 Vertical slice: type text, hear sound

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T03 | Preprocess, line split, tokenizer, classification, entropy string, modifiers → per-line parse result and UI token list (`04-seed-grammar.md` §2, §4, §5, §9). | `core/Tokenizer.{h,cpp}`, `core/Parse.{h,cpp}`, `tests/core/parse.test.cpp` | For each worked example: `E`, axes and lane fields match §8 | G2 (parse columns), F1 (parse only) |
| T04 | Generator, ChainSpec, canonical serialization, chain hash, catalog tables (IDs, weights, tags). `tools/vb_spec` CLI: `vb_spec "<text>"` prints the ChainSpec and hash; `vb_spec --corpus in out` writes hashes. | `core/Catalog.h`, `core/ChainSpec.h`, `core/Generate.{h,cpp}`, `core/Serialize.{h,cpp}`, `tools/vb_spec.cpp`, `tests/core/generate.test.cpp` | All 14 examples and both full dumps match exactly. If any differs: stop, ask the owner (K5) | G2 full, G5, F1 |
| T05 | DSP engine skeleton. `Module` interface, `Lane` (serial modules, lane mix at 100 %, pad to `L_FIXED`), `Engine` (dry delay, lane sum, Mix, Output, SafetyClip, NaN guard, meters, chain build and lock-free swap with 30 ms crossfade, retire FIFO). Effects SAT and DELAY only; other types build as a pass-through module. Zero lanes = delayed dry. Chain builds serialized by one non-audio mutex (`02-spec.md` §2 threads). Offline render harness for tests. | `dsp/Module.h`, `dsp/Lane.{h,cpp}`, `dsp/Engine.{h,cpp}`, `dsp/SafetyClip.h`, `dsp/fx/Sat.{h,cpp}`, `dsp/fx/Delay.{h,cpp}`, `tests/dsp/harness.{h,cpp}`, `tests/dsp/engine.test.cpp` | Text `hello` renders audibly different from dry; swap is click free | D1, D2 (SAT, DELAY), D6, D7, D11, R1 and R3 (2 seeds), R4, RT2 |
| T06 | Plugin wiring. Host params `mix`, `macro` (stored, no effect yet), `output`, bypass. State JSON v1 with text only. Text set through a temporary JUCE `TextEditor` in the editor. Dirty notification. | `plugin/VerbatimProcessor.{h,cpp}`, `plugin/State.{h,cpp}`, `plugin/VerbatimEditor.{h,cpp}`, `tests/plugin/state.test.cpp` | In REAPER: type text, hear SAT/DELAY chains, save and reload project restores text and sound. `setStateInformation` and `prepareToPlay` build the chain synchronously | R5, R7, F5, V1 (5) |
| T07 | WebView shell and K1 gate. Vite + React + TypeScript app with one plain `<textarea>`; embedded as binary data and served by a resource provider (no network); bridge `setText` / `getState`; WebView2 user-data folder (A8); E4 fallback. CMake builds `gui/` (`npm ci`, `npm run build`) and embeds `gui/dist`; the WebView2 SDK NuGet package is installed in CI and documented in `README.md`. Follow JUCE's `WebViewPluginDemo` for the pinned tag and verify every API name against that source. Then run the K1 host check. | `gui/` (scaffold), `plugin/Bridge.{h,cpp}`, `plugin/VerbatimEditor.{h,cpp}`, `plugin/CMakeLists.txt`, `tests/manual/daw-smoke.md` | Typing in the web text box, including spaces, changes the sound in REAPER, Live and FL Studio. Report results to the owner. If two or more hosts fail: stop and ask (K1) | V2 rows "load" and "type text" for 3 hosts; U9 (10 cycles) |

Milestone M1 review: G1, G2, G5, F1, D1, D2, D6, D7, D11, R1, R3, R4, R5, R7, F5, RT2, V1(5), K1 result.

## M2 Full grammar in the engine

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T08 | Golden corpus. Write `tests/golden/corpus.txt` to the content rules of G3, generate `corpus.hashes` with `vb_spec`, add the G4 CI guard script. | `tests/golden/*`, `.gitattributes`, `tests/core/golden.test.cpp`, `.github/workflows/ci.yml`, `tools/check_golden.py` | Identical hashes on all 5 jobs | G3, G4 |
| T09 | Lane stages: band, drive, split (with latency matching of the halves), lane mix (`mixAbs`, `mixSteps`), duck, level, width, DC block, chain (`&`) routing, lane-sum rule, padding per chain group to `L_FIXED`. | `dsp/Lane.{h,cpp}`, `dsp/Engine.cpp`, `tests/dsp/lane.test.cpp` | Each stage matches `03-dsp-catalog.md` §1.1 | D10, D7 |
| T10 | Lane feedback (`>`) with its filters, tanh and caps. | `dsp/Lane.{h,cpp}`, `tests/dsp/feedback.test.cpp` | Stable at step 3 with every implemented effect | D4 (lane), F3 (subset) |
| T11 | Modulation: LFO A, LFO B, ENV, routing to targets, 32-sample control grid, Macro (module mix and mod depth scaling). | `dsp/Mod.{h,cpp}`, `dsp/Lane.cpp`, `tests/dsp/mod.test.cpp` | Sources match `03-dsp-catalog.md` §1.2; Macro 0.5 is neutral | D8, R2 (implemented effects) |
| T12 | Time base and transport: host tempo read, `timeDiv` sync for DELAY and LFO A, 120 BPM default, tempo smoothing. Transport reset on stopped → playing (`02-spec.md` C9). | `dsp/TimeBase.h`, `dsp/fx/Delay.cpp`, `dsp/Mod.cpp`, `dsp/Engine.cpp`, `plugin/VerbatimProcessor.cpp`, `tests/dsp/timebase.test.cpp` | Periods match §1.3; two renders from the same play start are identical | D9 (DELAY, LFO A), R6 |

Milestone M2 review: all M1 IDs plus G3, G4, D4 (lane), D8, D9, D10, R2, R6.

## M3 Effect catalog

Every effect task: implement per `03-dsp-catalog.md` §2, register in the factory, add to the D-suite tables. Proof for each: D1, D2, D5, D8 for the new effects, plus the extras listed.

| ID | Task | Files | Extra proof |
|---|---|---|---|
| T13 | FILTER, CRUSH | `dsp/fx/Filter.*`, `dsp/fx/Crush.*` | D3 |
| T14 | CHORUS, PHASER, FLANGE (JUCE wrappers) | `dsp/fx/Chorus.*`, `dsp/fx/Phaser.*`, `dsp/fx/Flange.*` | D4 (PHASER, FLANGE) |
| T15 | VERB, COMP | `dsp/fx/Verb.*`, `dsp/fx/Comp.*` | D2 tail table |
| T16 | TREM, WOW, RING | `dsp/fx/Trem.*`, `dsp/fx/Wow.*`, `dsp/fx/Ring.*` | D9 (TREM) |
| T17 | COMB, FREEZE | `dsp/fx/Comb.*`, `dsp/fx/Freeze.*` | D4 (COMB), D9 (FREEZE) |
| T18 | PITCH | `dsp/fx/Pitch.*` | D4 (PITCH), D7 |
| T19 | GRAIN | `dsp/fx/Grain.*` | R1 with a GRAIN seed, RT2 |
| T20 | Calibration and seed-space bounds: trims table, audio fuzz. Narrow ranges in `03-dsp-catalog.md` only if S1 fails, and record each change in the task notes. | `dsp/Trims.h`, `tests/dsp/fuzz.test.cpp`, `tests/dsp/calibration.test.cpp` | D12, F2, F3, R1 and R2 on the full 30 seeds, P1 |

Milestone M3 review: all earlier IDs plus D3, D4, D5, D12, F2, F3, R1, R2 (full), P1.

## M4 Overrides

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T21 | `core::resolve`: holds, auto-hold on edit, tombstones tied to lane seed, added modules, limits (`02-spec.md` R1 to R7). State JSON gains `overrides`. | `core/Resolve.{h,cpp}`, `plugin/State.cpp`, `tests/core/resolve.test.cpp`, `tests/plugin/state.test.cpp` | Each rule R1 to R7 has a named test | F4, R5 with overrides, F5 |
| T22 | Bridge API complete: `setOverride`, `setUi`, `chain` and `levels` events (30 Hz, only while visible), mapped param values with units in the `chain` payload, slider relays for the 3 host params. | `plugin/Bridge.{h,cpp}`, `plugin/ParamText.{h,cpp}`, `tests/plugin/bridge.test.cpp` | Payloads match `05-ui-design.md` §6 | U3, P2, P3 |

Milestone M4 review: all earlier IDs plus F4, U3, P2, P3.

## M5 Interface

Before T23: invoke the design skills (impeccable, hallmark, uiux-pro-max, taste-skill, animotion) and re-read `05-ui-design.md` and `proto/paper.html`.

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T23 | Tokens, fonts, machine shell, Paper with the text editor (transparent textarea over rendered layers), token colouring from `core`, postmark, ignored-line style. Mock bridge for browser tests. | `gui/src/styles/tokens.css`, `gui/src/components/{Machine,Paper,InkLayer,Postmark}.tsx`, `gui/src/bridge/{index,mock}.ts`, `gui/tests/*` | Matches `05-ui-design.md` §2, §4; text entry native-fast | U1, U4, U8 |
| T24 | Key rows: icons, names on hover/focus, hold latch, stagger, live region. | `gui/src/components/{KeyRow,Key}.tsx`, `gui/src/icons.ts` | Click toggles hold end to end in the plugin | U7 (hold), U2 |
| T25 | Carriage scale sliders bound to host params, output meter with clip tick, `levels` feed. | `gui/src/components/{Scale,Slider,Meter}.tsx` | Dragging in UI moves the host param and back | U3 (in plugin), V2 "automate" in REAPER |
| T26 | Advanced: drawer with labelled sliders and units, hold toggle, remove, "+" key and add menu with limits. | `gui/src/components/{Drawer,AddMenu}.tsx` | Edit, add, remove persist through project reload | U7 (all ops), R5 |
| T27 | Motion pass with framer-motion: M1 to M8 and reduced-motion behaviour, shared variants in `gui/src/motion/`. Invoke the animotion skill first. | `gui/src/motion/*`, the components named in `05-ui-design.md` §5 | Every row of the motion table implemented where it says; living ink uses motion values, no per-frame React state | U1 and U8 still pass; reduced-motion run of U6 |
| T28 | Roll, states S0 and E1 to E4, resize constraints and saved size, HiDPI check, accessibility pass, design QA (`05-ui-design.md` §8). | `gui/src/components/RollButton.tsx`, `gui/src/words.ts`, `plugin/VerbatimEditor.cpp`, baselines in `gui/tests/__screenshots__` | Producer path (§1) completes by mouse and by keyboard only | U5, U6, U9 |

Milestone M5 review: all earlier IDs plus U1 to U9.

## M6 Hardening and release

| ID | Task | Files | Acceptance | Proof |
|---|---|---|---|---|
| T29 | Realtime safety: RTSan job, alloc guard across swaps and automation, lock audit script, CPU and build-time budgets. Fix what they find. | `.github/workflows/ci.yml`, `tests/dsp/rt.test.cpp`, `tools/lock_audit.py` | Zero reports | RT1, RT2, RT3, P1, P2, P3 |
| T30 | pluginval strictness 10 and Steinberg validator in CI; fix findings. | `.github/workflows/ci.yml` | Both pass | V1 (10), V3 |
| T31 | Full DAW smoke on the six hosts; fix or document each failure with the owner. | `tests/manual/daw-smoke.md` (filled in) | Checklist complete, every failure has a decision | V2, V4 |
| T32 | Release: Inno Setup installer with WebView2 check, version 1.0.0, `README.md` (install, the grammar cheat sheet, known limits), third-party licence notices, GitHub release workflow. | `installer/verbatim.iss`, `README.md`, `NOTICE.md`, `.github/workflows/release.yml` | Installer puts the plugin in `%CommonProgramFiles%\VST3`, a clean Windows VM loads it | Manual install test, V2 (REAPER) on the VM |
| T33 | Final review against `06-test-plan.md` (every ID) and build report (`HANDOFF.md`). | `docs/BUILD-REPORT.md` | Report lists what works, what is untested, known risks | Whole plan |

## Dependency notes

| Task | Needs |
|---|---|
| T04 | T03 |
| T05 | T04 (ChainSpec) |
| T07 | T06 |
| T08 | T04 (may be done right after T04 if convenient; must be before T09) |
| T13 to T19 | T11 (mod interface), independent of each other |
| T20 | T13 to T19 |
| T22 | T21, T07 |
| T23 to T28 | T22 |
| T29 to T33 | all before |
