#include "VerbatimEditor.h"

#include "VerbatimProcessor.h"

VerbatimEditor::VerbatimEditor (VerbatimProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    textBox.setMultiLine (true, true);
    textBox.setReturnKeyStartsNewLine (true);
    textBox.setText (juce::String::fromUTF8 (p.getText().c_str()), false);
    textBox.onTextChange = [this] { processor.setText (textBox.getText().toStdString()); };
    addAndMakeVisible (textBox);
    setSize (600, 300);
}

void VerbatimEditor::resized()
{
    textBox.setBounds (getLocalBounds().reduced (16));
}

void VerbatimEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}
