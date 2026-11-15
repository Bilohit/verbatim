#pragma once
#include <array>
#include <cstdint>

#include "Parse.h"

namespace vb
{
// Effect IDs, weights and param tags: docs/03-dsp-catalog.md section 2. Frozen with grammar v1.
enum EffectId : std::uint8_t
{
    SAT, CRUSH, FILTER, DELAY, VERB, CHORUS, PHASER, TREM,
    WOW, FREEZE, COMB, RING, PITCH, GRAIN, COMP, FLANGE, EFFECT_COUNT
};

struct Tag
{
    std::int8_t polarity; // 0 = untagged, +1 or -1
    Axis axis;
};

struct CatalogEntry
{
    const char* name;
    std::uint8_t weight;
    std::array<Tag, 4> tags; // p0..p3; p4 and p5 are never tagged
};

inline constexpr Tag none { 0, TONE };
inline constexpr Tag up (Axis a) { return { +1, a }; }
inline constexpr Tag down (Axis a) { return { -1, a }; }

inline constexpr std::array<CatalogEntry, EFFECT_COUNT> catalog { {
    { "SAT",    10, { up (HEAT),   up (TONE),   none,        none } },
    { "CRUSH",   6, { down (HEAT), up (TONE),   none,        none } },
    { "FILTER", 10, { up (TONE),   none,        none,        none } },
    { "DELAY",  10, { down (RATE), up (TAIL),   up (TONE),   none } },
    { "VERB",    8, { up (TAIL),   up (TONE),   none,        up (SPACE) } },
    { "CHORUS",  7, { up (RATE),   none,        none,        none } },
    { "PHASER",  7, { up (RATE),   none,        up (TONE),   up (TAIL) } },
    { "TREM",    6, { up (RATE),   none,        none,        up (SPACE) } },
    { "WOW",     6, { up (RATE),   none,        none,        up (TONE) } },
    { "FREEZE",  3, { down (RATE), none,        none,        none } },
    { "COMB",    5, { up (TONE),   up (TAIL),   up (TONE),   none } },
    { "RING",    4, { up (TONE),   none,        none,        none } },
    { "PITCH",   3, { none,        none,        none,        up (TAIL) } },
    { "GRAIN",   3, { down (RATE), up (RATE),   up (SPACE),  none } },
    { "COMP",    6, { down (HEAT), up (HEAT),   none,        up (TAIL) } },
    { "FLANGE",  5, { up (RATE),   none,        none,        up (TAIL) } },
} };
} // namespace vb
