#pragma once
#include <cstdint>

namespace vb
{
// SplitMix64 (docs/04-seed-grammar.md section 3).
struct SplitMix64
{
    std::uint64_t s;

    std::uint64_t next()
    {
        s += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::uint16_t u16() { return static_cast<std::uint16_t> (next() >> 48); }
};
} // namespace vb
