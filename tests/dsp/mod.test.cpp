// T11: modulation sources (catalog 1.2), Macro (1.4), D8, R2 (implemented effects), D1 mix-0 cases.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Generate.h"
#include "Mod.h"
#include "Module.h"
#include "Rng.h"

#include <cmath>
#include <cstring>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;
constexpr double pi = 3.141592653589793;

vb::LaneSpec laneWith (std::vector<vb::ModuleSpec> modules)
{
    vb::LaneSpec l;
    l.nModules = static_cast<std::uint8_t> (modules.size());
    for (std::size_t k = 0; k < modules.size(); ++k)
        l.slots[k] = modules[k];
    return l;
}

vb::ModuleSpec mid (std::uint8_t type)
{
    vb::ModuleSpec m;
    m.type = type;
    m.p.fill (32768);
    m.mix = 32768;
    return m;
}

// Runs LaneMod alone and returns the mod value of module 0 at each 32-sample tick.
std::vector<float> ticks (const vb::LaneSpec& spec, int count, int nTicks, const std::vector<float>& input = {})
{
    vbdsp::LaneMod mod;
    mod.prepare (spec, count, SR);
    std::vector<float> out;
    for (int t = 0; t < nTicks; ++t)
    {
        if (! input.empty())
            mod.trackInput (input.data() + t * 32, input.data() + t * 32, 32);
        mod.tick (120.0, 0.5f);
        out.push_back (mod.valueFor (0));
    }
    return out;
}

double periodFromZeroCrossings (const std::vector<float>& v, double tickSeconds)
{
    std::vector<double> ups;
    for (std::size_t i = 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
            ups.push_back (static_cast<double> (i));
    REQUIRE (ups.size() >= 3);
    return (ups.back() - ups.front()) / static_cast<double> (ups.size() - 1) * tickSeconds;
}
} // namespace

TEST_CASE ("LFO A: free rate exp(0.05, 8) Hz, sine shape, depth fraction", "[mod]")
{
    auto l = laneWith ({ mid (vb::DELAY) });
    l.lfoA = { 0, 52428, 65535, 0 }; // sine, rate u = 0.8
    const double hz = 0.05 * std::pow (8.0 / 0.05, 52428.0 / 65535.0);
    const auto v = ticks (l, 1, static_cast<int> (5.0 * SR / 32));
    CHECK (std::abs (periodFromZeroCrossings (v, 32.0 / SR) - 1.0 / hz) < 0.01 / hz);
    float peak = 0;
    for (auto x : v)
        peak = std::max (peak, std::abs (x));
    CHECK (std::abs (peak - 1.0f) < 0.01f);
}

TEST_CASE ("LFO A synced: period = 4 notes of 1/timeDiv at 120 BPM", "[mod]")
{
    auto l = laneWith ({ mid (vb::DELAY) });
    l.lfoA = { 0, 0, 65535, 0 };
    l.timeDiv = 8; // eighth notes at 120 BPM = 0.25 s; LFO period = 1 s
    const auto v = ticks (l, 1, static_cast<int> (5.0 * SR / 32));
    CHECK (std::abs (periodFromZeroCrossings (v, 32.0 / SR) - 1.0) < 0.01);
}

TEST_CASE ("LFO B: random walk bounded by depth, changes, deterministic", "[mod]")
{
    auto l = laneWith ({ mid (vb::DELAY) });
    l.seed = 1234;
    l.lfoB = { 65535, 32768, 0 }; // 2 Hz, half depth
    const auto a = ticks (l, 1, static_cast<int> (3.0 * SR / 32));
    const auto b = ticks (l, 1, static_cast<int> (3.0 * SR / 32));
    CHECK (a == b);
    float lo = 1, hi = -1;
    for (auto x : a)
    {
        lo = std::min (lo, x);
        hi = std::max (hi, x);
    }
    CHECK (hi <= 0.5f + 1e-6f);
    CHECK (lo >= -0.5f - 1e-6f);
    CHECK (hi - lo > 0.1f);
}

TEST_CASE ("ENV: -12 dBFS RMS input reads 1 x depth, unipolar", "[mod]")
{
    auto l = laneWith ({ mid (vb::DELAY) });
    l.env = { 65535, 0 };
    const auto in = sine (static_cast<std::size_t> (SR), 1000.0, SR, -12.0 + 3.0103); // RMS -12 dBFS
    const auto v = ticks (l, 1, static_cast<int> (SR / 32), in);
    CHECK (std::abs (v.back() - 1.0f) < 0.05f);
    for (auto x : v)
        CHECK (x >= 0.0f);
}

TEST_CASE ("Routing: target % moduleCount; macro scales depth by 2m", "[mod]")
{
    auto l = laneWith ({ mid (vb::DELAY), mid (vb::SAT) });
    l.env = { 65535, 3 }; // 3 % 2 = module 1
    vbdsp::LaneMod mod;
    mod.prepare (l, 2, SR);
    const auto in = sine (32 * 400, 1000.0, SR, -12.0 + 3.0103);
    for (int t = 0; t < 400; ++t)
    {
        mod.trackInput (in.data() + t * 32, in.data() + t * 32, 32);
        mod.tick (120.0, 0.25f);
    }
    CHECK (mod.valueFor (0) == 0.0f);
    CHECK (std::abs (mod.valueFor (1) - 0.5f) < 0.05f); // depth 1 * env 1 * 2 * 0.25
}

