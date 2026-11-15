#pragma once
#include "../Module.h"

#include <juce_dsp/juce_dsp.h>

namespace vbdsp
{
// SAT (catalog ID 0): drive, tilt tone, 4 shapes, bias; 4x oversampled (owner ruling at T05).
class Sat final : public Module
{
public:
    explicit Sat (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int latencySamples() const override { return latency; }

    static constexpr std::size_t oversamplingStages = 2; // 2^2 = 4x

private:
    float shape (float x) const;

    juce::dsp::Oversampling<float> os { 2, oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    int latency = 0;
    int shapeType = 0;
    float driveDb = 0, gain = 1, biasDc = 0, biasOut = 0, mix = 1, trim = 1, tiltLow = 1, tiltHigh = 1;
    double dcR = 0, tiltA = 0;
    double dcX1[2] {}, dcY1[2] {}, tiltLp[2] {};
    std::vector<float> ring[2];    // delays the dry path by the oversampler latency
    std::vector<float> dryOut[2];  // delayed dry for the current block
    int ringPos = 0;
};
} // namespace vbdsp
