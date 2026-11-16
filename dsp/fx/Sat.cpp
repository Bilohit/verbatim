#include "Sat.h"

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

Sat::Sat (const vb::ModuleSpec& spec)
{
    driveDb = lin (spec.p[0], 0.0f, 24.0f);
    const float toneDb = lin (spec.p[1], -6.0f, 6.0f);
    shapeType = disc (spec.p[2], 4);
    biasDc = lin (spec.p[3], 0.0f, 0.25f);
    mix = lin (spec.mix, 0.5f, 1.0f);
    driveTrim = dbToGain (-0.5f * driveDb);
    tiltLow = dbToGain (-0.5f * toneDb);
    tiltHigh = dbToGain (0.5f * toneDb);
    biasOut = shape (biasDc);
}

float Sat::shape (float x) const
{
    switch (shapeType)
    {
        case 0: return std::tanh (x);
        case 1: return std::tanh (x + 0.3f * x * x); // asymmetric tube; DC removed by the blocker
        case 2:
        {
            const float a = std::abs (x), k = 0.1f; // hard clip with a 0.1 quadratic knee
            float y;
            if (a < 1.0f - k)      y = a;
            else if (a < 1.0f + k) y = a - (a - (1.0f - k)) * (a - (1.0f - k)) / (4.0f * k);
            else                   y = 1.0f;
            return x < 0.0f ? -y : y;
        }
        default: return std::sin (x); // sine fold
    }
}

void Sat::prepare (double sampleRate, int maxBlock)
{
    os.initProcessing (static_cast<std::size_t> (maxBlock));
    latency = static_cast<int> (std::lround (os.getLatencyInSamples()));
    for (int c = 0; c < 2; ++c)
    {
        ring[c].assign (static_cast<std::size_t> (std::max (latency, 1)), 0.0f);
        dryOut[c].assign (static_cast<std::size_t> (maxBlock), 0.0f);
    }
    driveSmooth.prepare (sampleRate, 5.0);
    mixSmooth.prepare (sampleRate, 5.0);
    gainOs.assign (static_cast<std::size_t> (maxBlock), 1.0f);
    mixRamp.assign (static_cast<std::size_t> (maxBlock), 0.0f);
    dcR = std::exp (-2.0 * pi * 5.0 / sampleRate);
    tiltA = 1.0 - std::exp (-2.0 * pi * 1000.0 / sampleRate);
    reset();
}

void Sat::reset()
{
    os.reset();
    for (int c = 0; c < 2; ++c)
    {
        std::fill (ring[c].begin(), ring[c].end(), 0.0f);
        dcX1[c] = dcY1[c] = tiltLp[c] = 0.0;
    }
    ringPos = 0;
    driveSmooth.reset();
    mixSmooth.reset();
}

void Sat::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const float driveTarget = std::clamp (driveDb + 6.0f * ctx.mod, 0.0f, 24.0f);
    for (int i = 0; i < n; ++i)
        gainOs[static_cast<std::size_t> (i)] = static_cast<float> (gainMemo (driveSmooth.next (driveTarget), [] (float db) { return dbToGain (db); }));
    const float mixTarget = effectiveMix (mix, ctx);
    float* io[2] = { L, R };
    const int len = static_cast<int> (ring[0].size());

    // Dry path, delayed by the oversampler latency so the module mix stays phase aligned.
    for (int c = 0; c < 2; ++c)
    {
        int pos = ringPos;
        for (int i = 0; i < n; ++i)
        {
            dryOut[c][static_cast<std::size_t> (i)] = latency > 0 ? ring[c][static_cast<std::size_t> (pos)] : io[c][i];
            ring[c][static_cast<std::size_t> (pos)] = io[c][i];
            pos = pos + 1 == len ? 0 : pos + 1;
        }
    }
    ringPos = (ringPos + n) % len;

    juce::dsp::AudioBlock<float> block (io, 2, static_cast<std::size_t> (n));
    auto up = os.processSamplesUp (block);
    const std::size_t factor = up.getNumSamples() / static_cast<std::size_t> (n);
    for (std::size_t c = 0; c < up.getNumChannels(); ++c)
    {
        auto* x = up.getChannelPointer (c);
        for (std::size_t i = 0; i < up.getNumSamples(); ++i)
            x[i] = shape (x[i] * gainOs[i / factor] + biasDc) - biasOut;
    }
    os.processSamplesDown (block);

    for (int i = 0; i < n; ++i)
        mixRamp[static_cast<std::size_t> (i)] = mixSmooth.next (mixTarget);
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < n; ++i)
        {
            // DC block, tilt EQ at 1 kHz, output trim, then module mix.
            const double x = io[c][i];
            const double y = x - dcX1[c] + dcR * dcY1[c];
            dcX1[c] = x;
            dcY1[c] = y;
            tiltLp[c] += tiltA * (y - tiltLp[c]);
            const float wet = static_cast<float> (tiltLp[c] * tiltLow + (y - tiltLp[c]) * tiltHigh) * driveTrim;
            const float d = dryOut[c][static_cast<std::size_t> (i)];
            io[c][i] = d + (wet * trim - d) * mixRamp[static_cast<std::size_t> (i)];
        }
    }
}
} // namespace vbdsp
