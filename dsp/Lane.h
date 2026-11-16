#pragma once
#include "Mod.h"
#include "Module.h"

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

namespace vbdsp
{
// One lane (docs/03-dsp-catalog.md section 1.1): band, drive, modules (serial or split), lane mix,
// duck, level, width, DC block, NaN guard, pad.
class Lane
{
public:
    Lane (const vb::LaneSpec& spec, const std::vector<vb::ModuleSpec>& modules, int slotBase = 0);

    void prepare (double sampleRate, int maxBlock); // allocates; latencySamples() is valid afterwards
    void setPad (int samples);                      // allocates; delay added after the lane stages
    void reset();
    // Per-block values from the engine: smoothed Macro per sample, tempo.
    struct BlockCtx
    {
        const float* macro = nullptr;
        bool macroScaling = true;
        double bpm = 120.0;
    };

    // audio-thread
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const BlockCtx& ctx);

    int latencySamples() const; // module latency (split: the slower half)
    bool takesPreviousLane() const { return spec.chain != 0; }
    int nanCount() const { return nans; }
    float rms() const { return meterRms; }

private:
    struct Ring // fixed stereo delay line
    {
        std::vector<float> l, r;
        int len = 0, pos = 0;
        void setDelay (int samples);
        void clear();
        void process (float* L, float* R, int n);
    };

    void runModules (std::size_t from, std::size_t to, float* L, float* R, int n);
    void processModules (float* L, float* R, int n, int offset); // on the control grid; offset into the block
    void processModulesCell (float* L, float* R, int n);         // serial or split, one ctx
    int latencyOf (std::size_t from, std::size_t to) const;

    vb::LaneSpec spec;
    std::vector<std::unique_ptr<Module>> modules;
    LaneMod mod;
    int gridPos = 0;      // samples since the last control tick
    float cellMacro = 0.5f;
    const BlockCtx* block = nullptr;
    std::size_t splitAt = 0; // 0 = serial; else modules [0, splitAt) and [splitAt, n) run in parallel

    juce::dsp::StateVariableTPTFilter<float> bandHp, bandLp;
    float driveGain = 1.0f, driveNorm = 1.0f;
    float wet = 1.0f;
    float levelGain = 1.0f, sideGain = 1.0f;
    double duckAtt = 0.0, duckRel = 0.0, duckEnv = 0.0;

    std::vector<float> rawL, rawR, splitL, splitR;
    Ring dryDelay, splitDelay, pad;

    // Lane feedback (>): modules output -> 10 ms -> HP 80 Hz -> LP 6 kHz -> gain -> tanh -> modules input.
    std::vector<float> fbL, fbR;
    int fbLen = 0, fbPos = 0;
    float fbGain = 0.0f;
    double fbHpA = 0.0, fbLpA = 0.0, fbHp[2] {}, fbLp[2] {};
    double govAtt = 0.0, govRel = 0.0, govIn = 0.0, govFb = 0.0; // feedback governor envelopes
    bool splitDelayOnA = false;

    double dcR = 0.0;
    double dcX1[2] {}, dcY1[2] {};
    double meterCoef = 0.0, meterSq = 0.0;
    float meterRms = 0.0f;
    int nans = 0;
};
} // namespace vbdsp
