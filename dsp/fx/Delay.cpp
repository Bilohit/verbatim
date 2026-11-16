#include "Delay.h"

#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double maxDelayMs = 2000.0;
} // namespace

Delay::Delay (const vb::ModuleSpec& spec)
{
    timeMs = expMap (spec.p[0], 30.0f, 900.0f);
    feedback = lin (spec.p[1], 0.0f, 0.8f);
    toneHz = expMap (spec.p[2], 1000.0f, 12000.0f);
    mode = disc (spec.p[3], 3);
    mix = lin (spec.mix, 0.15f, 0.5f);
}

void Delay::prepare (double sampleRate, int)
{
    sr = sampleRate;
    size = static_cast<int> (std::ceil (maxDelayMs * 0.001 * sr * 1.01)) + 4;
    for (auto& l : line)
        l.assign (static_cast<std::size_t> (size), 0.0f);
    lpA = 1.0 - std::exp (-2.0 * pi * toneHz / sr);
    hpA = 1.0 - std::exp (-2.0 * pi * 60.0 / sr);
    wowInc = 2.0 * pi * 0.7 / sr;
    timeGlide.prepare (sr, 50.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Delay::reset()
{
    for (auto& l : line)
        std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    for (int c = 0; c < 2; ++c)
        lp[c] = hp[c] = 0.0;
    wowPhase = 0.0;
    timeGlide.reset();
    mixSmooth.reset();
}

float Delay::readLinear (const std::vector<float>& l, double delay) const
{
    double pos = writePos - delay;
    while (pos < 0)
        pos += size;
    const int i0 = static_cast<int> (pos);
    const double f = pos - i0;
    const int i1 = i0 + 1 == size ? 0 : i0 + 1;
    return static_cast<float> (l[static_cast<std::size_t> (i0)] + f * (l[static_cast<std::size_t> (i1)] - l[static_cast<std::size_t> (i0)]));
}

float Delay::readLagrange (const std::vector<float>& l, double delay) const
{
    // 3rd-order Lagrange over samples at offsets -1..2 around the integer part.
    double pos = writePos - delay;
    while (pos < 1)
        pos += size;
    const int i = static_cast<int> (pos);
    const double d = pos - i;
    auto at = [&] (int k) { return static_cast<double> (l[static_cast<std::size_t> (((i + k) % size + size) % size)]); };
    const double ym1 = at (-1), y0 = at (0), y1 = at (1), y2 = at (2);
    const double c0 = -d * (d - 1) * (d - 2) / 6.0, c1 = (d + 1) * (d - 1) * (d - 2) / 2.0;
    const double c2 = -(d + 1) * d * (d - 2) / 2.0, c3 = (d + 1) * d * (d - 1) / 6.0;
    return static_cast<float> (c0 * ym1 + c1 * y0 + c2 * y1 + c3 * y2);
}

void Delay::process (float* L, float* R, int n, const ModCtx& ctx)
{
    const float targetDelay = static_cast<float> (std::fmin (timeMs * (1.0f + 0.02f * ctx.mod), maxDelayMs) * 0.001 * sr);
    const float mixTarget = effectiveMix (mix, ctx);
    for (int i = 0; i < n; ++i)
    {
        double delay = timeGlide.next (targetDelay);
        const float m = mixSmooth.next (mixTarget);
        if (mode == 2)
        {
            delay *= 1.0 + 0.003 * std::sin (wowPhase);
            wowPhase += wowInc;
            if (wowPhase >= 2.0 * pi)
                wowPhase -= 2.0 * pi;
        }

        float tap[2];
        for (int c = 0; c < 2; ++c)
            tap[c] = mode == 2 ? readLagrange (line[c], delay) : readLinear (line[c], delay);

        // Loop: tone low-pass, fixed 60 Hz high-pass, feedback gain, tanh (tape adds a soft saturation).
        float loop[2];
        for (int c = 0; c < 2; ++c)
        {
            lp[c] += lpA * (tap[c] - lp[c]);
            hp[c] += hpA * (lp[c] - hp[c]);
            double s = lp[c] - hp[c];
            if (mode == 2)
                s = std::tanh (1.5 * s) / 1.5;
            loop[c] = static_cast<float> (std::tanh (feedback * s));
        }

        const float inL = L[i], inR = R[i];
        float wL, wR;
        if (mode == 1)
        {
            wL = 0.5f * (inL + inR) + loop[1];
            wR = loop[0];
        }
        else
        {
            wL = inL + loop[0];
            wR = inR + loop[1];
        }
        line[0][static_cast<std::size_t> (writePos)] = wL;
        line[1][static_cast<std::size_t> (writePos)] = wR;
        writePos = writePos + 1 == size ? 0 : writePos + 1;

        L[i] = inL + (tap[0] - inL) * m;
        R[i] = inR + (tap[1] - inR) * m;
    }
}
} // namespace vbdsp
