#pragma once
#include "../Module.h"

#include <vector>

namespace vbdsp
{
// COMB (ID 10): feedback comb tuned to an equal-tempered pitch, tanh and a damping low-pass in the loop.
class Comb final : public Module
{
public:
    explicit Comb (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return static_cast<int> (sr / 55.0) + 2; } // longest period

private:
    double pitchHz = 220.0, sr = 48000.0, lpA = 0.0, lp[2] {};
    float feedback = 0.5f, dampHz = 6000, polarity = 1, mix = 0.4f;
    std::vector<float> line[2];
    int size = 0, writePos = 0;
    Smooth cents, mixSmooth;
    Memo hzMemo;
};

// FREEZE (ID 9): captures a slice every `period` lengths and replays it `repeats` times (5 ms windows).
// shortcut: not locked to host transport position, upgrade if users need bar-aligned stutter.
class Freeze final : public Module
{
public:
    explicit Freeze (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float lengthMs = 200, mix = 0.5f;
    int period = 4, repeats = 2;
    double sr = 48000.0;
    std::vector<float> slice[2]; // loop buffer, 2 s
    long long pos = 0;           // samples since the current cycle started
    int len = 0;                 // slice length of the current cycle
    Smooth mixSmooth;
};
} // namespace vbdsp
