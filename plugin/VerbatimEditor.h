#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class VerbatimProcessor;

// Hosts the web UI in WebView2. If the runtime is missing, shows the native E4 message instead.
class VerbatimEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VerbatimEditor (VerbatimProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::Label missingLabel;           // E4
    juce::HyperlinkButton downloadLink; // E4
};
