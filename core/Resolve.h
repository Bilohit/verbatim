#pragma once
// Overrides on top of the generated ChainSpec (docs/02-spec.md section 3, rules R1-R7). Integer only.
#include "ChainSpec.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace vb
{
inline constexpr std::size_t MAX_ADDED = 2;
inline constexpr std::size_t MAX_LANE_MODULES = 8;
inline constexpr int MAX_SAT_PER_LANE = 2;

// Overrides for one lane index (R5: keyed by lane index, dormant while that lane does not exist).
struct LaneOverride
{
    std::map<int, ModuleSpec> holds;      // generated slot k -> stored module (R1, R2)
    std::set<int> removed;                // tombstoned generated slots (R3)
    std::uint64_t removedForSeed = 0;     // tombstones apply only while the lane seed equals this (R3)
    std::vector<ModuleSpec> added;        // always held, appended after generated slots (R4)
    bool operator== (const LaneOverride&) const = default;
};

using Overrides = std::array<LaneOverride, 4>;

struct ResolvedModule
{
    ModuleSpec spec;
    bool held = false;
    int slot = -1;  // generated slot index, or -1 for an added module
    int added = -1; // index into LaneOverride::added, or -1
    bool operator== (const ResolvedModule&) const = default;
};

struct ResolvedLane
{
    LaneSpec lane; // lane-level fields always follow the text (R7)
    std::vector<ResolvedModule> modules;
};

struct ResolvedChain
{
    std::vector<ResolvedLane> lanes;
};

ResolvedChain resolve (const ChainSpec& spec, const Overrides& overrides);

// UI operations, addressed by position in the resolved lane. Return false when rejected (limits, bad index).
enum class OverrideOp { Hold, Release, Edit, Remove, Add };
bool applyOverride (Overrides& overrides, const ChainSpec& spec, int lane, OverrideOp op, int position, const ModuleSpec& module = {});
} // namespace vb