TEST_CASE ("D1 module with mix 0 (Macro 0) equals its delayed dry input", "[D1]")
{
    for (std::uint8_t t : { vb::SAT, vb::DELAY })
    {
        auto m = vbdsp::makeModule (mid (t), 0, 0);
        m->prepare (SR, BLOCK);
        auto L = pinkNoise (24000, 21, -18.0);
        auto R = L;
        const auto in = L;
        vbdsp::ModCtx ctx;
        ctx.macro = 0.0f;
        for (std::size_t s = 0; s < L.size(); s += BLOCK)
            m->process (L.data() + s, R.data() + s, static_cast<int> (std::min<std::size_t> (BLOCK, L.size() - s)), ctx);
        const auto d = static_cast<std::size_t> (m->latencySamples());
        double worst = 0;
        for (std::size_t i = d; i < L.size(); ++i)
            worst = std::max (worst, std::abs (static_cast<double> (L[i]) - in[i - d]));
        CHECK (toDb (worst) < -120.0);
    }
}

TEST_CASE ("D1 lane with all modules mix 0 (Macro 0) equals DC-blocked delayed dry", "[D1]")
{
    vb::ChainSpec spec;
    spec.lanes.push_back (laneWith ({ mid (vb::SAT), mid (vb::DELAY), mid (vb::SAT) }));
    vbdsp::Engine e;
    e.setSpec (spec);
    e.prepare (SR, BLOCK);
    e.setMacro (0.0f);
    e.reset();
    const auto in = pinkNoise (48000, 22, -18.0);
    const auto out = render (e, in, in, BLOCK);
    const auto d = static_cast<std::size_t> (e.latencySamples());
    const double r = std::exp (-2.0 * pi * 5.0 / SR);
    double x1 = 0, y1 = 0, worst = 0;
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        const double x = i >= d ? in[i - d] : 0.0;
        const double y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        worst = std::max (worst, std::abs (out.L[i] - y));
    }
    CHECK (toDb (worst) < -120.0);
}

TEST_CASE ("D8 Macro 0.5 equals the chain with macro scaling disabled", "[D8]")
{
    const auto in = pinkNoise (static_cast<std::size_t> (2 * SR), 23, -18.0);
    for (const char* text : { "hello", "dark tape, slow echo!", "tape echo ~~~ ??? ::: 8" })
    {
        INFO (text);
        std::vector<float> outs[2];
        for (int scaling = 0; scaling < 2; ++scaling)
        {
            vbdsp::Engine e;
            e.setSpec (vb::generate (text));
            e.prepare (SR, BLOCK);
            e.setMacroScalingEnabled (scaling == 1);
            e.reset();
            outs[scaling] = render (e, in, in, BLOCK).L;
        }
        double worst = 0;
        for (std::size_t i = 0; i < in.size(); ++i)
            worst = std::max (worst, std::abs (static_cast<double> (outs[0][i]) - outs[1][i]));
        CHECK (toDb (worst) < -120.0);
    }
}

TEST_CASE ("D8 sweeping mod -1..1 and Macro 0..1 on every module: no clicks", "[D8]")
{
    const auto n = static_cast<std::size_t> (4 * SR);
    const auto in = sine (n, 100.0, SR, -30.0);
    double inStep = 0;
    for (std::size_t i = 1; i < n; ++i)
        inStep = std::max (inStep, std::abs (static_cast<double> (in[i]) - in[i - 1]));

    for (std::uint8_t t = 0; t < vb::EFFECT_COUNT; ++t)
    {
        INFO ("type " << vb::catalog[t].name);
        auto l = laneWith ({ mid (t) });
        l.lfoA = { 0, 65535, 65535, 0 }; // 8 Hz sine, full depth, on module 0
        vb::ChainSpec spec;
        spec.lanes.push_back (l);
        vbdsp::Engine e;
        e.setSpec (spec);
        e.prepare (SR, BLOCK);
        e.reset();
        const auto out = render (e, in, in, BLOCK, [&] (std::size_t start) {
            e.setMacro (static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (start) / static_cast<double> (n))));
        });
        double step = 0;
        for (std::size_t i = 1; i < n; ++i)
            step = std::max (step, std::abs (static_cast<double> (out.L[i]) - out.L[i - 1]));
        CHECK (toDb (step) < -40.0);
    }
}

TEST_CASE ("R2 block-size invariance (implemented effects)", "[R2]")
{
    const auto in = pinkNoise (static_cast<std::size_t> (3 * SR), 24, -18.0);
    for (const char* text : { "hello", "dark tape, slow echo!", "tape echo ~~~ ??? ::: 8", "warm tape\n\n& long echo... ~?",
                              "lo-fi \"radio\" > 7", "a\nb\nc\nd" })
    {
        INFO (text);
        auto run = [&] (std::function<int (std::size_t)> blockAt) {
            vbdsp::Engine e;
            e.setSpec (vb::generate (text));
            e.prepare (SR, 4096);
            e.reset();
            auto L = in, R = in;
            for (std::size_t s = 0; s < L.size();)
            {
                const int b = static_cast<int> (std::min<std::size_t> (static_cast<std::size_t> (blockAt (s)), L.size() - s));
                e.process (L.data() + s, R.data() + s, b);
                s += static_cast<std::size_t> (b);
            }
            return L;
        };
        const auto ref = run ([] (std::size_t) { return 512; });
        vb::SplitMix64 rng { 77 };
        for (const auto& other : { run ([] (std::size_t) { return 32; }), run ([] (std::size_t) { return 4096; }),
                                   run ([&] (std::size_t) { return 17 + static_cast<int> (rng.next() % 1008); }) })
        {
            double worst = 0;
            for (std::size_t i = 0; i < ref.size(); ++i)
                worst = std::max (worst, std::abs (static_cast<double> (ref[i]) - other[i]));
            CHECK (toDb (worst) < -100.0);
        }
    }
}
