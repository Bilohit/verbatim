// T09: D10 (each lane stage alone) and D7 (split and chained lanes). docs/03-dsp-catalog.md section 1.1.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Module.h"

#include <cmath>
#include <functional>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;
constexpr double pi = 3.141592653589793;

vb::ModuleSpec satLinear() // SAT at 0 dB drive, flat tone, tanh, no bias: identity at low level
{
    vb::ModuleSpec m;
    m.type = vb::SAT;
    m.p = { 0, 32768, 0, 0, 0, 0 };
    m.mix = 65535;
    return m;
}

vb::LaneSpec lane (std::vector<vb::ModuleSpec> modules = {}, const std::function<void (vb::LaneSpec&)>& set = {})
{
    vb::LaneSpec l;
    l.nModules = static_cast<std::uint8_t> (modules.size());
    for (std::size_t k = 0; k < modules.size(); ++k)
        l.slots[k] = modules[k];
    if (set)
        set (l);
    return l;
}

vb::ChainSpec chainOf (std::vector<vb::LaneSpec> lanes)
{
    vb::ChainSpec c;
    c.lanes = std::move (lanes);
    return c;
}

std::unique_ptr<vbdsp::Engine> engineFor (const vb::ChainSpec& spec, float mix = 1.0f)
{
    auto e = std::make_unique<vbdsp::Engine>();
    e->setSpec (spec);
    e->prepare (SR, BLOCK);
    e->setMix (mix);
    e->reset();
    return e;
}

// Steady-state gain in dB of a stereo signal (L, R) through the chain, measured over the last half second.
double gainDb (const vb::ChainSpec& spec, double freq, double peakDbfs, bool antiPhase = false)
{
    auto e = engineFor (spec);
    const auto n = static_cast<std::size_t> (SR);
    const auto inL = sine (n, freq, SR, peakDbfs);
    auto inR = inL;
    if (antiPhase)
        for (auto& v : inR)
            v = -v;
    const auto out = render (*e, inL, inR, BLOCK);
    const auto from = n / 2;
    return toDb (rms (out.L.data() + from, n - from) / rms (inL.data() + from, n - from));
}

// Digital 2nd-order Butterworth (TPT/bilinear, cutoff prewarped): analog response at warped frequencies.
double butterworthDb (double f, double fc, bool highpass)
{
    const auto warp = [] (double x) { return std::tan (pi * x / SR); };
    const double r = highpass ? warp (fc) / warp (f) : warp (f) / warp (fc);
    return -10.0 * std::log10 (1.0 + r * r * r * r);
}

std::size_t argmaxAbs (const std::vector<float>& x)
{
    std::size_t k = 0;
    for (std::size_t i = 1; i < x.size(); ++i)
        if (std::abs (x[i]) > std::abs (x[k]))
            k = i;
    return k;
}
} // namespace

TEST_CASE ("D10 band: 12 dB/oct HP at fc/2.4 and LP at fc*2.4", "[D10]")
{
    const std::uint16_t u = 32768;
    const double fc = 200.0 * std::pow (4000.0 / 200.0, u / 65535.0);
    const auto spec = chainOf ({ lane ({}, [&] (vb::LaneSpec& l) { l.band = 1; l.bandCenter = u; }) });
    for (double f : { fc, fc / 2.4, fc * 2.4, fc / 6.0, fc * 6.0 })
    {
        const double expected = butterworthDb (f, fc / 2.4, true) + butterworthDb (f, fc * 2.4, false);
        INFO ("f " << f << " expected " << expected);
        CHECK (std::abs (gainDb (spec, f, -20.0) - expected) < 0.5);
    }
}

TEST_CASE ("D10 drive: small-signal gain is g^0.5 (+2 dB per step)", "[D10]")
{
    for (std::uint8_t d : { 1, 2, 3 })
    {
        const auto spec = chainOf ({ lane ({}, [&] (vb::LaneSpec& l) { l.drive = d; }) });
        CHECK (std::abs (gainDb (spec, 1000.0, -50.0) - 2.0 * d) < 0.5);
    }
}

TEST_CASE ("D10 split: two equal halves sum to +3 dB", "[D10]")
{
    const auto serial = chainOf ({ lane ({ satLinear(), satLinear() }) });
    const auto split = chainOf ({ lane ({ satLinear(), satLinear() }, [] (vb::LaneSpec& l) { l.split = 1; }) });
    CHECK (std::abs (gainDb (serial, 1000.0, -40.0)) < 0.5);
    CHECK (std::abs (gainDb (split, 1000.0, -40.0) - 20.0 * std::log10 (2.0 * 0.707)) < 0.5);
}

