#include "State.h"

#include "ChainSpec.h"

#include <juce_core/juce_core.h>

namespace vbplugin
{
std::string toJson (const PluginState& state)
{
    auto* params = new juce::DynamicObject();
    params->setProperty ("mix", state.mix);
    params->setProperty ("macro", state.macro);
    params->setProperty ("output", state.outputDb);

    auto* root = new juce::DynamicObject();
    root->setProperty ("stateVersion", STATE_VERSION);
    root->setProperty ("grammarVersion", static_cast<int> (vb::GRAMMAR_VERSION));
    // shortcut: text goes through juce::String, so invalid UTF-8 bytes are not preserved; the UI only produces valid UTF-8.
    root->setProperty ("text", juce::String::fromUTF8 (state.text.data(), static_cast<int> (state.text.size())));
    root->setProperty ("params", juce::var (params));
    return juce::JSON::toString (juce::var (root), true).toStdString();
}

bool fromJson (const std::string& json, PluginState& out)
{
    juce::var root;
    if (json.empty() || juce::JSON::parse (juce::String::fromUTF8 (json.data(), static_cast<int> (json.size())), root).failed())
        return false;
    if (! root.isObject())
        return false;
    const auto stateVersion = root["stateVersion"];
    const auto grammarVersion = root["grammarVersion"];
    const auto text = root["text"];
    if (! stateVersion.isInt() || static_cast<int> (stateVersion) != STATE_VERSION)
        return false;
    if (! grammarVersion.isInt() || static_cast<int> (grammarVersion) != vb::GRAMMAR_VERSION)
        return false;
    if (! text.isString())
        return false;

    PluginState s;
    s.text = text.toString().toStdString();
    const auto params = root["params"];
    if (params.isObject())
    {
        auto num = [&] (const char* key, float fallback) {
            const auto v = params[key];
            return v.isDouble() || v.isInt() ? static_cast<float> (static_cast<double> (v)) : fallback;
        };
        s.mix = juce::jlimit (0.0f, 100.0f, num ("mix", s.mix));
        s.macro = juce::jlimit (0.0f, 1.0f, num ("macro", s.macro));
        s.outputDb = juce::jlimit (-24.0f, 12.0f, num ("output", s.outputDb));
    }
    out = s;
    return true;
}
} // namespace vbplugin
