#pragma once
#include "../Module.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace vbdsp
{
// VERB (ID 4): own predelay line into juce::Reverb (the Freeverb core juce::dsp::Reverb wraps), fully wet;
// the module mix is applied here. shortcut: Freeverb core, replace with an FDN if the tail quality is criticised.
class Verb final : public Module
{
public:
    explicit Verb (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return preLen + static_cast<int> (1617.0 / 44100.0 * sr); } // predelay + longest comb

private:
    juce::Reverb::Parameters params;
    float predelayMs = 0, mix = 0.2f;
    juce::Reverb reverb;
    std::vector<float> pre[2], dryL, dryR;
    int preLen = 0, prePos = 0;
    double sr = 48000.0;
    Smooth mixSmooth;
};
} // namespace vbdsp
