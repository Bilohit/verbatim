#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vb
{
inline constexpr std::uint16_t GRAMMAR_VERSION = 1;
inline constexpr std::size_t GENERATED_SLOTS = 6;

// All-integer chain description (docs/02-spec.md section 3).
struct ModuleSpec
{
    std::uint8_t type = 0;
    std::array<std::uint16_t, 6> p {};
    std::uint16_t mix = 0;
    bool operator== (const ModuleSpec&) const = default;
};

struct LaneSpec
{
    std::uint64_t seed = 0;
    std::uint8_t nModules = 0;
    std::uint8_t drive = 0, duck = 0, feedback = 0, split = 0, chain = 0, band = 0;
    std::uint16_t bandCenter = 0;
    std::int8_t mixSteps = 0;
    std::uint8_t mixAbs = 255, timeDiv = 0;
    std::int8_t level = 0, space = 0;
    std::uint8_t emphasis = 0;
    struct { std::uint8_t shape = 0; std::uint16_t rate = 0, depth = 0; std::uint8_t target = 0; } lfoA;
    struct { std::uint16_t rate = 0, depth = 0; std::uint8_t target = 0; } lfoB;
    struct { std::uint16_t depth = 0; std::uint8_t target = 0; } env;
    // All six generated slots; the lane's modules are slots[0 .. nModules-1] (the rest serve holds, R1).
    std::array<ModuleSpec, GENERATED_SLOTS> slots {};
    std::string entropy; // E; not part of the canonical bytes
};

struct ChainSpec
{
    std::uint16_t grammarVersion = GRAMMAR_VERSION;
    std::vector<LaneSpec> lanes;
};
} // namespace vb
