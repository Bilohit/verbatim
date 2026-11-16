// T21: rules R1-R7 (docs/02-spec.md section 3) and the core part of F4.
#include <catch2/catch_test_macros.hpp>

#include "Catalog.h"
#include "Generate.h"
#include "Resolve.h"
#include "Rng.h"

using namespace vb;

namespace
{
std::vector<int> types (const ResolvedLane& l)
{
    std::vector<int> t;
    for (const auto& m : l.modules)
        t.push_back (m.spec.type);
    return t;
}

int satCount (const ResolvedLane& l)
{
    int n = 0;
    for (const auto& m : l.modules)
        n += m.spec.type == SAT ? 1 : 0;
    return n;
}
} // namespace

TEST_CASE ("No overrides: resolved lanes equal the generated modules", "[resolve]")
{
    const auto spec = generate ("hello");
    const auto r = resolve (spec, {});
    REQUIRE (r.lanes.size() == 1);
    CHECK (types (r.lanes[0]) == std::vector<int> { SAT, FREEZE, COMP, COMB });
    for (const auto& m : r.lanes[0].modules)
        CHECK (! m.held);
}

TEST_CASE ("R1 hold survives a reroll and keeps its slot", "[resolve][R1]")
{
    Overrides o;
    auto spec = generate ("hello");
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Hold, 1));
    const auto held = spec.lanes[0].slots[1];
    spec = generate ("goodbye"); // reroll
    const auto r = resolve (spec, o);
    CHECK (r.lanes[0].modules[1].spec == held);
    CHECK (r.lanes[0].modules[1].held);
}

TEST_CASE ("R1 a held slot beyond nModules extends the lane with the generated slots in between", "[resolve][R1]")
{
    Overrides o;
    o[0].holds[5] = ModuleSpec { DELAY, { 1, 2, 3, 4, 5, 6 }, 7 };
    const auto spec = generate ("hello"); // 4 modules
    const auto r = resolve (spec, o);
    REQUIRE (r.lanes[0].modules.size() == 6);
    CHECK (r.lanes[0].modules[4].spec == spec.lanes[0].slots[4]);
    CHECK (! r.lanes[0].modules[4].held);
    CHECK (r.lanes[0].modules[5].spec == o[0].holds[5]);
}

TEST_CASE ("R2 editing a module stores an auto-hold with the edited values", "[resolve][R2]")
{
    Overrides o;
    const auto spec = generate ("hello");
    auto edited = spec.lanes[0].slots[2];
    edited.p[0] = 1234;
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Edit, 2, edited));
    const auto r = resolve (spec, o);
    CHECK (r.lanes[0].modules[2].held);
    CHECK (r.lanes[0].modules[2].spec.p[0] == 1234);
}

TEST_CASE ("R3 removing a generated slot tombstones it for the current lane seed only", "[resolve][R3]")
{
    Overrides o;
    const auto spec = generate ("hello");
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Remove, 1)); // FREEZE
    CHECK (types (resolve (spec, o).lanes[0]) == std::vector<int> { SAT, COMP, COMB });
    // A reroll changes the seed: the tombstone no longer applies.
    const auto other = generate ("goodbye");
    CHECK (resolve (other, o).lanes[0].modules.size() == other.lanes[0].nModules);
    // Back to the same seed: still removed.
    CHECK (types (resolve (spec, o).lanes[0]) == std::vector<int> { SAT, COMP, COMB });
}

TEST_CASE ("R3 removing a held or added module deletes that override", "[resolve][R3]")
{
    Overrides o;
    const auto spec = generate ("hello");
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Add, -1, ModuleSpec { DELAY, {}, 100 }));
    REQUIRE (resolve (spec, o).lanes[0].modules.size() == 5);
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Remove, 4));
    CHECK (o[0].added.empty());
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Hold, 0));
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Remove, 0));
    CHECK (o[0].holds.empty());
    CHECK (types (resolve (spec, o).lanes[0]) == std::vector<int> { FREEZE, COMP, COMB }); // held slot gone (ruling T21)
}

