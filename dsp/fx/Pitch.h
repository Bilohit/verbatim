#pragma once
#include "../Module.h"

#include <vector>

namespace vbdsp
{
// PITCH (ID 12): two delay taps half a window apart, sliding at (1 - ratio) samples per sample, sin^2
// crossfaded; zero reported latency. Feedback goes back into the line through tanh.
class Pitch final : public Module
{
public:
    explicit Pitch (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return windowSamples + 2; }

private:
    int semitones = 0;
    float detuneCents = 0, windowMs = 50, feedback = 0, mix = 0.3f;
    double sr = 48000.0, phase = 0.0;
    int windowSamples = 0, size = 0, writePos = 0;
    std::vector<float> line[2];
    float last[2] {};
    Smooth cents, mixSmooth;
    Memo ratioMemo;
};
} // namespace vbdsp
