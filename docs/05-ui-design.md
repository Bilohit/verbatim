# 05 UI design

Direction chosen by the owner: **typewriter**. Reference prototype: `proto/paper.html` (typewriter variant; the fax variant and `proto/directions.html` are rejected explorations, kept for context). The prototype proves look and feel only; its JavaScript grammar is fake. Real tokens and chains come from `core` over the bridge.

Principles from the owner: icon-led, minimal text, no on-screen instructions, simple and elegant, built around what a music producer needs from first load to finished sound.

## 1. The producer's path

| Moment | What they want | What the UI gives | Access |
|---|---|---|---|
| Loads Verbatim on a track | Know it is alive and not breaking their mix | Blank paper, blinking caret already focused, audio passes through untouched | 0 clicks |
| First sound | Hear something fast, without reading | Type any word: keys appear, sound changes on each keystroke. Or press Roll (dice): a random phrase types itself | type, or 1 click |
| Understand it | See what the text made | One row of keys per line of text, one key per effect, icon only. Hover or focus a key: its name | glance / hover |
| Explore | Try many sounds quickly | Edit words (reroll). Roll again. Undo with Ctrl+Z in the text box | type |
| Shape | Push the sound a direction | Direction words and punctuation change the sound without rerolling. Punctuation prints in red, direction words underlined, so the user sees which characters are controls | type |
| Keep the good part | Protect an effect they like while still exploring | Click a key: it latches down with a red ring (held). Held effects survive rerolls | 1 click |
| Fit it in the mix | Balance and level | Mix, Macro, Output on the carriage scale. Output meter always visible | 1 drag |
| Fine-tune | Exact settings, add or remove an effect | Advanced: drawer with labelled sliders for the selected effect, remove, add | 1 click + drag |
| Perform / automate | Move the whole sound over time | Automate Macro (one gesture moves every effect), Mix, Output from the DAW | DAW lane |
| Come back later | Same sound as before | Project recall restores text, holds, edits. The text alone is a shareable preset (copy and paste) | none |

What is deliberately absent: preset browser, per-effect bypass, tabs, a manual, tooltips that explain grammar. Names appear only on hover/focus and in Advanced.

## 2. Layout

Default window 900 × 620. Resizable 720 × 520 to 1600 × 1000, free aspect. Size is saved in state.

```
┌────────────────────────────────────────────────────────────────────┐
│ Verbatim                               ▮▮ out meter   [dice] [adv] │  top bar
│   ┌────────────────────────────────────────────────────────┐       │
│   │ dark tape, slow echo!                          (seed)   │       │  paper: text is the hero
│   │ the second line is its own voice...           postmark  │       │  grows with lines, scrolls after 8
│   └────────────────────────────────────────────────────────┘       │
│ ════════════════════ platen ═══════════════════════════════════    │
│ (mix) ├┼┼┼┼▌┼┼┤   (macro) ├┼┼▌┼┼┼┼┤   (output) ├┼┼┼┼▌┼┼┤           │  carriage scale: 3 sliders, icon labels
│  (o) (o) (o) (o) (o)                                                │  key row, lane 0
│    (o) (o) (o) (o)                                                  │  key row, lane 1 (staggered like a keyboard)
│ ┌ Advanced drawer (only when open and a key is selected) ────────┐ │
│ │ Delay   time ───▌── 240 ms   feedback ──▌─── 0.41  …  [hold][bin]│ │
│ └────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────┘
```

| Region | Content | Notes |
|---|---|---|
| Top bar | Wordmark, output meter (2 bars), Roll, Advanced toggle | The only persistent text is the wordmark. |
| Paper | Text editor, postmark | Font size `clamp(1.2rem, 2.7vw, 1.7rem)`. Max 8 visible lines then the paper scrolls. |
| Postmark | Round red stamp with the first 8 hex digits of a hash of all lane seeds | Changes when any lane rerolls. Not interactive. Hidden when text is empty. |
| Carriage scale | Mix, Macro, Output | Ruler-tick track, red marker. Double-click resets to default. Bound to host params. |
| Key rows | One row per lane, staggered 26 px per row. Round keys 50 px, icon 22 px | Rows wrap; a lane has at most 8 keys. |
| Drawer | Selected effect's name, param sliders with value and unit, mix, hold toggle, remove | Advanced only. |

## 3. Components and states