TEST_CASE ("R4 added modules are held, appended, max 2 per lane, max 2 SAT per lane", "[resolve][R4]")
{
    Overrides o;
    const auto spec = generate ("hello"); // has 1 SAT
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Add, -1, ModuleSpec { SAT, {}, 0 }));
    CHECK_FALSE (applyOverride (o, spec, 0, OverrideOp::Add, -1, ModuleSpec { SAT, {}, 0 })); // third SAT
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Add, -1, ModuleSpec { RING, {}, 0 }));
    CHECK_FALSE (applyOverride (o, spec, 0, OverrideOp::Add, -1, ModuleSpec { RING, {}, 0 })); // third added
    const auto r = resolve (spec, o);
    CHECK (types (r.lanes[0]) == std::vector<int> { SAT, FREEZE, COMP, COMB, SAT, RING });
    CHECK (r.lanes[0].modules[4].held);
    CHECK (satCount (r.lanes[0]) == 2);
}

TEST_CASE ("R4 a reroll never leaves more than 2 SAT: unheld generated SAT is dropped", "[resolve][R4]")
{
    Overrides o;
    o[0].added = { ModuleSpec { SAT, {}, 0 }, ModuleSpec { SAT, {}, 1 } };
    const auto r = resolve (generate ("hello"), o); // generated SAT + 2 added SAT
    CHECK (satCount (r.lanes[0]) == 2);
    CHECK (r.lanes[0].modules.front().spec.type != SAT);
}

TEST_CASE ("R5 overrides are keyed by lane index and stay dormant while the lane is missing", "[resolve][R5]")
{
    Overrides o;
    const auto two = generate ("a\nb");
    REQUIRE (applyOverride (o, two, 1, OverrideOp::Hold, 0));
    CHECK (resolve (generate ("a"), o).lanes.size() == 1);
    CHECK (o[1].holds.size() == 1);
    CHECK (resolve (generate ("a\nc"), o).lanes[1].modules[0].held);
}

TEST_CASE ("R7 held modules ignore later direction words; lane fields follow the text", "[resolve][R7]")
{
    Overrides o;
    const auto plain = generate ("tape echo");
    REQUIRE (applyOverride (o, plain, 0, OverrideOp::Hold, 0));
    const auto dark = generate ("dark tape echo, !!");
    const auto r = resolve (dark, o);
    CHECK (r.lanes[0].modules[0].spec == plain.lanes[0].slots[0]);
    CHECK (r.lanes[0].lane.drive == 2);
    CHECK (r.lanes[0].lane.duck == 1);
}

TEST_CASE ("Release returns a held slot to the generated module", "[resolve]")
{
    Overrides o;
    const auto spec = generate ("hello");
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Hold, 3));
    REQUIRE (applyOverride (o, spec, 0, OverrideOp::Release, 3));
    CHECK (o[0].holds.empty());
}

TEST_CASE ("F4 core: random override sequences keep lane invariants", "[F4]")
{
    static const char* texts[] = { "hello", "tape echo", "dark tape, slow echo!", "a\nb\nc\nd", "wait.. what...", "don't stop", "x" };
    SplitMix64 r { 0xF4 };
    for (int seq = 0; seq < 5000; ++seq)
    {
        Overrides o;
        auto spec = generate (texts[r.next() % std::size (texts)]);
        for (int step = 0; step < 12; ++step)
        {
            const auto op = static_cast<OverrideOp> (r.next() % 5);
            if (r.next() % 6 == 0)
                spec = generate (texts[r.next() % std::size (texts)]);
            const int lane = static_cast<int> (r.next() % 4);
            const int pos = static_cast<int> (r.next() % 10) - 1;
            ModuleSpec m { static_cast<std::uint8_t> (r.next() % EFFECT_COUNT), {}, static_cast<std::uint16_t> (r.next()) };
            applyOverride (o, spec, lane, op, pos, m);
            for (const auto& l : resolve (spec, o).lanes)
            {
                REQUIRE (l.modules.size() <= MAX_LANE_MODULES);
                REQUIRE (satCount (l) <= MAX_SAT_PER_LANE);
            }
        }
    }
}
