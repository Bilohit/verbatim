// T20: F2 (audio fuzz = S1), F3 (extremes), R1/R2 on 30 corpus seeds, P1 (CPU). docs/06-test-plan.md.
// Set VERBATIM_FULL_FUZZ=1 for the full local runs (F2 2000 seeds, F3 200 seeds); CI runs the subsets.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Generate.h"
#include "Rng.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using namespace harness;

namespace
{
bool fullRun()
{
    const char* v = std::getenv ("VERBATIM_FULL_FUZZ");
    return v != nullptr && v[0] == '1';
}

// Fixed-seed random seed texts: words, direction words, ops, numbers, CAPS, 1 to 4 lines.
std::vector<std::string> fuzzTexts (std::size_t count, std::uint64_t seed)
{
    static const char* words[] = { "tape", "echo", "radio", "glass", "river", "static", "noise", "dust", "neon", "velvet",
                                   "rust", "signal", "orbit", "ghost", "paper", "smoke", "metal", "honey", "winter", "machine" };
    static const char* mods[] = { "dark", "bright", "cold", "warm", "slow", "fast", "narrow", "wide", "short", "long", "thin",
                                  "deep", "soft", "loud", "!", "?", "~", ",", "-", "+", ">", "/", ":", "&", "\"", ".", "...",
                                  "8", "16", "3", "50%", "TAPE", "!!!", "~~~", ">>>", "???", ":::" };
    vb::SplitMix64 r { seed };
    std::vector<std::string> out;
    for (std::size_t i = 0; i < count; ++i)
    {
        std::string t;
        const auto lines = 1 + r.next() % 4;
        for (std::uint64_t l = 0; l < lines; ++l)
        {
            if (l > 0)
                t += '\n';
            const auto nw = 1 + r.next() % 3, nm = r.next() % 4;
            for (std::uint64_t k = 0; k < nw + nm; ++k)
            {
                t += (k % 2 == 0 || nm == 0) ? words[r.next() % std::size (words)] : mods[r.next() % std::size (mods)];
                t += ' ';
            }
        }
        out.push_back (t);
    }
    return out;
}

int hexVal (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<std::string> corpusTexts()
{
    std::ifstream in (VERBATIM_GOLDEN_DIR "/corpus.txt", std::ios::binary);
    std::vector<std::string> texts;
    std::string line;
    while (std::getline (in, line))
    {
        std::string t;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '\x5C' && i + 3 < line.size() && line[i + 1] == 'x' && hexVal (line[i + 2]) >= 0 && hexVal (line[i + 3]) >= 0)
            {
                t += static_cast<char> (hexVal (line[i + 2]) * 16 + hexVal (line[i + 3]));
                i += 3;
            }
            else
                t += line[i];
        }
        texts.push_back (t);
    }
    return texts;
}

// 30 corpus seeds: greedy until all 16 effects and LFO B are covered, then the next records in order.
std::vector<std::string> thirtySeeds()
{
    std::vector<std::string> picked;
    std::array<bool, vb::EFFECT_COUNT> seen {};
    bool lfoB = false;
    const auto all = corpusTexts();
    std::vector<bool> used (all.size());
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        bool adds = false;
        for (const auto& l : vb::generate (all[i]).lanes)
        {
            for (int k = 0; k < l.nModules; ++k)
                adds = adds || ! seen[l.slots[static_cast<std::size_t> (k)].type];
            adds = adds || (l.lfoB.depth > 0 && ! lfoB);
        }
        if (! adds)
            continue;
        for (const auto& l : vb::generate (all[i]).lanes)
        {
            for (int k = 0; k < l.nModules; ++k)
                seen[l.slots[static_cast<std::size_t> (k)].type] = true;
            lfoB = lfoB || l.lfoB.depth > 0;
        }
        picked.push_back (all[i]);
        used[i] = true;
    }
    for (std::size_t i = 0; i < all.size() && picked.size() < 30; ++i)
        if (! used[i] && ! vb::generate (all[i]).lanes.empty())
            picked.push_back (all[i]);
    for (bool s : seen)
        REQUIRE (s);
    REQUIRE (lfoB);
    return picked;
}