| Component | States |
|---|---|
| Key (effect) | default; hover (rim brightens, name tooltip after 500 ms); focus-visible (2 px ring, instant); pressed (down 4 px); held (stays down, red rim, `aria-pressed=true`); selected in Advanced (ring in ink colour); entering (drops in); leaving (fades up) |
| Ghost key "+" | Advanced only, end of each row; opens the add menu; disabled at 8 modules |
| Add menu | Popover grid of the 16 effect icons with names; SAT disabled when the lane has 2; Esc closes |
| Slider | default; hover; focus-visible; dragging (value shown above the marker); double-click = default |
| Icon button (Roll, Advanced) | default; hover; focus-visible; active; toggled (Advanced) |
| Text | plain word (ink); direction word (underlined); op (red, bold); number (underlined); ignored line (struck through, faint) |
| Output meter | Level bars; a red tick holds 1 s when SafetyClip is active |

### Interaction rules

| Action | Result |
|---|---|
| Click key, Advanced off | Toggle hold. |
| Click key, Advanced on | Select it and open the drawer. Hold is toggled in the drawer. Editing any slider auto-holds (`02-spec.md` R2). |
| Remove (bin) in drawer | Removes the module (`02-spec.md` R3). Focus moves to the row's "+" key. |
| Roll | Replaces the text with 2 to 5 random words from a bundled 200-word list, typed at 25 ms per character. Text undo restores the previous text. |
| Keyboard | Tab order: text, Roll, Advanced, Mix, Macro, Output, keys by row, drawer. Keys are buttons: Space/Enter activates. Esc closes the drawer or add menu. Arrow keys move sliders. |
| Text entry | Every key press goes to the text box while it is focused, including space (risk K1 in `02-spec.md`). |

### App states

| ID | State | Display |
|---|---|---|
| S0 | Empty | Blank paper, caret, no keys, no postmark. Audio passthrough. |
| S1 | Typing | Text, keys, postmark. Ink of each line darkens with that lane's level. |
| S2 | Held | Held keys latched with red rim. |
| S3 | Advanced | Bars under keys become visible, "+" keys, drawer for the selected key. |
| E1 | Too many lines | Lines after the 4th struck through and faint. Screen-reader status: "Only the first four lines make sound." No visible sentence. |
| E2 | Text full (4096 bytes) | Further input ignored; postmark shakes once (reduced motion: postmark turns ink-black for 1 s). Status announced. |
| E3 | State not loadable | Paper shows the stored text read-only if available; keys area shows one line: "Saved with a newer Verbatim. Update to edit." Audio passthrough. |
| E4 | WebView2 missing | Native JUCE component: "Verbatim needs the Microsoft WebView2 runtime." plus a button that opens the download page. Audio still processes the saved state. |

## 4. Visual system (tokens)

Values taken from `proto/paper.html`. Define as CSS custom properties in `gui/src/styles/tokens.css`; components use tokens only.

| Token | Value | Use |
|---|---|---|
| `--body` | `oklch(76% 0.05 168)` | Mint enamel machine body |
| `--body-ink` | `oklch(24% 0.03 168)` | Text and icons on the body |
| `--hover` | `oklch(70% 0.05 168)` | Hover fill on the body |
| `--paper` | `oklch(97.5% 0.008 95)` | Paper |
| `--ink` | `oklch(22% 0.012 265)` | Typed text at full level |
| `--ink-faint` | `oklch(52% 0.012 265)` | Typed text at rest (4.6:1 on paper) |
| `--ribbon` | `oklch(50% 0.2 27)` | Punctuation, postmark, held rim, slider marker, caret |
| `--key` / `--key-ink` / `--key-rim` / `--key-drop` | `oklch(27% 0.02 168)` / `oklch(95% 0.02 95)` / `oklch(90% 0.012 168)` / `oklch(45% 0.05 168)` | Keys |
| `--platen` | `oklch(22% 0.01 168)` | Roller bar under the paper |
| `--focus` | `oklch(24% 0.03 168)` | 2 px focus ring, 3 px offset |
| Radius | body 18 px, keys and icon buttons round, paper and drawer 0 | One system |
| Spacing | 4 px scale: 4, 8, 12, 16, 24, 32 | |

| Role | Font | Use |
|---|---|---|
| Text | Courier Prime 400 / 700 | The seed text only |
| UI | Archivo 400..700 | Wordmark, drawer labels, menus |
| Data | IBM Plex Mono 400 | Postmark, slider values |

Icons: Phosphor, regular weight, one family. Verify each name exists in the installed package at build.

| Effect | Icon | Effect | Icon |
|---|---|---|---|
| SAT | `fire` | WOW | `vinyl-record` |
| CRUSH | `stairs` | FREEZE | `snowflake` |
| FILTER | `funnel` | COMB | `hash` |
| DELAY | `timer` | RING | `bell-simple` |
| VERB | `waves` | PITCH | `arrows-vertical` |
| CHORUS | `users-three` | GRAIN | `dots-nine` |
| PHASER | `spiral` | COMP | `arrows-in-line-vertical` |
| TREM | `wave-sine` | FLANGE | `airplane-tilt` |

