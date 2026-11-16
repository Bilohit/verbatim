// T13: D3 FILTER response (docs/03-dsp-catalog.md, FILTER). Ruling at T13: the response is checked against the exact
// digital response of the specified design (TPT SVF, cascaded for 24 dB); the -3 dB point and slope are checked
// where they are defined (Q = 0.707 low/high-pass).
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Module.h"
#include "Trims.h"

#include <cmath>

using namespace harness;

namespace
{
constexpr double SR = 48000.0;
constexpr double pi = 3.141592653589793;

// Mode ranges from the catalog: LP exp(200, 12000), HP exp(40, 2000), BP and notch exp(150, 6000).
std::uint16_t cutoffU (int mode, double hz)
{
    const double lo = mode == 0 ? 200.0 : mode == 1 ? 40.0 : 150.0;
    const double hi = mode == 0 ? 12000.0 : mode == 1 ? 2000.0 : 6000.0;
    return static_cast<std::uint16_t> (std::lround (65535.0 * std::log (hz / lo) / std::log (hi / lo)));
}

double cutoffHz (int mode, std::uint16_t u)
{
    const double lo = mode == 0 ? 200.0 : mode == 1 ? 40.0 : 150.0;
    const double hi = mode == 0 ? 12000.0 : mode == 1 ? 2000.0 : 6000.0;
    return lo * std::pow (hi / lo, u / 65535.0);
}

double stageDb (int mode, double w, double q)
{
    const double den = std::sqrt ((1 - w * w) * (1 - w * w) + (w / q) * (w / q));
    const double mag = mode == 0 ? 1.0 / den : mode == 1 ? w * w / den : mode == 2 ? (w / q) / den : std::abs (1 - w * w) / den;
    return 20.0 * std::log10 (std::max (mag, 1e-12));
}

// 24 dB mode = a stage at Q followed by a Butterworth stage (T20). Band-pass carries +13 dB make-up (T20).
double theoryDb (int mode, double f, double fc, double q, int stages)
{
    const double w = std::tan (pi * f / SR) / std::tan (pi * fc / SR);
    const double db = stageDb (mode, w, q) + (stages == 2 ? stageDb (mode, w, 1.0 / std::sqrt (2.0)) : 0.0);
    return db + (mode == 2 ? 13.0 : 0.0);
}

// Steady-state gain of the FILTER module at full mix for a sine at f.
double measuredDb (const vb::ModuleSpec& spec, double f)
{
    auto m = vbdsp::makeModule (spec, 0, 0);
    m->prepare (SR, 512);
    const auto n = static_cast<std::size_t> (SR / 2);
    auto L = sine (n, f, SR, -30.0);
    auto R = L;
    const auto in = L;
    vbdsp::ModCtx ctx;
    ctx.macro = 1.0f; // full mix: m = mix + (1-mix)*0.75 with mix u = 65535 -> 1.0
    for (std::size_t s = 0; s < n; s += 512)
        m->process (L.data() + s, R.data() + s, static_cast<int> (std::min<std::size_t> (512, n - s)), ctx);
    const auto from = n / 2;
    // Relative to the constant FILTER output trim (dsp/Trims.h): D3 checks the response shape.
    return toDb (rms (L.data() + from, n - from) / rms (in.data() + from, n - from)) - vbdsp::trimDb[vb::FILTER];
}

vb::ModuleSpec filter (int mode, std::uint16_t cutU, std::uint16_t qU, int slope)
{
    vb::ModuleSpec m;
    m.type = vb::FILTER;
    m.p = { cutU, qU, static_cast<std::uint16_t> (mode * 16384 + 8192), static_cast<std::uint16_t> (slope * 32768 + 16384), 0, 0 };
    m.mix = 65535;
    return m;
}
} // namespace

TEST_CASE ("D3 FILTER matches the TPT SVF response in every mode, cutoff, Q and slope", "[D3]")
{
    const std::uint16_t qButter = 8192, qTwo = 32768; // Q = exp(0.5, 8) -> 0.707, 2.0
    for (int mode = 0; mode < 4; ++mode)
        for (double target : { 300.0, 700.0, 1500.0 })
            for (auto qU : { qButter, qTwo })
                for (int slope = 0; slope < 2; ++slope)
                {
                    const auto cu = cutoffU (mode, target);
                    const double fc = cutoffHz (mode, cu), q = 0.5 * std::pow (16.0, qU / 65535.0);
                    const auto spec = filter (mode, cu, qU, slope);
                    for (double ratio : { 0.25, 0.5, 0.8, 1.25, 2.0, 4.0 })
                    {
                        if (mode == 3 && std::abs (ratio - 1.0) < 0.3)
                            continue; // the notch depth near fc is not a stable measurement
                        const double expected = theoryDb (mode, fc * ratio, fc, q, slope + 1);
                        if (expected < -60.0)
                            continue;
                        INFO ("mode " << mode << " fc " << fc << " Q " << q << " slope " << slope << " f " << fc * ratio);
                        CHECK (std::abs (measuredDb (spec, fc * ratio) - expected) < 1.0);
                    }
                }
}

TEST_CASE ("D3 FILTER Butterworth LP/HP: -3 dB at cutoff (+-5 %) and slope one octave past cutoff", "[D3]")
{
    for (int mode = 0; mode < 2; ++mode)
        for (double target : { 300.0, 700.0, 1500.0 })
        {
            const auto cu = cutoffU (mode, target);
            const double fc = cutoffHz (mode, cu);
            const auto spec = filter (mode, cu, 8192, 0);
            INFO ("mode " << mode << " fc " << fc);
            // -3 dB point: search on a fine grid around fc.
            double best = 0, bestErr = 1e9;
            for (double r = 0.9; r <= 1.1; r += 0.005)
            {
                const double err = std::abs (measuredDb (spec, fc * r) + 3.0103);
                if (err < bestErr)
                {
                    bestErr = err;
                    best = r;
                }
            }
            CHECK (std::abs (best - 1.0) <= 0.05);
            // Slope over the octave beyond one octave past the cutoff.
            const double a = mode == 0 ? 2.0 : 0.5, b = mode == 0 ? 4.0 : 0.25;
            const double slope = measuredDb (spec, fc * a) - measuredDb (spec, fc * b);
            CHECK (std::abs (slope - 12.0) <= 1.5);
        }
}
