#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class VerbatimProcessor;

// Temporary editor (T06): a plain text box that drives the processor. Replaced by the WebView at T07.
class VerbatimEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VerbatimEditor (VerbatimProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    VerbatimProcessor& processor;
    juce::TextEditor textBox;
};
