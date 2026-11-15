# 03 DSP catalog

Effect IDs, weights, param order and tags are part of grammar v1 (frozen, `02-spec.md` C8). Ranges, curves and DSP notes may be tuned until v1 ships, then they are frozen too (a retune after release changes how old seeds sound).

## 0. Conventions

| Item | Rule |
|---|---|
| Normalized value | `u` = u16 from ModuleSpec, `x = u / 65535.0` |
| `lin(lo, hi)` | `lo + (hi − lo) · x` |
| `exp(lo, hi)` | `lo · (hi / lo)^x` |
| `disc(n)` | `(u · n) >> 16`, integer 0..n−1 |
| Module mix | Every module blends `dry · (1 − m) + wet · m` with `m` from its `mix` range, then Macro (§1.4). |
| Mod input | Each module has one mod target. Mod value `M ∈ [−1, 1]` = sum of the lane sources aimed at it, clamped. Updated every 32 samples, smoothed. |
| Control rate | 32-sample grid counted from chain start (`02-spec.md` C6). |
| Randomness | SplitMix64 seeded with `laneSeed ^ (slotIndex + 1)`, reset in `reset()`. |
| Module interface | `prepare(sampleRate, maxBlock)`, `reset()`, `process(L, R, n, ctx)`, `latencySamples()`. `ctx` carries mod value, tempo, macro. No allocation in `process`. |
| Tags | Axis and polarity used by `bias` (`04-seed-grammar.md` §6). `+` means a positive axis raises `u`. Only p0..p3 can be tagged. p4, p5 unlisted = reserved. |

## 1. Lane and global stages

### 1.1 Lane chain, in order

