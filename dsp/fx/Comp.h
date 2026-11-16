#pragma once
#include "../Module.h"

#include <juce_dsp/juce_dsp.h>

namespace vbdsp
{
// COMP (ID 14): juce::dsp::Compressor with auto make-up gain; module mix applied here.
class Comp final : public Module
{
public:
    explicit Comp (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float thresholdDb = -20, ratio = 4, attackMs = 10, releaseMs = 100, mix = 0.75f, makeup = 1;
    juce::dsp::Compressor<float> comp;
    Smooth mixSmooth;
};
} // namespace vbdsp
