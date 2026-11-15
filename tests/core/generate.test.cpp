// G2 (full), G5, F1: docs/04-seed-grammar.md sections 6, 7, 8.
#include <catch2/catch_test_macros.hpp>

#include "Catalog.h"
#include "Generate.h"
#include "Rng.h"
#include "Serialize.h"

#include <array>
#include <string>
#include <thread>
#include <vector>

using namespace vb;

namespace
{
using Types = std::vector<int>;

Types typesOf (const LaneSpec& l)
{
    Types t;
    for (int k = 0; k < l.nModules; ++k)
        t.push_back (l.slots[static_cast<std::size_t> (k)].type);
    return t;
}

struct Example
{
    const char* text;
    std::vector<std::uint64_t> seeds;
    std::vector<Types> types;
    std::uint64_t chainHash;
};

const std::vector<Example>& examples()
{
    static const std::vector<Example> ex = {
        { "hello", { 0x4c66eed20a244bd7ull }, { { SAT, FREEZE, COMP, COMB } }, 0x692ac662981b2247ull },
        { "dark tape, slow echo!", { 0x1e17d464a144ec88ull }, { { VERB, TREM, CRUSH, WOW } }, 0xefd86bb250fc9113ull },
        { "tape echo", { 0x1e17d464a144ec88ull }, { { VERB, TREM, CRUSH, WOW } }, 0xf3a6d5f547a566b2ull },
        { "TAPE echo!!", { 0x1e17d464a144ec88ull }, { { VERB, TREM, CRUSH, WOW } }, 0x8638cfcd4f292db2ull },
        { "dark!!", { 0x912f19095a36327bull }, { { SAT, GRAIN, FLANGE, CRUSH } }, 0xe0d87e51fb3af63eull },
        { "echo 8 50%...", { 0x6cf1255201f4b196ull }, { { SAT, WOW, CHORUS, FILTER } }, 0x5db80c69b133ee0dull },
        { "lo-fi \"radio\" > 7", { 0xe6e79ff38a103f96ull }, { { VERB, CHORUS, FREEZE } }, 0x10150f4a0ea65015ull },
        { "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xD0\xBC\xD0\xB8\xD1\x80 \xE2\x9C\x89", { 0x2863b5217305d75cull },
          { { CHORUS, DELAY, RING, COMP } }, 0xb70518222f8d15f0ull },
        { "warm tape\n\n& long echo\xE2\x80\xA6 ~?", { 0x1fa9551cf86ece43ull, 0x6cf1255201f4b196ull },
          { { DELAY, SAT, CHORUS, TREM, VERB }, { SAT, WOW, CHORUS, FILTER } }, 0x99da9f7a8acef726ull },
        { "   ", {}, {}, 0x385f55d8eac1f68bull },
        { "a\nb\nc\nd\ne", { 0x57f8d3259af88044ull, 0xdefb7f93d4f9a822ull, 0xf66571c2d7356d5dull, 0xa795761d53a18b14ull },
          { { GRAIN, WOW, FLANGE }, { FLANGE, SAT, COMB, DELAY }, { PHASER, FILTER, COMP }, { PHASER, GRAIN, DELAY, FILTER, FLANGE, COMP } },
          0xe9dea3d9bbbe0be1ull },
        { "DARK Dark dark bright", { 0x912f19095a36327bull }, { { SAT, GRAIN, FLANGE, CRUSH } }, 0xda070e833b9a7952ull },
        { "wait.. what...", { 0x15d34abfabdde237ull }, { { FILTER, CHORUS, PHASER, COMB, SAT, DELAY } }, 0xe9dae3f35d0d3d2aull },
        { "don't stop", { 0x895c2f22a5d3a04cull }, { { TREM, FLANGE, FILTER, PHASER, SAT } }, 0xd38cd535f7756da2ull },
    };
    return ex;
}

void checkModule (const ModuleSpec& m, int type, std::array<int, 6> p, int mix)
{
    CHECK (m.type == type);
    for (std::size_t q = 0; q < 6; ++q)
        CHECK (m.p[q] == p[q]);
    CHECK (m.mix == mix);
}
} // namespace

TEST_CASE ("Catalog weights sum to 99", "[G2]")
{
    int sum = 0;
    for (const auto& e : catalog)
        sum += e.weight;
    CHECK (sum == 99);
}

TEST_CASE ("G2 worked examples: seeds, types and chain hashes", "[G2]")
{
    for (const auto& ex : examples())
    {
        INFO ("text: " << ex.text);
        const auto spec = generate (ex.text);
        CHECK (spec.grammarVersion == 1);
        REQUIRE (spec.lanes.size() == ex.seeds.size());
        for (std::size_t i = 0; i < ex.seeds.size(); ++i)
        {
            CHECK (spec.lanes[i].seed == ex.seeds[i]);
            CHECK (typesOf (spec.lanes[i]) == ex.types[i]);
        }
        CHECK (chainHash (spec) == ex.chainHash);
    }
}

