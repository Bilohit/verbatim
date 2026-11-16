#pragma once
// Per-lane modulation sources (docs/03-dsp-catalog.md section 1.2): LFO A, LFO B (random walk), ENV.
// Values are computed on a 32-sample control grid counted from chain start (docs/02-spec.md C6).
#include "ChainSpec.h"
#include "Rng.h"

#include <array>

namespace vbdsp
{
inline constexpr int CONTROL_TICK = 32;

class LaneMod
{
public:
    void prepare (const vb::LaneSpec& spec, int moduleCount, double sampleRate);
    void reset();

    void trackInput (const float* L, const float* R, int n); // ENV follower, every sample
    void tick (double bpm, float macro, bool macroScaling = true); // advance one 32-sample tick
    float valueFor (int moduleIndex) const { return values[static_cast<std::size_t> (moduleIndex)]; }

private:
    vb::LaneSpec spec;
    int count = 0;
    double sr = 48000.0;
    std::array<float, 8> values {};

    double phaseA = 0.0, square = 0.0, slewStep = 0.0;
    vb::SplitMix64 rng { 0 };
    double prevB = 0.0, nextB = 0.0, posB = 0.0, periodB = 1.0;
    double envMs = 0.0, envAtt = 0.0, envRel = 0.0;
};
} // namespace vbdsp
