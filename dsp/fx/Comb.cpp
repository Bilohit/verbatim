#include "Comb.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

// ---- COMB ----
Comb::Comb (const vb::ModuleSpec& spec)
{
    const double f = expMap (spec.p[0], 55.0f, 880.0f);
    pitchHz = 440.0 * std::pow (2.0, std::round (12.0 * std::log2 (f / 440.0)) / 12.0); // nearest semitone, A = 440
    feedback = lin (spec.p[1], 0.3f, 0.9f);
    dampHz = expMap (spec.p[2], 1500.0f, 12000.0f);
    polarity = disc (spec.p[3], 2) == 0 ? 1.0f : -1.0f;
    mix = lin (spec.mix, 0.2f, 0.6f);
}

void Comb::prepare (double sampleRate, int)
{
    sr = sampleRate;
    size = static_cast<int> (std::ceil (sr / (55.0 * std::pow (2.0, -0.5 / 12.0)))) + 4; // lowest pitch minus 50 cents
    for (auto& l : line)
        l.assign (static_cast<std::size_t> (size), 0.0f);
    lpA = 1.0 - std::exp (-2.0 * pi * std::min (static_cast<double> (dampHz), 0.45 * sr) / sr);
    cents.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Comb::reset()
{
    for (auto& l : line)
        std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    lp[0] = lp[1] = 0.0;
    cents.reset();
    mixSmooth.reset();
}

void Comb::process (float* L, float* R, int n, const ModCtx& ctx)
{
    const float mixTarget = effectiveMix (mix, ctx);
    float* io[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        const double hz = pitchHz * std::pow (2.0, cents.next (50.0f * ctx.mod) / 1200.0); // mod range +-50 cents
        double pos = writePos - sr / hz;
        while (pos < 0.0)
            pos += size;
        const int k = static_cast<int> (pos);
        const double f = pos - k;
        const float m = mixSmooth.next (mixTarget);
        for (int c = 0; c < 2; ++c)
        {
            auto& l = line[c];
            const double d = l[static_cast<std::size_t> (k)] + f * (l[static_cast<std::size_t> ((k + 1) % size)] - l[static_cast<std::size_t> (k)]);
            lp[c] += lpA * (d - lp[c]);
            const float y = io[c][i] + polarity * std::tanh (feedback * static_cast<float> (lp[c]));
            l[static_cast<std::size_t> (writePos)] = y;
            io[c][i] += (y - io[c][i]) * m;
        }
        writePos = writePos + 1 == size ? 0 : writePos + 1;
    }
}

// ---- FREEZE ----
Freeze::Freeze (const vb::ModuleSpec& spec)
{
    lengthMs = expMap (spec.p[0], 60.0f, 500.0f);
    period = 2 + disc (spec.p[1], 7);
    repeats = 1 + disc (spec.p[2], 3);
    mix = lin (spec.mix, 0.3f, 0.8f);
}

void Freeze::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : slice)
        s.assign (static_cast<std::size_t> (2.0 * sr) + 1, 0.0f);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Freeze::reset()
{
    for (auto& s : slice)
        std::fill (s.begin(), s.end(), 0.0f);
    pos = 0;
    len = 0;
    mixSmooth.reset();
}

void Freeze::process (float* L, float* R, int n, const ModCtx& ctx)
{
    // Slice length T when the lane has a time base (halved until it fits the 2 s loop buffer).
    const float ms = ctx.timeDiv != 0 ? noteMs (ctx, 2000.0f) : lengthMs;
    const int fade = std::max (1, static_cast<int> (0.005 * sr));
    const float mixTarget = effectiveMix (std::clamp (mix + 0.2f * ctx.mod, 0.3f, 0.8f), ctx); // mod +-0.2 on mix
    float* io[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        if (pos == 0 || len == 0)
            len = std::clamp (static_cast<int> (std::lround (ms * 0.001 * sr)), 1, static_cast<int> (slice[0].size()) - 1);
        const long long cycle = static_cast<long long> (period) * len;
        const long long playEnd = std::min (cycle, static_cast<long long> (repeats + 1) * len);
        const float m = mixSmooth.next (mixTarget);
        for (int c = 0; c < 2; ++c)
        {
            const float x = io[c][i];
            float wet = x;
            if (pos < len)
            {
                slice[c][static_cast<std::size_t> (pos)] = x; // capture
            }
            else if (pos < playEnd)
            {
                // Replay, windowed at each repeat edge with 5 ms fades against the live input.
                const auto k = static_cast<int> ((pos - len) % len);
                const float w = std::min ({ 1.0f, static_cast<float> (k) / fade, static_cast<float> (len - k) / fade });
                wet = slice[c][static_cast<std::size_t> (k)] * w + x * (1.0f - w);
            }
            io[c][i] = x + (wet - x) * m;
        }
        if (++pos >= cycle)
            pos = 0;
    }
}
} // namespace vbdsp
