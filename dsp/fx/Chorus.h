#pragma once
#include "../Module.h"

#include <juce_dsp/juce_dsp.h>

namespace vbdsp
{
// CHORUS (ID 5) and FLANGE (ID 15): juce::dsp::Chorus run fully wet; the module mix is applied here so
// Macro 0 is exactly dry. FLANGE is the same processor with a short centre delay (catalog).
class Chorus final : public Module
{
public:
    explicit Chorus (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return static_cast<int> ((delayMs + 10.0f * depth + 2.0f) * 0.001 * sr); } // JUCE: centre + 20 * depth * 0.5 ms

private:
    bool flange = false;
    float rate = 1, depth = 0.3f, delayMs = 10, feedback = 0, mix = 0.5f;
    double sr = 48000.0;
    juce::dsp::Chorus<float> chorus;
    std::vector<float> dryL, dryR;
    Smooth mixSmooth;
};
} // namespace vbdsp
