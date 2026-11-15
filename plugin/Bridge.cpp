#include "Bridge.h"

#include "VerbatimProcessor.h"

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

juce::var handleSetText (VerbatimProcessor& processor, const juce::Array<juce::var>& args)
{
    if (args.size() > 0 && args[0].isString())
        processor.setText (args[0].toString().toStdString());
    return {};
}

juce::var handleGetState (VerbatimProcessor& processor)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("text", juce::String::fromUTF8 (processor.getText().c_str()));
    return juce::var (o);
}

juce::WebBrowserComponent::Options webOptions (VerbatimProcessor& processor)
{
    using Options = juce::WebBrowserComponent::Options;
    return Options {}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2 {}.withUserDataFolder (
            juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory).getChildFile ("VerbatimWebView2")))
        .withNativeIntegrationEnabled()
        .withNativeFunction ("setText", [&processor] (const juce::Array<juce::var>& args, auto complete) {
            complete (handleSetText (processor, args));
        })
        .withNativeFunction ("getState", [&processor] (const juce::Array<juce::var>&, auto complete) {
            complete (handleGetState (processor));
        })
        .withResourceProvider ([] (const juce::String& url) { return getResource (url); });
}
} // namespace vbplugin
