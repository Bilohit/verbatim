// G1: primitive vectors from docs/04-seed-grammar.md section 3.
#include <catch2/catch_test_macros.hpp>

#include "Bias.h"
#include "Hash.h"
#include "Rng.h"

#include <array>
#include <cstdint>

namespace
{
std::array<std::uint8_t, 101> bytes0to100()
{
    std::array<std::uint8_t, 101> b {};
    for (std::size_t i = 0; i < b.size(); ++i)
        b[i] = static_cast<std::uint8_t> (i);
    return b;
}
} // namespace

TEST_CASE ("G1 XXH64 vectors", "[G1]")
{
    CHECK (vb::xxh64 ("", 0, 0) == 0xef46db3751d8e999ull);
    CHECK (vb::xxh64 ("abc", 3, 0) == 0x44bc2cf5ad770999ull);

    const auto b = bytes0to100();
    CHECK (vb::xxh64 (b.data(), b.size(), 0) == 0xe99038495f85381eull);
    CHECK (vb::xxh64 (b.data(), b.size(), vb::SALT) == 0x442bd2adcf827d01ull);
}

TEST_CASE ("G1 SALT is ASCII VERBATM1", "[G1]")
{
    CHECK (vb::SALT == 0x5645524241544D31ull);
}

TEST_CASE ("G1 SplitMix64 from state 0", "[G1]")
{
    vb::SplitMix64 r { 0 };
    CHECK (r.next() == 0xe220a8397b1dcdafull);
    CHECK (r.next() == 0x6e789e6aa1b965f4ull);
    CHECK (r.next() == 0x06c45d188009454full);
}

TEST_CASE ("G1 SplitMix64 u16 is the top 16 bits of next", "[G1]")
{
    vb::SplitMix64 r { 0 };
    CHECK (r.u16() == 0xe220);
}

TEST_CASE ("G1 bias vectors", "[G1]")
{
    CHECK (vb::bias (20000, 2) == 43553);
    CHECK (vb::bias (20000, -2) == 1862);
    CHECK (vb::bias (65535, 3) == 65535);
    CHECK (vb::bias (0, -3) == 0);
    CHECK (vb::bias (12345, 0) == 12345);
}
