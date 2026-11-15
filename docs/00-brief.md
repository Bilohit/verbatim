# 00 Brief: Verbatim

Working name was "Postage VST". Product name: **Verbatim** ("word for word": same text, same sound).

## One line
A VST3 audio effect where the user types text and the text deterministically generates an effects chain. Same text gives the same chain, bit for bit, forever (per grammar version).

## Owner's decisions (interview, 2026-11-16)

| # | Topic | Decision |
|---|---|---|
| Q1 | Formats / OS | VST3 only, Windows only. VST2 dropped (no Steinberg licence). CLAP, AU, macOS, Linux out of v1. |
| Q2 | Framework / UI | JUCE 8, UI in WebView (WebView2) with React + framer-motion. WebView cost accepted. |
| Q3 | Edit stability | Avalanche: changing an ordinary word rerolls that line's whole chain. |
| Q4 | Mapping depth | Hash plus semantic directions: direction words, punctuation, digits and CAPS are stable modifiers that do NOT reroll. Only ordinary words reroll. |
| Q5 | Chain shape | Hash sets module count and types, independent of text length. Each line of text is an independent parallel voice (lane). |
| Q6 | Tweak / lock | Hold per effect, global Mix / Macro / Output, and an Advanced view where effects can be adjusted, added and removed. |
| Q7 | v1 scope | 16 effects. |
| Q8 | Visual | Typewriter machine (see `05-ui-design.md`, `proto/paper.html`). Icon-led, minimal text, no on-screen instructions. Designed around a music producer's path from first load to finished sound. |
| Q9 | Licence | Open source, AGPLv3, free. |
| Q10 | Name | Verbatim. |

## Concept (owner's intent)
- Main page is a text box. Sentences, lines, words or single letters form the seed.
- Text generates a unique chain with unique parameters: delays, echoes, saturation, filters, LFOs, modulation.
- Punctuation marks have specific, unique functions.
- Inspirations (traits only, see `01-recon.md`): Output Thermal, Imagiro Piano, Transit, Lifeline Console.

## v1 scope

| In | Out (cut line) |
|---|---|
| VST3, Windows 10/11 x64 | VST2, AU, CLAP, AAX, macOS, Linux, standalone |
| 16 effects, max 4 lanes, max 8 modules per lane | MIDI input, sidechain input |
| Grammar v1: words, 14 direction words, 13 punctuation ops, digits, CAPS | Per-module host automation, module reorder, lane mute/solo |
| Hold, Advanced edit / add / remove | Preset browser, cloud sharing, copy protection |
| Host params: Mix, Macro, Output, Bypass | Module state carried across chain rerolls (tails reset, crossfaded) |
| Typewriter UI, resizable, keyboard and screen-reader usable | Light/dark variants, localisation |

## Glossary (use these names everywhere)

| Term | Meaning |
|---|---|
| Seed text | The full text in the box. |
| Lane | One non-blank line of text and the chain it generates. UI word: "voice". Max 4. |
| Module | One effect instance in a lane. UI word: "effect". |
| Slot | Position of a generated module in a lane, 0..5. |
| Entropy string `E` | The ordinary words of a line, the only input to the lane hash. |
| Modifier | Direction word, punctuation op, number or CAPS. Changes the sound without rerolling. |
| ChainSpec | The all-integer description of the generated chain. The thing that is deterministic bit for bit. |
| Override | Hold, edit, add or remove applied on top of the generated ChainSpec. |
| Hold | Freeze a module so text changes do not replace it. |
| Grammar version | Integer tag. v1 mappings never change after release. |
