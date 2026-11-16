#include "State.h"

#include "Catalog.h"
#include "ChainSpec.h"

#include <juce_core/juce_core.h>

namespace vbplugin
{
namespace
{
juce::var moduleToVar (const vb::ModuleSpec& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", static_cast<int> (m.type));
    juce::Array<juce::var> p;
    for (auto v : m.p)
        p.add (static_cast<int> (v));
    o->setProperty ("p", p);
    o->setProperty ("mix", static_cast<int> (m.mix));
    return juce::var (o);
}

bool u16 (const juce::var& v, std::uint16_t& out)
{
    if (! v.isInt() && ! v.isInt64())
        return false;
    const auto x = static_cast<juce::int64> (v);
    if (x < 0 || x > 65535)
        return false;
    out = static_cast<std::uint16_t> (x);
    return true;
}

bool moduleFromVar (const juce::var& v, vb::ModuleSpec& m)
{
    std::uint16_t type = 0;
    if (! v.isObject() || ! u16 (v["type"], type) || type >= vb::EFFECT_COUNT || ! u16 (v["mix"], m.mix))
        return false;
    m.type = static_cast<std::uint8_t> (type);
    const auto* p = v["p"].getArray();
    if (p == nullptr || p->size() != 6)
        return false;
    for (int i = 0; i < 6; ++i)
        if (! u16 ((*p)[i], m.p[static_cast<std::size_t> (i)]))
            return false;
    return true;
}

juce::var overridesToVar (const vb::Overrides& all)
{
    juce::Array<juce::var> lanes;
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const auto& o = all[i];
        if (o == vb::LaneOverride {})
            continue;
        auto* lane = new juce::DynamicObject();
        lane->setProperty ("lane", static_cast<int> (i));
        auto* holds = new juce::DynamicObject();
        for (const auto& [k, m] : o.holds)
            holds->setProperty (juce::Identifier (juce::String (k)), moduleToVar (m));
        lane->setProperty ("holds", juce::var (holds));
        juce::Array<juce::var> removed;
        for (int k : o.removed)
            removed.add (k);
        lane->setProperty ("removed", removed);
        lane->setProperty ("removedForSeed", juce::String::toHexString (static_cast<juce::int64> (o.removedForSeed)).paddedLeft ('0', 16));
        juce::Array<juce::var> added;
        for (const auto& m : o.added)
            added.add (moduleToVar (m));
        lane->setProperty ("added", added);
        lanes.add (juce::var (lane));
    }
    return lanes;
}

bool overridesFromVar (const juce::var& v, vb::Overrides& out)
{
    out = {};
    if (v.isVoid() || v.isUndefined())
        return true; // older blobs without overrides
    const auto* lanes = v.getArray();
    if (lanes == nullptr)
        return false;
    for (const auto& l : *lanes)
    {
        std::uint16_t idx = 0;
        if (! l.isObject() || ! u16 (l["lane"], idx) || idx >= out.size())
            return false;
        auto& o = out[idx];
        if (auto* holds = l["holds"].getDynamicObject())
            for (const auto& prop : holds->getProperties())
            {
                const int k = prop.name.toString().getIntValue();
                vb::ModuleSpec m;
                if (k < 0 || k >= static_cast<int> (vb::GENERATED_SLOTS) || ! moduleFromVar (prop.value, m))
                    return false;
                o.holds[k] = m;
            }
        if (const auto* removed = l["removed"].getArray())
            for (const auto& r : *removed)
            {
                std::uint16_t k = 0;
                if (! u16 (r, k) || k >= vb::GENERATED_SLOTS)
                    return false;
                o.removed.insert (k);
            }
        const auto seedHex = l["removedForSeed"].toString();
        if (seedHex.length() > 16 || ! seedHex.containsOnly ("0123456789abcdefABCDEF"))
            return false;
        o.removedForSeed = static_cast<std::uint64_t> (seedHex.getHexValue64());
        if (const auto* added = l["added"].getArray())
            for (const auto& a : *added)
            {
                vb::ModuleSpec m;
                if (o.added.size() >= vb::MAX_ADDED || ! moduleFromVar (a, m))
                    return false;
                o.added.push_back (m);
            }
    }
    return true;
}
} // namespace

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
    root->setProperty ("overrides", overridesToVar (state.overrides));
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
    if (! overridesFromVar (root["overrides"], s.overrides))
        return false;
    out = s;
    return true;
}
} // namespace vbplugin
