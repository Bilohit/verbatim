#include "Lane.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

void Lane::Ring::setDelay (int samples)
{
    len = std::max (samples, 0);
    l.assign (static_cast<std::size_t> (std::max (len, 1)), 0.0f);
    r.assign (static_cast<std::size_t> (std::max (len, 1)), 0.0f);
    pos = 0;
}

void Lane::Ring::clear()
{
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
    pos = 0;
}

void Lane::Ring::process (float* L, float* R, int n) // audio-thread
{
    if (len == 0)
        return;
    for (int i = 0; i < n; ++i)
    {
        const auto p = static_cast<std::size_t> (pos);
        const float a = l[p], b = r[p];
        l[p] = L[i];
        r[p] = R[i];
        L[i] = a;
        R[i] = b;
        pos = pos + 1 == len ? 0 : pos + 1;
    }
}

Lane::Lane (const vb::LaneSpec& s, const std::vector<vb::ModuleSpec>& specs, int slotBase) : spec (s)
{
    for (std::size_t k = 0; k < specs.size(); ++k)
        modules.push_back (makeModule (specs[k], s.seed, slotBase + static_cast<int> (k)));
    if (spec.split != 0 && modules.size() >= 2)
        splitAt = (modules.size() + 1) / 2; // ceil(n/2) modules in the first half
}

int Lane::latencyOf (std::size_t from, std::size_t to) const
{
    int l = 0;
    for (std::size_t k = from; k < to; ++k)
        l += modules[k]->latencySamples();
    return l;
}

int Lane::latencySamples() const
{
    if (splitAt == 0)
        return latencyOf (0, modules.size());
    return std::max (latencyOf (0, splitAt), latencyOf (splitAt, modules.size()));
}

void Lane::prepare (double sampleRate, int maxBlock)
{
    for (auto& m : modules)
        m->prepare (sampleRate, maxBlock);

    const juce::dsp::ProcessSpec ps { sampleRate, static_cast<juce::uint32> (maxBlock), 2 };
    const float fc = 200.0f * std::pow (4000.0f / 200.0f, static_cast<float> (spec.bandCenter) / 65535.0f);
    for (auto* f : { &bandHp, &bandLp })
    {
        f->prepare (ps);
        f->setResonance (1.0f / std::sqrt (2.0f));
    }
    bandHp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    bandLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    bandHp.setCutoffFrequency (fc / 2.4f);
    bandLp.setCutoffFrequency (std::min (fc * 2.4f, static_cast<float> (0.45 * sampleRate)));

    driveGain = std::pow (10.0f, 4.0f * spec.drive / 20.0f);
    driveNorm = 1.0f / std::sqrt (driveGain);
    wet = std::clamp ((spec.mixAbs == 255 ? 1.0f : spec.mixAbs / 100.0f) + 0.15f * spec.mixSteps, 0.10f, 1.0f);
    levelGain = std::pow (10.0f, 2.0f * spec.level / 20.0f);
    sideGain = std::pow (10.0f, 1.5f * spec.space / 20.0f);
    duckAtt = 1.0 - std::exp (-1.0 / (0.005 * sampleRate));
    duckRel = 1.0 - std::exp (-1.0 / (0.150 * sampleRate));

    const auto m = static_cast<std::size_t> (maxBlock);
    rawL.assign (m, 0.0f);
    rawR.assign (m, 0.0f);
    splitL.assign (m, 0.0f);
    splitR.assign (m, 0.0f);
    dryDelay.setDelay (latencySamples());
    if (splitAt != 0)
    {
        const int a = latencyOf (0, splitAt), b = latencyOf (splitAt, modules.size());
        splitDelayOnA = a < b;
        splitDelay.setDelay (std::abs (a - b));
    }
    pad.setDelay (0);
    dcR = std::exp (-2.0 * pi * 5.0 / sampleRate);
    meterCoef = 1.0 - std::exp (-1.0 / (0.3 * sampleRate));
    reset();
}

void Lane::setPad (int samples)
{
    pad.setDelay (samples);
}

