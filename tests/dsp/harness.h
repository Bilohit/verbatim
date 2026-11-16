#pragma once
// Offline render harness for dsp tests: deterministic test signals, block rendering through the
// Engine, and an allocation guard around Engine::process (RT2).
#include "Engine.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace harness
{
struct Stereo
{
    std::vector<float> L, R;
};

// Pink noise (Paul Kellet filter over SplitMix64 white), scaled to the given RMS in dBFS.
std::vector<float> pinkNoise (std::size_t n, std::uint64_t seed, double rmsDbfs);
std::vector<float> sine (std::size_t n, double freq, double sampleRate, double peakDbfs);

// Renders in fixed blocks. onBlock(startSample) runs before each block, outside the audio guard,
// standing in for the message thread.
Stereo render (vbdsp::Engine& engine, const std::vector<float>& inL, const std::vector<float>& inR, int block,
               const std::function<void (std::size_t)>& onBlock = {});

double rms (const float* x, std::size_t n);
double toDb (double linear);

// CPU time consumed by the calling thread (cycles on Windows, process CPU seconds elsewhere): immune to other
// processes' load, so timing tests like D5 compare work, not wall-clock noise.
double threadCpuTime();

// Allocations made on a thread while it is inside Engine::process during render().
std::size_t allocationsInAudio();
void resetAllocationCount();
} // namespace harness
