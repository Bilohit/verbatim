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
    std::vector<bool> summed;                 // false for a lane consumed by the next lane's chain (&)
    std::vector<std::vector<float>> outL, outR; // per-lane output
    std::vector<float> sumL, sumR;
    float norm = 1.0f;                        // 1 / sqrt(lanes in the sum)
    int warmup = 0;                           // silent pre-roll before this chain fades in

    // Output: dry*(1-mix) + wet*mix, or dry when there are no lanes. Returns NaN-guard events.
    int render (const float* inL, const float* inR, const float* dryL, const float* dryR, const float* mix,
                float* oL, float* oR, int n, const Lane::BlockCtx& ctx) // audio-thread
    {
        if (lanes.empty())
        {
            std::copy (dryL, dryL + n, oL);
            std::copy (dryR, dryR + n, oR);
            return 0;
        }
        std::fill (sumL.begin(), sumL.begin() + n, 0.0f);
        std::fill (sumR.begin(), sumR.begin() + n, 0.0f);
        int nans = 0;
        for (std::size_t i = 0; i < lanes.size(); ++i)
        {
            auto& lane = *lanes[i];
            const bool chained = i > 0 && lane.takesPreviousLane();
            const float* srcL = chained ? outL[i - 1].data() : inL;
            const float* srcR = chained ? outR[i - 1].data() : inR;
            const int before = lane.nanCount();
            lane.process (srcL, srcR, outL[i].data(), outR[i].data(), n, ctx);
            nans += lane.nanCount() - before;
            if (summed[i])
                for (std::size_t k = 0; k < static_cast<std::size_t> (n); ++k)
                {
                    sumL[k] += outL[i][k];
                    sumR[k] += outR[i][k];
                }
        }
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            oL[i] = dryL[i] * (1.0f - mix[i]) + sumL[k] * norm * mix[i];
            oR[i] = dryR[i] * (1.0f - mix[i]) + sumR[k] * norm * mix[i];
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

int Engine::swapWarmupSamples() const
{
    return active != nullptr ? active->warmup : 0;
}

Chain* Engine::build (const vb::ResolvedChain& chain) const
{
    auto* c = new Chain();
    const auto m = static_cast<std::size_t> (maxBlock);
    for (const auto& l : chain.lanes)
    {
        std::vector<vb::ModuleSpec> mods;
        for (const auto& rm : l.modules)
            mods.push_back (rm.spec);
        auto lane = std::make_unique<Lane> (l.lane, mods);
        lane->prepare (sampleRate, maxBlock);
        c->lanes.push_back (std::move (lane));
        c->outL.emplace_back (m, 0.0f);
        c->outR.emplace_back (m, 0.0f);
    }

    // Chain groups (docs/03-dsp-catalog.md 1.1): a lane consumed by the next lane's & leaves the sum;
    // the summed lane at the end of each group is padded so the group's latency is L_FIXED.
    const auto count = c->lanes.size();
    c->summed.assign (count, true);
    int groupLatency = 0, summedLanes = 0, groupWarmup = 0, warmup = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        groupLatency += c->lanes[i]->latencySamples();
        groupWarmup += c->lanes[i]->warmupSamples();
        warmup = std::max (warmup, groupWarmup);
        const bool consumed = i + 1 < count && c->lanes[i + 1]->takesPreviousLane();
        c->summed[i] = ! consumed;
        if (! consumed)
        {
            c->lanes[i]->setPad (lFixed - groupLatency);
            groupLatency = 0;
            groupWarmup = 0;
            ++summedLanes;
        }
    }
    c->norm = summedLanes > 0 ? 1.0f / std::sqrt (static_cast<float> (summedLanes)) : 1.0f;
    // A new chain stays silent until its delay lines are filled (at least L_FIXED, at most 250 ms),
    // then crossfades in over 30 ms (ruling at T16).
    c->warmup = std::clamp (warmup, lFixed, static_cast<int> (0.25 * sampleRate));
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
    for (auto* v : { &inL, &inR, &dryL, &dryR, &outA_L, &outA_R, &outB_L, &outB_R, &mixRamp, &gainRamp, &bypassRamp, &macroRamp })
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
    active = build (lastChain);
    fadeLen = static_cast<int> (std::lround (0.03 * sr));
    fadeInLen = std::max (1, static_cast<int> (std::lround (0.005 * sr)));
    fadeInPos = fadeInLen;

    mixCurrent = mixTarget.load();
    gainCurrent = dbToGain (outputDbTarget.load());
    bypassCurrent = bypassTarget.load();
    macroCurrent = macroTarget.load();
    bpmCurrent = bpmTarget.load();
}

