# 02 Spec: Verbatim v1

Names follow the glossary in `00-brief.md`. Grammar: `04-seed-grammar.md`. Effects: `03-dsp-catalog.md`. UI: `05-ui-design.md`.

## 1. Product

| Item | Value |
|---|---|
| Type | Stereo audio effect (insert). Buses: mono→mono, mono→stereo, stereo→stereo. Internal processing is always stereo. |
| Format / OS | VST3, Windows 10 and 11, x64. |
| Sample rates | 44.1 to 192 kHz. Block size 16 to 8192, any. |
| Latency | Constant, reported once in `prepareToPlay`: `L_FIXED = 8 × L_SAT` samples (`L_SAT` = integer latency of the 2× oversampler, read at prepare; 8 = 4 lanes × 2 SAT, the worst chained case). Never changes while loaded. |
| Host parameters | `mix` 0..100 %, default 100. `macro` 0..1, default 0.5. `output` −24..+12 dB, default 0. `bypass` (host bypass). All automatable, all smoothed 20 ms. |
| Not host parameters | Seed text, overrides, generated module params. They live in plugin state. |
| Licence | AGPLv3. Dependencies must be AGPL-compatible. |

## 2. Architecture

```
gui/ (React + framer-motion, in WebView2)
   | native functions: setText, setOverride, getState, setUi   events: chain, levels, params
plugin/ (JUCE 8)  VerbatimProcessor, VerbatimEditor, Bridge
   | message thread: text -> core.generate -> apply overrides -> build Chain (allocates) -> atomic handoff
dsp/  Engine, Lane, Module (16 effects)          audio thread: no alloc, no locks
core/ verbatim_core: tokenizer, generator, ChainSpec, serialization, XXH64, SplitMix64
       pure C++20, no JUCE, no floats in the generation path
```

| Component | Path | Depends on | Responsibility |
|---|---|---|---|
| core | `core/` | C++20 std, vendored `xxhash.h` | Text → ChainSpec. Overrides → ResolvedChain. Canonical serialization. Single source of grammar truth (the UI never re-implements it). |
| dsp | `dsp/` | core, `juce_dsp`, `juce_audio_basics` | ResolvedChain → running audio. Chain swap, safety, meters. |
| plugin | `plugin/` | dsp, JUCE plugin client, `juce_gui_extra` | Host params, state, WebView, bridge. |
| gui | `gui/` | React 18, TypeScript, Vite, `framer-motion`, `@phosphor-icons/react` | All visuals and motion. Built to static files, embedded as binary data, served by a JUCE resource provider. No network at run time. |
| tools | `tools/vb_spec` | core | CLI: text → ChainSpec dump and chain hash. Generates and checks the golden corpus. |

### Threads

| Thread | Does | Must never |
|---|---|---|
| Audio | `Engine::process`. Takes a pending chain pointer at block start, crossfades, pushes the old chain to the retire FIFO. Writes meter atomics. | Allocate, lock, log, touch the WebView. |
| Message | Everything else: generate, build and prepare chains, delete retired chains (30 Hz timer), send events to the UI, state save and load. | Block on the audio thread. |
| Any other non-audio thread | Hosts may call `setStateInformation` and `prepareToPlay` off the message thread. Both build the chain synchronously (so an offline render starts with the right chain) under one non-audio mutex shared with the message-thread build path. UI notification is posted to the message thread. | Take that mutex on the audio thread. |

### Chain swap
1. Message thread builds a complete `Chain` (all buffers allocated and prepared), then `pending.exchange(newChain)`. If the exchange returns a chain the audio thread never took, the message thread deletes it.
2. Audio thread, at block start, when not already fading and the retire FIFO has space: takes `pending`, runs old and new chains in parallel for 30 ms with a linear crossfade, then pushes the old chain to the retire FIFO (capacity 8).
3. New chains start with empty state (cleared buffers, LFO phase 0). Held modules also restart. `shortcut: tails are not carried across a reroll, upgrade if users complain about cut tails on held delays.`

## 3. Data model

