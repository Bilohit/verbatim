#include "Trem.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;

double triangle (double ph) { return ph < 0.25 ? 4.0 * ph : ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0; }

double uniform (vb::SplitMix64& r) { return static_cast<double> (r.next() >> 11) / 9007199254740992.0 * 2.0 - 1.0; }
} // namespace

// ---- TREM ----
Trem::Trem (const vb::ModuleSpec& spec)
{
    rateHz = expMap (spec.p[0], 0.5f, 12.0f);
    depth = lin (spec.p[1], 0.2f, 0.9f);
    shape = disc (spec.p[2], 3);
    pan = lin (spec.p[3], 0.0f, 1.0f);
    mix = lin (spec.mix, 0.7f, 1.0f);
}

void Trem::prepare (double sampleRate, int)
{
    sr = sampleRate;
    depthSmooth.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Trem::reset()
{
    phase = 0.0;
    square = 0.0;
    depthSmooth.reset();
    mixSmooth.reset();
}

void Trem::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    // Period T when the lane has a time base (max 2 s, the slowest free rate), else the free rate.
    const double hz = ctx.timeDiv != 0 ? 1000.0 / noteMs (ctx, 2000.0f) : rateHz;
    const double slew = 2.0 / (0.003 * sr);
    const float depthTarget = std::clamp (depth + 0.2f * ctx.mod, 0.2f, 0.9f); // mod range +-0.2
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        double s;
        if (shape == 0)
            s = std::sin (2.0 * pi * phase);
        else if (shape == 1)
            s = triangle (phase);
        else
        {
            square += std::clamp ((phase < 0.5 ? 1.0 : -1.0) - square, -slew, slew);
            s = square;
        }
        phase += hz / sr;
        phase -= std::floor (phase);

        const float d = depthSmooth.next (depthTarget), m = mixSmooth.next (mixTarget);
        const double sR = s * (1.0 - 2.0 * pan); // pan 1: right channel in opposite phase
        const float gL = static_cast<float> (1.0 - d * (0.5 + 0.5 * s));
        const float gR = static_cast<float> (1.0 - d * (0.5 + 0.5 * sR));
        L[i] += (L[i] * gL * trim - L[i]) * m;
        R[i] += (R[i] * gR * trim - R[i]) * m;
    }
}

// ---- WOW ----
Wow::Wow (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot) : seed (laneSeed ^ static_cast<std::uint64_t> (slot + 1))
{
    rateHz = expMap (spec.p[0], 0.3f, 1.5f);
    depthMs = lin (spec.p[1], 0.5f, 5.0f);
    flutterMs = lin (spec.p[2], 0.0f, 0.4f);
    ageHz = expMap (spec.p[3], 3500.0f, 16000.0f);
    mix = lin (spec.mix, 0.8f, 1.0f);
}

void Wow::prepare (double sampleRate, int)
{
    sr = sampleRate;
    size = static_cast<int> (std::ceil (0.030 * sr)) + 4; // 15 ms base + 6 ms wow + flutter
    for (auto& l : line)
        l.assign (static_cast<std::size_t> (size), 0.0f);
    lpA = 1.0 - std::exp (-2.0 * pi * std::min (static_cast<double> (ageHz), 0.45 * sr) / sr);
    depthSmooth.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Wow::reset()
{
    for (auto& l : line)
        std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    wowPhase = flutterPhase = 0.0;
    rng = vb::SplitMix64 { seed };
    flutterAmp = 1.0 + 0.1 * uniform (rng);
    lp[0] = lp[1] = 0.0;
    depthSmooth.reset();
    mixSmooth.reset();
}

void Wow::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const float depthTarget = std::clamp (depthMs + ctx.mod, 0.5f, 5.0f); // mod range +-1 ms
    const float mixTarget = effectiveMix (mix, ctx);
    float* io[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        const double ms = 15.0 + depthSmooth.next (depthTarget) * std::sin (2.0 * pi * wowPhase)
                        + flutterMs * flutterAmp * std::sin (2.0 * pi * flutterPhase);
        wowPhase += rateHz / sr;
        wowPhase -= std::floor (wowPhase);
        flutterPhase += 9.0 / sr;
        if (flutterPhase >= 1.0)
        {
            flutterPhase -= 1.0;
            flutterAmp = 1.0 + 0.1 * uniform (rng); // 10 % random per flutter cycle
        }
        const float m = mixSmooth.next (mixTarget);
        double pos = writePos - ms * 0.001 * sr;
        while (pos < 1.0)
            pos += size;
        const int k = static_cast<int> (pos);
        const double d = pos - k;
        for (int c = 0; c < 2; ++c)
        {
            auto& l = line[c];
            l[static_cast<std::size_t> (writePos)] = io[c][i];
            auto at = [&] (int o) { return static_cast<double> (l[static_cast<std::size_t> (((k + o) % size + size) % size)]); };
            // 3rd-order Lagrange over offsets -1..2
            const double y = -d * (d - 1) * (d - 2) / 6.0 * at (-1) + (d + 1) * (d - 1) * (d - 2) / 2.0 * at (0)
                           - (d + 1) * d * (d - 2) / 2.0 * at (1) + (d + 1) * d * (d - 1) / 6.0 * at (2);
            lp[c] += lpA * (y - lp[c]);
            io[c][i] += (static_cast<float> (lp[c]) * trim - io[c][i]) * m;
        }
        writePos = writePos + 1 == size ? 0 : writePos + 1;
    }
}

// ---- RING ----
Ring::Ring (const vb::ModuleSpec& spec)
{
    freqHz = expMap (spec.p[0], 20.0f, 2000.0f);
    wave = disc (spec.p[1], 2);
    mix = lin (spec.mix, 0.15f, 0.6f);
}

void Ring::prepare (double sampleRate, int)
{
    sr = sampleRate;
    octaves.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Ring::reset()
{
    phase = 0.0;
    octaves.reset();
    mixSmooth.reset();
}

void Ring::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        // Mod range +-1 octave, clamped to the mapped range.
        const double f = freqMemo (octaves.next (ctx.mod), [this] (float oct) { return std::clamp (freqHz * std::pow (2.0, static_cast<double> (oct)), 20.0, 2000.0); });
        const double c = wave == 0 ? std::sin (2.0 * pi * phase) : triangle (phase);
        phase += f / sr;
        phase -= std::floor (phase);
        const float m = mixSmooth.next (mixTarget);
        L[i] += (static_cast<float> (L[i] * c) * trim - L[i]) * m;
        R[i] += (static_cast<float> (R[i] * c) * trim - R[i]) * m;
    }
}
} // namespace vbdsp
