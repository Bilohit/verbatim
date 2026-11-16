#include "Filter.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

Filter::Filter (const vb::ModuleSpec& spec)
{
    mode = disc (spec.p[2], 4);
    cutoffHz = mode == 0 ? expMap (spec.p[0], 200.0f, 12000.0f)
             : mode == 1 ? expMap (spec.p[0], 40.0f, 2000.0f)
                         : expMap (spec.p[0], 150.0f, 6000.0f);
    q = expMap (spec.p[1], 0.5f, 8.0f);
    stages = disc (spec.p[3], 2) + 1;
    mix = lin (spec.mix, 0.7f, 1.0f);
}

void Filter::prepare (double sampleRate, int)
{
    sr = sampleRate;
    octaves.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Filter::reset()
{
    for (int s = 0; s < 2; ++s)
        for (int c = 0; c < 2; ++c)
            s1[s][c] = s2[s][c] = 0.0;
    octaves.reset();
    mixSmooth.reset();
}

void Filter::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const double r2 = 1.0 / q; // 2R
    // 24 dB mode: the second stage is Butterworth, so the resonant peak stays at Q (a cascade of two
    // Q stages reached +34 dB and broke S1; ruling at T20).
    const double r2b = std::sqrt (2.0);
    // Band-pass passes about 0.7 octave of a broadband signal; +13 dB make-up keeps it near unity
    // loudness (DSP note tuned at T20 for D12).
    const double bpMakeup = 4.4668359215096318; // +13 dB
    const float mixTarget = effectiveMix (mix, ctx);
    float* io[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        // Mod range +-2 octaves, clamped 20 Hz .. 0.45 fs.
        const double g = gMemo (octaves.next (2.0f * ctx.mod), [this] (float oct) {
            const double fc = std::clamp (cutoffHz * std::pow (2.0, static_cast<double> (oct)), 20.0, 0.45 * sr);
            return std::tan (pi * fc / sr);
        });
        const double h = 1.0 / (1.0 + r2 * g + g * g), hb = 1.0 / (1.0 + r2b * g + g * g);
        const float m = mixSmooth.next (mixTarget);
        for (int c = 0; c < 2; ++c)
        {
            const double dry = io[c][i];
            double x = dry;
            for (int s = 0; s < stages; ++s)
            {
                const double rs = s == 0 ? r2 : r2b;
                const double hp = (x - (rs + g) * s1[s][c] - s2[s][c]) * (s == 0 ? h : hb);
                const double bp = g * hp + s1[s][c];
                s1[s][c] = g * hp + bp;
                const double lp = g * bp + s2[s][c];
                s2[s][c] = g * bp + lp;
                x = mode == 0 ? lp : mode == 1 ? hp : mode == 2 ? rs * bp : x - rs * bp;
            }
            if (mode == 2)
                x *= bpMakeup; // once, after the cascade
            io[c][i] = static_cast<float> (dry + (x * trim - dry) * m);
        }
    }
}
} // namespace vbdsp
