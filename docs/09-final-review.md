# 09 Final review

Cross-document consistency check of `00` to `08` and `HANDOFF.md`, done 2026-11-16.

## Consistency

| Check | Method | Result |
|---|---|---|
| Product name, format, OS, licence | read `00`, `02` §1, `HANDOFF` | Verbatim, VST3, Windows x64, AGPLv3 everywhere |
| Versions | `GRAMMAR_VERSION = 1`, `stateVersion 1`, JUCE 8.0.x pinned, Node 20, C++20 | same in `02`, `04`, `HANDOFF` |
| Limits | 4 lanes, 2..6 generated modules, 8 modules per lane, 2 added, 2 SAT per lane, 4096 bytes | same in `00`, `02`, `04`, `05`, `06` |
| Latency | `L_FIXED = 8 × L_SAT`, padding per chain group | `02` §1 and §5, `03` §1.1, `06` D7; no stale "2 ×" left (searched) |
| Host params | `mix` 0..100 default 100, `macro` 0..1 default 0.5, `output` −24..+12 dB default 0, bypass | ranges and defaults in `02` §1; names match in `03` §1.4, `05` §6, `06` U3 (the prototype's Mix at 80 is illustrative) |
| Ops and directions | 13 ops, 14 direction words, 7 axes | `04` §5 tables match `03` §1.1 stage definitions one to one |
| Effect table | 16 IDs, weights sum 99, tags | `03` §2 matches the table the vectors in `04` §8 were computed with |
| Bridge names | `setText`, `setOverride`, `getState`, `setUi`, events `chain`, `levels` | `02` §2 sketch, `05` §6, `07` T07 and T22 |
| Test IDs | every ID defined in `06` is used in `07`; every ID used in `07` exists in `06` | yes |
| Glossary | lane / module / slot / hold / override / ChainSpec | used consistently; UI words "voice" and "effect" only in `05` |
| Punctuation style | no em or en dashes in docs or prototype text | searched, none |

## Handoff self-containment

| Question | Answer |
|---|---|
| Can a fresh session build without this conversation? | Yes: the prompt names every doc, the order, the rules, the gates and the end state. |
| Does it restate the frozen items? | Yes (grammar, catalog IDs/weights/tags, C1 to C9, golden values). |
| Does it carry the workflow changes asked for (recon from docs only, test-first with evidence, commit per task, milestone reviews, ask only when blocked, build report)? | Yes. |
| Same tools and skills, animotion mandatory, Sonnet-low subagent rule? | Yes. |

## Verified vs not verified

| Item | Status |
|---|---|
| XXH64 implementation used for the vectors | Checked against two known vectors (empty string, `abc`). |
| Worked examples in `04` §8 | Computed by a reference script written from the spec; a 3000-string fuzz of that script did not crash. Not yet reproduced by a second implementation (that is task T04). |
| Prototypes | Rendered and viewed at 1040 to 1100 px wide in headless Chrome: both files, default and Advanced states. Not checked at phone widths or with a screen reader. |
| Phosphor icon names | 9 of 16 effect icons seen rendered in the prototype; the rest must be verified at T24. |
| JUCE WebView API names, `Oversampling` integer-latency option, pluginval flags, RTSan platform support | From recon and memory, not from the pinned sources. The builder is told to verify against the pinned version. |
| DSP ranges and trims | Designed, not listened to. T20 calibrates. |
| Float determinism across compilers | Not promised (C7). |

## Open items for the owner

| # | Item | Needed by |
|---|---|---|
| 1 | Manufacturer name and plugin ID | T01 |
| 2 | Which DAWs you own, for the K1 gate and the smoke list | T07, T31 |
| 3 | The reference script that produced the vectors is in the session scratch folder and will be lost. Say so if you want it kept in the repo as a second oracle. | before T04 |
