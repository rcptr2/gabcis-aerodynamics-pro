/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "AboutPanel.h"
#include "AeroDynamicsLookAndFeel.h"

namespace
{
    const char* const kAttributionUtf8 =
        "This plugin was built by Claude Code (Sonnet 5), based on a joint idea by "
        "G\xc3\xa1""bor Tomori and Gemini, under G\xc3\xa1""bor Tomori's direction "
        "and with Gemini's review, in 2026.";

    const char* const kTestedUtf8 = "Tested on Intel Mac OS 15.7.7, Standalone";
}

AboutPanel::AboutPanel()
{
    setInterceptsMouseClicks (true, true);

    titleLabel.setText ("Gabci's AeroDynamics", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::textColour);

    versionLabel.setText (juce::String ("Version ") + JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setFont (juce::FontOptions (14.0f));
    versionLabel.setJustificationType (juce::Justification::centred);
    versionLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::activeGlowLow);

    attributionLabel.setText (juce::String (juce::CharPointer_UTF8 (kAttributionUtf8)), juce::dontSendNotification);
    attributionLabel.setFont (juce::FontOptions (13.5f));
    attributionLabel.setJustificationType (juce::Justification::centred);
    attributionLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::textColour);
    attributionLabel.setMinimumHorizontalScale (1.0f);

    testedLabel.setText (juce::String (juce::CharPointer_UTF8 (kTestedUtf8)), juce::dontSendNotification);
    testedLabel.setFont (juce::FontOptions (12.0f));
    testedLabel.setJustificationType (juce::Justification::centred);
    testedLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::dimTextColour);

    closeHintLabel.setText ("Click anywhere to close", juce::dontSendNotification);
    closeHintLabel.setFont (juce::FontOptions (11.5f));
    closeHintLabel.setJustificationType (juce::Justification::centred);
    closeHintLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::dimTextColour);

    for (auto* label : { &titleLabel, &versionLabel, &attributionLabel, &testedLabel, &closeHintLabel })
    {
        addAndMakeVisible (label);
        label->setInterceptsMouseClicks (false, false);
    }
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.65f));

    const auto cardBounds = getLocalBounds().toFloat().reduced (getWidth() * 0.12f, getHeight() * 0.14f);
    g.setColour (AeroDynamicsLookAndFeel::panelColour);
    g.fillRoundedRectangle (cardBounds, 14.0f);
    g.setColour (AeroDynamicsLookAndFeel::idleGlowColour.withAlpha (0.35f));
    g.drawRoundedRectangle (cardBounds, 14.0f, 1.5f);
}

void AboutPanel::resized()
{
    auto cardBounds = getLocalBounds().reduced ((int) (getWidth() * 0.12f), (int) (getHeight() * 0.14f));
    cardBounds.reduce (24, 20);

    titleLabel.setBounds (cardBounds.removeFromTop (32));
    cardBounds.removeFromTop (6);
    versionLabel.setBounds (cardBounds.removeFromTop (22));
    cardBounds.removeFromTop (18);
    attributionLabel.setBounds (cardBounds.removeFromTop (juce::jmax (60, cardBounds.getHeight() - 70)));
    cardBounds.removeFromTop (10);
    testedLabel.setBounds (cardBounds.removeFromTop (20));
    cardBounds.removeFromTop (10);
    closeHintLabel.setBounds (cardBounds.removeFromTop (18));
}

void AboutPanel::mouseUp (const juce::MouseEvent&)
{
    if (onClose != nullptr)
        onClose();
}
