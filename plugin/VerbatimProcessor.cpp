#include "VerbatimProcessor.h"

#include "State.h"
#include "VerbatimEditor.h"

#include "Generate.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    return { std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f),
             std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "macro", 1 }, "Macro", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f),
             std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f),
             std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false) };
}
} // namespace

VerbatimProcessor::VerbatimProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "params", layout())
{
    mix = dynamic_cast<juce::AudioParameterFloat*> (params.getParameter ("mix"));
    macro = dynamic_cast<juce::AudioParameterFloat*> (params.getParameter ("macro"));
    output = dynamic_cast<juce::AudioParameterFloat*> (params.getParameter ("output"));
    bypass = dynamic_cast<juce::AudioParameterBool*> (params.getParameter ("bypass"));
    startTimerHz (30);
}

VerbatimProcessor::~VerbatimProcessor()
{
    stopTimer();
}

void VerbatimProcessor::prepareToPlay (double sampleRate, int block)
{
    maxBlock = block;
    scratch.setSize (2, block);
    engine.prepare (sampleRate, block); // builds the chain for the current text synchronously
    setLatencySamples (engine.latencySamples());
}

void VerbatimProcessor::reset()
{
    engine.reset();
}

bool VerbatimProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // mono->mono, mono->stereo, stereo->stereo (docs/02-spec.md section 1).
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    return (in == mono && out == mono) || (in == mono && out == stereo) || (in == stereo && out == stereo);
}

void VerbatimProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if (maxBlock == 0 || ins == 0 || outs == 0)
        return;

    engine.setMix (mix->get() / 100.0f);
    engine.setOutputDb (output->get());
    engine.setBypass (bypass->get());

    double bpm = 0.0; // unknown -> 120 (A3)
    bool playing = false;
    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
        {
            if (const auto b = pos->getBpm())
                bpm = *b;
            playing = pos->getIsPlaying();
        }
    engine.setTransport (bpm, playing);

    // Internal processing is always stereo; hosts may send blocks larger than prepared.
    auto* L = scratch.getWritePointer (0);
    auto* R = scratch.getWritePointer (1);
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
    {
        const int n = std::min (maxBlock, buffer.getNumSamples() - start);
        std::copy_n (buffer.getReadPointer (0, start), n, L);
        std::copy_n (buffer.getReadPointer (ins > 1 ? 1 : 0, start), n, R);
        engine.process (L, R, n);
        if (outs == 1)
        {
            auto* o = buffer.getWritePointer (0, start);
            for (int i = 0; i < n; ++i)
                o[i] = 0.5f * (L[i] + R[i]);
        }
        else
        {
            std::copy_n (L, n, buffer.getWritePointer (0, start));
            std::copy_n (R, n, buffer.getWritePointer (1, start));
        }
    }
}

juce::AudioProcessorEditor* VerbatimProcessor::createEditor()
{
    return new VerbatimEditor (*this);
}

void VerbatimProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    std::lock_guard<std::mutex> lock (stateMutex);
    if (stateError)
    {
        dest.append (unloadableBlob.data(), unloadableBlob.size());
        return;
    }
    vbplugin::PluginState s;
    s.text = text;
    s.mix = mix->get();
    s.macro = macro->get();
    s.outputDb = output->get();
    s.bypass = bypass->get();
    s.overrides = overrides;
    s.uiW = ui.w;
    s.uiH = ui.h;
    s.advanced = ui.advanced;
    const auto json = vbplugin::toJson (s);
    dest.append (json.data(), json.size());
}

void VerbatimProcessor::setStateInformation (const void* data, int size)
{
    const std::string blob (static_cast<const char*> (data), static_cast<std::size_t> (std::max (size, 0)));
    vbplugin::PluginState s;
    if (! vbplugin::fromJson (blob, s))
    {
        // State E3: audio passthrough, keep the stored blob until the user types.
        std::lock_guard<std::mutex> lock (stateMutex);
        stateError = true;
        unloadableBlob = blob;
        text.clear();
        spec = {};
        overrides = {};
        engine.setSpec (spec);
        ++generation;
        return;
    }
    mix->setValueNotifyingHost (mix->convertTo0to1 (s.mix));
    macro->setValueNotifyingHost (macro->convertTo0to1 (s.macro));
    output->setValueNotifyingHost (output->convertTo0to1 (s.outputDb));
    bypass->setValueNotifyingHost (s.bypass ? 1.0f : 0.0f); // VST3 validator: bypass persists with the state
    {
        std::lock_guard<std::mutex> lock (stateMutex);
        overrides = s.overrides;
        ui = { s.uiW, s.uiH, s.advanced };
    }
    applyText (s.text);
    ++generation;
}

void VerbatimProcessor::setText (const std::string& newText)
{
    applyText (newText);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

std::string VerbatimProcessor::getText() const
{
    std::lock_guard<std::mutex> lock (stateMutex);
    return text;
}

bool VerbatimProcessor::setOverride (int lane, vb::OverrideOp op, int position, const vb::ModuleSpec& module)
{
    {
        std::lock_guard<std::mutex> lock (stateMutex);
        if (stateError || ! vb::applyOverride (overrides, spec, lane, op, position, module))
            return false;
        engine.setResolved (vb::resolve (spec, overrides));
    }
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

VerbatimProcessor::UiState VerbatimProcessor::getUi() const
{
    std::lock_guard<std::mutex> lock (stateMutex);
    return ui;
}

void VerbatimProcessor::setUi (const UiState& newUi)
{
    std::lock_guard<std::mutex> lock (stateMutex);
    ui = newUi;
}

vb::ResolvedChain VerbatimProcessor::getResolved() const
{
    std::lock_guard<std::mutex> lock (stateMutex);
    return vb::resolve (spec, overrides);
}

bool VerbatimProcessor::isStateError() const
{
    std::lock_guard<std::mutex> lock (stateMutex);
    return stateError;
}

void VerbatimProcessor::applyText (const std::string& newText)
{
    std::lock_guard<std::mutex> lock (stateMutex);
    text = newText;
    stateError = false;
    unloadableBlob.clear();
    spec = vb::generate (newText);
    engine.setResolved (vb::resolve (spec, overrides)); // synchronous build (docs/02-spec.md section 2)
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VerbatimProcessor();
}
