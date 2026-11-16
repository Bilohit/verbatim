#include "Crush.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
Crush::Crush (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot)
    : seed (laneSeed ^ static_cast<std::uint64_t> (slot + 1))
{
    bits = lin (spec.p[0], 4.0f, 12.0f);
    rateHz = expMap (spec.p[1], 2000.0f, 24000.0f);
    jitter = lin (spec.p[2], 0.0f, 0.3f);
    mix = lin (spec.mix, 0.3f, 1.0f);
}

void Crush::prepare (double sampleRate, int)
{
    sr = sampleRate;
    octaves.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Crush::reset()
{
    rng = vb::SplitMix64 { seed };
    holdLeft = 0.0;
    held[0] = held[1] = 0.0f;
    octaves.reset();
    mixSmooth.reset();
}

void Crush::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const float step = std::pow (2.0f, 1.0f - bits); // full scale +-1 split into 2^bits levels
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        // Mod range +-1 octave on the hold rate, clamped to the mapped range.
        const double rate = rateMemo (octaves.next (ctx.mod), [this] (float oct) { return std::clamp (rateHz * std::pow (2.0, static_cast<double> (oct)), 2000.0, 24000.0); });
        holdLeft -= 1.0;
        if (holdLeft <= 0.0)
        {
            held[0] = L[i];
            held[1] = R[i];
            const double u = static_cast<double> (rng.next() >> 11) / 9007199254740992.0 * 2.0 - 1.0;
            holdLeft += sr / rate * (1.0 + jitter * u);
        }
        const float m = mixSmooth.next (mixTarget);
        const float wetL = std::round (held[0] / step) * step, wetR = std::round (held[1] / step) * step;
        L[i] += (wetL * trim - L[i]) * m;
        R[i] += (wetR * trim - R[i]) * m;
    }
}
} // namespace vbdsp
