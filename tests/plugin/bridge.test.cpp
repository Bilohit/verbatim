// T22: bridge payloads (docs/05-ui-design.md section 6), U3 automation round trip, P2 build time, P3 typing storm.
#include <catch2/catch_test_macros.hpp>

#include "plugin/Bridge.h"
#include "plugin/ParamText.h"
#include "plugin/State.h"
#include "plugin/VerbatimProcessor.h"

#include "Catalog.h"
#include "Generate.h"
#include "Resolve.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>

// P3 memory check: live allocation count for this test binary.
namespace
{
std::atomic<long long> liveAllocs { 0 };
}
void* operator new (std::size_t size)
{
    liveAllocs.fetch_add (1);
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return operator new (size); }
void operator delete (void* p) noexcept
{
    if (p != nullptr)
        liveAllocs.fetch_sub (1);
    std::free (p);
}
void operator delete[] (void* p) noexcept { operator delete (p); }
void operator delete (void* p, std::size_t) noexcept { operator delete (p); }
void operator delete[] (void* p, std::size_t) noexcept { operator delete (p); }

namespace
{
constexpr double SR = 48000.0;
constexpr int BLOCK = 512;

struct JuceInit
{
    juce::ScopedJuceInitialiser_GUI init;
};

juce::var call (juce::var (*fn) (VerbatimProcessor&, const juce::Array<juce::var>&), VerbatimProcessor& p, juce::var arg)
{
    return fn (p, juce::Array<juce::var> { arg });
}

juce::var overrideArg (int lane, const char* op, int slot, juce::var module = {})
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("lane", lane);
    o->setProperty ("op", op);
    o->setProperty ("slot", slot);
    o->setProperty ("module", module);
    return juce::var (o);
}
} // namespace

TEST_CASE ("setText returns tokens, chain, seedHash, ignoredLines, full", "[bridge]")
{
    JuceInit juce;
    VerbatimProcessor p;
    auto r = call (vbplugin::handleSetText, p, "Dark tape, 8 x\nb\nc\nd\ne");
    const auto* lines = r["tokens"].getArray();
    REQUIRE (lines != nullptr);
    REQUIRE (lines->size() == 5);
    const auto* first = (*lines)[0].getArray();
    REQUIRE (first->size() == 5);
    CHECK ((*first)[0]["class"].toString() == "direction");
    CHECK ((*first)[2]["class"].toString() == "op");
    CHECK ((*first)[3]["class"].toString() == "number");
    CHECK ((*first)[1]["start"].operator int() == 5);
    CHECK ((*first)[1]["length"].operator int() == 4);
    CHECK ((*(*lines)[4].getArray())[0]["class"].toString() == "ignored");
    CHECK (r["ignoredLines"].getArray()->size() == 1);
    CHECK ((*r["ignoredLines"].getArray())[0].operator int() == 4);
    CHECK (r["seedHash"].toString().length() == 8);
    CHECK (! static_cast<bool> (r["full"]));
    CHECK (r["chain"].getArray()->size() == 4);

    r = call (vbplugin::handleSetText, p, juce::String::repeatedString ("a", 5000));
    CHECK (static_cast<bool> (r["full"]));
    CHECK (call (vbplugin::handleSetText, p, "").getProperty ("seedHash", "x").toString().isEmpty());
}

TEST_CASE ("chain payload: per module type, name, held, params with mapped values and units", "[bridge]")
{
    JuceInit juce;
    VerbatimProcessor p;
    const auto r = call (vbplugin::handleSetText, p, "hello");
    const auto& lane = *r["chain"].getArray()->getFirst().getProperty ("modules", {}).getArray();
    REQUIRE (lane.size() == 4);
    CHECK (static_cast<int> (lane[0]["type"]) == vb::SAT);
    CHECK (lane[0]["name"].toString() == "SAT");
    CHECK (! static_cast<bool> (lane[0]["held"]));
    const auto& params = *lane[0]["params"].getArray();
    REQUIRE (params.size() == 5); // drive, tone, shape, bias, mix
    CHECK (params[0]["name"].toString() == "drive");
    CHECK (params[0]["text"].toString().endsWith ("dB"));
    CHECK (static_cast<int> (params[0]["u"]) == 23281);
    CHECK (std::abs (static_cast<double> (params[0]["value"]) - 24.0 * 23281.0 / 65535.0) < 1e-3);
    CHECK (params[4]["name"].toString() == "mix");
    CHECK (params[4]["text"].toString().endsWith ("%"));
    // Every effect has named params with text for every catalog type.
    for (std::uint8_t t = 0; t < vb::EFFECT_COUNT; ++t)
    {
        vb::ModuleSpec m;
        m.type = t;
        const auto v = vbplugin::moduleParams (m, 0);
        REQUIRE (v.size() >= 2);
        for (const auto& q : v)
            CHECK (q["text"].toString().isNotEmpty());
    }
}

