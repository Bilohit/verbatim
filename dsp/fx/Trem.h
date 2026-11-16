#pragma once
#include "../Module.h"
#include "Rng.h"

#include <vector>

namespace vbdsp
{
// TREM (ID 7): gain LFO (sine, triangle, square with 3 ms slew); pan 0 = tremolo, 1 = auto-pan.
class Trem final : public Module
{
public:
    explicit Trem (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float rateHz = 2, depth = 0.5f, pan = 0, mix = 0.8f;
    int shape = 0;
    double sr = 48000.0, phase = 0.0, square = 0.0;
    Smooth depthSmooth, mixSmooth;
};

// WOW (ID 8): modulated delay line, base 15 ms, wow LFO + 9 Hz flutter with PRNG jitter, age low-pass.
class Wow final : public Module
{
public:
    Wow (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return static_cast<int> ((15.0 + depthMs + 1.0 + 1.1 * flutterMs) * 0.001 * sr); }

private:
    float rateHz = 1, depthMs = 2, flutterMs = 0.2f, ageHz = 8000, mix = 0.9f;
    std::uint64_t seed = 0;
    vb::SplitMix64 rng { 0 };
    double sr = 48000.0, wowPhase = 0.0, flutterPhase = 0.0, flutterAmp = 1.0, lpA = 0.0, lp[2] {};
    std::vector<float> line[2];
    int size = 0, writePos = 0;
    Smooth depthSmooth, mixSmooth;
};

// RING (ID 11): ring modulator with a sine or triangle carrier.
class Ring final : public Module
{
public:
    explicit Ring (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float freqHz = 200, mix = 0.4f;
    int wave = 0;
    double sr = 48000.0, phase = 0.0;
    Smooth octaves, mixSmooth;
    Memo freqMemo;
};
} // namespace vbdsp