Controls: Mix `drop-half`, Macro `magic-wand`, Output `speaker-high`, Roll `dice-five`, Advanced `sliders-horizontal`, add `plus`, remove `trash`, hold `lock-simple` / `lock-simple-open`.

## 5. Motion (animotion: framer-motion)

All motion lives in `gui/src/motion/` (shared variants, easings, the reduced-motion hook) and in the components listed. Library: `framer-motion`, as the animotion skill specifies. Simple hover and focus use CSS transitions. Only `transform` and `opacity` animate. Continuous values use motion values, never React state.

| # | Motion | Purpose | Where | Spec |
|---|---|---|---|---|
| M1 | Keys drop in / lift out on reroll | Shows which effects changed | `components/KeyRow.tsx` (`AnimatePresence`) | in: y −8 → 0, opacity 0 → 1, 240 ms, ease `[0.16, 1, 0.3, 1]`, stagger 40 ms; out: opacity → 0, 120 ms. Unchanged keys do not move. |
| M2 | Key latch | Feedback for hold | `components/Key.tsx` | `whileTap` y 4; held `animate` y 4; 90 ms |
| M3 | Living ink | The signal-flow language: a line's ink darkens with its lane level | `components/InkLayer.tsx` | A second copy of the text in full ink over the faint copy; per-line opacity = motion value fed by `levels` events (30 Hz), spring stiffness 180, damping 24. Lines ≤ 200 characters add a per-character phase offset; longer lines animate per line. |
| M4 | Typebar strike | Per-character feedback while typing | `components/InkLayer.tsx` | The newly typed character: opacity 1 → resting, scale 1.12 → 1, 90 ms |
| M5 | Line feed | The one authored moment: Enter moves the paper up one line like a carriage return | `components/Paper.tsx` | y +1 line → 0, 160 ms |
| M6 | Postmark stamp | Seed changed | `components/Postmark.tsx` | scale 1.25 → 1, opacity 0 → 1, 150 ms |
| M7 | Drawer | Advanced open/close | `components/Drawer.tsx` | opacity and y 12 → 0, 220 ms |
| M8 | Roll typing | Shows the text being written | `components/RollButton.tsx` | 25 ms per character |

Reduced motion (`useReducedMotion`): M1, M6, M7 become 120 ms opacity fades; M2 instant; M3 updates opacity at 5 Hz with no spring and no per-character phase; M4, M5 off; M8 instant.

## 6. Bridge (UI ↔ plugin)

| Direction | Name | Payload |
|---|---|---|
| UI → C++ | `setText(text)` | returns `{ tokens, chain, seedHash, ignoredLines, full }` |
| UI → C++ | `setOverride({ lane, op: hold / release / edit / remove / add, slot, module })` | returns the resolved `chain` |
| UI → C++ | `getState()` | text, chain, overrides, ui size, advanced flag |
| UI → C++ | `setUi({ w, h, advanced })` | |
| C++ → UI | `levels` | `{ lanes: [rms × 4], out: [peakL, peakR], clip: bool }` at 30 Hz, only while the editor is visible |
| C++ → UI | `chain` | Sent when state is loaded by the host |
| Both | `mix`, `macro`, `output` | JUCE WebView slider relays / parameter attachments, with gesture begin and end |

`chain` gives, per lane, per module: type ID, held flag, params as mapped values with units (the UI never maps ranges itself).

## 7. Accessibility and resize

| Item | Rule |
|---|---|
| Contrast | Text ≥ 4.5:1, icons and control edges ≥ 3:1 (token table meets this). |
| Not colour alone | Held = position + rim + `aria-pressed`. Ops = colour + bold. Ignored lines = strike-through. |
| Names | Every icon control has an `aria-label` (effect name, "Mix", "Roll", ...). Key rows are lists labelled "Voice 1".. |
| Status | One polite live region: voice and effect counts, E1, E2. |
| Focus | Visible ring, never animated. Focus is restored to a sensible control after remove/add. |
| Resize | CSS container queries on the machine width: < 800 px drops the row stagger and stacks the scale sliders in two rows. Text size follows the clamp. No horizontal scroll at 720 px. |
| HiDPI | Vector icons and CSS only; verify at 100, 150, 200 % Windows scaling. |

## 8. Design QA before UI tasks close

Run the uiux-pro-max checklist and the impeccable craft floor on the built UI at 720, 900 and 1600 px wide. Themed browser surfaces: selection colour, caret colour, scrollbar on the paper, tabular numerals for values.
