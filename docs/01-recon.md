# 01 Recon (2026-11-16)

Facts only. Folder held only `FABLE_PROMPT.md`; no prior code or docs.

## Inspirations

| Product | Facts | Trait to extract |
|---|---|---|
| Output Thermal | Main page = XY pad + 2 macros + dry/wet; detail on separate Advanced page. 3 serial stages, each band-limitable (30 Hz-22 kHz), each with a distortion slot + a second FX slot; master comp/filter. Macros drag onto knobs to set depth. [1][2][3] | One hero control up front, depth behind a second page. Per-stage frequency band. |
| Imagiro Piano | Lofi piano; pianos recorded in real rooms "to capture imperfections"; controls glide/tone/loop/reverse aimed at "sounds that couldn't exist"; instant load. VST3/AU/AAX. [4] | Few named controls, character over precision, imperfection as feature. |
| Transit / Transit 2 (Baby Audio x Andrew Huang) | One macro knob drives params across up to 7 FX; v2: 28 modules, 6 motion modes (one active at a time), light/dark UI, icons and colour over text. Criticised: no output meter, tiny params. [5][6][7] | One gesture moves the whole chain. Always show output level. Keep params legible. |
| Lifeline Console (Excite Audio) | 5 drag-reorder modules (Pre-amp, EQ, Comp, Modulation, Wear); resizable, scrollable; every module display reacts to live audio. [8][9] | Chain drawn as modules whose visuals are driven by the signal. |

## Frameworks

| Option | Facts |
|---|---|
| JUCE 8 | Dual AGPLv3 / commercial. Starter free to $20k/yr revenue, Indie $800 perpetual to $300k, Pro $3500 (forum-sourced, verify EULA). VST3/AU/AAX native. [10] |
| JUCE WebView (WebBrowserComponent) | Reported: UI freeze with multiple instances if JS evaluated before first page load; WebView2 freeze across two DAWs on Windows; scaling bug in Ableton Live (8.0.13); intermittent WebKit crash in Live 12 on macOS (8.0.12); space bar swallowed (keyboard focus); weak on Linux. Anecdotal, not benchmarked. [11][12][13][14] |
| CLAP | JUCE has no native CLAP in 8; `clap-juce-extensions` (unofficial, JUCE 6-8) broke on 8.0.11 (fix PR pending when found). JUCE 9 preview announces CLAP work; no release date found. [15][16] |
| iPlug2 | Permissive licence; CLAP, VST3, AUv2/3, AAX, WAM; native-webview UI option. No DAW-compat data found. [17] |

## Determinism and testing

| Item | Facts |
|---|---|
| XXH64 | Spec guarantees identical output for any CPU/OS; input read little-endian. [18] |
| PRNG | splitmix64 and PCG32 are pure integer ops (no platform dependence by construction). Not confirmed by search; to be proven by golden test. |
| Float determinism | Not found in search. Assumed risk: `-ffast-math`, FMA contraction and libm `sin/exp/tanh` differ per compiler. Addressed in `02-spec.md`. |
| pluginval | Strictness 1-10; 5 = minimum for host compat, 10 = max; random seed changes per run unless fixed; `--skip-gui-tests` for headless CI. [19] |
| RealtimeSanitizer | Clang `-fsanitize=realtime` + `[[clang::nonblocking]]` flags alloc/locks at run time; negligible slowdown. [20] |

## Sources
1. https://www.musicradar.com/reviews/output-thermal
2. https://tapeop.com/reviews/gear/144/thermal-distortion-plug-in
3. https://www.attackmagazine.com/news/outputs-thermal-promises-interactive-distortion/
4. https://imagiro-website.pages.dev/product/piano
5. https://musictech.com/reviews/plug-ins/baby-audio-transit-review/
6. https://rekkerd.org/review-transit-2-motion-effects-playground-by-baby-audio/
7. https://www.recordproduction.com/reviews/baby-audio-transit
8. https://musicradar.com/reviews/excite-audio-lifeline-console
9. https://www.pluginboutique.com/products/9066
10. https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265
11. https://forum.juce.com/t/br-webview-freezes-daw/64917
12. https://forum.juce.com/t/crash-when-using-webview2-plugins-in-multiple-daws-in-parallel-on-windows/64853
13. https://forum.juce.com/t/webkit-processthrottler-crash-in-juce-8-webview-plugin-macos/68055
14. https://forum.juce.com/t/fixing-webview-keyboard-focus-issue/63987
15. https://forum.juce.com/t/juce-8-0-11-is-out/67725
16. https://forum.juce.com/t/the-juce-9-preview-branch-is-available-now/68864
17. https://iplug2.github.io
18. https://www.ietf.org/archive/id/draft-josefsson-xxhash-00.html
19. https://github.com/Tracktion/pluginval/blob/master/README.md
20. https://clang.llvm.org/docs/RealtimeSanitizer.html
