# 08 Plan review (adversarial pass)

Done inline by the planner (a subagent would have needed the whole doc set re-read; not cheaper). Scope: `02` to `07`. Every finding below is fixed in the named doc unless marked "accepted".

## Findings and fixes

| # | Type | Finding | Fix | Where |
|---|---|---|---|---|
| 1 | Contradiction | Empty text gives zero lanes, so `wet = 0`; with default Mix 100 the plugin would output silence, against A1 (passthrough). | Zero lanes: output = delayed dry, Mix ignored. Lane with zero modules passes its input. | `02` §5, `03` §1.4, test D1 |
| 2 | Edge case | Lanes chained with `&` each padded to `L_FIXED` would stack latency and misalign with the dry path. | Padding is per chain group, at the summed lane. `L_FIXED` raised from 2 to 8 × `L_SAT` (4 lanes × 2 SAT). | `02` §1, §5, `03` §1.1, test D7, T09 |
| 3 | Edge case | `split` with SAT in only one half: halves have different latency, comb filtering. | Delay the lower-latency half. | `03` §1.1, D7, T09 |
| 4 | Determinism | LFO phases, FREEZE counters and PRNGs free-run from chain build, so two DAW bounces of the same project differ. The smoke test "bounce twice and null" could never pass. | Reset chain state on transport stopped → playing (C9, D16). Loop wraps and jumps do not reset. | `02` §6, §9, `03` §1.4, test R6, T12 |
| 5 | Determinism | Git line-ending conversion on Windows could rewrite the golden corpus bytes. | Corpus is pure-ASCII escaped records and `tests/golden/*` is `-text` in `.gitattributes`. | `06` G3, T08 |
| 6 | Missing | `setStateInformation` / `prepareToPlay` can arrive off the message thread; an async chain build would let an offline render start with the wrong chain. | Synchronous build under one non-audio mutex. | `02` §2, T05, T06 |
| 7 | Contradiction | Architecture sketch listed a native `roll` function; Roll is UI-only (A9). | Sketch lists `setUi` instead. | `02` §2 |
| 8 | Over-scope | Postmark click-to-copy: not asked for; select-all and copy already works in the text box. | Removed. | `05` §2 |
| 9 | Test gap | "Macro 0.5 is neutral" had no test. | D8 extended. | `06` D8 |
| 10 | Test gap | Mono bus layouts untested. | R7 added, run from T06. | `06` R7, T06 |
| 11 | Unclear acceptance | F2 (2000 seeds × 5 s) would take 40+ minutes in CI and would be skipped in practice. | 2 s renders; CI runs 300, the full set runs at T20 and T33. | `06` F2 |
| 12 | Unclear acceptance | pluginval GUI tests in a headless CI session are unreliable with a WebView. | CI skips GUI tests; GUI run is local with the log committed. | `06` V1 |
| 13 | Missing | T07 did not say how the web build and the WebView2 SDK reach the CMake build. | Added to T07. | `07` T07 |
| 14 | Over-scope check | Five CI jobs for a Windows-only plugin. | Accepted: the brief requires the golden corpus on every OS and compiler, and `core` is portable by design. Non-Windows jobs build only `core`/`dsp` tests. | none |
| 15 | Over-scope check | 16 effects including PITCH and GRAIN. | Accepted: owner's choice (Q7). Both have low generation weight and their own tasks (T18, T19) so they can slip without blocking M4 to M6. | none |
| 16 | Determinism risk | JUCE DSP classes (Reverb, Chorus, Phaser, Compressor) could change between JUCE versions and alter how seeds sound. | Accepted with guard: JUCE pinned to one tag; a JUCE upgrade is a grammar-level change needing owner approval (C8 applies to sound once v1 ships). | `02` A4, `HANDOFF.md` |
| 17 | Determinism risk | Text hashed as raw bytes: the same visible glyph in composed and decomposed Unicode forms gives different seeds. | Accepted (D6): documented limit, listed in README by T32. | `02` D6 |
| 18 | Risk | Held modules lose their tails on every reroll. | Accepted for v1 (marked shortcut in `02` §2). | none |
| 19 | Risk | K1 (host swallows space) can invalidate the UI approach. | Already gated at T07, before any UI work. | `07` T07 |

## Checks that found nothing

| Check | Result |
|---|---|
| Names across docs (lane, module, slot, hold, ChainSpec, param names, effect IDs) | consistent with the glossary in `00` |
| Every test ID in `06` is used by a task in `07` | yes (G1 to G5, F1 to F5, D1 to D12, R1 to R7, RT1 to RT3, P1 to P3, V1 to V4, U1 to U9) |
| Every interview decision Q1 to Q10 appears in `02` | yes |
| Grammar tags in `03` equal the table used to compute the vectors in `04` | yes, re-read side by side |
| Vertical slice early | T05/T06 give audible output from typed text; T07 gives the web text box |
