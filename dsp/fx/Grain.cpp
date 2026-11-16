#include "Grain.h"

#include <algorithm>
#include <cmath>

namespace vbdsp
{
namespace
{
constexpr double pi = 3.14159265358979323846;
}

Grain::Grain (const vb::ModuleSpec& spec, std::uint64_t laneSeed, int slot) : seed (laneSeed ^ static_cast<std::uint64_t> (slot + 1))
{
    sizeMs = expMap (spec.p[0], 20.0f, 200.0f);
    density = expMap (spec.p[1], 4.0f, 40.0f);
    sprayMs = lin (spec.p[2], 0.0f, 500.0f);
    sprayPan = unit (spec.p[2]);
    pitchSet = disc (spec.p[3], 4);
    reverseProb = lin (spec.p[4], 0.0f, 0.5f);
    mix = lin (spec.mix, 0.2f, 0.6f);
}

void Grain::prepare (double sampleRate, int)
{
    sr = sampleRate;
    size = static_cast<int> (sr) + 4; // 1 s buffer
    buffer.assign (static_cast<std::size_t> (size), 0.0f);
    // Overlapping grains add; scale by the expected overlap so density does not set the level.
    norm = 1.0 / std::sqrt (std::max (1.0, static_cast<double> (density) * sizeMs * 0.001));
    spraySmooth.prepare (sr, 5.0);
    mixSmooth.prepare (sr, 5.0);
    reset();
}

void Grain::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    rng = vb::SplitMix64 { seed };
    untilNext = 0.0;
    for (auto& v : voices)
        v.active = false;
    spraySmooth.reset();
    mixSmooth.reset();
}

void Grain::spawn (float sprayMsNow)
{
    const auto v = std::find_if (voices.begin(), voices.end(), [] (const Voice& x) { return ! x.active; });
    if (v == voices.end())
        return; // all 16 busy: skip this grain
    static constexpr int sets[4][3] = { { 0, 0, 0 }, { 0, 12, 0 }, { 0, -12, 0 }, { 0, 7, 12 } };
    static constexpr int setSizes[4] = { 1, 2, 2, 3 };
    const int pick = static_cast<int> (uniform01() * setSizes[pitchSet]);
    v->ratio = std::pow (2.0, sets[pitchSet][std::min (pick, setSizes[pitchSet] - 1)] / 12.0);
    v->length = std::max (2, static_cast<int> (sizeMs * 0.001 * sr));
    v->reverse = uniform01() < reverseProb;
    // Start far enough behind the write head that the read never passes it, plus the spray jitter.
    const double back = v->length * std::max (1.0, v->ratio) + uniform01() * sprayMsNow * 0.001 * sr + 2.0;
    v->start = writePos - std::min (back, static_cast<double> (size - 2));
    const double pan = (uniform01() * 2.0 - 1.0) * sprayPan; // random pan within +-x
    v->gainL = static_cast<float> (std::cos ((pan + 1.0) * pi / 4.0));
    v->gainR = static_cast<float> (std::sin ((pan + 1.0) * pi / 4.0));
    v->t = 0;
    v->active = true;
}

void Grain::process (float* L, float* R, int n, const ModCtx& ctx)
{
    const float sprayTarget = std::clamp (sprayMs + 100.0f * ctx.mod, 0.0f, 500.0f); // mod range +-100 ms
    const float mixTarget = effectiveMix (mix, ctx);
    const double interval = sr / density;
    for (int i = 0; i < n; ++i)
    {
        buffer[static_cast<std::size_t> (writePos)] = 0.5f * (L[i] + R[i]);
        const float spray = spraySmooth.next (sprayTarget);
        untilNext -= 1.0;
        if (untilNext <= 0.0)
        {
            spawn (spray);
            untilNext += interval;
        }
        double outL = 0.0, outR = 0.0;
        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            const double t = v.reverse ? v.length - 1 - v.t : v.t;
            double pos = v.start + t * v.ratio;
            while (pos < 0.0)
                pos += size;
            const int k = static_cast<int> (pos) % size;
            const double f = pos - std::floor (pos);
            const double s = buffer[static_cast<std::size_t> (k)] + f * (buffer[static_cast<std::size_t> ((k + 1) % size)] - buffer[static_cast<std::size_t> (k)]);
            const double w = 0.5 * (1.0 - std::cos (2.0 * pi * v.t / (v.length - 1))); // Hann
            outL += s * w * v.gainL;
            outR += s * w * v.gainR;
            if (++v.t >= v.length)
                v.active = false;
        }
        const float m = mixSmooth.next (mixTarget);
        L[i] += (static_cast<float> (outL * norm) - L[i]) * m;
        R[i] += (static_cast<float> (outR * norm) - R[i]) * m;
        writePos = writePos + 1 == size ? 0 : writePos + 1;
    }
}
} // namespace vbdsp
