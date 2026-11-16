#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "Bridge.h"

class VerbatimProcessor;

// Hosts the web UI in WebView2. If the runtime is missing, shows the native E4 message instead.
class VerbatimEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VerbatimEditor (VerbatimProcessor&);
    ~VerbatimEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override; // 30 Hz: levels while visible, chain after a host state load

    VerbatimProcessor& processor;
    vbplugin::ParamRelays relays;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::Label missingLabel;           // E4
    juce::HyperlinkButton downloadLink; // E4
    int seenGeneration = 0;
};
