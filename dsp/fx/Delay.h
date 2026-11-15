#pragma once
#include "../Module.h"

#include <vector>

namespace vbdsp
{
// DELAY (catalog ID 3): stereo, ping-pong or tape; tanh and tone filters in the loop.
class Delay final : public Module
{
public:
    explicit Delay (const vb::ModuleSpec& spec);
    void prepare (double sampleRate, int maxBlock) override;
    void reset() override;
    void process (float* L, float* R, int n, const ModCtx& ctx) override;

private:
    float readLinear (const std::vector<float>& line, double delay) const;
    float readLagrange (const std::vector<float>& line, double delay) const;

    float timeMs = 0, feedback = 0, toneHz = 0, mix = 0;
    int mode = 0; // 0 stereo, 1 ping-pong, 2 tape
    double sr = 48000.0;
    std::vector<float> line[2];
    int size = 0, writePos = 0;
    double lpA = 0, hpA = 0, lp[2] {}, hp[2] {};
    double wowPhase = 0, wowInc = 0;
};
} // namespace vbdsp
