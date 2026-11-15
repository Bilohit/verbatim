#include "harness.h"

#include "Rng.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>

namespace
{
thread_local bool inAudio = false;
std::atomic<std::size_t> audioAllocs { 0 };

void* allocate (std::size_t size)
{
    if (inAudio)
        audioAllocs.fetch_add (1);
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
} // namespace

// RT2 allocation guard: replaces the global allocator for this test binary.
void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    if (inAudio)
        audioAllocs.fetch_add (1);
    return std::malloc (size == 0 ? 1 : size);
}
void* operator new[] (std::size_t size, const std::nothrow_t& t) noexcept { return operator new (size, t); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

namespace harness
{
std::vector<float> pinkNoise (std::size_t n, std::uint64_t seed, double rmsDbfs)
{
    vb::SplitMix64 rng { seed };
    std::vector<double> y (n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double w = static_cast<double> (rng.next() >> 11) / 9007199254740992.0 * 2.0 - 1.0;
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        y[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
        b6 = w * 0.115926;
    }
    double sum = 0;
    for (auto v : y)
        sum += v * v;
    const double scale = std::pow (10.0, rmsDbfs / 20.0) / std::sqrt (sum / static_cast<double> (n));
    std::vector<float> out (n);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = static_cast<float> (y[i] * scale);
    return out;
}

std::vector<float> sine (std::size_t n, double freq, double sampleRate, double peakDbfs)
{
    const double a = std::pow (10.0, peakDbfs / 20.0);
    std::vector<float> out (n);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = static_cast<float> (a * std::sin (2.0 * 3.14159265358979323846 * freq * static_cast<double> (i) / sampleRate));
    return out;
}

Stereo render (vbdsp::Engine& engine, const std::vector<float>& inL, const std::vector<float>& inR, int block,
               const std::function<void (std::size_t)>& onBlock)
{
    Stereo out { inL, inR };
    for (std::size_t start = 0; start < inL.size(); start += static_cast<std::size_t> (block))
    {
        const int n = static_cast<int> (std::min<std::size_t> (static_cast<std::size_t> (block), inL.size() - start));
        if (onBlock)
            onBlock (start);
        inAudio = true;
        engine.process (out.L.data() + start, out.R.data() + start, n);
        inAudio = false;
    }
    return out;
}

double rms (const float* x, std::size_t n)
{
    double s = 0;
    for (std::size_t i = 0; i < n; ++i)
        s += static_cast<double> (x[i]) * x[i];
    return std::sqrt (s / static_cast<double> (n));
}

double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1e-30)); }

std::size_t allocationsInAudio() { return audioAllocs.load(); }
void resetAllocationCount() { audioAllocs.store (0); }
} // namespace harness
