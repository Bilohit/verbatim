#pragma once
#include "../Module.h"
#include "Rng.h"

namespace vbdsp
{
// CRUSH (catalog ID 1): sample-and-hold with PRNG jitter, then fractional-bit quantizer. Aliasing intended.
class Crush final : public Module
{
public:
    Crush (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float bits = 8, rateHz = 8000, jitter = 0, mix = 1;
    std::uint64_t seed = 0;
    double sr = 48000.0, holdLeft = 0.0;
    float held[2] {};
    vb::SplitMix64 rng { 0 };
    Smooth octaves, mixSmooth;
    Memo rateMemo;
};
} // namespace vbdsp
