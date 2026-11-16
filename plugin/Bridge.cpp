#include "Bridge.h"

#include "VerbatimProcessor.h"

#include "Catalog.h"
#include "Hash.h"
#include "ParamText.h"
#include "Parse.h"

#include <map>

#include <BinaryData.h>

#include <map>

namespace vbplugin
{
namespace
{
juce::ZipFile& guiZip()
{
    static juce::MemoryInputStream stream (VerbatimGui::gui_zip, static_cast<std::size_t> (VerbatimGui::gui_zipSize), false);
    static juce::ZipFile zip (stream);
    return zip;
}

juce::String mimeFor (const juce::String& path)
{
    static const std::map<juce::String, juce::String> types {
        { "html", "text/html" }, { "js", "text/javascript" }, { "css", "text/css" }, { "svg", "image/svg+xml" },
        { "woff2", "font/woff2" }, { "png", "image/png" }, { "json", "application/json" },
    };
    const auto it = types.find (path.fromLastOccurrenceOf (".", false, false).toLowerCase());
    return it != types.end() ? it->second : juce::String ("application/octet-stream");
}
} // namespace

std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);
    auto& zip = guiZip();
    // The zip is built with "cmake -E tar" from inside dist/, so entries may carry a "./" prefix.
    const auto* entry = zip.getEntry (path);
    if (entry == nullptr)
        entry = zip.getEntry ("./" + path);
    if (entry == nullptr)
        return std::nullopt;

    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (*entry));
    if (in == nullptr)
        return std::nullopt;
    std::vector<std::byte> data (static_cast<std::size_t> (in->getTotalLength()));
    in->read (data.data(), data.size());
    return juce::WebBrowserComponent::Resource { std::move (data), mimeFor (path) };
}

namespace
{
juce::var textPayload (VerbatimProcessor& processor)
{
    const auto text = processor.getText();
    const auto parsed = vb::parse (text);
    const auto chain = processor.getResolved();

    // Postmark: first 8 hex digits of XXH64 over every lane seed (little-endian); empty without lanes.
    juce::String seedHash;
    if (! chain.lanes.empty())
    {
        std::vector<std::uint8_t> bytes;
        for (const auto& l : chain.lanes)
            for (int b = 0; b < 8; ++b)
                bytes.push_back (static_cast<std::uint8_t> (l.lane.seed >> (8 * b)));
        seedHash = juce::String::toHexString (static_cast<juce::int64> (vb::xxh64 (bytes.data(), bytes.size(), 0))).paddedLeft ('0', 16).substring (0, 8);
    }
    juce::Array<juce::var> ignored;
    for (std::size_t i = 0; i < parsed.lines.size(); ++i)
        if (parsed.lines[i].ignored)
            ignored.add (static_cast<int> (i));

    auto* o = new juce::DynamicObject();
    o->setProperty ("text", juce::String::fromUTF8 (text.data(), static_cast<int> (text.size())));
    o->setProperty ("tokens", tokensPayload (text));
    o->setProperty ("chain", chainPayload (chain));
    o->setProperty ("seedHash", seedHash);
    o->setProperty ("ignoredLines", ignored);
    o->setProperty ("full", text.size() >= vb::MAX_TEXT_BYTES);
    o->setProperty ("stateError", processor.isStateError());
    return juce::var (o);
}

bool moduleArg (const juce::var& v, vb::ModuleSpec& m)
{
    if (! v.isObject())
        return false;
    const int type = v["type"];
    if (type < 0 || type >= vb::EFFECT_COUNT)
        return false;
    m.type = static_cast<std::uint8_t> (type);
    m.mix = static_cast<std::uint16_t> (juce::jlimit (0, 65535, static_cast<int> (v["mix"])));
    if (const auto* p = v["p"].getArray())
        for (int i = 0; i < std::min (6, p->size()); ++i)
            m.p[static_cast<std::size_t> (i)] = static_cast<std::uint16_t> (juce::jlimit (0, 65535, static_cast<int> ((*p)[i])));
    return true;
}
} // namespace

juce::var handleSetText (VerbatimProcessor& processor, const juce::Array<juce::var>& args)
{
    if (args.size() > 0 && args[0].isString())
        processor.setText (args[0].toString().toStdString());
    return textPayload (processor);
}

