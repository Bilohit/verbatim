// T06: R5, R7, F5, state JSON, dirty notification. docs/06-test-plan.md.
#include <catch2/catch_test_macros.hpp>

#include "plugin/State.h"
#include "plugin/VerbatimProcessor.h"

#include "Catalog.h"
#include "Generate.h"
#include "Resolve.h"
#include "Rng.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

struct JuceInit
{
    juce::ScopedJuceInitialiser_GUI init;
};

std::vector<float> noise (std::size_t n, std::uint64_t seed)
{
    vb::SplitMix64 r { seed };
    std::vector<float> x (n);
    for (auto& v : x)
        v = static_cast<float> (static_cast<double> (r.next() >> 11) / 9007199254740992.0 - 0.5) * 0.25f;
    return x;
}

// Renders `in` through the processor, duplicated to every input channel; returns every output channel.
std::vector<std::vector<float>> render (VerbatimProcessor& p, const std::vector<float>& in)
{
    const int ins = p.getTotalNumInputChannels(), outs = p.getTotalNumOutputChannels();
    std::vector<std::vector<float>> out (static_cast<std::size_t> (outs), std::vector<float> (in.size()));
    juce::AudioBuffer<float> buf (std::max (ins, outs), BLOCK);
    juce::MidiBuffer midi;
    for (std::size_t s = 0; s < in.size(); s += BLOCK)
    {
        const int n = static_cast<int> (std::min<std::size_t> (BLOCK, in.size() - s));
        buf.setSize (std::max (ins, outs), n, false, false, true);
        buf.clear();
        for (int c = 0; c < ins; ++c)
            buf.copyFrom (c, 0, in.data() + s, n);
        p.processBlock (buf, midi);
        for (int c = 0; c < outs; ++c)
            std::memcpy (out[static_cast<std::size_t> (c)].data() + s, buf.getReadPointer (c), sizeof (float) * static_cast<std::size_t> (n));
    }
    return out;
}

void loadText (VerbatimProcessor& p, const std::string& text)
{
    vbplugin::PluginState s;
    s.text = text;
    const auto json = vbplugin::toJson (s);
    p.setStateInformation (json.data(), static_cast<int> (json.size()));
}

bool sameBits (const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0;
}
} // namespace

TEST_CASE ("State JSON round trip keeps text and host params", "[state]")
{
    vbplugin::PluginState s;
    s.text = "dark tape, slow echo!\n\xD0\xBF \xE2\x9C\x89";
    s.mix = 42.5f;
    s.macro = 0.25f;
    s.outputDb = -6.0f;
    s.bypass = true;
    vbplugin::PluginState back;
    REQUIRE (vbplugin::fromJson (vbplugin::toJson (s), back));
    CHECK (back.text == s.text);
    CHECK (back.mix == s.mix);
    CHECK (back.macro == s.macro);
    CHECK (back.outputDb == s.outputDb);
    CHECK (back.bypass);
}

TEST_CASE ("V3 bypass persists through the component state; program 0 has a name", "[V3]")
{
    JuceInit juce;
    VerbatimProcessor p;
    auto* bypass = p.getBypassParameter();
    bypass->setValueNotifyingHost (1.0f);
    juce::MemoryBlock blob;
    p.getStateInformation (blob);
    bypass->setValueNotifyingHost (0.0f);
    p.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    CHECK (bypass->getValue() == 1.0f);
    CHECK (p.getProgramName (0).isNotEmpty());
}

TEST_CASE ("R5 state round trip renders bit identical", "[R5]")
{
    JuceInit juce;
    const auto in = noise (static_cast<std::size_t> (5 * SR), 11);

    VerbatimProcessor a;
    loadText (a, "hello");
    a.prepareToPlay (SR, BLOCK);
    const auto outA = render (a, in);
    juce::MemoryBlock blob;
    a.getStateInformation (blob);

    VerbatimProcessor b;
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    b.prepareToPlay (SR, BLOCK);
    const auto outB = render (b, in);

    CHECK (b.getText() == "hello");
    CHECK (sameBits (outA[0], outB[0]));
    CHECK (sameBits (outA[1], outB[1]));
}

TEST_CASE ("R7 mono layouts render bit identically and mono equals the stereo mid", "[R7]")
{
    JuceInit juce;
    const auto in = noise (static_cast<std::size_t> (3 * SR), 12);

    auto make = [&] (juce::AudioChannelSet inSet, juce::AudioChannelSet outSet) {
        auto p = std::make_unique<VerbatimProcessor>();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (inSet);
        layout.outputBuses.add (outSet);
        REQUIRE (p->setBusesLayout (layout));
        loadText (*p, "hello");
        p->prepareToPlay (SR, BLOCK);
        return p;
    };
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();

    const auto st = render (*make (stereo, stereo), in);
    for (auto [i, o] : { std::pair { mono, mono }, std::pair { mono, stereo } })
    {
        const auto r1 = render (*make (i, o), in);
        const auto r2 = render (*make (i, o), in);
        for (std::size_t c = 0; c < r1.size(); ++c)
            CHECK (sameBits (r1[c], r2[c]));
        if (o == mono)
        {
            double worst = 0;
            for (std::size_t k = 0; k < in.size(); ++k)
                worst = std::max (worst, std::abs (r1[0][k] - 0.5 * (st[0][k] + st[1][k])));
            CHECK (20.0 * std::log10 (worst + 1e-30) < -100.0);
        }
    }
}

