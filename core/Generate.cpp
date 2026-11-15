#include "Generate.h"

#include "Bias.h"
#include "Catalog.h"
#include "Hash.h"
#include "Rng.h"

#include <algorithm>

namespace vb
{
namespace
{
constexpr std::uint8_t COUNTS[] = { 2, 3, 3, 4, 4, 4, 5, 5, 6 };

std::uint16_t u16of (std::uint32_t v) { return static_cast<std::uint16_t> (v); }
std::uint16_t minDepth (std::uint32_t v) { return u16of (std::min<std::uint32_t> (65535u, v)); }
} // namespace

// docs/04-seed-grammar.md section 6. Draw order is frozen.
LaneSpec generateLane (const LineParse& line)
{
    LaneSpec l;
    l.entropy = line.entropy;
    l.seed = xxh64 (line.entropy.data(), line.entropy.size(), SALT);

    l.drive = line.drive;
    l.duck = line.duck;
    l.feedback = line.feedback;
    l.split = line.split;
    l.chain = line.chain;
    l.band = line.band;
    l.mixSteps = line.mixSteps;
    l.mixAbs = line.mixAbs;
    l.timeDiv = line.timeDiv;
    l.level = line.axes[LEVEL];
    l.space = line.axes[SPACE];
    l.emphasis = line.emphasis;

    SplitMix64 r0 { l.seed };
    std::uint32_t d[9];
    for (auto& v : d)
        v = r0.u16();

    l.nModules = COUNTS[(d[0] * 9u) >> 16];
    l.lfoA.shape = static_cast<std::uint8_t> ((d[1] * 4u) >> 16);
    l.lfoA.rate = u16of (bias (d[2], line.axes[RATE]));
    l.lfoA.depth = minDepth ((d[3] < 32768u ? 0u : d[3] - 32768u) + 13107u * line.wobble);
    l.lfoA.target = static_cast<std::uint8_t> ((d[4] * 6u) >> 16);
    l.lfoB.rate = u16of (bias (d[5], line.axes[RATE]));
    l.lfoB.depth = minDepth (13107u * line.wander);
    l.lfoB.target = static_cast<std::uint8_t> ((d[6] * 6u) >> 16);
    l.env.depth = minDepth (16384u * line.follow);
    l.env.target = static_cast<std::uint8_t> ((d[7] * 6u) >> 16);
    l.bandCenter = u16of (d[8]);

    std::array<bool, EFFECT_COUNT> inPool;
    inPool.fill (true);
    const int mixBias = std::clamp (line.axes[DEPTH] + static_cast<int> (line.emphasis), -3, 3);

    for (std::uint64_t k = 0; k < GENERATED_SLOTS; ++k)
    {
        std::uint8_t seedBytes[8];
        for (int b = 0; b < 8; ++b)
            seedBytes[b] = static_cast<std::uint8_t> (l.seed >> (8 * b));
        SplitMix64 rk { xxh64 (seedBytes, 8, k + 1) };

        std::uint64_t tot = 0;
        for (int t = 0; t < EFFECT_COUNT; ++t)
            if (inPool[static_cast<std::size_t> (t)])
                tot += catalog[static_cast<std::size_t> (t)].weight;

        std::uint64_t x = ((rk.next() >> 32) * tot) >> 32;
        std::uint8_t type = 0;
        for (int t = 0; t < EFFECT_COUNT; ++t)
        {
            if (! inPool[static_cast<std::size_t> (t)])
                continue;
            const auto w = catalog[static_cast<std::size_t> (t)].weight;
            if (x < w)
            {
                type = static_cast<std::uint8_t> (t);
                break;
            }
            x -= w;
        }
        inPool[type] = false;

        auto& m = l.slots[k];
        m.type = type;
        for (auto& p : m.p)
            p = rk.u16();
        m.mix = rk.u16();
        for (std::size_t q = 0; q < 4; ++q)
        {
            const auto& tag = catalog[type].tags[q];
            if (tag.polarity != 0)
                m.p[q] = u16of (bias (m.p[q], line.axes[tag.axis] * tag.polarity));
        }
        m.mix = u16of (bias (m.mix, mixBias));
    }
    return l;
}

ChainSpec generate (std::string_view text)
{
    ChainSpec spec;
    for (const auto& line : parse (text).lanes)
        spec.lanes.push_back (generateLane (line));
    return spec;
}
} // namespace vb
