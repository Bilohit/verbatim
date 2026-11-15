#pragma once
// UI <-> plugin bridge (docs/05-ui-design.md section 6) and the embedded GUI resource provider.
#include <juce_gui_extra/juce_gui_extra.h>

#include <optional>

class VerbatimProcessor;

namespace vbplugin
{
// Serves the zipped gui/dist build embedded as binary data. "/" maps to index.html.
std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

// Native function handlers (message thread).
juce::var handleSetText (VerbatimProcessor& processor, const juce::Array<juce::var>& args);
juce::var handleGetState (VerbatimProcessor& processor);

// WebView2 backend, per-user temp user-data folder (A8), native integration, functions and resources.
juce::WebBrowserComponent::Options webOptions (VerbatimProcessor& processor);
} // namespace vbplugin