void Engine::setSpec (const vb::ChainSpec& spec)
{
    setResolved (vb::resolve (spec, {}));
}

void Engine::setResolved (const vb::ResolvedChain& chain)
{
    std::lock_guard<std::mutex> lock (buildMutex);
    lastChain = chain;
    if (maxBlock == 0)
        return; // not prepared yet: prepare() builds it
    delete pending.exchange (build (chain)); // deletes a chain the audio thread never took
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
    macroCurrent = macroTarget.load();
    bpmCurrent = bpmTarget.load();
}

void Engine::transportReset() // audio-thread
{
    // Stopped -> playing (docs/02-spec.md C9): finish any swap at once, take a pending chain without a
    // crossfade, clear all chain state and the dry delay, then fade in over 5 ms.
    const auto pushRetire = [this] (Chain* c) {
        const int w = retireWrite.load (std::memory_order_relaxed);
        retire[w % retireCapacity] = c;
        retireWrite.store (w + 1, std::memory_order_release);
    };
    const auto space = [this] { return retireCapacity - (retireWrite.load (std::memory_order_relaxed) - retireRead.load (std::memory_order_acquire)); };
    if (fading.load (std::memory_order_relaxed) && space() > 0)
    {
        pushRetire (old);
        old = nullptr;
        fading.store (false);
    }
    if (! fading.load (std::memory_order_relaxed) && space() > 0)
        if (auto* next = pending.exchange (nullptr))
        {
            pushRetire (active);
            active = next;
        }
    for (auto& l : active->lanes)
        l->reset();
    std::fill (dryBufL.begin(), dryBufL.end(), 0.0f);
    std::fill (dryBufR.begin(), dryBufR.end(), 0.0f);
    dryPos = 0;
    fadeInPos = 0;
}

void Engine::process (float* L, float* R, int n) VB_NONBLOCKING // audio-thread
{
    juce::ScopedNoDenormals noDenormals;
    if (active == nullptr || n <= 0)
        return;

    const bool playing = playingNow.load (std::memory_order_relaxed);
    if (playing && ! wasPlaying)
        transportReset();
    wasPlaying = playing;
    // Tempo changes glide over about 50 ms.
    bpmCurrent += (bpmTarget.load (std::memory_order_relaxed) - bpmCurrent) * (1.0 - std::exp (-n / (0.05 * sampleRate)));

    // Take a pending chain when not fading and the retire FIFO can accept the old one.
    const int used = retireWrite.load (std::memory_order_relaxed) - retireRead.load (std::memory_order_acquire);
    if (! fading.load (std::memory_order_relaxed) && used >= retireCapacity && pending.load (std::memory_order_relaxed) != nullptr)
        deferredTakes.fetch_add (1);
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
    const float macT = macroTarget.load (std::memory_order_relaxed);
    const float macStep = (macT - macroCurrent) / steps;
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
        macroCurrent = std::abs (macT - macroCurrent) <= std::abs (macStep) ? macT : macroCurrent + macStep;
        macroRamp[k] = macroCurrent;
    }

    Lane::BlockCtx ctx;
    ctx.macro = macroRamp.data();
    ctx.macroScaling = macroScaling.load (std::memory_order_relaxed);
    ctx.bpm = bpmCurrent;
    int nans = active->render (inL.data(), inR.data(), dryL.data(), dryR.data(), mixRamp.data(), outA_L.data(), outA_R.data(), n, ctx);
    if (fading.load (std::memory_order_relaxed))
    {
        nans += old->render (inL.data(), inR.data(), dryL.data(), dryR.data(), mixRamp.data(), outB_L.data(), outB_R.data(), n, ctx);
        // The new chain first runs its warm-up at weight 0 so its empty delay lines fill
        // before it becomes audible; then a linear 30 ms crossfade.
        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (i);
            const float t = std::clamp (static_cast<float> (fadePos + i - active->warmup) / static_cast<float> (fadeLen), 0.0f, 1.0f);
            outA_L[k] = outB_L[k] * (1.0f - t) + outA_L[k] * t;
            outA_R[k] = outB_R[k] * (1.0f - t) + outA_R[k] * t;
        }
        fadePos += n;
        if (fadePos >= active->warmup + fadeLen)
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
        if (fadeInPos < fadeInLen)
        {
            const float g = static_cast<float> (fadeInPos++) / static_cast<float> (fadeInLen);
            L[i] *= g;
            R[i] *= g;
        }
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