juce::var handleSetOverride (VerbatimProcessor& processor, const juce::Array<juce::var>& args)
{
    bool ok = false;
    if (args.size() > 0 && args[0].isObject())
    {
        const auto& a = args[0];
        static const std::map<juce::String, vb::OverrideOp> ops { { "hold", vb::OverrideOp::Hold }, { "release", vb::OverrideOp::Release },
                                                                  { "edit", vb::OverrideOp::Edit }, { "remove", vb::OverrideOp::Remove },
                                                                  { "add", vb::OverrideOp::Add } };
        const auto op = ops.find (a["op"].toString());
        vb::ModuleSpec m;
        const bool needsModule = op != ops.end() && (op->second == vb::OverrideOp::Edit || op->second == vb::OverrideOp::Add);
        if (op != ops.end() && (! needsModule || moduleArg (a["module"], m)))
            ok = processor.setOverride (static_cast<int> (a["lane"]), op->second, static_cast<int> (a["slot"]), m);
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", ok);
    o->setProperty ("chain", chainPayload (processor.getResolved()));
    return juce::var (o);
}

juce::var handleSetUi (VerbatimProcessor& processor, const juce::Array<juce::var>& args)
{
    if (args.size() > 0 && args[0].isObject())
    {
        auto ui = processor.getUi();
        const auto& a = args[0];
        if (a.hasProperty ("w"))
            ui.w = juce::jlimit (720, 1600, static_cast<int> (a["w"]));
        if (a.hasProperty ("h"))
            ui.h = juce::jlimit (520, 1000, static_cast<int> (a["h"]));
        if (a.hasProperty ("advanced"))
            ui.advanced = static_cast<bool> (a["advanced"]);
        processor.setUi (ui);
    }
    return {};
}

juce::var handleGetState (VerbatimProcessor& processor)
{
    auto state = textPayload (processor);
    const auto ui = processor.getUi();
    auto* u = new juce::DynamicObject();
    u->setProperty ("w", ui.w);
    u->setProperty ("h", ui.h);
    u->setProperty ("advanced", ui.advanced);
    state.getDynamicObject()->setProperty ("ui", juce::var (u));
    return state;
}

juce::var levelsPayload (VerbatimProcessor& processor)
{
    const auto& m = processor.meters();
    juce::Array<juce::var> lanes;
    for (const auto& l : m.laneRms)
        lanes.add (l.load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("lanes", lanes);
    o->setProperty ("out", juce::Array<juce::var> { m.peakL.load(), m.peakR.load() });
    o->setProperty ("clip", m.clip.load());
    return juce::var (o);
}

juce::var chainEventPayload (VerbatimProcessor& processor)
{
    return handleGetState (processor);
}

ParamRelays::ParamRelays (VerbatimProcessor& p)
    : mixAttachment (*p.params.getParameter ("mix"), mix, nullptr),
      macroAttachment (*p.params.getParameter ("macro"), macro, nullptr),
      outputAttachment (*p.params.getParameter ("output"), output, nullptr)
{
}

juce::WebBrowserComponent::Options webOptions (VerbatimProcessor& processor, ParamRelays& relays)
{
    using Options = juce::WebBrowserComponent::Options;
    auto fn = [&processor] (juce::var (*handler) (VerbatimProcessor&, const juce::Array<juce::var>&)) {
        return [&processor, handler] (const juce::Array<juce::var>& args, auto complete) { complete (handler (processor, args)); };
    };
    return Options {}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2 {}.withUserDataFolder (
            juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory).getChildFile ("VerbatimWebView2")))
        .withNativeIntegrationEnabled()
        .withOptionsFrom (relays.mix)
        .withOptionsFrom (relays.macro)
        .withOptionsFrom (relays.output)
        .withNativeFunction ("setText", fn (handleSetText))
        .withNativeFunction ("setOverride", fn (handleSetOverride))
        .withNativeFunction ("setUi", fn (handleSetUi))
        .withNativeFunction ("getState", [&processor] (const juce::Array<juce::var>&, auto complete) { complete (handleGetState (processor)); })
        .withResourceProvider ([] (const juce::String& url) { return getResource (url); });
}
} // namespace vbplugin
