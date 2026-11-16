# Verbatim 1.0.0: build report

Final review against [`06-test-plan.md`](06-test-plan.md), done at the end of the build plan
([`07-build-plan.md`](07-build-plan.md), T01 to T33). It covers what works, how each test ID was checked, what is
not tested yet, the known risks, and every place the build departs from the spec or the plan, with the reason.

## Summary

- All 16 effects, the seed grammar, lanes, modulation, the override layer (hold / edit / add / remove), state,
  the web editor and the installer are built.
- Every automated test ID passes on Windows (MSVC, x64): 131 C++ tests, 59 editor tests, pluginval at
  strictness 10 and the Steinberg VST3 validator.
- The grammar is frozen: 420 golden texts hash to the same chain on MSVC, clang-cl, Linux clang, Linux gcc and
  macOS clang (last cross-platform run at T19; the code that makes chains has not changed since).
- Not done yet: the DAW pass in real hosts (V2, V4, K1), the clean-machine installer test, and the Linux
  sanitizer jobs (RT1). GitHub Actions does not start jobs on this account, so CI is set to manual
  (see [Continuous integration](#continuous-integration)).

## How to build, test and install

Needs Windows 10 or 11 x64, Visual Studio 2022 or newer Build Tools (C++ workload, its bundled CMake 3.24+),
Node 20+ and Python 3. JUCE, xxHash, Catch2 and the WebView2 SDK are fetched by CMake.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release                      # 131 C++ tests
cd gui; npx playwright test; cd ..                     # 59 editor tests (uses installed Chrome)
python tools/lock_audit.py dsp                         # RT3
$env:VERBATIM_P1_MIN=10; build\tests\Release\verbatim_dsp_tests.exe "[P1]"            # P1 at the 10x gate
python tools/check_golden.py origin/main..HEAD         # G4
$env:VERBATIM_FULL_FUZZ=1; build\tests\Release\verbatim_dsp_tests.exe "F2*,F3*"   # full F2 / F3
tools\bin\pluginval.exe --strictness-level 10 --seed 1 --validate build\Verbatim_artefacts\Release\VST3\Verbatim.vst3
tools\bin\validator.exe build\Verbatim_artefacts\Release\VST3\Verbatim.vst3
```

Installer: `ISCC.exe installer\verbatim.iss` (Inno Setup 6 or 7) writes `build\installer\Verbatim-1.0.0-Setup.exe`.
It installs `Verbatim.vst3` into `C:\Program Files\Common Files\VST3` and offers the WebView2 download if the
runtime is missing. Without the installer, copy the bundle there by hand (needs admin).

## Results by test ID

Final run: 2026-11-16, Release build of commit `T32` plus this report, Windows 11, MSVC.

### Grammar and determinism

| ID | Result | Evidence |
|---|---|---|
| G1 | Pass | Every primitive vector: XXH64, SplitMix64, u16, bias, salt |
| G2 | Pass | All 14 worked examples: parse, seeds, types, chain hashes, two full dumps |
| G3 | Pass | 420 corpus records match their frozen chain hashes |
| G4 | Pass | `check_golden.py` self-test, and no untagged change to `corpus.hashes` in the whole history |
| G5 | Pass | Each corpus text generated twice in one process and from two threads |
| F1 | Pass | 100 000 random byte strings through parse and generate stay in range |

### DSP

| ID | Result | Evidence |
|---|---|---|
| F2 | Pass | Full run, 2000 seeds (CI subset in ctest): no NaN, never above 0 dBFS, in level window |
| F3 | Pass | Full run, 200 seeds at Macro 0 and 1, 0 dBFS square, DC, silence, 44.1 to 192 kHz, 16 to 8192 blocks |
| D1 | Pass | Six null cases (module, lane, global Mix 0, empty text, every effect at Macro 0) |
| D2 | Pass | Every effect's impulse decays below -90 dBFS, no DC |
| D3 | Pass | FILTER matches the TPT SVF design within 1 dB in every mode, cutoff, Q and slope; -3 dB at cutoff for Butterworth (see ruling) |
| D4 | Pass | DELAY, COMB, PHASER, FLANGE, PITCH at max feedback inside lane feedback 3: decay, peak at most 4x |
| D5 | Pass | 60 s of 1e-30 noise costs at most 20 % more CPU than normal signal, every module |
| D6 | Pass | SAT aliasing below -60 dBc at 24 dB drive (4x oversampling) |
| D7 | Pass | `L_FIXED` reported; impulses align with dry for no SAT, SAT, split, 2/3/4 chained lanes, PITCH |
| D8 | Pass | Mod and Macro sweeps on every module; Macro 0.5 equals the scaled build |
| D9 | Pass | DELAY, TREM, FREEZE, LFO A lock to timeDiv 4/8/16 at 90/120/174 BPM |
| D10 | Pass | Band, drive, split, chain, duck, lane mix, level, width each measured |
| D11 | Pass | SafetyClip transparent below -3 dBFS, never above 1.0 |
| D12 | Pass | Each effect at u = 32768 outputs -18 +/- 3 dBFS RMS on -18 dBFS pink noise |

### Repeatability, state and real-time safety

| ID | Result | Evidence |
|---|---|---|
| R1 | Pass | 30 corpus seeds covering all 16 effects, bit identical across runs |
| R2 | Pass | Same seeds bit identical at block 32, 512, 4096 and random sizes |
| R3 | Pass | `reset()` then render equals a fresh render |
| R4 | Pass | Mid-render text change: no click, crossfade within 30 ms after warm-up |
| R5 | Pass | Save state, new instance, load: identical render, including overrides |
| R6 | Pass | Transport start resets LFO and time base: two different pre-rolls give identical audio after play |
| R7 | Pass | R1 holds for mono to mono and mono to stereo |
| F4 | Pass | 5000 random override sequences keep lane invariants; JSON round trip identical (5000 sequences) |
| F5 | Pass | Truncated, corrupt and wrong-version state blobs load safely |
| RT1 | Not run | RealtimeSanitizer and ASan/UBSan jobs written in `ci.yml`; need Linux, see below |
| RT2 | Pass | Zero allocations on the audio thread through chain swaps, overrides and automation (worst-case chains with PITCH, GRAIN, VERB), a GRAIN seed and the R4 swap |
| RT3 | Pass | Lock audit: 24 audio-thread function bodies, 0 problems |
| P1 | Pass | True worst case (4 lanes x 8 modules with PITCH, GRAIN, VERB): 10.8x real time, run with `VERBATIM_P1_MIN=10` (ctest default gate is 4x so slower CI machines do not fail) |
| P2 | Pass | Chain build on the message thread: median 1.09 ms, worst 1.66 ms |
| P3 | Pass | 50 text changes a second for 20 s: +0.5 % CPU |

### Plugin and editor

| ID | Result | Evidence |
|---|---|---|
| V1 | Pass | pluginval strictness 10 with GUI tests, seeds 1 to 3: SUCCESS (`tests/manual/pluginval-s10-gui-seed*.log`) |
| V2 | Not run | DAW smoke checklist ready (`tests/manual/daw-smoke.md`); owner runs it in their own hosts |
| V3 | Pass | Steinberg VST3 validator: 47 passed, 0 failed (`tests/manual/vst3-validator.log`) |
| V4 | Not run | Sample-rate and buffer changes in REAPER; part of the DAW pass. The F3 sweep covers 44.1 to 192 kHz and 16 to 8192 blocks in the harness |
| U1 | Pass | Key to painted character, 200 presses: p95 9.8 ms (gate 16 ms) |
| U2 | Pass | Key to updated key row through a 5 ms mock bridge: p95 8.8 ms (gate 100 ms) |
| U3 | Pass | Mix, Macro and Output set from the host reach the engine and the editor |
| U4 | Pass | Token colouring matches the core's token list for all 14 worked examples |
| U5 | Pass | S0 to S3 and E1 to E4 reachable, screenshot baselines match |
| U6 | Pass | axe-core: no serious or critical issues in S0 to S3 and E1; keyboard-only producer path works |
| U7 | Pass | Hold, edit, add and remove through the editor against the mock bridge |
| U8 | Pass | 4096 bytes and 200 lines: typing p95 8.3 ms, under the U1 gate |
| U9 | Pass | Editor opened and closed 100 times; two editors open at once |

## Not tested, or only tested by hand

- **DAW hosts (V2, V4, K1).** Deferred by the owner until after the build. The plugin passes pluginval and the
  Steinberg validator, but no host has run it yet. The biggest open question is K1: whether every host lets the
  space bar reach the text box instead of starting the transport. If one does not, the fallback in
  [`02-spec.md`](02-spec.md) (a native text field) applies.
- **RT1 (Linux sanitizers).** The jobs exist in `.github/workflows/ci.yml` but have never run. RT2 and RT3
  cover allocation and locks on Windows; RT1 would add a second, compiler-checked view.
- **Clean-machine install.** The installer compiles without warnings; installing it on a fresh Windows VM and
  loading it in REAPER is still to do.
- **HiDPI at 100 / 150 / 200 % in a host.** Correct by construction (vector icons, CSS layout), not yet seen in
  a host.
- **U2 in a host.** Measured against the mock bridge only; the plan also asks for one run in a real host.

## Continuous integration

GitHub Actions refuses to start jobs on this account ("recent account payments have failed or your spending
limit needs to be increased"), even with the repository public. The account has no paid products, so this needs
GitHub Support, not a payment. Until then:

- `ci.yml` and `release.yml` run only when started by hand from the Actions tab.
- Every Windows check above is run locally before each commit.
- The cross-compiler golden check (clang-cl, Linux, macOS) last ran green at T19. The code that makes chains
  (`core/` minus the T21 override layer) and the golden files have not changed since.

## Known risks and limits

- **Space bar in hosts (K1).** See above. Highest risk to the typing experience.
- **Swap response time.** A new chain plays after a silent warm-up (its delay lines fill) plus a 30 ms
  crossfade: up to about 280 ms after a keystroke on delay-heavy chains.
- **Feedback governor.** Lane feedback cannot exceed the lane's input level, so long feedback tails are cut
  shorter after the input stops than a free-running loop would be.
- **First keystrokes.** Opening the editor for the first time can delay the first key or two while fonts and
  the web view warm up.
- **Bit identity.** Audio is bit identical for the same build on the same machine. Across compilers or CPUs it
  matches to floating-point rounding; the chain itself is identical everywhere.
- **Unicode.** Text is hashed as raw UTF-8: a composed letter and a letter plus combining accent are different
  seeds.
- **Scope.** Windows, VST3, no MIDI, no sidechain, only Mix, Macro, Output and Bypass are automatable.

## Deviations from the spec and plan

Each was decided during the build and recorded with its cost if wrong. Grouped by task.

**Setup and tools**

- JUCE pinned to `8.0.17-security`, the newest 8.0.x tag. A different tag needs the owner's approval.
- npm instead of pnpm for the editor.
- The WebView2 SDK is fetched by CMake from nuget.org rather than a per-user NuGet install: the same package,
  reproducible on any machine.
- Playwright 1.55 on the installed Chrome: 1.52 hangs on Node 24, and Edge was not installed.
- DAW checks (K1, V2) deferred by the owner; pluginval and the test harness stand in until then.
- GitHub Actions set to manual (account billing block, above).
- Inno Setup 7 used locally; the script works with 6 or 7.

**Grammar (T03)**

- NUM rule read as "first in-range number wins, out-of-range numbers are skipped". Now frozen by the corpus.
- The chain field stays raw on lane 0; the DSP ignores it there, as the catalog says.

**Engine (T05 to T12)**

- SAT oversamples 4x (owner approved): 2x reached only -37 dBc against D6's -60. Costs 488 samples of latency
  and more SAT CPU.
- The lane-level D1 reference includes the lane DC blocker (owner approved).
- A swapped-in chain runs a silent warm-up before its 30 ms fade, so its empty delay lines do not click (R4).
- State JSON also stores the host parameters (Mix, Macro, Output, Bypass), which the spec omitted.
- State text goes through `juce::String`, so invalid UTF-8 is not preserved; the editor only produces valid UTF-8.
- D10 band reference is the digital (prewarped bilinear) Butterworth, not the analog one: a digital filter
  cannot match analog near Nyquist.
- The lane-mix dry path is the raw lane input, delayed by the lane's latency.
- Lane feedback governor (fed-back level at most feedback gain x input envelope): without it, modules with gain
  above 1 self-oscillated (F3).
- LFO B's random generator is seeded from the lane seed xor `"LFOB"`; the catalog only defined module seeds.
- Modulated parameters are smoothed with a 5 ms one-pole (DELAY time 50 ms glide); the spec said "smoothed".
- Transport start also resets the dry delay and takes a pending chain without a fade, so audio is bit identical
  from the first sample after play (R6).

**Effects (T13 to T20)**

- FILTER uses its own TPT state-variable filter (JUCE's has no notch); same algorithm. Its 24 dB mode uses a
  Butterworth second stage, and band-pass gets +13 dB make-up so it sits at level (D12).
- D3 checks the exact response of the specified design in every mode (within 1 dB); the literal "-3 dB at
  cutoff" only holds for Q 0.707, so it is checked there.
- PHASER feedback range 0 to 0.7 and FLANGE -0.7 to 0.7 (from 0.85): JUCE's loops have no soft clip and
  peaked at 5.9x and 4.4x against D4's 4x.
- CHORUS and FLANGE share one source file; FREEZE and COMB share one.
- VERB uses `juce::Reverb` directly, wet level 1/3 (unity, JUCE scales by 3 inside).
- FREEZE loops up to 2 s.
- GRAIN output is scaled by 1 / sqrt(density x size) so level does not grow with overlap.
- Per-effect wet trims centre D12 on -18 dBFS, capped at +6 dB (VERB 4.5, PHASER and FLANGE 0): full
  centring broke D4 and F3 peaks. Lane band gets +6 dB make-up.
- D5 takes the fastest of 9 interleaved timings (single runs were too noisy) and measures thread CPU time.
  PITCH skips its soft clip below 1e-15, which cost 26 % on denormal input.

**Overrides and plugin (T21, T22)**

- Removing a held module deletes the hold and hides the generated slot for the current text only.
- The override layer keeps at most 2 SAT per lane, dropping unheld generated SAT first.

**Editor (T23 to T28)**

- Latency tests type at 30 ms a key (human pace). Typing faster than that showed a one-time cold-start queue
  of about 160 ms on the first keys after opening.
- Per-character ink animation only runs while the text is at most 1200 characters (U8).
- E3 (state that cannot be loaded) shows empty read-only paper; the plugin does not try to recover text from a
  broken state.
- U6 waits for animations and transitions to settle before the accessibility scan. Under heavy CPU load the
  scan sometimes caught a key's tooltip half-faded in and flagged its contrast. The tooltip passes at full
  opacity.

**Real-time safety (T29)**

- RT1 sanitizer jobs written but not run (no Linux machine, CI blocked).
- The lock audit first counted marker comments (27) and skipped a body whose marker sat on a continuation
  line (`Chain::render`). It now walks to each function body, skips declarations and counts bodies: 24, all clean.

**Release (T32)**

- The uninstaller lives in `C:\Program Files\Verbatim`, not in the shared VST3 folder.
- The release workflow runs only from a `v*` tag and stamps the installer with that version.
