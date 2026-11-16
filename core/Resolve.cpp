#include "Resolve.h"

#include "Catalog.h"

#include <algorithm>

namespace vb
{
namespace
{
int satCount (const std::vector<ResolvedModule>& mods)
{
    return static_cast<int> (std::count_if (mods.begin(), mods.end(), [] (const ResolvedModule& m) { return m.spec.type == SAT; }));
}

// Latency budget (R4): at most 2 SAT per lane. Drop unheld generated SAT first, then added, then held.
void enforceSatLimit (std::vector<ResolvedModule>& mods)
{
    auto dropFirst = [&] (auto pred) {
        const auto it = std::find_if (mods.begin(), mods.end(), [&] (const ResolvedModule& m) { return m.spec.type == SAT && pred (m); });
        if (it == mods.end())
            return false;
        mods.erase (it);
        return true;
    };
    while (satCount (mods) > MAX_SAT_PER_LANE)
        if (! dropFirst ([] (const ResolvedModule& m) { return ! m.held; }))
            if (! dropFirst ([] (const ResolvedModule& m) { return m.added >= 0; }))
                dropFirst ([] (const ResolvedModule&) { return true; });
}

void tombstone (LaneOverride& o, std::uint64_t seed, int slot)
{
    if (o.removedForSeed != seed)
    {
        o.removed.clear();
        o.removedForSeed = seed;
    }
    o.removed.insert (slot);
}
} // namespace

ResolvedChain resolve (const ChainSpec& spec, const Overrides& overrides)
{
    ResolvedChain out;
    for (std::size_t i = 0; i < spec.lanes.size(); ++i)
    {
        const auto& gen = spec.lanes[i];
        const auto& o = overrides[i];
        ResolvedLane lane { gen, {} };

        // R1: a hold beyond nModules extends the lane with the generated slots in between.
        int length = gen.nModules;
        for (const auto& [k, m] : o.holds)
            if (k >= 0 && k < static_cast<int> (GENERATED_SLOTS))
                length = std::max (length, k + 1);

        const bool tombstonesValid = o.removedForSeed == gen.seed; // R3
        for (int k = 0; k < length; ++k)
        {
            if (const auto h = o.holds.find (k); h != o.holds.end())
                lane.modules.push_back ({ h->second, true, k, -1 });
            else if (! (tombstonesValid && o.removed.count (k) != 0))
                lane.modules.push_back ({ gen.slots[static_cast<std::size_t> (k)], false, k, -1 });
        }
        for (std::size_t a = 0; a < o.added.size() && a < MAX_ADDED; ++a) // R4
            lane.modules.push_back ({ o.added[a], true, -1, static_cast<int> (a) });

        enforceSatLimit (lane.modules);
        out.lanes.push_back (std::move (lane));
    }
    return out;
}

bool applyOverride (Overrides& overrides, const ChainSpec& spec, int laneIndex, OverrideOp op, int position, const ModuleSpec& module)
{
    if (laneIndex < 0 || laneIndex >= static_cast<int> (spec.lanes.size()) || module.type >= EFFECT_COUNT)
        return false;
    auto& o = overrides[static_cast<std::size_t> (laneIndex)];
    const auto seed = spec.lanes[static_cast<std::size_t> (laneIndex)].seed;
    const auto lane = resolve (spec, overrides).lanes[static_cast<std::size_t> (laneIndex)];

    if (op == OverrideOp::Add)
    {
        if (o.added.size() >= MAX_ADDED || lane.modules.size() >= MAX_LANE_MODULES)
            return false;
        if (module.type == SAT && satCount (lane.modules) >= MAX_SAT_PER_LANE)
            return false;
        o.added.push_back (module);
        return true;
    }

    if (position < 0 || position >= static_cast<int> (lane.modules.size()))
        return false;
    const auto& m = lane.modules[static_cast<std::size_t> (position)];
    switch (op)
    {
        case OverrideOp::Hold:
            if (m.added < 0)
                o.holds[m.slot] = m.spec;
            return true;
        case OverrideOp::Release:
            if (m.added >= 0)
                return false; // added modules are always held
            o.holds.erase (m.slot);
            return true;
        case OverrideOp::Edit: // R2: an edit is an auto-hold with the edited values
            if (module.type == SAT && m.spec.type != SAT && satCount (lane.modules) >= MAX_SAT_PER_LANE)
                return false;
            if (m.added >= 0)
                o.added[static_cast<std::size_t> (m.added)] = module;
            else
                o.holds[m.slot] = module;
            return true;
        case OverrideOp::Remove: // R3
            if (m.added >= 0)
            {
                o.added.erase (o.added.begin() + m.added);
                return true;
            }
            o.holds.erase (m.slot); // ruling T21: a removed held module also disappears
            tombstone (o, seed, m.slot);
            return true;
        case OverrideOp::Add:
            break;
    }
    return false;
}
} // namespace vb
