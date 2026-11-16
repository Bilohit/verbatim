#pragma once
#include "../Module.h"
#include "Rng.h"

#include <array>
#include <vector>

namespace vbdsp
{
// GRAIN (ID 13): up to 16 preallocated Hann grains read from a 1 s buffer of the mono input, with position
// spray, random pan, a pitch set and reverse probability. All randomness from the module PRNG.
class Grain final : public Module
{
public:
    Grain (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;
    int warmupSamples() const override { return static_cast<int> ((2.0 * sizeMs + sprayMs) * 0.001 * sr); }

private:
    struct Voice
    {
        bool active = false;
        double start = 0.0, ratio = 1.0; // buffer position at t = 0, playback ratio
        int length = 0, t = 0;
        bool reverse = false;
        float gainL = 0.0f, gainR = 0.0f;
    };

    double uniform01() { return static_cast<double> (rng.next() >> 11) / 9007199254740992.0; }
    void spawn (float sprayMsNow);

    float sizeMs = 60, density = 12, sprayMs = 100, sprayPan = 0.2f, reverseProb = 0, mix = 0.4f;
    int pitchSet = 0;
    std::uint64_t seed = 0;
    vb::SplitMix64 rng { 0 };
    double sr = 48000.0, untilNext = 0.0, norm = 1.0;
    std::vector<float> buffer;
    int size = 0, writePos = 0;
    std::array<Voice, 16> voices {};
    Smooth spraySmooth, mixSmooth;
};
} // namespace vbdsp
