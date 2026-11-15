# 04 Seed grammar v1

Normative. `GRAMMAR_VERSION = 1`. Everything here is frozen once v1 ships (`02-spec.md` C8). All arithmetic is unsigned integer, wrapping at the stated width, unless marked signed. `>>` is a logical shift. Effect IDs, weights and param tags come from `03-dsp-catalog.md` §2.

## 1. Pipeline

```
text ─ preprocess ─ split lines ─ per line: tokenize ─ classify ─┬─ entropy string E ─ XXH64 ─ lane seed S ─ generate modules
                                                                  └─ modifiers ─ bias params, set lane fields
all lanes ─ ChainSpec ─ canonical bytes ─ XXH64 ─ chain hash (golden tests)
```

## 2. Preprocess and lines

| Step | Rule |
|---|---|
| P1 | Input is the UTF-8 byte string of the text. No Unicode normalization, no validation. Invalid bytes are processed like any other byte. |
| P2 | Replace `CR LF` with `LF`, then every remaining `CR` with `LF`. |
| P3 | If longer than 4096 bytes: cut at 4096, then step back while the byte at the cut is a continuation byte (`b & 0xC0 == 0x80`). |
| P4 | Split on `LF`. A line is blank if every byte is whitespace (see T-WS). Drop blank lines. |
| P5 | The first 4 non-blank lines are lanes 0..3. Later non-blank lines are ignored (the UI marks them). |

## 3. Primitives

**XXH64**: reference algorithm, 64-bit, little-endian reads. Vendored `xxhash.h` (`XXH64(data, len, seed)`).

**SplitMix64** (state `s`, u64):
```
next():  s += 0x9E3779B97F4A7C15
         z = s
         z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
         z = (z ^ (z >> 27)) * 0x94D049BB133111EB
         return z ^ (z >> 31)
u16():   return next() >> 48
```

**Constants**

| Name | Value |
|---|---|
| `SALT` | `0x5645524241544D31` (ASCII "VERBATM1") |
| `COUNTS` | `[2,3,3,4,4,4,5,5,6]` |
| `DIVS` | `[1,2,3,4,6,8,12,16,24,32]` |

**bias(u, e)**: `u` is u16 held in ≥32-bit, `e` signed −3..3. Integer division truncates.
```
if e > 0:  v0 = v = 65535 - u;  repeat e times: v = (v * v0) / 65535;   return 65535 - v
if e < 0:  u0 = u;              repeat |e| times: u = (u * u0) / 65535; return u
if e == 0: return u
```

**Primitive test vectors**

| Input | Output |
|---|---|
| `XXH64("", 0)` | `ef46db3751d8e999` |
| `XXH64("abc", 0)` | `44bc2cf5ad770999` |
| `XXH64(bytes 0x00..0x64 (101 bytes), 0)` | `e99038495f85381e` |
| `XXH64(bytes 0x00..0x64, SALT)` | `442bd2adcf827d01` |
| SplitMix64 state 0, first three `next()` | `e220a8397b1dcdaf`, `6e789e6aa1b965f4`, `06c45d188009454f` |
| `bias(20000, +2)` / `bias(20000, −2)` | `43553` / `1862` |
| `bias(65535, +3)` / `bias(0, −3)` | `65535` / `0` |

## 4. Tokenizer (byte level, per line)

Scan left to right at position `i`. First matching rule wins.

| Order | Token | Match | Consumes |
|---|---|---|---|
| 1 | ELLIPSIS | bytes `E2 80 A6` (U+2026) | 3 bytes |
| 2 | ELLIPSIS or STOP | a run of `r` bytes `.`: if `r ≥ 3` one ELLIPSIS; else `r` STOP tokens | the whole run |
| 3 | NUM or PCT | a run of ASCII digits; value = decimal, saturating at 9999 (runs longer than 6 digits = 9999). If the next byte is `%`: PCT and consume it; else NUM | run (+1) |
| 4 | T-WS (whitespace) | byte ≤ 0x20 or 0x7F (space, tab, every control byte) | 1 byte, emits nothing |
| 5 | WORD | a run of word bytes: `A-Z`, `a-z`, `'` (0x27), or any byte ≥ 0x80 that is not the start of `E2 80 A6` | the run |
| 6 | OP | one of `! ? ~ , - + > / : & "` | 1 byte |
| 7 | SYM | any other byte (all remaining ASCII punctuation, for example `@ # $ % ^ * ( ) [ ] { } < = _ ;` and the backslash, pipe and backtick) | 1 byte |

