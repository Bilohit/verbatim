#include "Pitch.h"

#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr int intervals[] = { -12, -7, -5, 3, 4, 5, 7, 12 };
} // namespace

Pitch::Pitch (const vb::ModuleSpec& spec)
{
    semitones = intervals[disc (spec.p[0], 8)];
    detuneCents = lin (spec.p[1], -15.0f, 15.0f);
    windowMs = lin (spec.p[2], 30.0f, 80.0f);
    feedback = lin (spec.p[3], 0.0f, 0.4f);
    mix = lin (spec.mix, 0.2f, 0.5f);
}

void Pitch::prepare (double sampleRate, int)
{
    sr = sampleRate;
    windowSamples = static_cast<int> (std::lround (windowMs * 0.001 * sr));
    size = windowSamples + 4;
    for (auto& l : line)
        l.assign (static_cast<std::size_t> (size), 0.0f);
    cents.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Pitch::reset()
{
    for (auto& l : line)
        std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    phase = 0.0;
    last[0] = last[1] = 0.0f;
    cents.reset();
    mixSmooth.reset();
}

void Pitch::process (float* L, float* R, int n, const ModCtx& ctx) // audio-thread
{
    const float mixTarget = effectiveMix (mix, ctx);
    float* io[2] = { L, R };
    const double w = windowSamples;
    for (int i = 0; i < n; ++i)
    {
        // Mod range +-10 cents on the detune.
        const double ratio = ratioMemo (cents.next (detuneCents + 10.0f * ctx.mod), [this] (float c) { return std::pow (2.0, (semitones + c / 100.0) / 12.0); });
        phase += (1.0 - ratio) / w; // tap delay = phase * window; shrinks when shifting up
        phase -= std::floor (phase);
        const float m = mixSmooth.next (mixTarget);
        const double ph[2] = { phase, phase + 0.5 - std::floor (phase + 0.5) };
        for (int c = 0; c < 2; ++c)
        {
            auto& l = line[c];
            const float fb = feedback * last[c];
            l[static_cast<std::size_t> (writePos)] = io[c][i] + (std::abs (fb) < 1e-15f ? fb : std::tanh (fb)); // tanh(x) == x in float below 1e-15; skips the slow tiny-argument path (D5)
            double y = 0.0;
            for (double p : ph)
            {
                double pos = writePos - (1.0 + p * (w - 1.0));
                while (pos < 0.0)
                    pos += size;
                const int k = static_cast<int> (pos);
                const double f = pos - k;
                const double a = l[static_cast<std::size_t> (k)], b = l[static_cast<std::size_t> ((k + 1) % size)];
                const double s = std::sin (pi * p);
                y += (a + f * (b - a)) * s * s; // sin^2 windows of the two taps sum to 1
            }
            last[c] = static_cast<float> (y);
            io[c][i] += (static_cast<float> (y) * trim - io[c][i]) * m;
        }
        writePos = writePos + 1 == size ? 0 : writePos + 1;
    }
}
} // namespace vbdsp
