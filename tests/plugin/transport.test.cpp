// T12: R6 transport reset (docs/02-spec.md C9) at the processor level, with a scripted play head.
#include <catch2/catch_test_macros.hpp>

#include "plugin/State.h"
#include "plugin/VerbatimProcessor.h"

#include "Rng.h"

#include <cstring>

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

struct ScriptedPlayHead final : juce::AudioPlayHead
{
    bool playing = false;
    double bpm = 120.0, ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        return p;
    }
};

std::vector<float> noise (std::size_t n, std::uint64_t seed)
{
    vb::SplitMix64 r { seed };
    std::vector<float> x (n);
    for (auto& v : x)
        v = static_cast<float> (static_cast<double> (r.next() >> 11) / 9007199254740992.0 - 0.5) * 0.25f;
    return x;
}

// Renders blocks of `in`; before each block, script(blockIndex) may change the play head.
std::vector<float> run (const std::string& text, const std::vector<float>& in, const std::function<void (std::size_t, ScriptedPlayHead&)>& script)
{
    juce::ScopedJuceInitialiser_GUI init;
    VerbatimProcessor p;
    ScriptedPlayHead head;
    p.setPlayHead (&head);
    vbplugin::PluginState s;
    s.text = text;
    const auto json = vbplugin::toJson (s);
    p.setStateInformation (json.data(), static_cast<int> (json.size()));
    p.prepareToPlay (SR, BLOCK);

    std::vector<float> out (in.size());
    juce::AudioBuffer<float> buf (2, BLOCK);
    juce::MidiBuffer midi;
    for (std::size_t b = 0; b * BLOCK < in.size(); ++b)
    {
        script (b, head);
        for (int c = 0; c < 2; ++c)
            buf.copyFrom (c, 0, in.data() + b * BLOCK, BLOCK);
        p.processBlock (buf, midi);
        std::memcpy (out.data() + b * BLOCK, buf.getReadPointer (0), sizeof (float) * BLOCK);
        head.ppq += BLOCK / SR * head.bpm / 60.0 * (head.playing ? 1.0 : 0.0);
    }
    return out;
}
} // namespace

TEST_CASE ("R6 two bounces from the same play start are bit identical after the fade", "[R6]")
{
    const std::size_t stop1 = 281, stop2 = 1031, play = 940; // blocks: ~3 s, ~11 s stopped; ~10 s playing
    const auto after = noise (play * BLOCK, 3);
    auto in1 = noise (stop1 * BLOCK, 1);
    auto in2 = noise (stop2 * BLOCK, 2);
    in1.insert (in1.end(), after.begin(), after.end());
    in2.insert (in2.end(), after.begin(), after.end());

    const auto text = std::string ("warm tape\n\n& long echo\xE2\x80\xA6 ~?");
    const auto o1 = run (text, in1, [&] (std::size_t b, ScriptedPlayHead& h) { h.playing = b >= stop1; });
    const auto o2 = run (text, in2, [&] (std::size_t b, ScriptedPlayHead& h) { h.playing = b >= stop2; });

    REQUIRE (std::memcmp (o1.data() + stop1 * BLOCK, o2.data() + stop2 * BLOCK, play * BLOCK * sizeof (float)) == 0);
}

TEST_CASE ("R6 a position jump while playing does not reset (tails survive loops)", "[R6]")
{
    const auto in = noise (400 * BLOCK, 4);
    const auto text = std::string ("warm tape");
    const auto straight = run (text, in, [] (std::size_t, ScriptedPlayHead& h) { h.playing = true; });
    const auto jumped = run (text, in, [] (std::size_t b, ScriptedPlayHead& h) {
        h.playing = true;
        if (b == 200)
            h.ppq = 0.0; // loop wrap
    });
    CHECK (std::memcmp (straight.data(), jumped.data(), straight.size() * sizeof (float)) == 0);

    // Stopping and starting again does reset, so the outputs differ after the restart.
    const auto restarted = run (text, in, [] (std::size_t b, ScriptedPlayHead& h) { h.playing = b != 200; });
    CHECK (std::memcmp (straight.data() + 201 * BLOCK, restarted.data() + 201 * BLOCK, 100 * BLOCK * sizeof (float)) != 0);
}
