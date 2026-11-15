#include "VerbatimEditor.h"

#include "Bridge.h"
#include "VerbatimProcessor.h"

VerbatimEditor::VerbatimEditor (VerbatimProcessor& p) : AudioProcessorEditor (p)
{
    const auto options = vbplugin::webOptions (p);
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
    setResizable (true, true);
    setResizeLimits (720, 520, 1600, 1000);
    setSize (900, 620);
}

void VerbatimEditor::resized()
{
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
