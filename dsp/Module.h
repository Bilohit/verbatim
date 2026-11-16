#pragma once
// Module interface and parameter mapping helpers (docs/03-dsp-catalog.md section 0).
#include "ChainSpec.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

namespace vbdsp
{
inline float unit (std::uint16_t u) { return static_cast<float> (u) / 65535.0f; }
inline float lin (std::uint16_t u, float lo, float hi) { return lo + (hi - lo) * unit (u); }
inline float expMap (std::uint16_t u, float lo, float hi) { return lo * std::pow (hi / lo, unit (u)); }
inline int disc (std::uint16_t u, int n) { return static_cast<int> ((static_cast<std::uint32_t> (u) * static_cast<std::uint32_t> (n)) >> 16); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// Context for one module call: constant over one 32-sample control cell.
struct ModCtx
{
    float mod = 0.0f;          // M in [-1, 1], already scaled by Macro
    float macro = 0.5f;
    bool macroScaling = true;  // false only in the D8 reference build
    double bpm = 120.0;        // smoothed host tempo (120 when unknown)
    std::uint8_t timeDiv = 0;  // lane time base: 0 = free, else notes of 1/timeDiv
};

// Note length T = 4/timeDiv beats at bpm, halved until it fits maxMs (docs/03-dsp-catalog.md 1.3).
inline float noteMs (const ModCtx& ctx, float maxMs)
{
    float ms = static_cast<float> (4.0 / ctx.timeDiv * 60000.0 / ctx.bpm);
    while (ms > maxMs)
        ms *= 0.5f;
    return ms;
}

// Effective module mix under Macro (docs/03-dsp-catalog.md 1.4). Macro 0.5 returns mix unchanged.
inline float effectiveMix (float mix, const ModCtx& ctx)
{
    if (! ctx.macroScaling)
        return mix;
    const float m = ctx.macro;
    return m <= 0.5f ? mix * (2.0f * m) : mix + (1.0f - mix) * (2.0f * m - 1.0f) * 0.75f;
}

// Caches an expensive function of a smoothed value: recomputes only when the value changes (P1, T20).
struct Memo
{
    float key = std::numeric_limits<float>::quiet_NaN();
    double value = 0.0;
    template <typename F>
    double operator() (float k, F&& f)
    {
        if (k != key)
        {
            key = k;
            value = f (k);
        }
        return value;
    }
};

// Per-sample one-pole smoother; starts at its first target, so a constant target is exact.
struct Smooth
{
    float cur = 0.0f, coef = 1.0f;
    bool started = false;
    void prepare (double sampleRate, double ms) { coef = static_cast<float> (1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate))); started = false; }
    void reset() { started = false; }
    float next (float target)
    {
        if (! started)
        {
            cur = target;
            started = true;
        }
        cur += coef * (target - cur);
        return cur;
    }
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0; // may allocate
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, const ModCtx& ctx) = 0; // audio-thread
    virtual int latencySamples() const { return 0; }
    // Time until internal delay lines give continuous output after reset; a new chain stays silent this long.
    virtual int warmupSamples() const { return 0; }

    float trim = 1.0f; // constant wet-path output trim from dsp/Trims.h (set by makeModule)
};

// Builds the module for a spec. Effects not yet implemented are pass-through modules.
std::unique_ptr<Module> makeModule (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot);

// Integer latency of SAT's 4x oversampler (L_SAT), independent of sample rate.
int satLatencySamples();
} // namespace vbdsp
