#pragma once
// UI <-> plugin bridge (docs/05-ui-design.md section 6) and the embedded GUI resource provider.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <optional>

class VerbatimProcessor;

namespace vbplugin
{
// Serves the zipped gui/dist build embedded as binary data. "/" maps to index.html.
std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

// Native function handlers (message thread).
juce::var handleSetText (VerbatimProcessor& processor, const juce::Array<juce::var>& args);     // -> { tokens, chain, seedHash, ignoredLines, full }
juce::var handleSetOverride (VerbatimProcessor& processor, const juce::Array<juce::var>& args); // { lane, op, slot, module } -> { ok, chain }
juce::var handleSetUi (VerbatimProcessor& processor, const juce::Array<juce::var>& args);       // { w, h, advanced }
juce::var handleGetState (VerbatimProcessor& processor);                                        // text, tokens, chain, ui, stateError

// Event payloads (C++ -> UI).
juce::var levelsPayload (VerbatimProcessor& processor); // { lanes: [rms x4], out: [peakL, peakR], clip }
juce::var chainEventPayload (VerbatimProcessor& processor);

// Slider relays for the three host params, attached with gesture begin / end.
struct ParamRelays
{
    explicit ParamRelays (VerbatimProcessor& processor);
    juce::WebSliderRelay mix { "mix" }, macro { "macro" }, output { "output" };
    juce::WebSliderParameterAttachment mixAttachment, macroAttachment, outputAttachment;
};

// WebView2 backend, per-user temp user-data folder (A8), native integration, functions, relays, resources.
juce::WebBrowserComponent::Options webOptions (VerbatimProcessor& processor, ParamRelays& relays);
} // namespace vbplugin
