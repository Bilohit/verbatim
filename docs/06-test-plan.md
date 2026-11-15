# 06 Test plan

Framework: Catch2 v3 (`tests/core`, `tests/dsp`, `tests/plugin`), Vitest + Playwright (`gui/tests`), pluginval, manual DAW smoke. Every test ID below is referenced by a task in `07-build-plan.md`.

## CI matrix

| Job | OS / compiler | Runs |
|---|---|---|
| `win-msvc` | windows-latest, MSVC 2022, Release | Build plugin + gui. All Catch2 suites. Vitest, Playwright. pluginval. Alloc guard. |
| `win-clangcl` | windows-latest, clang-cl | core + dsp suites (G, F, D, R) |
| `linux-clang` | ubuntu-latest, clang ≥ 20 | core + dsp suites; second run of dsp suite with `-fsanitize=realtime` (RT1); ASan + UBSan run of F |
| `linux-gcc` | ubuntu-latest, gcc | core suite (G, F1) |
| `macos-clang` | macos-latest (arm64) | core suite (G, F1) |

A red job blocks merge. Golden files are never regenerated in CI.

## G. Golden determinism

| ID | Test | Pass |
|---|---|---|
| G1 | Primitive vectors: every row of `04-seed-grammar.md` §3 | exact |
| G2 | Worked examples: all 14 rows of `04-seed-grammar.md` §8 (lanes, `E`, seed, n, types, modifiers, chainHash) and both full dumps | exact |
| G3 | Corpus: `tests/golden/corpus.txt` (≥ 300 seed texts, one per record, bytes written as `\xNN` escapes so the file is pure ASCII; `.gitattributes` marks `tests/golden/*` as `-text`) with `tests/golden/corpus.hashes` (chainHash per record). Must include: every op alone and tripled, every direction word, CAPS, numbers 0..70 and `%`, 1 to 6 lines, empty, whitespace only, emoji, RTL (Arabic, Hebrew), CJK, combining marks, invalid UTF-8 bytes, control characters, CRLF, 4096 and 10 000 byte texts | exact on all 5 CI jobs |
| G4 | Corpus hash file is write-protected by process: test fails if `corpus.hashes` changed in the same commit without the commit message tag `[grammar-change-approved]` (checked by a CI script) | enforced |
| G5 | Stability: generate each corpus text twice in one process and from two threads | identical |

The corpus hashes are first produced by `tools/vb_spec` only after G1 and G2 pass, then committed and frozen.

## F. Seed fuzzer

| ID | Test | Pass |
|---|---|---|
| F1 | Core fuzz: 100 000 random byte strings (lengths 0..12 000; mixes of ASCII, multi-byte UTF-8, RTL, emoji, control bytes, invalid sequences, long runs of one op), fixed fuzz seed | no crash, no UB (sanitizers), lanes ≤ 4, modules per lane 2..6, every field inside its range in `02-spec.md` §3 |
| F2 | Audio fuzz: 2000 random seed texts (fixed fuzz seed), each renders 2 s of −18 dBFS RMS pink noise at 48 kHz / 512, Mix 100, Macro 0.5. CI runs the first 300; the full 2000 run locally at T20 and T33 | no NaN/Inf, NaN-guard counter 0, peak ≤ 1.0, RMS of the last 1 s within −36..−8 dBFS (this is S1) |
| F3 | Extremes: same as F2 for 200 seeds with Macro 0 and 1, input at 0 dBFS square wave, input silence, input DC, sample rates 44.1 k and 192 k, block sizes 16 and 8192 | no NaN/Inf, peak ≤ 1.0; silence in gives output below −60 dBFS after 30 s (no self-oscillation) |
| F4 | Override fuzz: 5000 random sequences of hold / edit / add / remove / text change through `core::resolve` | invariants: ≤ 8 modules per lane, ≤ 2 SAT per lane, no crash, state JSON round-trips to an identical ResolvedChain |
| F5 | State fuzz: truncated, corrupt and wrong-version state blobs into `setStateInformation` | no crash, state E3, audio passthrough |