TEST_CASE ("F5 corrupt, truncated and future state blobs: E3 and passthrough", "[F5]")
{
    JuceInit juce;
    vbplugin::PluginState good;
    good.text = "hello";
    const auto json = vbplugin::toJson (good);

    std::vector<std::string> blobs = {
        "", "{", "not json at all", json.substr (0, json.size() / 2),
        R"({"stateVersion":2,"grammarVersion":1,"text":"hello"})",
        R"({"stateVersion":1,"grammarVersion":99,"text":"hello"})",
        R"({"stateVersion":1,"grammarVersion":1,"text":5})",
        std::string ("\x00\xff\xfe{\"text\"", 7),
    };
    vb::SplitMix64 r { 5 };
    for (int i = 0; i < 200; ++i) // random byte flips of a valid blob
    {
        auto s = json;
        s[static_cast<std::size_t> (r.next() % s.size())] = static_cast<char> (r.next() & 0xFF);
        blobs.push_back (s);
    }

    const auto in = noise (static_cast<std::size_t> (SR / 2), 13);
    for (std::size_t i = 0; i < blobs.size(); ++i)
    {
        VerbatimProcessor p;
        p.setStateInformation (blobs[i].data(), static_cast<int> (blobs[i].size()));
        if (i < 8)
        {
            INFO ("blob " << i);
            CHECK (p.isStateError());
            p.prepareToPlay (SR, BLOCK);
            const auto out = render (p, in);
            const int d = p.getLatencySamples();
            double worst = 0;
            for (std::size_t k = static_cast<std::size_t> (d); k < in.size(); ++k)
                worst = std::max (worst, std::abs (static_cast<double> (out[0][k]) - in[k - static_cast<std::size_t> (d)]));
            CHECK (worst < 1e-6);
            // The stored blob is kept until the user types.
            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            CHECK (std::string (static_cast<const char*> (saved.getData()), saved.getSize()) == blobs[i]);
        }
    }
}

TEST_CASE ("Typing marks the project dirty (non-parameter state changed)", "[state]")
{
    JuceInit juce;
    struct L : juce::AudioProcessorListener
    {
        bool dirty = false;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override { dirty = dirty || d.nonParameterStateChanged; }
    } listener;
    VerbatimProcessor p;
    p.addListener (&listener);
    p.setText ("hello");
    CHECK (listener.dirty);
    CHECK (! p.isStateError());
    p.removeListener (&listener);
}

TEST_CASE ("F4 random override sequences round-trip through state JSON to an identical ResolvedChain", "[F4]")
{
    static const char* texts[] = { "hello", "tape echo", "dark tape, slow echo!", "a\nb\nc\nd", "wait.. what...", "x" };
    vb::SplitMix64 r { 0xF4F4 };
    for (int seq = 0; seq < 5000; ++seq)
    {
        vbplugin::PluginState s;
        s.text = texts[r.next() % std::size (texts)];
        auto spec = vb::generate (s.text);
        for (int step = 0; step < 8; ++step)
        {
            if (r.next() % 5 == 0)
            {
                s.text = texts[r.next() % std::size (texts)];
                spec = vb::generate (s.text);
            }
            vb::ModuleSpec m { static_cast<std::uint8_t> (r.next() % vb::EFFECT_COUNT), {}, static_cast<std::uint16_t> (r.next()) };
            for (auto& p : m.p)
                p = static_cast<std::uint16_t> (r.next());
            vb::applyOverride (s.overrides, spec, static_cast<int> (r.next() % 4), static_cast<vb::OverrideOp> (r.next() % 5),
                               static_cast<int> (r.next() % 9) - 1, m);
        }
        vbplugin::PluginState back;
        REQUIRE (vbplugin::fromJson (vbplugin::toJson (s), back));
        REQUIRE (back.overrides == s.overrides);
        const auto a = vb::resolve (spec, s.overrides), b = vb::resolve (vb::generate (back.text), back.overrides);
        REQUIRE (a.lanes.size() == b.lanes.size());
        for (std::size_t i = 0; i < a.lanes.size(); ++i)
            REQUIRE (a.lanes[i].modules == b.lanes[i].modules);
    }
}

TEST_CASE ("R5 with overrides: hold, edit, add and remove survive save and reload bit identically", "[R5]")
{
    JuceInit juce;
    const auto in = noise (static_cast<std::size_t> (3 * SR), 14);
    VerbatimProcessor a;
    loadText (a, "hello");
    a.prepareToPlay (SR, BLOCK);
    REQUIRE (a.setOverride (0, vb::OverrideOp::Hold, 0));
    auto edited = a.getResolved().lanes[0].modules[1].spec;
    edited.p[0] = 999;
    REQUIRE (a.setOverride (0, vb::OverrideOp::Edit, 1, edited));
    REQUIRE (a.setOverride (0, vb::OverrideOp::Add, -1, vb::ModuleSpec { vb::DELAY, { 30000, 20000, 40000, 0, 0, 0 }, 40000 }));
    REQUIRE (a.setOverride (0, vb::OverrideOp::Remove, 3));
    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    a.prepareToPlay (SR, BLOCK); // start from the resolved chain directly, as a reload does
    const auto outA = render (a, in);

    VerbatimProcessor b;
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    b.prepareToPlay (SR, BLOCK);
    const auto outB = render (b, in);
    CHECK (b.getResolved().lanes[0].modules == a.getResolved().lanes[0].modules);
    CHECK (sameBits (outA[0], outB[0]));
    CHECK (sameBits (outA[1], outB[1]));
}
