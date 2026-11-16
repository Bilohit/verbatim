#pragma once
// Engine: dry delay, lane sum, Mix, Output, SafetyClip, meters, and lock-free chain swap with a
// 30 ms crossfade (docs/02-spec.md sections 2 and 5).
#include "ChainSpec.h"
#include "Resolve.h"
#include "Lane.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace vbdsp
{
struct Chain;

class Engine
{
public:
    Engine();
    ~Engine();

    // Non-audio threads. prepare() builds the active chain synchronously.
    void prepare (double sampleRate, int maxBlock);
    void setSpec (const vb::ChainSpec& spec); // no overrides
    void setResolved (const vb::ResolvedChain& chain);
    void collectGarbage();
    void reset();

    // Audio thread.
    void process (float* L, float* R, int n);

    void setMix (float mix01) { mixTarget.store (mix01); }
    void setOutputDb (float db) { outputDbTarget.store (db); }
    void setMacro (float m01) { macroTarget.store (m01); }
    void setMacroScalingEnabled (bool on) { macroScaling.store (on); } // test hook for D8
    // Host tempo (bpm <= 0: unknown, 120 used) and play state; call before process() each block.
    void setTransport (double bpm, bool playing) { bpmTarget.store (bpm > 0.0 ? bpm : 120.0); playingNow.store (playing); }
    void setBypass (bool b) { bypassTarget.store (b ? 1.0f : 0.0f); } // ramps to the delayed dry signal

    int latencySamples() const { return lFixed; }
    bool swapInProgress() const { return fading.load(); }
    int swapWarmupSamples() const; // silent pre-roll of the active chain before its crossfade
    int nanGuardCount() const { return nanTotal.load(); }
    int retireOverflows() const { return deferredTakes.load(); } // pending chains left waiting because the retire FIFO was full

    struct Meters
    {
        std::atomic<float> laneRms[4] {};
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
        std::atomic<bool> clip { false };
    } meters;

private:
    Chain* build (const vb::ResolvedChain& chain) const;

    std::mutex buildMutex; // non-audio threads only
    vb::ResolvedChain lastChain;
    double sampleRate = 0.0;
    int maxBlock = 0;
    int lFixed = 0;

    Chain* active = nullptr;
    Chain* old = nullptr;
    std::atomic<Chain*> pending { nullptr };
    std::atomic<bool> fading { false };
    int fadePos = 0, fadeLen = 0;

    static constexpr int retireCapacity = 8;
    Chain* retire[retireCapacity] {};
    std::atomic<int> retireWrite { 0 }, retireRead { 0 };

    std::vector<float> inL, inR, dryL, dryR, outA_L, outA_R, outB_L, outB_R, mixRamp, gainRamp, dryBufL, dryBufR;
    int dryPos = 0;
    std::atomic<float> mixTarget { 1.0f }, outputDbTarget { 0.0f }, bypassTarget { 0.0f }, macroTarget { 0.5f };
    std::atomic<bool> macroScaling { true };
    std::atomic<double> bpmTarget { 120.0 };
    std::atomic<bool> playingNow { false };
    bool wasPlaying = false;
    double bpmCurrent = 120.0;
    int fadeInPos = 0, fadeInLen = 1; // 5 ms fade-in after a transport reset
    void transportReset();            // audio-thread
    float mixCurrent = 1.0f, gainCurrent = 1.0f, bypassCurrent = 0.0f, macroCurrent = 0.5f;
    std::vector<float> bypassRamp, macroRamp;
    std::atomic<int> nanTotal { 0 }, deferredTakes { 0 };
};
} // namespace vbdsp
