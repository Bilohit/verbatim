#include "Verb.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
Verb::Verb (const vb::ModuleSpec& spec)
{
    params.roomSize = lin (spec.p[0], 0.3f, 0.95f);
    params.damping = lin (spec.p[1], 0.8f, 0.1f); // brighter as u rises
    predelayMs = lin (spec.p[2], 0.0f, 80.0f);
    params.width = lin (spec.p[3], 0.5f, 1.0f);
    params.wetLevel = 1.0f / 3.0f; // juce::Reverb scales wet by 3 internally; this makes the wet path unity
    params.dryLevel = 0.0f;
    params.freezeMode = 0.0f;
    mix = lin (spec.mix, 0.10f, 0.45f);
}

void Verb::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.setParameters (params);
    preLen = std::max (1, static_cast<int> (std::lround (predelayMs * 0.001 * sampleRate)));
    for (auto& p : pre)
        p.assign (static_cast<std::size_t> (preLen), 0.0f);
    dryL.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    dryR.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    mixSmooth.prepare (sampleRate, 5.0);
    reset();
}

void Verb::reset()
{
    reverb.reset();
    for (auto& p : pre)
        std::fill (p.begin(), p.end(), 0.0f);
    prePos = 0;
    mixSmooth.reset();
}

void Verb::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    std::copy (L, L + n, dryL.begin());
    std::copy (R, R + n, dryR.begin());
    for (int i = 0; i < n; ++i) // predelay
    {
        const auto p = static_cast<std::size_t> (prePos);
        std::swap (L[i], pre[0][p]);
        std::swap (R[i], pre[1][p]);
        prePos = prePos + 1 == preLen ? 0 : prePos + 1;
    }
    reverb.processStereo (L, R, n);
    // Mod range +-0.15 on mix, clamped to the mapped range.
    const float mixTarget = effectiveMix (std::clamp (mix + 0.15f * ctx.mod, 0.10f, 0.45f), ctx);
    for (int i = 0; i < n; ++i)
    {
        const float w = mixSmooth.next (mixTarget);
        const auto k = static_cast<std::size_t> (i);
        L[i] = dryL[k] + (L[i] * trim - dryL[k]) * w;
        R[i] = dryR[k] + (R[i] * trim - dryR[k]) * w;
    }
}
} // namespace vbdsp
