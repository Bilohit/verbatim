# DAW smoke checklist (V2, V4)

Status: K1 and V2 are deferred by the owner (2026-11-16) until after "considerable development".
Until then, pluginval with GUI tests (`pluginval-s5-gui-seed1.log`) and the automated suites stand in.

Install: copy `build/Verbatim_artefacts/Release/VST3/Verbatim.vst3` to `C:\Program Files\Common Files\VST3\`.

## Per host (REAPER, Ableton Live 12, FL Studio, Bitwig Studio, Studio One, Cubase)

| # | Step | Pass | REAPER | Live | FL | Bitwig | S1 | Cubase |
|---|---|---|---|---|---|---|---|---|
| 1 | Scan and load Verbatim on an audio track | Loads, editor opens with caret in the text box | | | | | | |
| 2 | **K1:** click the text box, type `dark tape, slow echo!` including spaces | Every key (space too) reaches the text box; host transport does not start | | | | | | |
| 3 | Play audio while typing | Sound changes per keystroke, no clicks | | | | | | |
| 4 | Save, close, reopen the project | Same text, same sound | | | | | | |
| 5 | Automate Mix and Macro | Automation plays back | | | | | | |
| 6 | Resize the editor | 720x520 to 1600x1000 | | | | | | |
| 7 | Two instances with editors open | Both work | | | | | | |
| 8 | Host bypass | Smooth, latency compensated | | | | | | |
| 9 | Offline bounce twice, null the bounces | Difference below -100 dBFS | | | | | | |

## V4 (REAPER)

| Step | Pass | Result |
|---|---|---|
| Change sample rate 44.1k to 96k and back while loaded | No crash, latency correct, sound recovers | |
| Change buffer 64 to 2048 and back | Same | |
