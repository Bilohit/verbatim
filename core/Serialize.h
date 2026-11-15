#pragma once
#include <cstdint>
#include <vector>

#include "ChainSpec.h"

namespace vb
{
// Canonical little-endian bytes (docs/04-seed-grammar.md section 7).
std::vector<std::uint8_t> serialize (const ChainSpec& spec);
std::uint64_t chainHash (const ChainSpec& spec);
} // namespace vb