void Lane::reset()
{
    for (auto& m : modules)
        m->reset();
    bandHp.reset();
    bandLp.reset();
    dryDelay.clear();
    splitDelay.clear();
    pad.clear();
    duckEnv = 0.0;
    for (int c = 0; c < 2; ++c)
        dcX1[c] = dcY1[c] = 0.0;
    meterSq = 0.0;
    meterRms = 0.0f;
}

void Lane::runModules (std::size_t from, std::size_t to, float* L, float* R, int n)
{
    const ModCtx ctx;
    for (std::size_t k = from; k < to; ++k)
        modules[k]->process (L, R, n, ctx);
}

void Lane::process (const float* inL, const float* inR, float* outL, float* outR, int n) // audio-thread
{
    std::copy (inL, inL + n, rawL.begin());
    std::copy (inR, inR + n, rawR.begin());
    std::copy (inL, inL + n, outL);
    std::copy (inR, inR + n, outR);
    float* io[2] = { outL, outR };

    if (spec.band != 0)
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                io[c][i] = bandLp.processSample (c, bandHp.processSample (c, io[c][i]));

    if (spec.drive != 0)
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                io[c][i] = std::tanh (io[c][i] * driveGain) * driveNorm;

    if (splitAt == 0)
    {
        runModules (0, modules.size(), outL, outR, n);
    }
    else
    {
        std::copy (outL, outL + n, splitL.begin());
        std::copy (outR, outR + n, splitR.begin());
        runModules (0, splitAt, outL, outR, n);
        runModules (splitAt, modules.size(), splitL.data(), splitR.data(), n);
        if (splitDelayOnA)
            splitDelay.process (outL, outR, n);
        else
            splitDelay.process (splitL.data(), splitR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            outL[i] = (outL[i] + splitL[k]) * 0.707f;
            outR[i] = (outR[i] + splitR[k]) * 0.707f;
        }
    }

    // Lane mix against the lane input delayed by the module latency.
    float* dryL = splitL.data();
    float* dryR = splitR.data();
    std::copy (rawL.begin(), rawL.begin() + n, dryL);
    std::copy (rawR.begin(), rawR.begin() + n, dryR);
    dryDelay.process (dryL, dryR, n);
    if (wet != 1.0f)
        for (int i = 0; i < n; ++i)
        {
            outL[i] = dryL[i] * (1.0f - wet) + outL[i] * wet;
            outR[i] = dryR[i] * (1.0f - wet) + outR[i] * wet;
        }

    // Duck: up to 3*duck dB, driven by the lane input envelope, full depth at -12 dBFS.
    if (spec.duck != 0)
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            const double a = std::max (std::abs (rawL[k]), std::abs (rawR[k]));
            duckEnv += (a > duckEnv ? duckAtt : duckRel) * (a - duckEnv);
            const double depth = std::min (1.0, duckEnv / 0.25118864315095801);
            const float g = static_cast<float> (std::pow (10.0, -3.0 * spec.duck * depth / 20.0));
            outL[i] *= g;
            outR[i] *= g;
        }

    if (spec.level != 0)
        for (int i = 0; i < n; ++i)
        {
            outL[i] *= levelGain;
            outR[i] *= levelGain;
        }

    if (spec.space != 0)
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (outL[i] + outR[i]), side = 0.5f * (outL[i] - outR[i]) * sideGain;
            outL[i] = mid + side;
            outR[i] = mid - side;
        }

    // DC block (5 Hz one-pole high-pass).
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
        for (int i = 0; i < n && finite; ++i)
            finite = std::isfinite (io[c][i]);
    if (! finite)
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        reset();
        ++nans;
    }

    pad.process (outL, outR, n);

    for (int i = 0; i < n; ++i)
        meterSq += meterCoef * (0.5 * (static_cast<double> (outL[i]) * outL[i] + static_cast<double> (outR[i]) * outR[i]) - meterSq);
    meterRms = static_cast<float> (std::sqrt (meterSq));
}
} // namespace vbdsp
