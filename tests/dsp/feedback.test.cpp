// T10: D4 (lane feedback) and F3 subset (stable at feedback 3 with every effect type).
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Rng.h"

#include <cmath>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

vb::ChainSpec feedbackLane (std::vector<vb::ModuleSpec> modules, std::uint8_t feedback)
{
    vb::ChainSpec c;
    vb::LaneSpec l;
    l.nModules = static_cast<std::uint8_t> (modules.size());
    for (std::size_t k = 0; k < modules.size(); ++k)
        l.slots[k] = modules[k];
    l.feedback = feedback;
    c.lanes.push_back (l);
    return c;
}

struct Result
{
    double prePeak;  // peak before SafetyClip, from a render at Output -24 dB
    double tailDb;   // peak over the last second
    bool finite;
};

// 0 dBFS white noise burst for burstSeconds, then silence, at Output -24 dB (SafetyClip stays transparent).
Result burstThenSilence (const vb::ChainSpec& spec, double burstSeconds, double totalSeconds)
{
    vbdsp::Engine e;
    e.setSpec (spec);
    e.prepare (SR, BLOCK);
    e.setOutputDb (-24.0f);
    e.reset();
    const auto n = static_cast<std::size_t> (totalSeconds * SR);
    std::vector<float> L (n, 0.0f), R (n, 0.0f);
    vb::SplitMix64 rng { 99 };
    for (std::size_t i = 0; i < static_cast<std::size_t> (burstSeconds * SR); ++i)
    {
        L[i] = (rng.next() & 1) ? 1.0f : -1.0f;
        R[i] = (rng.next() & 1) ? 1.0f : -1.0f;
    }
    const auto out = render (e, L, R, BLOCK);
    Result r { 0.0, -300.0, true };
    const double undo = std::pow (10.0, 24.0 / 20.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double a = std::max (std::abs (out.L[i]), std::abs (out.R[i]));
        r.finite = r.finite && std::isfinite (a);
        r.prePeak = std::max (r.prePeak, a * undo);
        if (i >= n - static_cast<std::size_t> (SR))
            r.tailDb = std::max (r.tailDb, toDb (a * undo));
    }
    return r;
}

vb::ModuleSpec maxed (std::uint8_t type)
{
    vb::ModuleSpec m;
    m.type = type;
    m.p.fill (65535);
    m.mix = 65535;
    return m;
}
} // namespace

TEST_CASE ("D4 lane feedback 3: decays, pre-clip peak at most 4 (12 dB)", "[D4]")
{
    const auto r = burstThenSilence (feedbackLane ({}, 3), 0.5, 5.0);
    CHECK (r.finite);
    CHECK (r.prePeak <= 4.0);
    CHECK (r.tailDb < -90.0);
}

TEST_CASE ("D4 lane feedback steps change the output", "[D4]")
{
    // A zero-module lane with feedback differs from one without (the loop is active).
    const auto off = burstThenSilence (feedbackLane ({}, 0), 0.1, 0.5);
    const auto on = burstThenSilence (feedbackLane ({}, 3), 0.1, 0.5);
    CHECK (on.prePeak > off.prePeak * 1.05);
}

TEST_CASE ("F3 subset: every effect type at max params with lane feedback 3 stays stable", "[F3]")
{
    for (std::uint8_t t = 0; t < vb::EFFECT_COUNT; ++t)
    {
        INFO ("type " << vb::catalog[t].name);
        const auto r = burstThenSilence (feedbackLane ({ maxed (t) }, 3), 1.0, 31.0);
        CHECK (r.finite);
        CHECK (r.prePeak <= 4.0);
        CHECK (r.tailDb < -60.0);
    }
}

TEST_CASE ("D4 effects at max feedback inside lane feedback 3: decay, pre-clip peak at most 4", "[D4]")
{
    // Feedback param index per effect (catalog): DELAY p1, PHASER p3, FLANGE p3. Extended by T17/T18.
    struct Case { std::uint8_t type; int fbParam; };
    for (const auto c : { Case { vb::DELAY, 1 }, Case { vb::PHASER, 3 }, Case { vb::FLANGE, 3 }, Case { vb::COMB, 1 } })
    {
        INFO ("type " << vb::catalog[c.type].name);
        vb::ModuleSpec m;
        m.type = c.type;
        m.p.fill (32768);
        m.p[static_cast<std::size_t> (c.fbParam)] = 65535;
        m.mix = 65535;
        const auto r = burstThenSilence (feedbackLane ({ m }, 3), 0.5, 12.0);
        CHECK (r.finite);
        CHECK (r.prePeak <= 4.0);
        CHECK (r.tailDb < -60.0);
    }
}
