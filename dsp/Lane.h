#pragma once
#include "Module.h"

#include <atomic>
#include <memory>
#include <vector>

namespace vbdsp
{
// One lane: modules in series, lane mix, DC block, NaN guard, pad to L_FIXED (docs/03-dsp-catalog.md 1.1).
class Lane
{
public:
    Lane (const vb::LaneSpec& spec, const std::vector<vb::ModuleSpec>& modules, int slotBase = 0);

    void prepare (double sampleRate, int maxBlock, int lFixed); // allocates
    void reset();
    // audio-thread
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    int latencySamples() const;
    int nanCount() const { return nans; }
    float rms() const { return meterRms; }

private:
    vb::LaneSpec spec;
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<float> padL, padR;
    int padLen = 0, padPos = 0;
    double dcR = 0.0;
    double dcX1[2] {}, dcY1[2] {};
    double meterCoef = 0.0, meterSq = 0.0;
    float meterRms = 0.0f;
    int nans = 0;
};
} // namespace vbdsp