TEST_CASE ("D10 chain (&): the consumed lane leaves the sum", "[D10]")
{
    const auto parallel = chainOf ({ lane(), lane() });
    const auto chained = chainOf ({ lane(), lane ({}, [] (vb::LaneSpec& l) { l.chain = 1; }) });
    const auto chainOnLaneZero = chainOf ({ lane ({}, [] (vb::LaneSpec& l) { l.chain = 1; }) });
    CHECK (std::abs (gainDb (parallel, 1000.0, -20.0) - 20.0 * std::log10 (2.0 / std::sqrt (2.0))) < 0.5);
    CHECK (std::abs (gainDb (chained, 1000.0, -20.0)) < 0.5);
    CHECK (std::abs (gainDb (chainOnLaneZero, 1000.0, -20.0)) < 0.5);
}

TEST_CASE ("D10 duck: up to 3 dB per step at full depth (-12 dBFS and above)", "[D10]")
{
    for (std::uint8_t d : { 1, 3 })
    {
        const auto spec = chainOf ({ lane ({}, [&] (vb::LaneSpec& l) { l.duck = d; }) });
        CHECK (std::abs (gainDb (spec, 1000.0, -6.0) + 3.0 * d) < 0.5);
    }
}

TEST_CASE ("D10 lane mix: mixAbs and mixSteps set the wet weight w (clamped 0.10..1.0)", "[D10]")
{
    // DELAY with full mix (m = 0.5): the impulse's first sample is in*(1-w) + w*in*(1-m) = 1 - w*m.
    vb::ModuleSpec delay;
    delay.type = vb::DELAY;
    delay.p = { 0, 0, 0, 0, 0, 0 };
    delay.mix = 65535;
    struct Case { std::uint8_t mixAbs; std::int8_t steps; double w; };
    for (const auto c : { Case { 255, 0, 1.0 }, Case { 50, 0, 0.5 }, Case { 255, -1, 0.85 }, Case { 50, 2, 0.8 }, Case { 5, 0, 0.10 }, Case { 0, -3, 0.10 } })
    {
        INFO ("mixAbs " << int (c.mixAbs) << " steps " << int (c.steps));
        auto e = engineFor (chainOf ({ lane ({ delay }, [&] (vb::LaneSpec& l) { l.mixAbs = c.mixAbs; l.mixSteps = c.steps; }) }));
        std::vector<float> in (4096, 0.0f);
        in[0] = 0.5f;
        const auto out = render (*e, in, in, BLOCK);
        const double y0 = out.L[static_cast<std::size_t> (e->latencySamples())] / 0.5;
        CHECK (std::abs (y0 - (1.0 - c.w * 0.5)) < 0.01);
    }
}

TEST_CASE ("D10 level: 2 dB per step", "[D10]")
{
    for (std::int8_t lv : { -3, -1, 2 })
    {
        const auto spec = chainOf ({ lane ({}, [&] (vb::LaneSpec& l) { l.level = lv; }) });
        CHECK (std::abs (gainDb (spec, 1000.0, -20.0) - 2.0 * lv) < 0.5);
    }
}

TEST_CASE ("D10 width: side gain 1.5 dB per step, mid untouched", "[D10]")
{
    for (std::int8_t sp : { -2, 3 })
    {
        const auto spec = chainOf ({ lane ({}, [&] (vb::LaneSpec& l) { l.space = sp; }) });
        CHECK (std::abs (gainDb (spec, 1000.0, -20.0, true) - 1.5 * sp) < 0.5);
        CHECK (std::abs (gainDb (spec, 1000.0, -20.0, false)) < 0.5);
    }
}

TEST_CASE ("D7 split with SAT in one half and 2/3/4 chained lanes each with 2 SAT stay aligned", "[D7]")
{
    vbdsp::Engine probe;
    probe.prepare (SR, BLOCK);
    const auto lFixed = static_cast<std::size_t> (probe.latencySamples());

    vb::ModuleSpec idle = satLinear();
    idle.type = vb::DELAY; // zero-latency module; mix low so the impulse peak stays at the direct sound
    idle.mix = 0;
    std::vector<vb::ChainSpec> specs = { chainOf ({ lane ({ satLinear(), idle }, [] (vb::LaneSpec& l) { l.split = 1; }) }) };
    for (int k = 2; k <= 4; ++k)
    {
        std::vector<vb::LaneSpec> lanes;
        for (int i = 0; i < k; ++i)
            lanes.push_back (lane ({ satLinear(), satLinear() }, [&] (vb::LaneSpec& l) { l.chain = i > 0 ? 1 : 0; }));
        specs.push_back (chainOf (lanes));
    }
    for (std::size_t s = 0; s < specs.size(); ++s)
    {
        INFO ("spec " << s);
        auto e = engineFor (specs[s], 0.5f);
        CHECK (static_cast<std::size_t> (e->latencySamples()) == lFixed);
        std::vector<float> in (8192, 0.0f);
        in[0] = 0.1f;
        const auto out = render (*e, in, in, BLOCK);
        CHECK (argmaxAbs (out.L) == lFixed);
    }
}
