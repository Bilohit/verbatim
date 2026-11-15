#pragma once
#include <cstddef>
#include <cstdint>

#define XXH_INLINE_ALL
#include "xxhash.h"

namespace vb
{
// Grammar v1 salt, ASCII "VERBATM1" (docs/04-seed-grammar.md section 3).
inline constexpr std::uint64_t SALT = 0x5645524241544D31ull;

// XXH64, reference algorithm, little-endian reads.
inline std::uint64_t xxh64 (const void* data, std::size_t len, std::uint64_t seed)
{
    return XXH64 (data, len, seed);
}
} // namespace vb
