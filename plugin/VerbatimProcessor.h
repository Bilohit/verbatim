#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Engine.h"

#include <atomic>
#include <mutex>
#include <string>

// Verbatim audio processor: host params, state, and the text -> chain path (docs/02-spec.md).
class VerbatimProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    VerbatimProcessor();
    ~VerbatimProcessor() override;

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    // Message thread: user typed. Regenerates the chain and marks the project dirty.
    void setText (const std::string& text);
    std::string getText() const;
    bool isStateError() const;

    // Message thread: hold / release / edit / remove / add (docs/02-spec.md R1-R7). False when rejected.
    bool setOverride (int lane, vb::OverrideOp op, int position, const vb::ModuleSpec& module = {});
    vb::ResolvedChain getResolved() const;

    struct UiState
    {
        int w = 900, h = 620;
        bool advanced = false;
    };
    UiState getUi() const;
    void setUi (const UiState& ui);

    // Incremented whenever the host loads state (any thread); the editor sends a `chain` event on change.
    int stateGeneration() const { return generation.load(); }
    const vbdsp::Engine::Meters& meters() const { return engine.meters; }
    void collectGarbage() { engine.collectGarbage(); }
    int retireOverflows() const { return engine.retireOverflows(); }

    juce::AudioProcessorValueTreeState params;

private:
    void applyText (const std::string& text); // builds the chain synchronously
    void timerCallback() override { engine.collectGarbage(); } // 30 Hz, message thread: delete retired chains

    vbdsp::Engine engine;
    juce::AudioParameterFloat* mix = nullptr;
    juce::AudioParameterFloat* macro = nullptr;
    juce::AudioParameterFloat* output = nullptr;
    juce::AudioParameterBool* bypass = nullptr;

    mutable std::mutex stateMutex; // non-audio threads only
    std::string text;
    vb::ChainSpec spec;
    vb::Overrides overrides;
    bool stateError = false;
    UiState ui;
    std::atomic<int> generation { 0 };
    std::string unloadableBlob; // kept verbatim while in state E3

    juce::AudioBuffer<float> scratch;
    int maxBlock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerbatimProcessor)
};
