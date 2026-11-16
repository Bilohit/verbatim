// M3 per-effect suite: D1 (Macro 0 = delayed dry), D2 (impulse tail), D5 (denormals) for every implemented effect.
// Each effect task adds its types to `implemented`.
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Engine.h"
#include "Module.h"

#include <chrono>
#include <cmath>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

struct Fx
{
    std::uint8_t type;
    double maxTailSeconds; // D2: impulse response below -90 dBFS after this, params at u = 32768
};

const std::vector<Fx> implemented = {
    { vb::SAT, 1.0 }, { vb::DELAY, 5.0 }, { vb::FILTER, 1.0 }, { vb::CRUSH, 1.0 },
    { vb::CHORUS, 1.0 }, { vb::PHASER, 2.0 }, { vb::FLANGE, 2.0 },
    { vb::VERB, 6.0 }, { vb::COMP, 1.0 },
};

vb::ModuleSpec mid (std::uint8_t type)
{
    vb::ModuleSpec m;
    m.type = type;
    m.p.fill (32768);
    m.mix = 32768;
    return m;
}

vb::ChainSpec single (std::uint8_t type)
{
    vb::ChainSpec c;
    vb::LaneSpec l;
    l.seed = 0x1234;
    l.nModules = 1;
    l.slots[0] = mid (type);
    c.lanes.push_back (l);
    return c;
}
} // namespace

TEST_CASE ("D1 every implemented effect at Macro 0 equals its delayed dry input", "[D1]")
{
    for (const auto& fx : implemented)
    {
        INFO ("type " << vb::catalog[fx.type].name);
        auto m = vbdsp::makeModule (mid (fx.type), 0x1234, 0);
        m->prepare (SR, BLOCK);
        auto L = pinkNoise (24000, 31, -18.0);
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

TEST_CASE ("D2 every implemented effect: impulse decays below -90 dBFS, no DC", "[D2]")
{
    for (const auto& fx : implemented)
    {
        INFO ("type " << vb::catalog[fx.type].name);
        vbdsp::Engine e;
        e.setSpec (single (fx.type));
        e.prepare (SR, BLOCK);
        e.reset();
        std::vector<float> in (static_cast<std::size_t> (10 * SR), 0.0f);
        in[0] = 0.5f;
        const auto out = render (e, in, in, BLOCK);
        double tail = 0, mean = 0;
        for (std::size_t i = 0; i < out.L.size(); ++i)
        {
            REQUIRE (std::isfinite (out.L[i]));
            if (i >= static_cast<std::size_t> (fx.maxTailSeconds * SR))
                tail = std::max (tail, static_cast<double> (std::abs (out.L[i])));
            if (i >= out.L.size() - static_cast<std::size_t> (SR))
                mean += out.L[i];
        }
        CHECK (toDb (tail) < -90.0);
        CHECK (toDb (std::abs (mean / SR)) < -80.0);
    }
}

TEST_CASE ("D5 denormals: 60 s of 1e-30 noise costs at most 20 % more than normal signal", "[D5]")
{
    const auto n = static_cast<std::size_t> (60 * SR);
    const auto normal = pinkNoise (n, 32, -18.0);
    auto tiny = normal;
    for (auto& v : tiny)
        v *= 1e-30f / 0.125f;
    tiny.resize (n + static_cast<std::size_t> (5 * SR), 0.0f); // then silence
    for (const auto& fx : implemented)
    {
        INFO ("type " << vb::catalog[fx.type].name);
        auto timeOf = [&] (const std::vector<float>& in) {
            vbdsp::Engine e;
            e.setSpec (single (fx.type));
            e.prepare (SR, BLOCK);
            e.reset();
            const auto t0 = std::chrono::steady_clock::now();
            render (e, in, in, BLOCK);
            const auto secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            return secs / static_cast<double> (in.size());
        };
        const double base = std::min (timeOf (normal), timeOf (normal));
        const double denorm = std::min (timeOf (tiny), timeOf (tiny));
        INFO ("per-sample time normal " << base << " tiny " << denorm);
        CHECK (denorm <= base * 1.2);
    }
}

TEST_CASE ("Every implemented effect is active: full-mix output differs from dry", "[effects]")
{
    for (const auto& fx : implemented)
    {
        INFO ("type " << vb::catalog[fx.type].name);
        auto spec = mid (fx.type);
        spec.mix = 65535;
        auto m = vbdsp::makeModule (spec, 0x1234, 0);
        m->prepare (SR, BLOCK);
        auto L = pinkNoise (48000, 33, -18.0);
        auto R = L;
        const auto in = L;
        vbdsp::ModCtx ctx;
        for (std::size_t s = 0; s < L.size(); s += BLOCK)
            m->process (L.data() + s, R.data() + s, static_cast<int> (std::min<std::size_t> (BLOCK, L.size() - s)), ctx);
        const auto d = static_cast<std::size_t> (m->latencySamples());
        double diff = 0;
        for (std::size_t i = d; i < L.size(); ++i)
            diff += (static_cast<double> (L[i]) - in[i - d]) * (static_cast<double> (L[i]) - in[i - d]);
        CHECK (toDb (std::sqrt (diff / static_cast<double> (L.size() - d))) > -40.0);
    }
}