## D. DSP unit tests (per module unless noted)

| ID | Test | Pass |
|---|---|---|
| D1 | Null: module with mix 0; lane with all modules mix 0; Mix 0 globally; empty text at Mix 100; lane with zero modules. Each vs delayed dry | difference below −120 dBFS |
| D2 | Impulse: impulse in, 10 s out | finite, decays below −90 dBFS within the module's max tail (table in test), no DC above −80 dBFS |
| D3 | Filter response: FILTER LP/HP/BP/notch at 3 cutoffs × 2 Q × 2 slopes, measured with a log sweep | −3 dB point within ±5 % of cutoff; slope within ±1.5 dB/oct of nominal one octave past cutoff |
| D4 | Feedback stability: DELAY, COMB, PHASER, FLANGE, PITCH at max feedback, lane feedback 3, with 0 dBFS noise burst then silence | output decays; peak never above 1.0 before SafetyClip × 4 (12 dB) |
| D5 | Denormals: 60 s of 1e−30 noise then silence into every module | processing time per block does not rise more than 20 % vs normal signal |
| D6 | SAT aliasing: 5 kHz sine at −6 dBFS, drive 24 dB, 48 kHz | aliased components below −60 dBc with oversampling on |
| D7 | Latency: plugin reports `L_FIXED`; impulse alignment against the dry path for: no SAT, 1 SAT, 2 SAT, split with SAT in one half, 2/3/4 lanes chained with `&` each holding 2 SAT | exact sample alignment |
| D8 | Mod and macro: sweeping mod −1..1 and Macro 0..1 on every module. Macro 0.5 vs a build of the chain with macro scaling disabled | no clicks: sample-to-sample step below −40 dBFS on a steady sine; Macro 0.5 difference below −120 dBFS |
| D9 | Time base: DELAY, TREM, FREEZE, LFO A at `timeDiv` 4, 8, 16 and 90, 120, 174 BPM | period within ±1 sample of the formula in `03-dsp-catalog.md` §1.3 |
| D10 | Lane stages: band, drive, split, chain (`&`), duck, lane mix, level, width each toggled alone | measured gain / response matches `03-dsp-catalog.md` §1.1 within ±0.5 dB |
| D11 | SafetyClip: sine sweep −20..+12 dBFS | output = input below −3 dBFS (error < −100 dBFS); never above 1.0 |
| D12 | Calibration: each effect at `u = 32768`, −18 dBFS pink in | output RMS −18 ± 3 dBFS |

## R. Offline render

| ID | Test | Pass |
|---|---|---|
| R1 | Bit identity: 30 corpus seeds (all 16 effects covered, including GRAIN, WOW, CRUSH, LFO B), fresh processor instance each time, 10 s at 48 kHz / 512, rendered twice | `memcmp` equal |
| R2 | Block-size invariance: same 30 seeds at block 32, 512, 4096 and a random-size sequence (17..1024) | peak difference below −100 dBFS |
| R3 | Reset: process 5 s, call `reset()`, process again | second render equals the first bit for bit |
| R4 | Swap: change text mid-render at a fixed sample | no sample-to-sample step above −30 dBFS relative to signal RMS across the swap; total swap fade = 30 ms ± 1 block |
| R5 | State round trip: render, save state, new instance, load state, render | bit identical to R1 output |
| R6 | Transport reset: run 1 = 3 s stopped then play; run 2 = 11 s stopped (different input) then play; compare 10 s after play starts. Also: a position jump while playing does not reset (tail continues) | bit identical after the 5 ms fade; tail present after jump |
| R7 | Bus layouts: R1 for mono→mono and mono→stereo | bit identical twice; mono→mono output equals the mid of the stereo render within −100 dBFS |

## RT. Realtime safety

