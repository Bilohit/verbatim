// T29: RT2 across swaps and automation, P1 at the true worst case (4 lanes x 8 modules). docs/06-test-plan.md.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Generate.h"
#include "Resolve.h"

#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstdlib>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

// "wait.. what..." generates 6 modules per lane; two added modules make 8 (R4).
vb::ResolvedChain worstCase()
{
    const auto spec = vb::generate ("wait.. what... >>> ~~ ??\nwait.. what... >>> ~~ ??\nwait.. what... >>> ~~ ??\nwait.. what... >>> ~~ ??");
    vb::Overrides o;
    for (int lane = 0; lane < 4; ++lane)
    {
        // Slot 0 becomes PITCH through an edit (an auto-hold, R2), so the lane holds PITCH, GRAIN and VERB.
        vb::applyOverride (o, spec, lane, vb::OverrideOp::Edit, 0, vb::ModuleSpec { vb::PITCH, { 60000, 40000, 40000, 40000, 0, 0 }, 40000 });
        vb::applyOverride (o, spec, lane, vb::OverrideOp::Add, -1, vb::ModuleSpec { vb::VERB, { 40000, 30000, 30000, 30000, 0, 0 }, 40000 });
        vb::applyOverride (o, spec, lane, vb::OverrideOp::Add, -1, vb::ModuleSpec { vb::GRAIN, { 30000, 50000, 30000, 30000, 30000, 0 }, 40000 });
    }
    return vb::resolve (spec, o);
}
} // namespace

TEST_CASE ("RT2 no allocation on the audio thread across chain swaps, overrides and automation", "[RT2]")
{
    vbdsp::Engine e;
    e.setSpec (vb::generate ("hello"));
    e.prepare (SR, BLOCK);
    e.reset();
    static const char* texts[] = { "dark tape, slow echo!", "wait.. what...", "a\nb\nc\nd", "dark!!", "lo-fi \"radio\" > 7", "hello" };
    const auto in = pinkNoise (static_cast<std::size_t> (10 * SR), 71, -18.0);
    std::size_t block = 0;
    resetAllocationCount();
    render (e, in, in, BLOCK, [&] (std::size_t) {
        // Automation every block: Mix, Macro, Output, bypass, tempo and transport.
        const double t = static_cast<double> (block) / 50.0;
        e.setMix (static_cast<float> (0.5 + 0.5 * std::sin (t)));
        e.setMacro (static_cast<float> (0.5 + 0.5 * std::cos (t * 1.3)));
        e.setOutputDb (static_cast<float> (-6.0 * (1.0 + std::sin (t * 0.7))));
        e.setBypass (block % 300 > 280);
        e.setTransport (90.0 + 60.0 * std::sin (t * 0.2), block % 200 > 20);
        if (block % 20 == 0) // a keystroke: rebuild and swap, sometimes with overrides
            e.setResolved (block % 60 == 0 ? worstCase() : vb::resolve (vb::generate (texts[(block / 20) % std::size (texts)]), {}));
        e.collectGarbage();
        ++block;
    });
    CHECK (allocationsInAudio() == 0);
    CHECK (e.nanGuardCount() == 0);
}

TEST_CASE ("P1 CPU at the true worst case: 4 lanes x 8 modules with PITCH, GRAIN, VERB, 60 s at 48 kHz / 512", "[P1]")
{
    const auto chain = worstCase();
    REQUIRE (chain.lanes.size() == 4);
    bool hasPitch = false;
    for (const auto& l : chain.lanes)
    {
        REQUIRE (l.modules.size() == 8);
        for (const auto& m : l.modules)
            hasPitch = hasPitch || m.spec.type == vb::PITCH;
    }
    vbdsp::Engine e;
    e.setResolved (chain);
    e.prepare (SR, BLOCK);
    e.reset();
    const auto in = pinkNoise (static_cast<std::size_t> (60 * SR), 72, -18.0);
    const double t0 = threadCpuTime();
    const auto start = std::chrono::steady_clock::now();
    render (e, in, in, BLOCK);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const double factor = 60.0 / secs;
    (void) t0;
    const char* minEnv = std::getenv ("VERBATIM_P1_MIN");
    const double minFactor = minEnv != nullptr ? std::atof (minEnv) : 4.0;
    std::printf ("P1 (8 modules) realtime factor %.1fx (gate %.0fx), PITCH in chain: %s\n", factor, minFactor, hasPitch ? "yes" : "no");
    CHECK (hasPitch);
    CHECK (factor >= minFactor);
}