Notes: a `%` not directly after digits is SYM. Emoji, accented letters, Cyrillic, Arabic, CJK are word bytes and need no special handling (RTL text is hashed in logical byte order).

## 5. Classification

| Token | Effect |
|---|---|
| WORD | `low` = the word with ASCII `A-Z` mapped to `a-z` (other bytes untouched). If `low` is in the direction table: apply that direction. Otherwise append `low` to the entropy list. |
| WORD, CAPS | Independently of the above: if the word has ≥ 2 ASCII letters and none is lowercase, `shout += 1`. (So `DARK` is a direction and a shout.) |
| SYM | Append the single byte to the entropy list. |
| OP, STOP, ELLIPSIS | Count per kind. |
| NUM | First NUM with value 1..64: `timeDiv` = the entry of `DIVS` nearest to it (ties go to the smaller entry). NUM outside 1..64 and later NUMs are ignored. No NUM: `timeDiv = 0`. |
| PCT | First PCT: `mixAbs = min(value, 100)`. Later PCTs ignored. No PCT: `mixAbs = 255`. |

**Entropy string** `E` = entropy list joined with one byte `0x20`. May be empty.

### Direction words (14)

| Axis | −1 | +1 | Acts on |
|---|---|---|---|
| TONE | `dark` | `bright` | params tagged TONE |
| HEAT | `cold` | `warm` | params tagged HEAT |
| RATE | `slow` | `fast` | params tagged RATE, LFO A and B rate |
| SPACE | `narrow` | `wide` | params tagged SPACE, lane `space` |
| TAIL | `short` | `long` | params tagged TAIL |
| DEPTH | `thin` | `deep` | every module `mix` |
| LEVEL | `soft` | `loud` | lane `level` |

Each axis value = sum of its words, then (TAIL only) `+ count(ELLIPSIS) − count(STOP)`, then clamped to −3..3.

### Punctuation ops (13)

| Mark | Name | Lane field | Function (DSP in `03-dsp-catalog.md` §1) |
|---|---|---|---|
| `!` | Heat | `drive = min(3, n)` | Input drive stage, +4 dB per step into a soft clipper. |
| `?` | Wander | `wander = min(3, n)` | Slow random LFO B depth. |
| `~` | Wobble | `wobble = min(3, n)` | Adds depth to LFO A. |
| `.` | Stop | TAIL axis −1 each | Shorter tails (feedback, decay). |
| `...` or `…` | Sustain | TAIL axis +1 each | Longer tails. |
| `,` | Duck | `duck = min(3, n)` | Lane ducks under the input, 3 dB per step. |
| `-` | Dry | `mixSteps −1` each | Lane mix 15 % drier per step. |
| `+` | Wet | `mixSteps +1` each | Lane mix 15 % wetter per step. |
| `>` | Feedback | `feedback = min(3, n)` | Lane output fed back to its input. |
| `/` | Split | `split = 1` if n ≥ 1 | First half of the lane's modules runs parallel to the second half. |
| `:` | Follow | `follow = min(3, n)` | Envelope follower modulates a module. |
| `&` | Chain | `chain = 1` if n ≥ 1 | This lane takes the previous lane's output as input (serial instead of parallel). No effect on lane 0. |
| `"` | Band | `band = 1` if n ≥ 1 | Lane processes only a frequency band. |

`mixSteps = clamp(count(+) − count(-), −3, 3)`. `emphasis = min(3, shout)`. `level` and `space` = their axis values.

Other semantics: digits → time base (`timeDiv`: delays, tremolo and LFO A sync to a 1/`timeDiv` note) and lane mix (`NN%`). CAPS → `emphasis`, biases every module `mix` upward. Whitespace → separator only; amount and position never matter. Emoji and non-ASCII → entropy only.

## 6. Generation (per lane)

