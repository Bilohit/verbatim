#pragma once
// Module interface and parameter mapping helpers (docs/03-dsp-catalog.md section 0).
#include "ChainSpec.h"

#include <cmath>
#include <cstdint>
#include <memory>

namespace vbdsp
{
inline float unit (std::uint16_t u) { return static_cast<float> (u) / 65535.0f; }
inline float lin (std::uint16_t u, float lo, float hi) { return lo + (hi - lo) * unit (u); }
inline float expMap (std::uint16_t u, float lo, float hi) { return lo * std::pow (hi / lo, unit (u)); }
inline int disc (std::uint16_t u, int n) { return static_cast<int> ((static_cast<std::uint32_t> (u) * static_cast<std::uint32_t> (n)) >> 16); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// Per-block context for a module. Modulation and macro arrive with T11, tempo with T12.
struct ModCtx
{
    float mod = 0.0f;
    float macro = 0.5f;
    double bpm = 120.0;
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0; // may allocate
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, const ModCtx& ctx) = 0; // audio-thread
    virtual int latencySamples() const { return 0; }
};

// Builds the module for a spec. Effects not yet implemented are pass-through modules.
std::unique_ptr<Module> makeModule (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot);

// Integer latency of SAT's 4x oversampler (L_SAT), independent of sample rate.
int satLatencySamples();
} // namespace vbdsp
