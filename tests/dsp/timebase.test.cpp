// T12: D9 time base for DELAY and LFO A (docs/03-dsp-catalog.md 1.3).
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Mod.h"
#include "Module.h"

#include <cmath>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

// Note length T = 4/timeDiv beats; halved until it fits maxMs.
double noteSamples (int timeDiv, double bpm, double maxMs)
{
    double ms = 4.0 / timeDiv * 60000.0 / bpm;
    while (ms > maxMs)
        ms *= 0.5;
    return ms * 0.001 * SR;
}
} // namespace

TEST_CASE ("D9 DELAY time follows 1/timeDiv at host tempo", "[D9]")
{
    for (int div : { 4, 8, 16 })
        for (double bpm : { 90.0, 120.0, 174.0 })
        {
            INFO ("timeDiv " << div << " bpm " << bpm);
            vb::ChainSpec spec;
            vb::LaneSpec l;
            l.nModules = 1;
            l.timeDiv = static_cast<std::uint8_t> (div);
            l.slots[0].type = vb::DELAY;
            l.slots[0].p = { 0, 0, 65535, 0, 0, 0 }; // feedback 0, bright loop, stereo
            l.slots[0].mix = 65535;
            spec.lanes.push_back (l);

            vbdsp::Engine e;
            e.setSpec (spec);
            e.prepare (SR, BLOCK);
            e.setTransport (bpm, true);
            e.reset();
            std::vector<float> in (static_cast<std::size_t> (3 * SR), 0.0f);
            in[0] = 0.5f;
            const auto out = render (e, in, in, BLOCK, [&] (std::size_t) { e.setTransport (bpm, true); });

            // Echo = the largest sample after the direct sound.
            const auto direct = static_cast<std::size_t> (e.latencySamples());
            std::size_t peak = direct + 8;
            for (std::size_t i = direct + 8; i < out.L.size(); ++i)
                if (std::abs (out.L[i]) > std::abs (out.L[peak]))
                    peak = i;
            const double expected = noteSamples (div, bpm, 2000.0);
            CHECK (std::abs (static_cast<double> (peak - direct) - expected) <= 1.0);
        }
}

TEST_CASE ("D9 LFO A period = 4 notes of 1/timeDiv at host tempo", "[D9]")
{
    for (int div : { 4, 8, 16 })
        for (double bpm : { 90.0, 120.0, 174.0 })
        {
            INFO ("timeDiv " << div << " bpm " << bpm);
            vb::LaneSpec l;
            l.nModules = 1;
            l.timeDiv = static_cast<std::uint8_t> (div);
            l.lfoA = { 0, 0, 65535, 0 };
            vbdsp::LaneMod mod;
            mod.prepare (l, 1, SR);
            const double expected = 4.0 * noteSamples (div, bpm, 1e12);
            const int nTicks = static_cast<int> (6.0 * expected / vbdsp::CONTROL_TICK);
            std::vector<double> ups;
            float prev = 0.0f;
            for (int t = 0; t < nTicks; ++t)
            {
                mod.tick (bpm, 0.5f);
                const float v = mod.valueFor (0);
                if (t > 0 && prev < 0.0f && v >= 0.0f) // interpolate the zero crossing between ticks
                    ups.push_back ((t - 1 + prev / (prev - v)) * vbdsp::CONTROL_TICK);
                prev = v;
            }
            REQUIRE (ups.size() >= 3);
            const double period = (ups.back() - ups.front()) / static_cast<double> (ups.size() - 1);
            CHECK (std::abs (period - expected) <= 1.0);
        }
}

TEST_CASE ("D9 TREM period = T at host tempo", "[D9]")
{
    for (int div : { 4, 8, 16 })
        for (double bpm : { 90.0, 120.0, 174.0 })
        {
            INFO ("timeDiv " << div << " bpm " << bpm);
            vb::ModuleSpec spec;
            spec.type = vb::TREM;
            spec.p = { 32768, 65535, 0, 0, 0, 0 }; // sine, full depth, in-phase
            spec.mix = 65535;
            auto m = vbdsp::makeModule (spec, 0, 0);
            m->prepare (SR, BLOCK);
            vbdsp::ModCtx ctx;
            ctx.bpm = bpm;
            ctx.timeDiv = static_cast<std::uint8_t> (div);
            const double expected = noteSamples (div, bpm, 2000.0);
            const auto n = static_cast<std::size_t> (6.0 * expected);
            std::vector<float> L (n, 0.5f), R (n, 0.5f);
            for (std::size_t s = 0; s < n; s += BLOCK)
                m->process (L.data() + s, R.data() + s, static_cast<int> (std::min<std::size_t> (BLOCK, n - s)), ctx);
            double mean = 0;
            for (auto v : L)
                mean += v;
            mean /= static_cast<double> (n);
            std::vector<double> ups;
            for (std::size_t i = 1; i < n; ++i)
            {
                const double a = L[i - 1] - mean, b = L[i] - mean;
                if (a < 0.0 && b >= 0.0)
                    ups.push_back (static_cast<double> (i - 1) + a / (a - b));
            }
            REQUIRE (ups.size() >= 3);
            const double period = (ups.back() - ups.front()) / static_cast<double> (ups.size() - 1);
            CHECK (std::abs (period - expected) <= 1.0);
        }
}
