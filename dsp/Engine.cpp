#include "Engine.h"

#include "SafetyClip.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

namespace vbdsp
{
// A fully prepared chain. Built and deleted on non-audio threads only.
struct Chain
{
    std::vector<std::unique_ptr<Lane>> lanes;
    std::vector<float> laneL, laneR, sumL, sumR;

    // Output: dry*(1-mix) + wet*mix, or dry when there are no lanes. Returns NaN-guard events.
    int render (const float* inL, const float* inR, const float* dryL, const float* dryR, const float* mix,
                float* outL, float* outR, int n) // audio-thread
    {
        if (lanes.empty())
        {
            std::copy (dryL, dryL + n, outL);
            std::copy (dryR, dryR + n, outR);
            return 0;
        }
        std::fill (sumL.begin(), sumL.begin() + n, 0.0f);
        std::fill (sumR.begin(), sumR.begin() + n, 0.0f);
        int nans = 0;
        for (auto& lane : lanes)
        {
            const int before = lane->nanCount();
            lane->process (inL, inR, laneL.data(), laneR.data(), n);
            nans += lane->nanCount() - before;
            for (std::size_t k = 0; k < static_cast<std::size_t> (n); ++k)
            {
                sumL[k] += laneL[k];
                sumR[k] += laneR[k];
            }
        }
        const float norm = 1.0f / std::sqrt (static_cast<float> (lanes.size()));
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            outL[i] = dryL[i] * (1.0f - mix[i]) + sumL[k] * norm * mix[i];
            outR[i] = dryR[i] * (1.0f - mix[i]) + sumR[k] * norm * mix[i];
        }
        return nans;
    }
};

Engine::Engine() = default;

Engine::~Engine()
{
    delete active;
    delete old;
    delete pending.exchange (nullptr);
    collectGarbage();
}

Chain* Engine::build (const vb::ChainSpec& spec) const
{
    auto* c = new Chain();
    for (const auto& l : spec.lanes)
    {
        std::vector<vb::ModuleSpec> mods (l.slots.begin(), l.slots.begin() + l.nModules);
        auto lane = std::make_unique<Lane> (l, mods);
        lane->prepare (sampleRate, maxBlock, lFixed);
        c->lanes.push_back (std::move (lane));
    }
    const auto m = static_cast<std::size_t> (maxBlock);
    c->laneL.assign (m, 0.0f);
    c->laneR.assign (m, 0.0f);
    c->sumL.assign (m, 0.0f);
    c->sumR.assign (m, 0.0f);
    return c;
}

void Engine::prepare (double sr, int block)
{
    std::lock_guard<std::mutex> lock (buildMutex);
    sampleRate = sr;
    maxBlock = block;
    lFixed = 8 * satLatencySamples();

    const auto m = static_cast<std::size_t> (block);
    for (auto* v : { &inL, &inR, &dryL, &dryR, &outA_L, &outA_R, &outB_L, &outB_R, &mixRamp, &gainRamp, &bypassRamp })
        v->assign (m, 0.0f);
    dryBufL.assign (static_cast<std::size_t> (std::max (lFixed, 1)), 0.0f);
    dryBufR.assign (static_cast<std::size_t> (std::max (lFixed, 1)), 0.0f);
    dryPos = 0;

    delete active;
    delete old;
    delete pending.exchange (nullptr);
    old = nullptr;
    fading.store (false);
    collectGarbage();
    active = build (lastSpec);
    fadeLen = static_cast<int> (std::lround (0.03 * sr));

    mixCurrent = mixTarget.load();
    gainCurrent = dbToGain (outputDbTarget.load());
    bypassCurrent = bypassTarget.load();
}

void Engine::setSpec (const vb::ChainSpec& spec)
{
    std::lock_guard<std::mutex> lock (buildMutex);
    lastSpec = spec;
    if (maxBlock == 0)
        return; // not prepared yet: prepare() builds it
    delete pending.exchange (build (spec)); // deletes a chain the audio thread never took
}

void Engine::collectGarbage()
{
    int r = retireRead.load (std::memory_order_relaxed);
    const int w = retireWrite.load (std::memory_order_acquire);
    while (r != w)
    {
        delete retire[r % retireCapacity];
        ++r;
    }
    retireRead.store (r, std::memory_order_release);
}

void Engine::reset()
{
    if (active != nullptr)
        for (auto& l : active->lanes)
            l->reset();
    delete old;
    old = nullptr;
    fading.store (false);
    std::fill (dryBufL.begin(), dryBufL.end(), 0.0f);
    std::fill (dryBufR.begin(), dryBufR.end(), 0.0f);
    dryPos = 0;
    mixCurrent = mixTarget.load();
    gainCurrent = dbToGain (outputDbTarget.load());
    bypassCurrent = bypassTarget.load();
}