TEST_CASE ("G2 full dump example 1: hello", "[G2]")
{
    const auto spec = generate ("hello");
    REQUIRE (spec.lanes.size() == 1);
    const auto& l = spec.lanes[0];
    CHECK (l.lfoA.shape == 1);
    CHECK (l.lfoA.rate == 33025);
    CHECK (l.lfoA.depth == 8994);
    CHECK (l.lfoA.target == 3);
    CHECK (l.lfoB.rate == 36481);
    CHECK (l.lfoB.depth == 0);
    CHECK (l.lfoB.target == 0);
    CHECK (l.env.depth == 0);
    CHECK (l.env.target == 2);
    CHECK (l.bandCenter == 30407);
    checkModule (l.slots[0], SAT, { 23281, 45101, 38859, 22320, 2547, 14925 }, 41917);
    checkModule (l.slots[1], FREEZE, { 1936, 7938, 48863, 3845, 55571, 58815 }, 33379);
    checkModule (l.slots[2], COMP, { 57674, 51154, 22431, 1115, 46750, 27956 }, 14077);
    checkModule (l.slots[3], COMB, { 6507, 63294, 29733, 55027, 4994, 13608 }, 13433);
}

TEST_CASE ("G2 full dump example 2: dark tape, slow echo!", "[G2]")
{
    const auto spec = generate ("dark tape, slow echo!");
    REQUIRE (spec.lanes.size() == 1);
    const auto& l = spec.lanes[0];
    CHECK (l.lfoA.shape == 2);
    CHECK (l.lfoA.rate == 18883);
    CHECK (l.lfoA.depth == 0);
    CHECK (l.lfoA.target == 2);
    CHECK (l.lfoB.rate == 15170);
    CHECK (l.lfoB.depth == 0);
    CHECK (l.lfoB.target == 0);
    CHECK (l.env.depth == 0);
    CHECK (l.env.target == 3);
    CHECK (l.bandCenter == 7895);
    checkModule (l.slots[0], VERB, { 12533, 8166, 58372, 64386, 49130, 57258 }, 3649);
    checkModule (l.slots[1], TREM, { 20367, 58877, 30781, 11618, 51539, 62537 }, 9682);
    checkModule (l.slots[2], CRUSH, { 57110, 49680, 23, 11473, 11974, 24423 }, 27908);
    checkModule (l.slots[3], WOW, { 6033, 20538, 55541, 1933, 45028, 31865 }, 47542);

    const auto plain = generate ("tape echo");
    CHECK (plain.lanes[0].lfoA.rate == 35179);
    CHECK (plain.lanes[0].lfoB.rate == 31531);
}

TEST_CASE ("G2 example 9 lane 1 depths", "[G2]")
{
    const auto spec = generate ("warm tape\n\n& long echo\xE2\x80\xA6 ~?");
    REQUIRE (spec.lanes.size() == 2);
    CHECK (spec.lanes[1].chain == 1);
    CHECK (spec.lanes[1].lfoA.depth == 13107);
    CHECK (spec.lanes[1].lfoB.depth == 13107);
}

TEST_CASE ("Serialization of the empty chain is the 7-byte header", "[G2]")
{
    const auto bytes = serialize (generate (""));
    CHECK (bytes == std::vector<std::uint8_t> { 'V', 'B', 'T', 'M', 1, 0, 0 });
}

TEST_CASE ("G5 stability: twice in one process and from two threads", "[G5]")
{
    std::vector<std::uint64_t> a, b, c;
    for (const auto& ex : examples())
    {
        a.push_back (chainHash (generate (ex.text)));
        b.push_back (chainHash (generate (ex.text)));
    }
    std::thread t ([&] {
        for (const auto& ex : examples())
            c.push_back (chainHash (generate (ex.text)));
    });
    std::vector<std::uint64_t> d;
    for (const auto& ex : examples())
        d.push_back (chainHash (generate (ex.text)));
    t.join();
    CHECK (a == b);
    CHECK (a == c);
    CHECK (a == d);
}

TEST_CASE ("F1 generate fuzz: 100000 random byte strings stay in range", "[F1]")
{
    SplitMix64 rng { 0x6E6E6E6Eull };
    for (int iter = 0; iter < 100000; ++iter)
    {
        const auto len = static_cast<std::size_t> ((rng.next() >> 32) % 12001u);
        std::string s;
        s.reserve (len);
        while (s.size() < len)
        {
            const auto v = rng.next();
            s += (v & 7u) == 0 ? '\n' : static_cast<char> ((v >> 16) & 0xFF);
        }
        const auto spec = generate (s);
        REQUIRE (spec.lanes.size() <= 4);
        for (const auto& l : spec.lanes)
        {
            REQUIRE ((l.nModules >= 2 && l.nModules <= 6));
            REQUIRE (l.lfoA.shape <= 3);
            REQUIRE (l.lfoA.target <= 5);
            REQUIRE (l.lfoB.target <= 5);
            REQUIRE (l.env.target <= 5);
            std::array<bool, 16> seen {};
            for (const auto& m : l.slots)
            {
                REQUIRE (m.type <= 15);
                REQUIRE (! seen[m.type]); // a type is drawn at most once per lane
                seen[m.type] = true;
            }
        }
    }
}