TEST_CASE ("setOverride via the bridge: hold, edit, add, remove, rejection", "[bridge][U7]")
{
    JuceInit juce;
    VerbatimProcessor p;
    call (vbplugin::handleSetText, p, "hello");
    auto r = call (vbplugin::handleSetOverride, p, overrideArg (0, "hold", 1));
    CHECK (static_cast<bool> (r["ok"]));
    CHECK (static_cast<bool> ((*(*r["chain"].getArray())[0]["modules"].getArray())[1]["held"]));

    auto* mod = new juce::DynamicObject();
    mod->setProperty ("type", vb::RING);
    mod->setProperty ("p", juce::Array<juce::var> { 1000, 0, 0, 0, 0, 0 });
    mod->setProperty ("mix", 30000);
    r = call (vbplugin::handleSetOverride, p, overrideArg (0, "add", -1, juce::var (mod)));
    CHECK (static_cast<bool> (r["ok"]));
    CHECK ((*r["chain"].getArray())[0]["modules"].getArray()->size() == 5);

    r = call (vbplugin::handleSetOverride, p, overrideArg (0, "remove", 4));
    CHECK ((*r["chain"].getArray())[0]["modules"].getArray()->size() == 4);

    r = call (vbplugin::handleSetOverride, p, overrideArg (3, "hold", 0)); // lane 3 does not exist
    CHECK (! static_cast<bool> (r["ok"]));
    r = call (vbplugin::handleSetOverride, p, overrideArg (0, "explode", 0));
    CHECK (! static_cast<bool> (r["ok"]));
}

TEST_CASE ("setUi and getState: ui size and advanced flag persist in state", "[bridge]")
{
    JuceInit juce;
    VerbatimProcessor a;
    auto* ui = new juce::DynamicObject();
    ui->setProperty ("w", 1000);
    ui->setProperty ("h", 700);
    ui->setProperty ("advanced", true);
    call (vbplugin::handleSetUi, a, juce::var (ui));
    call (vbplugin::handleSetText, a, "hello");
    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    VerbatimProcessor b;
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    const auto s = vbplugin::handleGetState (b);
    CHECK (s["text"].toString() == "hello");
    CHECK (static_cast<int> (s["ui"]["w"]) == 1000);
    CHECK (static_cast<int> (s["ui"]["h"]) == 700);
    CHECK (static_cast<bool> (s["ui"]["advanced"]));
    CHECK (s["chain"].getArray()->size() == 1);
    CHECK (s["tokens"].getArray()->size() == 1);
}

TEST_CASE ("levels payload shape and the chain event generation on host state load", "[bridge]")
{
    JuceInit juce;
    VerbatimProcessor p;
    const auto l = vbplugin::levelsPayload (p);
    CHECK (l["lanes"].getArray()->size() == 4);
    CHECK (l["out"].getArray()->size() == 2);
    CHECK (l["clip"].isBool());
    const auto before = p.stateGeneration();
    vbplugin::PluginState s;
    s.text = "hello";
    const auto json = vbplugin::toJson (s);
    p.setStateInformation (json.data(), static_cast<int> (json.size()));
    CHECK (p.stateGeneration() == before + 1);
}