void Engine::process (float* L, float* R, int n) // audio-thread
{
    juce::ScopedNoDenormals noDenormals;
    if (active == nullptr || n <= 0)
        return;

    // Take a pending chain when not fading and the retire FIFO can accept the old one.
    const int used = retireWrite.load (std::memory_order_relaxed) - retireRead.load (std::memory_order_acquire);
    if (! fading.load (std::memory_order_relaxed) && used < retireCapacity)
        if (auto* next = pending.exchange (nullptr))
        {
            old = active;
            active = next;
            fadePos = 0;
            fading.store (true);
        }

    std::copy (L, L + n, inL.begin());
    std::copy (R, R + n, inR.begin());

    // Dry delay of L_FIXED samples.
    const int dlen = static_cast<int> (dryBufL.size());
    for (int i = 0; i < n; ++i)
    {
        const auto p = static_cast<std::size_t> (dryPos);
        const auto k = static_cast<std::size_t> (i);
        dryL[k] = lFixed > 0 ? dryBufL[p] : inL[k];
        dryR[k] = lFixed > 0 ? dryBufR[p] : inR[k];
        dryBufL[p] = inL[k];
        dryBufR[p] = inR[k];
        dryPos = dryPos + 1 == dlen ? 0 : dryPos + 1;
    }

    // 20 ms linear smoothing of Mix and Output gain.
    const float mixT = mixTarget.load (std::memory_order_relaxed);
    const float gainT = dbToGain (outputDbTarget.load (std::memory_order_relaxed));
    const float steps = static_cast<float> (0.02 * sampleRate);
    const float byT = bypassTarget.load (std::memory_order_relaxed);
    const float mixStep = (mixT - mixCurrent) / steps, gainStep = (gainT - gainCurrent) / steps, byStep = (byT - bypassCurrent) / steps;
    for (int i = 0; i < n; ++i)
    {
        const auto k = static_cast<std::size_t> (i);
        mixCurrent = std::abs (mixT - mixCurrent) <= std::abs (mixStep) ? mixT : mixCurrent + mixStep;
        gainCurrent = std::abs (gainT - gainCurrent) <= std::abs (gainStep) ? gainT : gainCurrent + gainStep;
        bypassCurrent = std::abs (byT - bypassCurrent) <= std::abs (byStep) ? byT : bypassCurrent + byStep;
        mixRamp[k] = mixCurrent;
        gainRamp[k] = gainCurrent;
        bypassRamp[k] = bypassCurrent;
    }

    int nans = active->render (inL.data(), inR.data(), dryL.data(), dryR.data(), mixRamp.data(), outA_L.data(), outA_R.data(), n);
    if (fading.load (std::memory_order_relaxed))
    {
        nans += old->render (inL.data(), inR.data(), dryL.data(), dryR.data(), mixRamp.data(), outB_L.data(), outB_R.data(), n);
        // The new chain first runs L_FIXED samples at weight 0 so its empty delay lines fill
        // before it becomes audible; then a linear 30 ms crossfade.
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            const float t = std::clamp (static_cast<float> (fadePos + i - lFixed) / static_cast<float> (fadeLen), 0.0f, 1.0f);
            outA_L[k] = outB_L[k] * (1.0f - t) + outA_L[k] * t;
            outA_R[k] = outB_R[k] * (1.0f - t) + outA_R[k] * t;
        }
        fadePos += n;
        if (fadePos >= lFixed + fadeLen)
        {
            const int w = retireWrite.load (std::memory_order_relaxed);
            retire[w % retireCapacity] = old;
            retireWrite.store (w + 1, std::memory_order_release);
            old = nullptr;
            fading.store (false);
        }
    }
    if (nans > 0)
        nanTotal.fetch_add (nans);

    float pkL = 0, pkR = 0;
    bool clipped = false;
    for (int i = 0; i < n; ++i)
    {
        const auto k = static_cast<std::size_t> (i);
        const float l = outA_L[k] * gainRamp[k], r = outA_R[k] * gainRamp[k];
        clipped = clipped || std::abs (l) > 0.7079f || std::abs (r) > 0.7079f;
        const float b = bypassRamp[k];
        L[i] = b == 0.0f ? safetyClip (l) : safetyClip (l) * (1.0f - b) + dryL[k] * b;
        R[i] = b == 0.0f ? safetyClip (r) : safetyClip (r) * (1.0f - b) + dryR[k] * b;
        pkL = std::fmax (pkL, std::abs (L[i]));
        pkR = std::fmax (pkR, std::abs (R[i]));
    }
    meters.peakL.store (pkL);
    meters.peakR.store (pkR);
    meters.clip.store (clipped);
    for (std::size_t i = 0; i < 4; ++i)
        meters.laneRms[i].store (i < active->lanes.size() ? active->lanes[i]->rms() : 0.0f);
}
} // namespace vbdsp