```
ChainSpec     { grammarVersion: u16, lanes: LaneSpec[0..4] }
LaneSpec      { seed: u64, nModules: 2..6, modules: ModuleSpec[nModules],
                drive, duck, feedback: 0..3, split, chain, band: 0|1, bandCenter: u16,
                mixSteps: -3..3, mixAbs: 0..100 | 255 (none), timeDiv: 0 | {1,2,3,4,6,8,12,16,24,32},
                level, space: -3..3, emphasis: 0..3,
                lfoA {shape 0..3, rate u16, depth u16, target 0..5}, lfoB {rate, depth, target}, env {depth, target} }
ModuleSpec    { type: 0..15, p: u16[6], mix: u16 }
Override      (per lane index) { holds: map slot -> ModuleSpec, removed: set of slot, added: ModuleSpec[0..2], removedForSeed: u64 }
ResolvedChain { lanes: ResolvedLane[], each: LaneSpec fields + modules: ModuleSpec[0..8] with a `held` flag }
```

Resolve rules (core, deterministic):

| Rule | Detail |
|---|---|
| R1 Hold | A held slot `k` uses the stored ModuleSpec. If `k ≥ nModules`, the lane is extended to `k+1` using the generated slots in between (the generator always computes all 6 slots). |
| R2 Edit | Editing any param of a module stores it as a hold with the edited values (auto-hold). |
| R3 Remove | Removing a generated, unheld slot records a tombstone for the current lane seed. Tombstones are dropped when the lane seed changes. Removing a held or added module deletes that override. |
| R4 Add | Added modules are always held and are appended after the generated slots. Max 2 per lane (lane max 8 modules). Max 2 SAT per lane (latency budget); the UI disables SAT in the add menu at the limit. |
| R5 Identity | Overrides are keyed by lane index 0..3. If a lane disappears its overrides stay dormant in state and return when the lane exists again. |
| R6 Mod targets | `target % moduleCount` of the resolved lane. A lane with 0 modules has no modulation. |
| R7 Modifiers | Direction biases are baked into ModuleSpec at generation, so held modules ignore later direction words. Lane-level fields always follow the text. |

## 4. State

UTF-8 JSON in `getStateInformation`:

```json
{ "stateVersion": 1, "grammarVersion": 1, "text": "...", "overrides": [ { "lane": 0, "holds": { "1": {"type":3,"p":[..6],"mix":0} }, "removed": [2], "removedForSeed": "1e17d464a144ec88", "added": [] } ], "ui": { "w": 900, "h": 620, "advanced": false } }
```

| Case | Behaviour |
|---|---|
| Load with same `grammarVersion` | Regenerate from text, apply overrides. |
| Load with older `grammarVersion` | Generate with that older grammar (all grammar versions stay in the binary). v1 has only version 1. |
| Load with newer / unknown version, or corrupt JSON | Keep audio passthrough, show error state E3 (`05-ui-design.md`), do not overwrite the stored blob until the user types. |
| Text over 4096 bytes | Truncated at a UTF-8 boundary before parsing (grammar §2). |
| State change by typing or override | Call the host "non-parameter state changed" notification so the project is marked dirty. |
| Host presets | The state blob is the preset. The seed text is the shareable preset. |

## 5. Signal flow

```
in ─┬─ dry delay (L_FIXED) ─────────────────────────────────────┐
    └─ lanes (each: band? → drive → [+feedback] → modules → lane mix → duck → level → width → NaN guard)
         lane with `chain` flag takes the previous lane's output as its input; that previous lane leaves the sum
         each summed lane (the end of a chain group) is padded so the group's total latency = L_FIXED
         wet = Σ summed lane outputs × 1/√(lanes in sum)
out = SafetyClip( (dry × (1 − mix) + wet × mix) × outputGain )
```

Zero lanes (empty text): `out = SafetyClip(dry × outputGain)` regardless of `mix` (A1 passthrough). A lane whose modules were all removed passes its input through its lane stages.

Lane stages and constants: `03-dsp-catalog.md` §1.

## 6. Determinism contract