TEST_CASE ("U3 automation round trip: host -> UI relay, UI drag -> host gestures in order", "[U3]")
{
    JuceInit juce;
    VerbatimProcessor p;
    vbplugin::ParamRelays relays (p);

    // Host side changes reach the relays.
    for (auto [id, relay, value] : { std::tuple { "mix", &relays.mix, 42.0f }, std::tuple { "macro", &relays.macro, 0.25f }, std::tuple { "output", &relays.output, -6.0f } })
    {
        auto* param = p.params.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        CHECK (std::abs (relay->getValue() - value) < 1e-4);
    }

    // UI side: drag start, value, drag end on the macro relay.
    struct Gestures : juce::AudioProcessorListener
    {
        std::vector<std::string> log;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float v) override { log.push_back ("value " + std::to_string (v)); }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { log.push_back ("begin"); }
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override { log.push_back ("end"); }
    } g;
    p.addListener (&g);
    const auto options = juce::WebBrowserComponent::Options {}.withOptionsFrom (relays.macro);
    REQUIRE (options.getEventListeners().size() == 1);
    const auto& listener = options.getEventListeners().front().second;
    auto event = [] (const char* type, juce::var value = {}) {
        auto* o = new juce::DynamicObject();
        o->setProperty ("eventType", type);
        if (! value.isVoid())
            o->setProperty ("value", value);
        return juce::var (o);
    };
    listener (event ("sliderDragStarted"));
    listener (event ("valueChanged", 0.75));
    listener (event ("sliderDragEnded"));
    p.removeListener (&g);

    REQUIRE (g.log.size() == 3);
    CHECK (g.log[0] == "begin");
    CHECK (g.log[1].rfind ("value", 0) == 0);
    CHECK (g.log[2] == "end");
    CHECK (std::abs (p.params.getParameter ("macro")->getValue() - 0.75f) < 1e-6);
}

TEST_CASE ("P2 chain build time on the message thread, worst case: median <= 30 ms, max <= 60 ms", "[P2]")
{
    JuceInit juce;
    VerbatimProcessor p;
    p.prepareToPlay (SR, BLOCK);
    // Worst case: 4 lanes, each 6 generated modules plus 2 added (8 modules).
    const std::string text = "wait.. what...\nwait.. what...\nwait.. what...\nwait.. what...";
    p.setText (text);
    for (int lane = 0; lane < 4; ++lane)
    {
        REQUIRE (p.setOverride (lane, vb::OverrideOp::Add, -1, vb::ModuleSpec { vb::VERB, {}, 30000 }));
        REQUIRE (p.setOverride (lane, vb::OverrideOp::Add, -1, vb::ModuleSpec { vb::GRAIN, {}, 30000 }));
    }
    std::vector<double> ms;
    for (int i = 0; i < 30; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        p.setText (i % 2 == 0 ? text + " " : text);
        ms.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count());
    }
    std::sort (ms.begin(), ms.end());
    std::printf ("P2 build median %.2f ms max %.2f ms\n", ms[ms.size() / 2], ms.back());
    CHECK (ms[ms.size() / 2] <= 30.0);
    CHECK (ms.back() <= 60.0);
}

TEST_CASE ("P3 typing storm: 50 text changes per second for 20 s while processing", "[P3]")
{
    JuceInit juce;
    VerbatimProcessor p;
    p.prepareToPlay (SR, BLOCK);
    p.setText ("hello");
    juce::AudioBuffer<float> buf (2, BLOCK);
    juce::MidiBuffer midi;
    vb::SplitMix64 r { 3 };
    auto block = [&] {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < BLOCK; ++i)
                buf.setSample (c, i, static_cast<float> (static_cast<double> (r.next() >> 11) / 9007199254740992.0 - 0.5) * 0.2f);
        p.processBlock (buf, midi);
        bool finite = true, silent = true;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < BLOCK; ++i)
            {
                finite = finite && std::isfinite (buf.getSample (c, i));
                silent = silent && buf.getSample (c, i) == 0.0f;
            }
        return finite && ! silent;
    };
    for (int i = 0; i < 200; ++i) // settle, then take the memory baseline
        block();
    p.collectGarbage();
    const auto baseline = liveAllocs.load();

    static const char* words[] = { "tape", "echo", "dark", "glass", "river", "neon", "!", "...", "~", "&" };
    std::string text;
    bool ok = true;
    const int blocks = static_cast<int> (20 * SR / BLOCK);
    for (int b = 0; b < blocks; ++b)
    {
        if (b % 2 == 0) // one keystroke per ~21 ms (about 50 per second at 512-sample blocks)
        {
            text += words[r.next() % std::size (words)];
            text += ' ';
            if (text.size() > 60)
                text.clear();
            p.setText (text);
        }
        ok = ok && block();
        if (b % 3 == 0)
            p.collectGarbage(); // the editor's 30 Hz timer does this in the plugin
    }
    CHECK (ok);
    CHECK (p.retireOverflows() == 0);

    p.setText ("hello");
    for (int i = 0; i < 200; ++i)
        block();
    p.collectGarbage();
    const auto after = liveAllocs.load();
    std::printf ("P3 live allocations baseline %lld after %lld\n", baseline, after);
    CHECK (std::abs (static_cast<double> (after - baseline)) <= 0.05 * static_cast<double> (baseline));
}