| Stage | Active when | Definition |
|---|---|---|
| Input | always | Plugin input, or previous lane's output when `chain = 1` and lane index > 0. |
| Band | `band = 1` | Band-pass of the lane input: 12 dB/oct HP at `fc / 2.4` and 12 dB/oct LP at `fc · 2.4`, `fc = exp(200, 4000)` of `bandCenter`. |
| Drive | `drive > 0` | `y = tanh(x · g) / g^0.5`, `g = 10^(4·drive / 20)`. |
| Feedback in | `feedback > 0` | Adds the feedback tap (below). |
| Modules | always | Serial. If `split = 1` and ≥ 2 modules: modules `0 .. ceil(n/2)−1` and the rest run in parallel from the same input, the lower-latency half delayed to match the other, outputs summed × 0.707. Zero modules: pass. |
| Feedback tap | `feedback > 0` | Modules output → 10 ms delay → HP 80 Hz → LP 6 kHz → × `[0, 0.25, 0.40, 0.55][feedback]` → `tanh` → Feedback in. |
| Lane mix | always | `w = (mixAbs == 255 ? 1.0 : mixAbs / 100) + 0.15 · mixSteps`, clamped 0.10..1.0. `y = laneInput · (1 − w) + modulesOut · w` (lane input delayed by the lane's module latency). |
| Duck | `duck > 0` | Gain reduction up to `3 · duck` dB driven by the lane input envelope (attack 5 ms, release 150 ms, full depth at −12 dBFS). |
| Level | always | Gain `2 · level` dB. |
| Width | `space ≠ 0` | Mid/side: side gain `1.5 · space` dB. |
| DC block | always | One-pole HP 5 Hz. |
| NaN guard | always | `02-spec.md` §7. |
| Pad | lane is in the sum | Delay so the total latency of the lane, including every lane chained into it, = `L_FIXED`. Lanes consumed by a `chain` lane are not padded. |

### 1.2 Modulation sources (per lane)

| Source | Shape | Rate | Depth (as fraction of the target's mod range) |
|---|---|---|---|
| LFO A | `lfoA.shape`: 0 sine, 1 triangle, 2 ramp down, 3 smoothed square (5 ms slew) | free: `exp(0.05, 8)` Hz of `lfoA.rate`. If `timeDiv ≠ 0`: period = 4 notes of 1/`timeDiv` | `lfoA.depth / 65535` |
| LFO B | Random walk: new uniform target each period, cosine interpolated | `exp(0.03, 2)` Hz of `lfoB.rate` | `lfoB.depth / 65535` |
| ENV | Envelope follower of lane input (attack 10 ms, release 200 ms), scaled so −12 dBFS RMS = 1 | n/a | `env.depth / 65535` |

Each source goes to the module at index `target % moduleCount`. Unipolar ENV maps 0..1 to 0..+depth; LFOs are bipolar.

### 1.3 Time base

Note length `T = (4 / timeDiv)` beats at host tempo (120 BPM if unknown). Used by DELAY time, TREM period, FREEZE length, LFO A (period `4T`). If `T` exceeds a module's max time, halve it until it fits. Tempo changes are smoothed over 50 ms. `timeDiv = 0`: free-running values from params.

### 1.4 Global

| Stage | Definition |
|---|---|
| Lane sum | `wet = Σ laneOut × 1/√N`, N = lanes whose output is not consumed by a `chain` lane. N = 0: output is the delayed dry signal, Mix ignored. |
| Transport reset | Stopped → playing: `reset()` every lane, 5 ms fade-in (`02-spec.md` C9). |
| Macro `m` (0..1) | Per module effective mix: `m ≤ 0.5`: `mix · 2m`. `m > 0.5`: `mix + (1 − mix) · (2m − 1) · 0.75`. All mod depths × `2m`. `m = 0.5` leaves the seed untouched. |
| Mix | `out = dryDelayed · (1 − mix) + wet · mix`. |
| Output | Gain in dB. |
| SafetyClip | `a = |x|`. `a ≤ 0.7079`: pass. Else `0.7079 + 0.2921 · tanh((a − 0.7079) / 0.2921)`, sign restored, then clamp to ±1.0. |
| Meters | Per-lane RMS (300 ms) and output peak L/R, written to atomics each block. |

## 2. Effects

`w` = generation weight (sum 99).

| ID | Name | w | Mod target | Latency |
|---|---|---|---|---|
| 0 | SAT | 10 | drive | `L_SAT` |
| 1 | CRUSH | 6 | rate | 0 |
| 2 | FILTER | 10 | cutoff | 0 |
| 3 | DELAY | 10 | time | 0 |
| 4 | VERB | 8 | mix | 0 |
| 5 | CHORUS | 7 | depth | 0 |
| 6 | PHASER | 7 | centre freq | 0 |
| 7 | TREM | 6 | depth | 0 |
| 8 | WOW | 6 | wow depth | 0 |
| 9 | FREEZE | 3 | mix | 0 |
| 10 | COMB | 5 | pitch | 0 |
| 11 | RING | 4 | freq | 0 |
| 12 | PITCH | 3 | detune | 0 |
| 13 | GRAIN | 3 | spray | 0 |
| 14 | COMP | 6 | threshold | 0 |
| 15 | FLANGE | 5 | delay | 0 |

### Parameters

| ID | Param | Tag | Mapping | Notes |
|---|---|---|---|---|
| 0 SAT | p0 drive | HEAT + | `lin(0, 24)` dB | Mod range ±6 dB. |
| | p1 tone | TONE + | `lin(−6, 6)` dB tilt at 1 kHz, after the shaper | |
| | p2 shape | | `disc(4)`: tanh, asymmetric tube (`tanh(x + 0.3·x²)` DC-blocked), hard clip with 0.1 knee, sine fold | |
| | p3 bias | | `lin(0, 0.25)` DC offset before the shaper, removed after | |
| | mix | DEPTH + | `lin(0.5, 1.0)` | Output trim `−0.5 · drive` dB. 4× oversampled. |
| 1 CRUSH | p0 bits | HEAT − | `lin(4, 12)` bits, fractional allowed | Quantize with TPDF-free rounding. |
| | p1 rate | TONE + | `exp(2000, 24000)` Hz sample-and-hold | Mod range ±1 octave. |
| | p2 jitter | | `lin(0, 0.3)` of the hold period, from module PRNG | |
| | mix | DEPTH + | `lin(0.3, 1.0)` | Aliasing is intended; not oversampled. |
| 2 FILTER | p0 cutoff | TONE + | by mode: LP `exp(200, 12000)`, HP `exp(40, 2000)`, BP and notch `exp(150, 6000)` Hz | Mod range ±2 octaves, clamped 20 Hz..0.45·fs. |
| | p1 reso | | Q `exp(0.5, 8)` | |
| | p2 mode | | `disc(4)`: LP, HP, BP, notch | |
| | p3 slope | | `disc(2)`: 12, 24 dB/oct | TPT state-variable filter (`juce::dsp::StateVariableTPTFilter`), two in series for 24. |
| | mix | DEPTH + | `lin(0.7, 1.0)` | |
| 3 DELAY | p0 time | RATE − | `exp(30, 900)` ms, or `T` (§1.3), max 2000 ms | Mod range ±2 % (tape wobble), 50 ms glide on change. |
| | p1 feedback | TAIL + | `lin(0, 0.8)` | `tanh` in the loop. |
| | p2 tone | TONE + | loop LP `exp(1000, 12000)` Hz, fixed loop HP 60 Hz | |
| | p3 mode | | `disc(3)`: stereo, ping-pong, tape (adds 0.3 % wow at 0.7 Hz and soft saturation in the loop) | Linear interpolation; tape mode uses 3rd-order Lagrange. |
| | mix | DEPTH + | `lin(0.15, 0.5)` | |
| 4 VERB | p0 decay | TAIL + | room size `lin(0.3, 0.95)` | `juce::dsp::Reverb`. `shortcut: Freeverb core, replace with an FDN if the tail quality is criticised.` |
| | p1 damp | TONE + | damping `lin(0.8, 0.1)` (brighter as `u` rises) | |
| | p2 predelay | | `lin(0, 80)` ms | Own delay line before the reverb. |
| | p3 width | SPACE + | `lin(0.5, 1.0)` | |
| | mix | DEPTH + | `lin(0.10, 0.45)` | Mod range ±0.15 on mix. |
| 5 CHORUS | p0 rate | RATE + | `exp(0.1, 3)` Hz | `juce::dsp::Chorus`. |
| | p1 depth | | `lin(0.1, 0.6)` | Mod range ±0.2. |
| | p2 centre delay | | `lin(7, 25)` ms | |
| | p3 feedback | | `lin(−0.3, 0.3)` | |
| | mix | DEPTH + | `lin(0.3, 0.6)` | |
| 6 PHASER | p0 rate | RATE + | `exp(0.05, 4)` Hz | `juce::dsp::Phaser`. |
| | p1 depth | | `lin(0.3, 1.0)` | |
| | p2 centre freq | TONE + | `exp(300, 3000)` Hz | Mod range ±1 octave. |
| | p3 feedback | TAIL + | `lin(0, 0.85)` | |
| | mix | DEPTH + | `lin(0.3, 0.6)` | |
| 7 TREM | p0 rate | RATE + | `exp(0.5, 12)` Hz, or period `T` | |
| | p1 depth | | `lin(0.2, 0.9)` | Mod range ±0.2. |
| | p2 shape | | `disc(3)`: sine, triangle, square with 3 ms slew | |
| | p3 pan | SPACE + | `lin(0, 1)`: 0 = both channels in phase (tremolo), 1 = opposite phase (auto-pan) | |
| | mix | DEPTH + | `lin(0.7, 1.0)` | |
| 8 WOW | p0 wow rate | RATE + | `exp(0.3, 1.5)` Hz | Modulated delay line, base 15 ms, Lagrange interpolation. |
| | p1 wow depth | | `lin(0.5, 5)` ms | Mod range ±1 ms. |
| | p2 flutter | | `lin(0, 0.4)` ms at 9 Hz plus 10 % random from module PRNG | |
| | p3 age | TONE + | LP `exp(3500, 16000)` Hz | |
| | mix | DEPTH + | `lin(0.8, 1.0)` | |
| 9 FREEZE | p0 length | RATE − | `exp(60, 500)` ms, or `T` | Loop buffer 2 s. |
| | p1 period | | `2 + disc(7)` = every 2..8 lengths a new slice is captured | Counter starts at chain start / `reset()`. `shortcut: not locked to host transport position, upgrade if users need bar-aligned stutter.` |
| | p2 repeats | | `1 + disc(3)` repeats of the slice, 5 ms crossfades | |
| | mix | DEPTH + | `lin(0.3, 0.8)` | Mod range ±0.2 on mix. |
| 10 COMB | p0 pitch | TONE + | `exp(55, 880)` Hz, snapped to the nearest equal-tempered semitone (A = 440) | Mod range ±50 cents. |
| | p1 feedback | TAIL + | `lin(0.3, 0.9)` | `tanh` in the loop. |
| | p2 damp | TONE + | loop LP `exp(1500, 12000)` Hz | |
| | p3 polarity | | `disc(2)`: +, − | |
| | mix | DEPTH + | `lin(0.2, 0.6)` | |
| 11 RING | p0 freq | TONE + | `exp(20, 2000)` Hz | Mod range ±1 octave. |
| | p1 wave | | `disc(2)`: sine, triangle | |
| | mix | DEPTH + | `lin(0.15, 0.6)` | |
| 12 PITCH | p0 interval | | `disc(8)` of `[−12, −7, −5, +3, +4, +5, +7, +12]` semitones | Two crossfading delay taps (zero reported latency). |
| | p1 detune | | `lin(−15, 15)` cents | Mod range ±10 cents. |
| | p2 window | | `lin(30, 80)` ms | |
| | p3 feedback | TAIL + | `lin(0, 0.4)` | |
| | mix | DEPTH + | `lin(0.2, 0.5)` | |
| 13 GRAIN | p0 size | RATE − | `exp(20, 200)` ms, Hann window | Buffer 1 s, max 16 simultaneous grains, preallocated. |
| | p1 density | RATE + | `exp(4, 40)` grains/s | |
| | p2 spray | SPACE + | `lin(0, 500)` ms position jitter, each grain random pan ± `x` | Mod range ±100 ms. All randomness from module PRNG. |
| | p3 pitch set | | `disc(4)`: `{0}`, `{0, +12}`, `{0, −12}`, `{0, +7, +12}`, grain picks uniformly | |
| | p4 reverse | | `lin(0, 0.5)` probability | p4 is used and untagged. |
| | mix | DEPTH + | `lin(0.2, 0.6)` | |
| 14 COMP | p0 threshold | HEAT − | `lin(−30, −8)` dB | `juce::dsp::Compressor`. Mod range ±6 dB. |
| | p1 ratio | HEAT + | `exp(2, 10)` | |
| | p2 attack | | `exp(1, 50)` ms | |
| | p3 release | TAIL + | `exp(40, 400)` ms | |
| | mix | DEPTH + | `lin(0.5, 1.0)` | Auto make-up `−threshold · (1 − 1/ratio) · 0.5` dB. |
| 15 FLANGE | p0 rate | RATE + | `exp(0.05, 2)` Hz | `juce::dsp::Chorus` with a short centre delay. |
| | p1 depth | | `lin(0.2, 0.8)` | |
| | p2 delay | | `lin(1, 8)` ms centre delay | Mod range ±1 ms. |
| | p3 feedback | TAIL + | `lin(−0.85, 0.85)` | |
| | mix | DEPTH + | `lin(0.3, 0.5)` | |

## 3. Calibration

| Item | Rule |
|---|---|
| Per-effect trim | Each effect has one constant output trim (dB) chosen so that, at `u = 32768` for all params and −18 dBFS RMS pink noise in, output RMS is −18 ± 3 dBFS. Stored in one table `dsp/Trims.h`. Set in task T20. |
| Seed space | Test S1 (`02-spec.md` §7) must pass with the trims in place. If a seed fails, narrow that effect's range here; do not special-case seeds. |
| Mod ranges | "Mod range" is the excursion at `M = ±1`. The modulated value is clamped to the param's mapped range, except FILTER cutoff which is clamped as stated. |