```
S  = XXH64(E, SALT)
R0 = SplitMix64(S)                                  # structure stream
d[0..8] = nine R0.u16() draws, in order
nModules   = COUNTS[(d[0] * 9) >> 16]
lfoA.shape = (d[1] * 4) >> 16
lfoA.rate  = bias(d[2], RATE)
lfoA.depth = min(65535, (d[3] < 32768 ? 0 : d[3] - 32768) + 13107 * wobble)
lfoA.target= (d[4] * 6) >> 16
lfoB.rate  = bias(d[5], RATE)
lfoB.depth = min(65535, 13107 * wander)
lfoB.target= (d[6] * 6) >> 16
env.depth  = min(65535, 16384 * follow)
env.target = (d[7] * 6) >> 16
bandCenter = d[8]

pool = [0..15]                                       # effect IDs ascending
for k in 0..5:                                       # always all six slots
    Rk  = SplitMix64( XXH64( 8 bytes of S little-endian, seed = k + 1 ) )
    tot = sum of weight[t] for t in pool
    x   = ((Rk.next() >> 32) * tot) >> 32
    walk pool ascending: first t with x < weight[t] is the type, else x -= weight[t]
    remove t from pool
    p[0..5] = six Rk.u16() draws;  mix = one more Rk.u16()
    for q in 0..5: if param q of effect t has tag (axis, polarity): p[q] = bias(p[q], axis value * polarity)
    mix = bias(mix, clamp(DEPTH + emphasis, -3, 3))
    slot[k] = { t, p, mix }
modules = slot[0 .. nModules-1]
```

All 7 draws are taken for every slot even when the effect uses fewer params (unused values are stored and hashed; they are reserved). Discrete params are never tagged. Same text on two lines gives the same lane twice (lane index is not hashed).

## 7. Canonical serialization and chain hash

Little-endian, packed, no padding. `chainHash = XXH64(bytes, 0)`.

| Part | Fields in order (type) |
|---|---|
| Header | `"VBTM"` (4 bytes), `grammarVersion` (u16), `laneCount` (u8) |
| Per lane | `seed` (u64), `nModules` (u8), `drive` (u8), `duck` (u8), `feedback` (u8), `split` (u8), `chain` (u8), `band` (u8), `bandCenter` (u16), `mixSteps` (i8), `mixAbs` (u8), `timeDiv` (u8), `level` (i8), `space` (i8), `emphasis` (u8) |
| | `lfoA.shape` (u8), `lfoA.rate` (u16), `lfoA.depth` (u16), `lfoA.target` (u8), `lfoB.rate` (u16), `lfoB.depth` (u16), `lfoB.target` (u8), `env.depth` (u16), `env.target` (u8) |
| Per module | `type` (u8), `p[0..5]` (6 × u16), `mix` (u16) |

The chain hash covers the generated ChainSpec only (no overrides). `wander`, `wobble` and `follow` are represented by the depths they produce.

## 8. Worked examples (expected outputs)

Produced by a reference script written from this document. `02-spec.md` K5 applies if an implementation disagrees. Axes are listed as `[TONE, HEAT, RATE, SPACE, TAIL, DEPTH]`. Fields not shown are 0 (`mixAbs` 255, `timeDiv` 0).

