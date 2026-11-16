// T07: bridge handlers, embedded GUI resources, U9 (10 cycles). docs/06-test-plan.md.
#include <catch2/catch_test_macros.hpp>

#include "plugin/Bridge.h"
#include "plugin/VerbatimProcessor.h"

namespace
{
struct JuceInit
{
    juce::ScopedJuceInitialiser_GUI init;
};

std::string asString (const juce::WebBrowserComponent::Resource& r)
{
    return std::string (reinterpret_cast<const char*> (r.data.data()), r.data.size());
}
} // namespace

TEST_CASE ("Embedded GUI: root serves index.html, assets resolve, unknown paths do not", "[bridge]")
{
    JuceInit juce;
    const auto index = vbplugin::getResource ("/");
    REQUIRE (index.has_value());
    CHECK (index->mimeType == "text/html");
    const auto html = asString (*index);
    CHECK (html.find ("id=\"root\"") != std::string::npos);

    // The script referenced by index.html is served with a JavaScript MIME type.
    const auto at = html.find ("assets/");
    REQUIRE (at != std::string::npos);
    const auto script = html.substr (at, html.find ('"', at) - at);
    const auto js = vbplugin::getResource ("/" + script);
    REQUIRE (js.has_value());
    CHECK (js->mimeType == "text/javascript");
    CHECK (! js->data.empty());

    CHECK (! vbplugin::getResource ("/nope.txt").has_value());
}

TEST_CASE ("Bridge setText and getState drive the processor", "[bridge]")
{
    JuceInit juce;
    VerbatimProcessor p;
    vbplugin::handleSetText (p, juce::Array<juce::var> { juce::var ("dark tape, slow echo!") });
    CHECK (p.getText() == "dark tape, slow echo!");

    const auto state = vbplugin::handleGetState (p);
    CHECK (state["text"].toString() == "dark tape, slow echo!");

    // Non-ASCII survives the trip through juce::var.
    vbplugin::handleSetText (p, juce::Array<juce::var> { juce::var (juce::String::fromUTF8 ("\xD0\xBF\xD1\x80 \xE2\x9C\x89")) });
    CHECK (p.getText() == "\xD0\xBF\xD1\x80 \xE2\x9C\x89");
}

TEST_CASE ("U9 editor open and close 100 times, and two editors at once", "[U9]")
{
    JuceInit juce;
    VerbatimProcessor p;
    p.prepareToPlay (48000.0, 512);
    for (int i = 0; i < 100; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        REQUIRE (e != nullptr);
        CHECK (e->getWidth() == 900);
        CHECK (e->getHeight() == 620);
    }
    std::unique_ptr<juce::AudioProcessorEditor> a (p.createEditor()), b (p.createEditor());
    CHECK ((a != nullptr && b != nullptr));
}