| # | Guarantee | Scope |
|---|---|---|
| C1 | Text → ChainSpec is identical bit for bit on every OS, compiler and CPU, for a given grammar version, forever. | Enforced by golden corpus (`06-test-plan.md` G). |
| C2 | Generation uses only integer arithmetic on fixed-width types. No floats, no `std::` distributions, no `std::hash`, no locale, no Unicode tables. | Code review + C1. |
| C3 | Hash = XXH64. PRNG = SplitMix64. Constants in `04-seed-grammar.md` §3. | Primitive vectors. |
| C4 | All DSP randomness (wander LFO, grain spray, wow, crush jitter) comes from SplitMix64 seeded from the ModuleSpec and lane seed, reset in `reset()`. | Render test R. |
| C5 | Same binary, same sample rate, same block size, same input and state: rendered audio is bit identical. | Render test R1. |
| C6 | Different block sizes: output equal within −100 dBFS peak difference with static parameters. Control-rate updates run on a fixed 32-sample grid counted from chain start, not from host block edges. | Render test R2. |
| C7 | Across compilers or CPUs audio is NOT promised bit identical (libm, FMA). Build uses `/fp:precise`, `-ffp-contract=off`, never fast-math. | Documented limit. |
| C9 | Transport: when the host goes from stopped to playing, the engine resets all chain state (buffers, LFO phases, PRNGs) with a 5 ms fade-in. So two bounces from the same start give the same audio. Position jumps and loop wraps do not reset (tails survive loops). | Render test R6, smoke V2. |
| C8 | Changing any v1 mapping, table, constant, draw order or serialization field requires a new grammar version and the owner's approval. | `HANDOFF.md` rule. |

## 7. Safety

| Risk | Guard |
|---|---|
| Level | Final SafetyClip: transparent below −3 dBFS, tanh knee above, hard clamp at ±1.0. Output never exceeds 0 dBFS. |
| Feedback | Every internal feedback ≤ 0.9 with tanh in the loop. Lane feedback gain ≤ 0.55, band-limited 80 Hz..6 kHz, tanh. |
| Resonance | Filter Q ≤ 8. Comb feedback ≤ 0.9. |
| NaN / Inf | Per-lane check after the lane chain each block: if any non-finite sample, zero the lane block and `reset()` the lane. Counter exposed for tests. |
| Denormals | `ScopedNoDenormals` in `processBlock`. |
| DC | One-pole DC blocker (5 Hz) at each lane output. |
| Silence / pain | Parameter ranges in `03-dsp-catalog.md` are the bounded space. Test S1: for 2000 random seeds, −18 dBFS RMS pink noise in, Mix 100: output RMS within −36..−8 dBFS and peak ≤ 0 dBFS. |
| Clicks | 30 ms crossfade on chain swap. 20 ms smoothing on host params and on any override edit. |

## 8. Realtime rules
- Audio thread: no allocation, no locks, no syscalls, no exceptions. All buffers sized in `prepare` for `maxBlock` and the worst-case sample rate factor.
- Communication: atomics (params, meters, pending chain pointer) and one lock-free FIFO (retire).
- Oversampling: SAT only, 2×, `juce::dsp::Oversampling` half-band FIR equiripple with integer latency.
- Checks: RealtimeSanitizer job on Linux clang for the dsp test binary, plus an allocation guard in the Windows test binary (`06-test-plan.md` RT).

## 9. Decisions (options compared)

