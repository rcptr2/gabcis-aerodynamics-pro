/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "PluginEditor.h"

#include <cmath>

namespace
{
    int scaled (int value, float scaleFactor)
    {
        return (int) std::round ((float) value * scaleFactor);
    }
}

AeroDynamicsProAudioProcessorEditor::AeroDynamicsProAudioProcessorEditor (AeroDynamicsProAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      visualizer (p.getVisualizationPublisher(), p.apvts.getRawParameterValue (ParamIDs::turbulence))
{
    setLookAndFeel (&lookAndFeel);

    wordmarkLabel.setText ("Gabci's AeroDynamics", juce::dontSendNotification);
    wordmarkLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    wordmarkLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::textColour);
    addAndMakeVisible (wordmarkLabel);

    aboutButton.onClick = [this] { showAbout(); };
    addAndMakeVisible (aboutButton);

    addAndMakeVisible (visualizer);

    auto setUpKnob = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::dimTextColour);
        addAndMakeVisible (label);
    };

    setUpKnob (pressureSlider, pressureLabel, "Pressure");
    pressureSlider.setName (AeroDynamicsLookAndFeel::pressureSliderName);
    setUpKnob (viscositySlider, viscosityLabel, "Viscosity");
    setUpKnob (turbulenceSlider, turbulenceLabel, "Turbulence");
    setUpKnob (flowRateSlider, flowRateLabel, "Flow Rate");
    setUpKnob (mixSlider, mixLabel, "Mix");

    oversamplingLabel.setText ("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setJustificationType (juce::Justification::centredRight);
    oversamplingLabel.setColour (juce::Label::textColourId, AeroDynamicsLookAndFeel::dimTextColour);
    addAndMakeVisible (oversamplingLabel);

    oversamplingBox.addItemList (OversamplingOption::choices, 1);
    addAndMakeVisible (oversamplingBox);

    addAndMakeVisible (bypassButton);

    auto& apvts = processorRef.apvts;
    pressureAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::pressure, pressureSlider);
    viscosityAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::viscosity, viscositySlider);
    turbulenceAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::turbulence, turbulenceSlider);
    flowRateAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::flowRate, flowRateSlider);
    mixAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::mix, mixSlider);
    oversamplingAttachment = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::oversampling, oversamplingBox);
    bypassAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::bypass, bypassButton);

    // Fixed-aspect-ratio resizing (Phase 4 brief): setResizeLimits() both marks the
    // editor resizable and creates the default ComponentBoundsConstrainer, so the
    // aspect ratio must be set on it afterwards, not before.
    setResizeLimits (400, 300, 1600, 1200);
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) kDefaultWidth / (double) kDefaultHeight);

    setSize (kDefaultWidth, kDefaultHeight);
}

AeroDynamicsProAudioProcessorEditor::~AeroDynamicsProAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AeroDynamicsProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (AeroDynamicsLookAndFeel::backgroundColour);
}

void AeroDynamicsProAudioProcessorEditor::resized()
{
    const auto scaleFactor = (float) getWidth() / (float) kDefaultWidth;
    const auto S = [scaleFactor] (int v) { return scaled (v, scaleFactor); };

    auto bounds = getLocalBounds();

    if (aboutPanel != nullptr)
        aboutPanel->setBounds (bounds);

    auto header = bounds.removeFromTop (S (50));
    aboutButton.setBounds (header.removeFromRight (S (90)).reduced (S (8)));
    wordmarkLabel.setBounds (header.reduced (S (12), 0));
    wordmarkLabel.setFont (juce::FontOptions ((float) S (22), juce::Font::bold));

    auto bottomRow = bounds.removeFromBottom (S (40));
    bottomRow.reduce (S (16), S (4));
    bypassButton.setBounds (bottomRow.removeFromRight (S (110)));
    bottomRow.removeFromRight (S (16));
    oversamplingBox.setBounds (bottomRow.removeFromRight (S (80)));
    bottomRow.removeFromRight (S (8));
    oversamplingLabel.setBounds (bottomRow.removeFromRight (S (110)));

    auto controlsRow = bounds.removeFromBottom (S (150));
    controlsRow.reduce (S (16), S (4));

    auto layoutKnob = [&] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> slot)
    {
        label.setBounds (slot.removeFromTop (S (18)));
        label.setFont (juce::FontOptions ((float) S (13)));
        slider.setBounds (slot);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, S (70), S (18));
    };

    const auto slotWidth = controlsRow.getWidth() / 5;
    layoutKnob (pressureSlider, pressureLabel, controlsRow.removeFromLeft (slotWidth));
    layoutKnob (viscositySlider, viscosityLabel, controlsRow.removeFromLeft (slotWidth));
    layoutKnob (turbulenceSlider, turbulenceLabel, controlsRow.removeFromLeft (slotWidth));
    layoutKnob (flowRateSlider, flowRateLabel, controlsRow.removeFromLeft (slotWidth));
    layoutKnob (mixSlider, mixLabel, controlsRow);

    bounds.reduce (S (16), S (8));
    visualizer.setBounds (bounds);
}

void AeroDynamicsProAudioProcessorEditor::showAbout()
{
    aboutPanel = std::make_unique<AboutPanel>();
    aboutPanel->onClose = [this] { hideAbout(); };
    addAndMakeVisible (*aboutPanel);
    resized();
}

void AeroDynamicsProAudioProcessorEditor::hideAbout()
{
    aboutPanel.reset();
}
