#pragma once
#include "../Module.h"

#include <juce_dsp/juce_dsp.h>

namespace vbdsp
{
// PHASER (ID 6): juce::dsp::Phaser run fully wet; module mix applied here.
class Phaser final : public Module
{
public:
    explicit Phaser (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float rate = 1, depth = 0.5f, centreHz = 1000, feedback = 0, mix = 0.5f;
    juce::dsp::Phaser<float> phaser;
    std::vector<float> dryL, dryR;
    Smooth mixSmooth;
};
} // namespace vbdsp
