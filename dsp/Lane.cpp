#include "Lane.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

Lane::Lane (const vb::LaneSpec& s, const std::vector<vb::ModuleSpec>& specs, int slotBase) : spec (s)
{
    for (std::size_t k = 0; k < specs.size(); ++k)
        modules.push_back (makeModule (specs[k], s.seed, slotBase + static_cast<int> (k)));
}

void Lane::prepare (double sampleRate, int maxBlock, int lFixed)
{
    for (auto& m : modules)
        m->prepare (sampleRate, maxBlock);
    padLen = std::max (0, lFixed - latencySamples());
    padL.assign (static_cast<std::size_t> (std::max (padLen, 1)), 0.0f);
    padR.assign (static_cast<std::size_t> (std::max (padLen, 1)), 0.0f);
    dcR = std::exp (-2.0 * pi * 5.0 / sampleRate);
    meterCoef = 1.0 - std::exp (-1.0 / (0.3 * sampleRate));
    reset();
}

void Lane::reset()
{
    for (auto& m : modules)
        m->reset();
    std::fill (padL.begin(), padL.end(), 0.0f);
    std::fill (padR.begin(), padR.end(), 0.0f);
    padPos = 0;
    for (int c = 0; c < 2; ++c)
        dcX1[c] = dcY1[c] = 0.0;
    meterSq = 0.0;
    meterRms = 0.0f;
}

int Lane::latencySamples() const
{
    int l = 0;
    for (const auto& m : modules)
        l += m->latencySamples();
    return l;
}

void Lane::process (const float* inL, const float* inR, float* outL, float* outR, int n) // audio-thread
{
    std::copy (inL, inL + n, outL);
    std::copy (inR, inR + n, outR);

    const ModCtx ctx;
    for (auto& m : modules)
        m->process (outL, outR, n, ctx);

    // DC block (5 Hz one-pole high-pass).
    float* io[2] = { outL, outR };
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const double x = io[c][i];
            const double y = x - dcX1[c] + dcR * dcY1[c];
            dcX1[c] = x;
            dcY1[c] = y;
            io[c][i] = static_cast<float> (y);
        }

    // NaN guard: a non-finite sample zeroes the block and resets the lane.
    bool finite = true;
    for (int c = 0; c < 2 && finite; ++c)
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (io[c][i]))
            {
                finite = false;
                break;
            }
    if (! finite)
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        reset();
        ++nans;
    }

    // Pad so the total latency of the lane is L_FIXED.
    if (padLen > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            const auto p = static_cast<std::size_t> (padPos);
            const float l = padL[p], r = padR[p];
            padL[p] = outL[i];
            padR[p] = outR[i];
            outL[i] = l;
            outR[i] = r;
            padPos = padPos + 1 == padLen ? 0 : padPos + 1;
        }
    }

    for (int i = 0; i < n; ++i)
        meterSq += meterCoef * (0.5 * (static_cast<double> (outL[i]) * outL[i] + static_cast<double> (outR[i]) * outR[i]) - meterSq);
    meterRms = static_cast<float> (std::sqrt (meterSq));
}
} // namespace vbdsp