| # | Decision | Options | Pick | Reason |
|---|---|---|---|---|
| D1 | Hash input | (a) whole line (b) ordinary words only (c) per word | b | Owner confirmed: modifiers must be audible in isolation, words still avalanche. |
| D2 | Hash / PRNG | (a) XXH64 + SplitMix64 (b) XXH3 + PCG32 (c) FNV + xorshift | a | XXH64 has a frozen spec and endian-independent output; SplitMix64 is 4 lines of integer code with no state-size choices. |
| D3 | Streams | (a) one PRNG stream (b) one stream per slot + one structure stream | b | Slot k is independent of module count, so holds and lane extension are stable. |
| D4 | Param representation | (a) floats (b) u16 normalized integers, mapped to units in DSP | b | Keeps C1 free of libm; golden hash covers integers only. |
| D5 | Direction bias | (a) float pow (b) integer repeated multiply (c) range clamp | b | Deterministic, monotonic, never leaves 0..65535. |
| D6 | Unicode | (a) NFC + casefold (b) raw UTF-8 bytes, ASCII-only lowercase | b | Unicode tables change between versions; raw bytes cannot. Cost: composed and decomposed forms of the same glyph are different seeds. |
| D7 | Lines | (a) mood (b) serial sections (c) parallel lanes with `&` to chain | c | Owner asked for independent sentences that both shape the output. |
| D8 | Chain swap | (a) preallocate every effect in every slot (b) build off-thread, pointer handoff, crossfade | b | (a) costs hundreds of MB; (b) is the standard lock-free pattern. |
| D9 | Latency | (a) dynamic PDC per chain (b) constant `L_FIXED`, lanes padded | b | Latency changes on every keystroke would glitch the host. |
| D10 | Automation | (a) expose generated params (b) fixed 4 host params (c) 8 assignable slots | b | Generated param set is not stable for a host. (c) is the first post-v1 candidate. |
| D11 | Overrides | (a) per-param overrides (b) whole-module holds | b | One concept (hold) covers lock, edit and add. |
| D12 | Grammar location | (a) C++ and JS copies (b) C++ only, UI gets tokens over the bridge | b | One implementation, one golden corpus. |
| D13 | Reverb, chorus, phaser, flanger, compressor | (a) hand-written (b) `juce::dsp` classes | b | Already a dependency; hand-written DSP only where JUCE has none. |
| D14 | Text application | (a) apply on Enter (b) apply on every keystroke | b | Per-character feedback is the product; crossfade keeps it click free. |
| D16 | Repeatable bounces | (a) free-running state (b) derive every phase from host position (c) reset on transport start | c | (a) makes two bounces differ; (b) cannot restore buffer history anyway; (c) is one call. |
| D15 | UI tech | (a) JUCE native (b) WebView React | b | Owner decision Q2; animotion (framer-motion) runs only there. |

## 10. Assumed defaults (not asked; change by telling the builder)

| # | Assumption |
|---|---|
| A1 | Default `mix` is 100 %. First load has empty text and passes audio through unchanged. |
| A2 | Max 4 lanes, lane max 8 modules, text max 4096 bytes. Lines after the 4th are ignored and shown struck through. |
| A3 | No tempo information from host means 120 BPM. |
| A4 | Build: CMake ≥ 3.24, MSVC 2022, JUCE pinned to one 8.0.x tag (recorded in `CMakeLists.txt`), Catch2 v3, Node 20 LTS, pnpm or npm (builder's choice, record it). |
| A5 | CI: GitHub Actions. Windows job builds plugin, runs all tests and pluginval. Linux and macOS jobs build and run core and dsp tests only (golden determinism, RTSan). |
| A6 | Distribution: GitHub release with an Inno Setup installer that copies the `.vst3` and checks for the WebView2 runtime. Not code signed in v1 (SmartScreen warning accepted). |
| A7 | Fonts and icons are bundled (Courier Prime, Archivo, IBM Plex Mono: OFL; Phosphor: MIT). |
| A8 | WebView2 user-data folder is a per-user temp directory. If WebView2 is missing, the editor shows a native message with the download link; audio still works. |
| A9 | The "roll" control writes random words from a built-in list into the text box. It is a UI convenience and has no determinism role. |
| A10 | Company / manufacturer name and VST3 IDs: builder proposes, owner confirms at task T01. |

## 11. Risks

| # | Risk | Mitigation |
|---|---|---|
| K1 | Host swallows keys (space bar) before the WebView text box gets them. | Proven in task T07 in three hosts before any further UI work. If it fails in two or more: stop and ask the owner (fallback is a native `juce::TextEditor` for input under the web visuals). |
| K2 | JUCE WebView host bugs (freeze before first page load, scaling in Live). | Never call into the page before the page-loaded callback. Smoke list V2 in `06-test-plan.md`. |
| K3 | Chain build too slow for per-keystroke typing. | Build is on the message thread and coalesces to the latest text. Budget: build ≤ 30 ms for the worst chain (test P2). |
| K4 | CPU of worst-case chain (4 lanes × 8 modules). | Budget test P1. PITCH, GRAIN and VERB have low generation weights. |
| K5 | Spec text and worked examples disagree. | The examples were produced by a reference script from this spec. If C++ disagrees with an example, stop and ask the owner; do not edit the example. |
