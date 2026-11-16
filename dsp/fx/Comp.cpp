#include "Comp.h"

#include <algorithm>

namespace vbdsp
{
Comp::Comp (const vb::ModuleSpec& spec)
{
    thresholdDb = lin (spec.p[0], -30.0f, -8.0f);
    ratio = expMap (spec.p[1], 2.0f, 10.0f);
    attackMs = expMap (spec.p[2], 1.0f, 50.0f);
    releaseMs = expMap (spec.p[3], 40.0f, 400.0f);
    mix = lin (spec.mix, 0.5f, 1.0f);
    makeup = dbToGain (-thresholdDb * (1.0f - 1.0f / ratio) * 0.5f); // auto make-up from the base threshold
}

void Comp::prepare (double sampleRate, int maxBlock)
{
    comp.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlock), 2 });
    comp.setRatio (ratio);
    comp.setAttack (attackMs);
    comp.setRelease (releaseMs);
    comp.setThreshold (thresholdDb);
    mixSmooth.prepare (sampleRate, 5.0);
    reset();
}

void Comp::reset()
{
    comp.reset();
    mixSmooth.reset();
}

void Comp::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    comp.setThreshold (std::clamp (thresholdDb + 6.0f * ctx.mod, -30.0f, -8.0f)); // mod range +-6 dB
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        const float w = mixSmooth.next (mixTarget);
        const float l = comp.processSample (0, L[i]) * makeup, r = comp.processSample (1, R[i]) * makeup;
        L[i] += (l * trim - L[i]) * w;
        R[i] += (r * trim - R[i]) * w;
    }
}
} // namespace vbdsp
