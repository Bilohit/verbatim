#include "Phaser.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
Phaser::Phaser (const vb::ModuleSpec& spec)
{
    rate = expMap (spec.p[0], 0.05f, 4.0f);
    depth = lin (spec.p[1], 0.3f, 1.0f);
    centreHz = expMap (spec.p[2], 300.0f, 3000.0f);
    feedback = lin (spec.p[3], 0.0f, 0.7f); // narrowed from 0.85 at T14 (D4: linear JUCE loop peaked at 5.9x)
    mix = lin (spec.mix, 0.3f, 0.6f);
}

void Phaser::prepare (double sampleRate, int maxBlock)
{
    phaser.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlock), 2 });
    phaser.setRate (rate);
    phaser.setDepth (depth);
    phaser.setFeedback (feedback);
    phaser.setMix (1.0f);
    dryL.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    dryR.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    mixSmooth.prepare (sampleRate, 5.0);
    reset();
}

void Phaser::reset()
{
    phaser.reset();
    mixSmooth.reset();
}

void Phaser::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    // Mod: centre frequency +-1 octave, clamped to the mapped range.
    phaser.setCentreFrequency (std::clamp (centreHz * std::pow (2.0f, ctx.mod), 300.0f, 3000.0f));
    std::copy (L, L + n, dryL.begin());
    std::copy (R, R + n, dryR.begin());
    float* ch[2] = { L, R };
    juce::dsp::AudioBlock<float> block (ch, 2, static_cast<std::size_t> (n));
    phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
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
