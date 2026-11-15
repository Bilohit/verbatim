#include "VerbatimProcessor.h"

VerbatimProcessor::VerbatimProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void VerbatimProcessor::prepareToPlay (double, int) {}

bool VerbatimProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VerbatimProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Pass-through: input buffer is the output buffer.
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VerbatimProcessor();
}
