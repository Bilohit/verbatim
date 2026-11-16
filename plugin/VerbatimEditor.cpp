#include "VerbatimEditor.h"

#include "VerbatimProcessor.h"

VerbatimEditor::VerbatimEditor (VerbatimProcessor& p) : AudioProcessorEditor (p), processor (p), relays (p)
{
    const auto options = vbplugin::webOptions (p, relays);
    if (juce::WebBrowserComponent::areOptionsSupported (options))
    {
        web = std::make_unique<juce::WebBrowserComponent> (options);
        addAndMakeVisible (*web);
        web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }
    else
    {
        // E4 (docs/05-ui-design.md section 3): audio keeps processing the saved state.
        missingLabel.setText ("Verbatim needs the Microsoft WebView2 runtime.", juce::dontSendNotification);
        missingLabel.setJustificationType (juce::Justification::centred);
        downloadLink.setButtonText ("Download WebView2");
        downloadLink.setURL (juce::URL ("https://developer.microsoft.com/microsoft-edge/webview2/"));
        addAndMakeVisible (missingLabel);
        addAndMakeVisible (downloadLink);
    }
    seenGeneration = p.stateGeneration();
    const auto ui = p.getUi();
    setResizable (true, true);
    setResizeLimits (720, 520, 1600, 1000);
    setSize (ui.w, ui.h);
    startTimerHz (30);
}

VerbatimEditor::~VerbatimEditor()
{
    stopTimer();
}

void VerbatimEditor::timerCallback()
{
    if (web == nullptr)
        return;
    // emitEventIfBrowserIsVisible only sends while the editor is showing (K2: never before page load).
    web->emitEventIfBrowserIsVisible ("levels", vbplugin::levelsPayload (processor));
    if (const int g = processor.stateGeneration(); g != seenGeneration)
    {
        seenGeneration = g;
        web->emitEventIfBrowserIsVisible ("chain", vbplugin::chainEventPayload (processor));
    }
}

void VerbatimEditor::resized()
{
    auto ui = processor.getUi();
    ui.w = getWidth();
    ui.h = getHeight();
    processor.setUi (ui);
    if (web != nullptr)
    {
        web->setBounds (getLocalBounds());
        return;
    }
    auto r = getLocalBounds().withSizeKeepingCentre (getWidth(), 80);
    missingLabel.setBounds (r.removeFromTop (40));
    downloadLink.setBounds (r.withSizeKeepingCentre (240, 32));
}

void VerbatimEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c2b26));
}