| ID | Test | Pass |
|---|---|---|
| RT1 | RealtimeSanitizer (linux-clang): `Engine::process` marked `[[clang::nonblocking]]`; run R1 seeds plus 200 chain swaps and param automation | zero reports |
| RT2 | Alloc guard (Windows): test binary replaces global `operator new/delete`; a thread-local flag set around `process` | zero allocations inside `process`, including during swaps |
| RT3 | Lock audit: `grep` gate in CI for `std::mutex`, `CriticalSection`, `std::lock_guard`, `new`, `malloc`, `std::vector::push_back` in `dsp/` files marked `// audio-thread` | none |
| P1 | CPU: worst-case corpus seeds (4 lanes × 8 modules with PITCH, GRAIN, VERB) 60 s at 48 kHz / 512 | on the dev machine ≥ 10× realtime; in CI ≥ 4× realtime |
| P2 | Chain build time on the message thread, worst case | ≤ 30 ms median, ≤ 60 ms max |
| P3 | Typing storm: 50 text changes per second for 20 s while processing | no audio dropouts in the offline harness, retire FIFO never overflows, memory returns to baseline ± 5 % |

## V. Plugin validation and hosts

| ID | Test | Pass |
|---|---|---|
| V1 | pluginval, Windows, fixed `--seed`, 3 different seeds. CI runs with `--skip-gui-tests`; the run with GUI tests is done on the dev machine and its log committed under `tests/manual/` | pass. Strictness 5 is the gate from task T01 onward; 10 from T30. |
| V2 | DAW smoke (manual, checklist in `tests/manual/daw-smoke.md`): REAPER, Ableton Live 12, FL Studio, Bitwig Studio, Studio One, Cubase | for each: scan and load; type text including spaces; audio changes; save, close, reopen project, sound identical; automate Mix and Macro; resize; two instances with editors open; bypass; offline bounce twice and null the bounces (below −100 dBFS) |
| V3 | Steinberg VST3 validator (`validator.exe` from the VST3 SDK) | pass |
| V4 | Sample rate / buffer changes while loaded in REAPER (44.1 ↔ 96 k, 64 ↔ 2048) | no crash, latency reported correctly, sound recovers |

K1 gate (`02-spec.md`): the "type text including spaces" row of V2 is run for REAPER, Live and FL Studio at task T07.

## U. UI

| ID | Test | Pass |
|---|---|---|
| U1 | Input latency (Playwright, mock bridge): 200 key presses; time from `keydown` to the next frame where the character is painted | p95 ≤ 16 ms |
| U2 | Chain feedback latency (Playwright, mock bridge with 5 ms delay; and one manual run in the real plugin with timing logs): `keydown` to key row updated | p95 ≤ 100 ms |
| U3 | Automation round trip (plugin test, `tests/plugin`): set `mix`, `macro`, `output` from the host side → the UI relay reports the value; move the UI slider → host receives begin gesture, value, end gesture | values equal within 1e−6, gesture order correct |
| U4 | Token colouring: for the 14 worked examples the DOM classes match the token list from `core` | exact |
| U5 | States: S0..S3, E1..E4 each reachable and matching `05-ui-design.md` §3 (Playwright screenshots at 720, 900, 1600 px) | reviewed and committed as baselines |
| U6 | Accessibility: axe-core scan, keyboard-only walkthrough of the producer path (`05-ui-design.md` §1), reduced-motion run | zero serious axe violations; every step reachable by keyboard |
| U7 | Hold / edit / add / remove through the UI against the mock bridge | bridge receives the calls in `05-ui-design.md` §6 with correct payloads |
| U8 | Long text: 4096 bytes and 200 lines | no layout break, frame time ≤ 16 ms while typing |
| U9 | Editor open / close 100 times, two editors open | no crash, no leak (plugin test + manual) |

## Milestone review

After each milestone the builder runs every test ID assigned to tasks in that milestone plus all earlier ones, and lists results by ID. Final review runs the whole plan.
