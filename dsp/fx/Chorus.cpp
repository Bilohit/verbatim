#include "Chorus.h"

#include "Catalog.h"

#include <algorithm>

namespace vbdsp
{
Chorus::Chorus (const vb::ModuleSpec& spec) : flange (spec.type == vb::FLANGE)
{
    if (flange)
    {
        rate = expMap (spec.p[0], 0.05f, 2.0f);
        depth = lin (spec.p[1], 0.2f, 0.8f);
        delayMs = lin (spec.p[2], 1.0f, 8.0f);
        feedback = lin (spec.p[3], -0.7f, 0.7f); // narrowed from 0.85 at T14 (D4: peak 4.4x)
        mix = lin (spec.mix, 0.3f, 0.5f);
    }
    else
    {
        rate = expMap (spec.p[0], 0.1f, 3.0f);
        depth = lin (spec.p[1], 0.1f, 0.6f);
        delayMs = lin (spec.p[2], 7.0f, 25.0f);
        feedback = lin (spec.p[3], -0.3f, 0.3f);
        mix = lin (spec.mix, 0.3f, 0.6f);
    }
}

void Chorus::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    chorus.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlock), 2 });
    chorus.setRate (rate);
    chorus.setFeedback (feedback);
    chorus.setMix (1.0f);
    dryL.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    dryR.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    mixSmooth.prepare (sampleRate, 5.0);
    reset();
}

void Chorus::reset()
{
    chorus.reset();
    mixSmooth.reset();
}

void Chorus::process (float* L, float* R, int n, const ModCtx& ctx)
{
    // Mod: CHORUS depth +-0.2, FLANGE centre delay +-1 ms; clamped to the mapped ranges. Set once per call
    // (ctx.mod is constant per 32-sample control cell, so the result does not depend on host block size).
    const float m = ctx.mod;
    if (flange)
    {
        chorus.setDepth (depth);
        chorus.setCentreDelay (std::clamp (delayMs + m, 1.0f, 8.0f));
    }
    else
    {
        chorus.setDepth (std::clamp (depth + 0.2f * m, 0.1f, 0.6f));
        chorus.setCentreDelay (delayMs);
    }
    std::copy (L, L + n, dryL.begin());
    std::copy (R, R + n, dryR.begin());
    float* ch[2] = { L, R };
    juce::dsp::AudioBlock<float> block (ch, 2, static_cast<std::size_t> (n));
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        const float w = mixSmooth.next (mixTarget);
        const auto k = static_cast<std::size_t> (i);
        L[i] = dryL[k] + (L[i] * trim - dryL[k]) * w;
        R[i] = dryR[k] + (R[i] * trim - dryR[k]) * w;
    }
}
} // namespace vbdsp