std::unique_ptr<vbdsp::Engine> engineFor (const vb::ChainSpec& spec, double sr, int block, float macro = 0.5f)
{
    auto e = std::make_unique<vbdsp::Engine>();
    e->setSpec (spec);
    e->prepare (sr, block);
    e->setMacro (macro);
    e->reset();
    return e;
}
} // namespace

TEST_CASE ("F2 audio fuzz (S1): random seeds stay finite, under 0 dBFS and within -36..-8 dBFS RMS", "[F2]")
{
    constexpr double SR = 48000.0;
    const auto count = fullRun() ? 2000u : 300u;
    const auto in = pinkNoise (static_cast<std::size_t> (2 * SR), 61, -18.0);
    std::size_t failures = 0;
    for (const auto& text : fuzzTexts (count, 0xF2F2F2F2ull))
    {
        auto e = engineFor (vb::generate (text), SR, 512);
        const auto out = render (*e, in, in, 512);
        double peak = 0;
        bool finite = true;
        for (std::size_t i = 0; i < out.L.size(); ++i)
        {
            finite = finite && std::isfinite (out.L[i]) && std::isfinite (out.R[i]);
            peak = std::max (peak, static_cast<double> (std::max (std::abs (out.L[i]), std::abs (out.R[i]))));
        }
        const auto from = static_cast<std::size_t> (SR);
        const double db = toDb (std::sqrt (0.5 * (std::pow (rms (out.L.data() + from, from), 2) + std::pow (rms (out.R.data() + from, from), 2))));
        const bool ok = finite && e->nanGuardCount() == 0 && peak <= 1.0 && db >= -36.0 && db <= -8.0;
        if (! ok)
        {
            ++failures;
            std::printf ("F2 fail: rms %.1f dBFS peak %.3f nan %d text [%s]\n", db, peak, e->nanGuardCount(), text.c_str());
        }
    }
    CHECK (failures == 0);
}

TEST_CASE ("F3 extremes: Macro 0/1, square, DC, 44.1k/192k, blocks 16/8192, and no self-oscillation", "[F3]")
{
    const auto count = fullRun() ? 200u : 24u;
    const auto texts = fuzzTexts (count, 0xF3F3F3F3ull);
    std::size_t failures = 0;
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        const double sr = (i / 2) % 2 == 0 ? 44100.0 : 192000.0;
        const int block = (i / 4) % 2 == 0 ? 16 : 8192;
        const float macro = i % 2 == 0 ? 0.0f : 1.0f;
        const int input = static_cast<int> ((i / 8) % 3); // 0 square 0 dBFS, 1 DC, 2 burst then silence
        const auto seconds = input == 2 ? 31.0 : 2.0;
        const auto n = static_cast<std::size_t> (seconds * sr);
        std::vector<float> in (n, 0.0f);
        for (std::size_t k = 0; k < n; ++k)
        {
            const float sq = std::fmod (static_cast<double> (k) * 110.0 / sr, 1.0) < 0.5 ? 1.0f : -1.0f;
            in[k] = input == 0 ? sq : input == 1 ? 0.5f : (k < static_cast<std::size_t> (sr) ? sq : 0.0f);
        }
        auto e = engineFor (vb::generate (texts[i]), sr, block, macro);
        const auto out = render (*e, in, in, block);
        double peak = 0, tail = 0;
        bool finite = true;
        for (std::size_t k = 0; k < n; ++k)
        {
            const double a = std::max (std::abs (out.L[k]), std::abs (out.R[k]));
            finite = finite && std::isfinite (a);
            peak = std::max (peak, a);
            if (k >= n - static_cast<std::size_t> (sr))
                tail = std::max (tail, a);
        }
        const bool ok = finite && peak <= 1.0 && (input != 2 || toDb (tail) < -60.0);
        if (! ok)
        {
            ++failures;
            std::printf ("F3 fail: sr %.0f block %d macro %.0f input %d peak %.3f tail %.1f dB text [%s]\n", sr, block, macro, input, peak, toDb (tail), texts[i].c_str());
        }
    }
    CHECK (failures == 0);
}

