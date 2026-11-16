// T20: D12 calibration (docs/03-dsp-catalog.md section 3).
#include <catch2/catch_test_macros.hpp>

#include "harness.h"

#include "Catalog.h"
#include "Module.h"

#include <cstdio>

using namespace harness;

TEST_CASE ("D12 each effect at u = 32768 with -18 dBFS pink noise outputs -18 +- 3 dBFS RMS", "[D12]")
{
    constexpr double SR = 48000.0;
    constexpr int BLOCK = 512;
    const auto n = static_cast<std::size_t> (4 * SR);
    const auto in = pinkNoise (n, 51, -18.0);
    for (std::uint8_t t = 0; t < vb::EFFECT_COUNT; ++t)
    {
        vb::ModuleSpec spec;
        spec.type = t;
        spec.p.fill (32768);
        spec.mix = 32768;
        auto m = vbdsp::makeModule (spec, 0x5EED, 0);
        m->prepare (SR, BLOCK);
        auto L = in, R = in;
        const vbdsp::ModCtx ctx;
        for (std::size_t s = 0; s < n; s += BLOCK)
            m->process (L.data() + s, R.data() + s, static_cast<int> (std::min<std::size_t> (BLOCK, n - s)), ctx);
        const auto from = n / 2; // skip onset and warm-up
        const double db = toDb (std::sqrt (0.5 * (rms (L.data() + from, n - from) * rms (L.data() + from, n - from)
                                               + rms (R.data() + from, n - from) * rms (R.data() + from, n - from))));
        std::printf ("D12 %-6s %+.2f dBFS\n", vb::catalog[t].name, db);
        INFO ("type " << vb::catalog[t].name << " " << db << " dBFS");
        CHECK (std::abs (db + 18.0) <= 3.0);
    }
}
