// T05: D1, D2 (SAT, DELAY), D6, D7, D11, R1 and R3 (2 seeds), R4, RT2. docs/06-test-plan.md.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Engine.h"
#include "Module.h"
#include "SafetyClip.h"

#include "Catalog.h"
#include "Generate.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <cstring>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

vb::ModuleSpec mod (std::uint8_t type, std::uint16_t u = 32768, std::uint16_t mix = 32768)
{
    vb::ModuleSpec m;
    m.type = type;
    m.p.fill (u);
    m.mix = mix;
    return m;
}

// One lane holding exactly the given modules (bypasses generator ranges on purpose).
vb::ChainSpec laneOf (std::vector<vb::ModuleSpec> modules)
{
    vb::ChainSpec spec;
    vb::LaneSpec l;
    l.nModules = static_cast<std::uint8_t> (modules.size());
    for (std::size_t k = 0; k < modules.size(); ++k)
        l.slots[k] = modules[k];
    spec.lanes.push_back (l);
    return spec;
}

void prepared (vbdsp::Engine& e, const vb::ChainSpec& spec, float mix = 1.0f)
{
    e.setSpec (spec);
    e.prepare (SR, BLOCK);
    e.setMix (mix);
    e.reset(); // start smoothers at their targets
}

double maxDiff (const std::vector<float>& a, const std::vector<float>& b, std::size_t from = 0)
{
    double m = 0;
    for (std::size_t i = from; i < a.size(); ++i)
        m = std::max (m, std::abs (static_cast<double> (a[i]) - b[i]));
    return m;
}

std::vector<float> delayed (const std::vector<float>& x, int d)
{
    std::vector<float> y (x.size(), 0.0f);
    for (std::size_t i = static_cast<std::size_t> (d); i < x.size(); ++i)
        y[i] = x[i - static_cast<std::size_t> (d)];
    return y;
}