| # | Text | Lanes | `E` | Seed `S` | n | Types | Modifiers | chainHash |
|---|---|---|---|---|---|---|---|---|
| 1 | `hello` | 1 | `hello` | `4c66eed20a244bd7` | 4 | SAT, FREEZE, COMP, COMB | none | `692ac662981b2247` |
| 2 | `dark tape, slow echo!` | 1 | `tape echo` | `1e17d464a144ec88` | 4 | VERB, TREM, CRUSH, WOW | axes `[-1,0,-1,0,0,0]`, drive 1, duck 1 | `efd86bb250fc9113` |
| 3 | `tape echo` | 1 | `tape echo` | `1e17d464a144ec88` | 4 | VERB, TREM, CRUSH, WOW | none | `f3a6d5f547a566b2` |
| 4 | `TAPE echo!!` | 1 | `tape echo` | `1e17d464a144ec88` | 4 | VERB, TREM, CRUSH, WOW | emphasis 1, drive 2 | `8638cfcd4f292db2` |
| 5 | `dark!!` | 1 | (empty) | `912f19095a36327b` | 4 | SAT, GRAIN, FLANGE, CRUSH | axes `[-1,0,0,0,0,0]`, drive 2 | `e0d87e51fb3af63e` |
| 6 | `echo 8 50%...` | 1 | `echo` | `6cf1255201f4b196` | 4 | SAT, WOW, CHORUS, FILTER | TAIL +1, mixAbs 50, timeDiv 8 | `5db80c69b133ee0d` |
| 7 | `lo-fi "radio" > 7` | 1 | `lo fi radio` | `e6e79ff38a103f96` | 3 | VERB, CHORUS, FREEZE | feedback 1, band 1, mixSteps −1, timeDiv 6 | `10150f4a0ea65015` |
| 8 | `привет мир ✉` | 1 | same bytes | `2863b5217305d75c` | 4 | CHORUS, DELAY, RING, COMP | none | `b70518222f8d15f0` |
| 9 | `warm tape` LF LF `& long echo… ~?` | 2 | `tape` / `echo` | `1fa9551cf86ece43` / `6cf1255201f4b196` | 5 / 4 | DELAY, SAT, CHORUS, TREM, VERB / SAT, WOW, CHORUS, FILTER | lane 0: HEAT +1. lane 1: chain 1, TAIL +2, lfoA.depth 13107, lfoB.depth 13107 | `99da9f7a8acef726` |
| 10 | three spaces | 0 | | | | | | `385f55d8eac1f68b` |
| 11 | `a` LF `b` LF `c` LF `d` LF `e` | 4 | `a`,`b`,`c`,`d` | `57f8d3259af88044`, `defb7f93d4f9a822`, `f66571c2d7356d5d`, `a795761d53a18b14` | 3,4,3,6 | GRAIN, WOW, FLANGE / FLANGE, SAT, COMB, DELAY / PHASER, FILTER, COMP / PHASER, GRAIN, DELAY, FILTER, FLANGE, COMP | line `e` ignored | `e9dea3d9bbbe0be1` |
| 12 | `DARK Dark dark bright` | 1 | (empty) | `912f19095a36327b` | 4 | SAT, GRAIN, FLANGE, CRUSH | TONE −2, emphasis 1 | `da070e833b9a7952` |
| 13 | `wait.. what...` | 1 | `wait what` | `15d34abfabdde237` | 6 | FILTER, CHORUS, PHASER, COMB, SAT, DELAY | TAIL −1 (two stops, one sustain) | `e9dae3f35d0d3d2a` |
| 14 | `don't stop` | 1 | `don't stop` | `895c2f22a5d3a04c` | 5 | TREM, FLANGE, FILTER, PHASER, SAT | none | `d38cd535f7756da2` |

What the examples prove: 2, 3, 4 share a seed and types (modifiers do not reroll). 5 and 12 share the empty-entropy seed. 6 and lane 1 of 9 share the `echo` seed. 7 shows `-` inside a word acting as Dry and `7` snapping to 6. 11 shows the 4-lane limit.

### Full dump, example 1 (`hello`)

`lfoA` shape 1, rate 33025, depth 8994, target 3. `lfoB` rate 36481, depth 0, target 0. `env` depth 0, target 2. `bandCenter` 30407.

| Slot | Type | p0 | p1 | p2 | p3 | p4 | p5 | mix |
|---|---|---|---|---|---|---|---|---|
| 0 | SAT | 23281 | 45101 | 38859 | 22320 | 2547 | 14925 | 41917 |
| 1 | FREEZE | 1936 | 7938 | 48863 | 3845 | 55571 | 58815 | 33379 |
| 2 | COMP | 57674 | 51154 | 22431 | 1115 | 46750 | 27956 | 14077 |
| 3 | COMB | 6507 | 63294 | 29733 | 55027 | 4994 | 13608 | 13433 |

### Full dump, example 2 (`dark tape, slow echo!`)

`lfoA` shape 2, rate 18883, depth 0, target 2. `lfoB` rate 15170, depth 0, target 0. `env` depth 0, target 3. `bandCenter` 7895. (Example 3, same seed without modifiers: `lfoA.rate` 35179, `lfoB.rate` 31531.)

| Slot | Type | p0 | p1 | p2 | p3 | p4 | p5 | mix |
|---|---|---|---|---|---|---|---|---|
| 0 | VERB | 12533 | 8166 | 58372 | 64386 | 49130 | 57258 | 3649 |
| 1 | TREM | 20367 | 58877 | 30781 | 11618 | 51539 | 62537 | 9682 |
| 2 | CRUSH | 57110 | 49680 | 23 | 11473 | 11974 | 24423 | 27908 |
| 3 | WOW | 6033 | 20538 | 55541 | 1933 | 45028 | 31865 | 47542 |

## 9. Token classes for the UI

`core` returns, per line, a list of `(byteStart, byteLength, class)` with class ∈ `word`, `direction`, `op`, `number`, `ignored` (line beyond the 4th). The UI colours text from this list only.
