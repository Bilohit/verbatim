#include "Mod.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr std::uint64_t LFO_B_SALT = 0x4C464F42ull; // ASCII "LFOB"

double uniform (vb::SplitMix64& r) // [-1, 1)
{
    return static_cast<double> (r.next() >> 11) / 9007199254740992.0 * 2.0 - 1.0;
}
} // namespace

void LaneMod::prepare (const vb::LaneSpec& s, int moduleCount, double sampleRate)
{
    spec = s;
    count = std::min (moduleCount, static_cast<int> (values.size()));
    sr = sampleRate;
    const double u = spec.lfoB.rate / 65535.0;
    periodB = sr / (0.03 * std::pow (2.0 / 0.03, u));
    envAtt = 1.0 - std::exp (-1.0 / (0.010 * sr));
    envRel = 1.0 - std::exp (-1.0 / (0.200 * sr));
    slewStep = 2.0 * CONTROL_TICK / (0.005 * sr);
    reset();
}

void LaneMod::reset()
{
    phaseA = 0.0;
    square = 0.0;
    rng = vb::SplitMix64 { spec.seed ^ LFO_B_SALT };
    prevB = uniform (rng);
    nextB = uniform (rng);
    posB = 0.0;
    envMs = 0.0;
    values.fill (0.0f);
}

void LaneMod::trackInput (const float* L, const float* R, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const double x = 0.5 * (static_cast<double> (L[i]) * L[i] + static_cast<double> (R[i]) * R[i]);
        envMs += (x > envMs ? envAtt : envRel) * (x - envMs);
    }
}

void LaneMod::tick (double bpm, float macro, bool macroScaling)
{
    // LFO A at the current phase, then advance one tick.
    double a = 0.0;
    switch (spec.lfoA.shape)
    {
        case 0: a = std::sin (2.0 * pi * phaseA); break;
        case 1: a = phaseA < 0.25 ? 4.0 * phaseA : phaseA < 0.75 ? 2.0 - 4.0 * phaseA : 4.0 * phaseA - 4.0; break;
        case 2: a = 1.0 - 2.0 * phaseA; break;
        default:
        {
            const double target = phaseA < 0.5 ? 1.0 : -1.0; // square with a 5 ms slew
            square += std::clamp (target - square, -slewStep, slewStep);
            a = square;
            break;
        }
    }
    double hzA;
    if (spec.timeDiv != 0)
        hzA = 1.0 / ((16.0 / spec.timeDiv) * 60.0 / bpm); // period = 4 notes of 1/timeDiv
    else
        hzA = 0.05 * std::pow (8.0 / 0.05, spec.lfoA.rate / 65535.0);
    phaseA += hzA * CONTROL_TICK / sr;
    phaseA -= std::floor (phaseA);

    // LFO B: random walk, cosine interpolated between uniform targets, one target per period.
    const double t = posB / periodB;
    const double b = prevB + (nextB - prevB) * 0.5 * (1.0 - std::cos (pi * t));
    posB += CONTROL_TICK;
    while (posB >= periodB)
    {
        posB -= periodB;
        prevB = nextB;
        nextB = uniform (rng);
    }

    // ENV: lane input RMS, -12 dBFS RMS = 1, unipolar.
    const double env = std::min (1.0, std::sqrt (envMs) / 0.25118864315095801);

    const double scale = macroScaling ? 2.0 * macro : 1.0;
    values.fill (0.0f);
    if (count == 0)
        return;
    double sum[8] {};
    sum[spec.lfoA.target % count] += spec.lfoA.depth / 65535.0 * a;
    sum[spec.lfoB.target % count] += spec.lfoB.depth / 65535.0 * b;
    sum[spec.env.target % count] += spec.env.depth / 65535.0 * env;
    for (int k = 0; k < count; ++k)
        values[static_cast<std::size_t> (k)] = static_cast<float> (std::clamp (sum[k] * scale, -1.0, 1.0));
}
} // namespace vbdsp