std::vector<float> impulse (std::size_t n, float a)
{
    std::vector<float> x (n, 0.0f);
    x[0] = a;
    return x;
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

TEST_CASE ("D11 SafetyClip: transparent below -3 dBFS, never above 1.0", "[D11]")
{
    const double knee = std::pow (10.0, -3.0 / 20.0);
    for (double db = -20.0; db <= 12.0; db += 0.25)
    {
        const double a = std::pow (10.0, db / 20.0);
        for (int i = 0; i < 480; ++i)
        {
            const float x = static_cast<float> (a * std::sin (2.0 * 3.141592653589793 * i / 480.0));
            const float y = vbdsp::safetyClip (x);
            REQUIRE (std::abs (y) <= 1.0f);
            if (std::abs (x) < knee)
                REQUIRE (std::abs (static_cast<double> (y) - x) < 1e-5);
        }
    }
}

TEST_CASE ("D1 empty text at Mix 100 equals the delayed dry signal", "[D1]")
{
    vbdsp::Engine e;
    prepared (e, vb::generate (""));
    const auto in = pinkNoise (48000, 1, -18.0);
    const auto out = render (e, in, in, BLOCK);
    CHECK (toDb (maxDiff (out.L, delayed (in, e.latencySamples()))) < -120.0);
    CHECK (toDb (maxDiff (out.R, delayed (in, e.latencySamples()))) < -120.0);
}

TEST_CASE ("D1 Mix 0 with a chain equals the delayed dry signal", "[D1]")
{
    vbdsp::Engine e;
    prepared (e, vb::generate ("hello"), 0.0f);
    const auto in = pinkNoise (48000, 2, -18.0);
    const auto out = render (e, in, in, BLOCK);
    CHECK (toDb (maxDiff (out.L, delayed (in, e.latencySamples()))) < -120.0);
}

TEST_CASE ("D1 lane with zero modules equals the DC-blocked delayed dry signal", "[D1]")
{
    vbdsp::Engine e;
    prepared (e, laneOf ({}));
    const auto in = pinkNoise (48000, 3, -18.0);
    const auto out = render (e, in, in, BLOCK);

    // Reference: delayed dry through the lane's 5 Hz one-pole DC blocker (ruling at T05).
    const auto d = delayed (in, e.latencySamples());
    const double r = std::exp (-2.0 * 3.141592653589793 * 5.0 / SR);
    std::vector<float> ref (d.size());
    double x1 = 0, y1 = 0;
    for (std::size_t i = 0; i < d.size(); ++i)
    {
        const double y = d[i] - x1 + r * y1;
        x1 = d[i];
        y1 = y;
        ref[i] = static_cast<float> (y);
    }
    CHECK (toDb (maxDiff (out.L, ref)) < -120.0);
}

TEST_CASE ("D2 SAT and DELAY impulse responses decay with no DC", "[D2]")
{
    struct Case { std::uint8_t type; double maxTailSeconds; };
    for (const auto c : { Case { vb::SAT, 1.0 }, Case { vb::DELAY, 5.0 } })
    {
        INFO ("type " << int (c.type));
        vbdsp::Engine e;
        prepared (e, laneOf ({ mod (c.type) }));
        const auto in = impulse (static_cast<std::size_t> (10 * SR), 0.5f);
        const auto out = render (e, in, in, BLOCK);

        double tail = 0;
        for (std::size_t i = static_cast<std::size_t> (c.maxTailSeconds * SR); i < out.L.size(); ++i)
        {
            REQUIRE (std::isfinite (out.L[i]));
            tail = std::max (tail, static_cast<double> (std::abs (out.L[i])));
        }
        CHECK (toDb (tail) < -90.0);
        double mean = 0;
        for (std::size_t i = out.L.size() - static_cast<std::size_t> (SR); i < out.L.size(); ++i)
            mean += out.L[i];
        CHECK (toDb (std::abs (mean / SR)) < -80.0);
    }
}

TEST_CASE ("D6 SAT aliasing below -60 dBc at 24 dB drive", "[D6]")
{
    // drive u=65535 (24 dB), tone flat, shape tanh, no bias, full mix.
    vb::ModuleSpec m = mod (vb::SAT, 0, 65535);
    m.p[0] = 65535;
    m.p[1] = 32768;
    auto sat = vbdsp::makeModule (m, 0, 0);
    sat->prepare (SR, BLOCK);

    constexpr int order = 16, N = 1 << order;
    const std::size_t settle = 4800;
    auto L = sine (settle + N, 5000.0, SR, -6.0);
    auto R = L;
    vbdsp::ModCtx ctx;
    for (std::size_t s = 0; s < L.size(); s += BLOCK)
    {
        const int n = static_cast<int> (std::min<std::size_t> (BLOCK, L.size() - s));
        sat->process (L.data() + s, R.data() + s, n, ctx);
    }

    std::vector<float> fft (2 * N, 0.0f);
    for (int i = 0; i < N; ++i) // 4-term Blackman-Harris window
    {
        const double t = 2.0 * 3.141592653589793 * i / (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        fft[static_cast<std::size_t> (i)] = static_cast<float> (L[settle + static_cast<std::size_t> (i)] * w);
    }
    juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (fft.data());

    const double binHz = SR / N;
    double fund = 0, alias = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * binHz;
        const double mag = fft[static_cast<std::size_t> (k)];
        const double nearestHarm = std::round (f / 5000.0) * 5000.0;
        const bool harmonic = nearestHarm > 0 && std::abs (f - nearestHarm) < 8 * binHz;
        if (std::abs (f - 5000.0) < 8 * binHz)
            fund = std::max (fund, mag);
        else if (! harmonic && f > 20.0 && f < 20000.0)
            alias = std::max (alias, mag);
    }
    INFO ("worst alias " << toDb (alias / fund) << " dBc");
    CHECK (toDb (alias / fund) < -60.0);
}

TEST_CASE ("D7 latency: L_FIXED reported and impulses align with dry", "[D7]")
{
    vbdsp::Engine probe;
    probe.prepare (SR, BLOCK);
    const int lFixed = probe.latencySamples();
    CHECK (lFixed == 8 * vbdsp::satLatencySamples());
    CHECK (vbdsp::satLatencySamples() > 0);

    vb::ModuleSpec sat = mod (vb::SAT, 32768, 65535);
    sat.p[0] = 0;  // drive 0 dB
    sat.p[2] = 0;  // tanh
    sat.p[3] = 0;  // no bias
    const vb::ModuleSpec pass = mod (vb::VERB); // pass-through until T15

    for (const auto& spec : { laneOf ({ pass }), laneOf ({ sat }), laneOf ({ sat, sat }) })
    {
        vbdsp::Engine e;
        prepared (e, spec, 0.5f);
        CHECK (e.latencySamples() == lFixed);
        const auto in = impulse (8192, 0.1f);
        const auto out = render (e, in, in, BLOCK);
        CHECK (argmaxAbs (out.L) == static_cast<std::size_t> (lFixed));
    }
}

TEST_CASE ("R1 bit identity: fresh engine, same seed, rendered twice (2 seeds) and RT2", "[R1][RT2]")
{
    const auto in = pinkNoise (static_cast<std::size_t> (10 * SR), 7, -18.0);
    for (const char* text : { "hello", "dark tape, slow echo!" })
    {
        INFO (text);
        std::vector<float> first;
        for (int run = 0; run < 2; ++run)
        {
            vbdsp::Engine e;
            prepared (e, vb::generate (text));
            resetAllocationCount();
            const auto out = render (e, in, in, BLOCK);
            CHECK (allocationsInAudio() == 0);
            if (run == 0)
                first = out.L;
            else
                CHECK (std::memcmp (first.data(), out.L.data(), first.size() * sizeof (float)) == 0);
        }
    }
}

TEST_CASE ("R3 reset: second render after reset() equals the first bit for bit", "[R3]")
{
    const auto in = pinkNoise (static_cast<std::size_t> (5 * SR), 8, -18.0);
    for (const char* text : { "hello", "dark tape, slow echo!" })
    {
        vbdsp::Engine e;
        prepared (e, vb::generate (text));
        const auto a = render (e, in, in, BLOCK);
        e.reset();
        const auto b = render (e, in, in, BLOCK);
        CHECK (std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0);
        CHECK (std::memcmp (a.R.data(), b.R.data(), a.R.size() * sizeof (float)) == 0);
    }
}

TEST_CASE ("R4 swap: no click and a 30 ms fade, no allocation on the audio thread", "[R4][RT2]")
{
    vbdsp::Engine e;
    prepared (e, vb::generate ("hello"));
    const auto in = sine (static_cast<std::size_t> (3 * SR), 100.0, SR, -12.0);
    const std::size_t swapAt = static_cast<std::size_t> (SR); // a block boundary (48000 = 93.75 blocks -> next)
    std::size_t swapBlockStart = 0;
    std::vector<std::pair<std::size_t, bool>> fading;

    resetAllocationCount();
    const auto out = render (e, in, in, BLOCK, [&] (std::size_t start) {
        if (start >= swapAt && swapBlockStart == 0)
        {
            e.setSpec (vb::generate ("tape echo"));
            swapBlockStart = start;
        }
        if (swapBlockStart != 0)
            fading.emplace_back (start, e.swapInProgress());
        e.collectGarbage();
    });
    CHECK (allocationsInAudio() == 0);

    // Fade length: the swap starts at swapBlockStart and is over by 30 ms + one block.
    const auto fadeLen = static_cast<std::size_t> (std::lround (0.03 * SR));
    std::size_t endedAt = 0;
    for (const auto& [start, f] : fading)
        if (start > swapBlockStart && ! f && endedAt == 0)
            endedAt = start;
    REQUIRE (endedAt > 0);
    // The audible fade follows the new chain's silent warm-up (ruling at T16). Observed at block starts,
    // so the swap ended somewhere in (endedAt - BLOCK, endedAt].
    const auto warmup = static_cast<std::size_t> (e.swapWarmupSamples());
    CHECK (endedAt - swapBlockStart >= warmup + fadeLen);
    CHECK (endedAt - BLOCK - swapBlockStart < warmup + fadeLen + BLOCK);

    auto maxStep = [&] (std::size_t from, std::size_t to) {
        double m = 0;
        for (std::size_t i = from + 1; i < to; ++i)
            m = std::max (m, std::abs (static_cast<double> (out.L[i]) - out.L[i - 1]));
        return m;
    };
    const double steady = std::max (maxStep (swapBlockStart - static_cast<std::size_t> (SR / 2), swapBlockStart),
                                    maxStep (swapBlockStart + static_cast<std::size_t> (SR / 2), out.L.size()));
    const double across = maxStep (swapBlockStart, swapBlockStart + static_cast<std::size_t> (e.swapWarmupSamples()) + fadeLen + BLOCK);
    const double signalRms = rms (out.L.data(), out.L.size());
    INFO ("steady " << steady << " across " << across << " rms " << signalRms);
    CHECK (across <= steady + std::pow (10.0, -30.0 / 20.0) * signalRms);
}
