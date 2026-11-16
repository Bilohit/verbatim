#pragma once
#include "../Module.h"

namespace vbdsp
{
// FILTER (catalog ID 2): TPT state-variable filter (Zavalishin), LP/HP/BP/notch, 12 or 24 dB/oct.
// Ruling at T13: hand-rolled because juce::dsp::StateVariableTPTFilter has no notch output; same algorithm.
class Filter final : public Module
{
public:
    explicit Filter (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float cutoffHz = 1000, q = 0.707f, mix = 1;
    int mode = 0, stages = 1;
    double sr = 48000.0;
    double s1[2][2] {}, s2[2][2] {}; // [stage][channel]
    Smooth octaves, mixSmooth;
};
} // namespace vbdsp