TEST_CASE ("R1 bit identity on 30 corpus seeds covering all 16 effects and LFO B", "[R1]")
{
    constexpr double SR = 48000.0;
    const auto in = pinkNoise (static_cast<std::size_t> (10 * SR), 62, -18.0);
    for (const auto& text : thirtySeeds())
    {
        INFO (text);
        const auto a = render (*engineFor (vb::generate (text), SR, 512), in, in, 512);
        const auto b = render (*engineFor (vb::generate (text), SR, 512), in, in, 512);
        CHECK (std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0);
        CHECK (std::memcmp (a.R.data(), b.R.data(), a.R.size() * sizeof (float)) == 0);
    }
}

TEST_CASE ("R2 block-size invariance on 30 corpus seeds", "[R2]")
{
    constexpr double SR = 48000.0;
    const auto in = pinkNoise (static_cast<std::size_t> (3 * SR), 63, -18.0);
    for (const auto& text : thirtySeeds())
    {
        INFO (text);
        auto run = [&] (const std::function<int()>& next) {
            auto e = engineFor (vb::generate (text), SR, 4096);
            auto L = in, R = in;
            for (std::size_t s = 0; s < L.size();)
            {
                const int b = static_cast<int> (std::min<std::size_t> (static_cast<std::size_t> (next()), L.size() - s));
                e->process (L.data() + s, R.data() + s, b);
                s += static_cast<std::size_t> (b);
            }
            return L;
        };
        const auto ref = run ([] { return 512; });
        vb::SplitMix64 rng { 64 };
        for (const auto& other : { run ([] { return 32; }), run ([] { return 4096; }), run ([&] { return 17 + static_cast<int> (rng.next() % 1008); }) })
        {
            double worst = 0;
            for (std::size_t i = 0; i < ref.size(); ++i)
                worst = std::max (worst, std::abs (static_cast<double> (ref[i]) - other[i]));
            CHECK (toDb (worst) < -100.0);
        }
    }
}

TEST_CASE ("P1 CPU: worst-case 4 lanes x 6 generated modules with PITCH, GRAIN, VERB, 60 s at 48 kHz / 512", "[P1]")
{
    constexpr double SR = 48000.0;
    vb::ChainSpec spec;
    // 6 = the generated maximum; 8-module lanes (2 added modules) need overrides and run again at T29.
    const std::uint8_t types[6] = { vb::PITCH, vb::GRAIN, vb::VERB, vb::SAT, vb::DELAY, vb::COMB };
    for (int l = 0; l < 4; ++l)
    {
        vb::LaneSpec lane;
        lane.seed = 0x9000u + static_cast<unsigned> (l);
        lane.nModules = 6;
        for (std::size_t k = 0; k < 6; ++k)
        {
            lane.slots[k].type = types[k];
            lane.slots[k].p.fill (static_cast<std::uint16_t> (20000 + 5000 * k));
            lane.slots[k].mix = 40000;
        }
        lane.feedback = 3;
        lane.lfoA = { 0, 30000, 30000, 2 };
        lane.lfoB = { 30000, 30000, 1 };
        lane.env = { 30000, 0 };
        lane.band = 1;
        spec.lanes.push_back (lane);
    }
    auto e = engineFor (spec, SR, 512);
    const auto in = pinkNoise (static_cast<std::size_t> (60 * SR), 65, -18.0);
    const auto t0 = std::chrono::steady_clock::now();
    render (*e, in, in, 512);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double factor = 60.0 / secs;
    const char* minEnv = std::getenv ("VERBATIM_P1_MIN");
    const double minFactor = minEnv != nullptr ? std::atof (minEnv) : 4.0;
    std::printf ("P1 realtime factor %.1fx (gate %.0fx)\n", factor, minFactor);
    CHECK (factor >= minFactor);
}
