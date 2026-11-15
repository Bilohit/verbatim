#include "Serialize.h"

#include "Hash.h"

namespace vb
{
namespace
{
struct Writer
{
    std::vector<std::uint8_t>& out;
    void u8 (std::uint8_t v) { out.push_back (v); }
    void i8 (std::int8_t v) { out.push_back (static_cast<std::uint8_t> (v)); }
    void u16 (std::uint16_t v) { u8 (static_cast<std::uint8_t> (v)); u8 (static_cast<std::uint8_t> (v >> 8)); }
    void u64 (std::uint64_t v)
    {
        for (int b = 0; b < 8; ++b)
            u8 (static_cast<std::uint8_t> (v >> (8 * b)));
    }
};
} // namespace

std::vector<std::uint8_t> serialize (const ChainSpec& spec)
{
    std::vector<std::uint8_t> out;
    Writer w { out };
    for (char c : { 'V', 'B', 'T', 'M' })
        w.u8 (static_cast<std::uint8_t> (c));
    w.u16 (spec.grammarVersion);
    w.u8 (static_cast<std::uint8_t> (spec.lanes.size()));

    for (const auto& l : spec.lanes)
    {
        w.u64 (l.seed);
        w.u8 (l.nModules);
        w.u8 (l.drive);
        w.u8 (l.duck);
        w.u8 (l.feedback);
        w.u8 (l.split);
        w.u8 (l.chain);
        w.u8 (l.band);
        w.u16 (l.bandCenter);
        w.i8 (l.mixSteps);
        w.u8 (l.mixAbs);
        w.u8 (l.timeDiv);
        w.i8 (l.level);
        w.i8 (l.space);
        w.u8 (l.emphasis);
        w.u8 (l.lfoA.shape);
        w.u16 (l.lfoA.rate);
        w.u16 (l.lfoA.depth);
        w.u8 (l.lfoA.target);
        w.u16 (l.lfoB.rate);
        w.u16 (l.lfoB.depth);
        w.u8 (l.lfoB.target);
        w.u16 (l.env.depth);
        w.u8 (l.env.target);
        for (std::size_t k = 0; k < l.nModules; ++k)
        {
            const auto& m = l.slots[k];
            w.u8 (m.type);
            for (auto p : m.p)
                w.u16 (p);
            w.u16 (m.mix);
        }
    }
    return out;
}

std::uint64_t chainHash (const ChainSpec& spec)
{
    const auto bytes = serialize (spec);
    return xxh64 (bytes.data(), bytes.size(), 0);
}
} // namespace vb
