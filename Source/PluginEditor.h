/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AboutPanel.h"
#include "AeroDynamicsLookAndFeel.h"
#include "FluidVisualizerComponent.h"
#include "PluginProcessor.h"

//==============================================================================
/** Main editor for Gabci's AeroDynamics -- Phase 4 UI, per the design brief:

    - Resizable with a LOCKED aspect ratio (setFixedAspectRatio on the
      ComponentBoundsConstrainer), default 800x600. resized() computes a single
      `scaleFactor = getWidth() / (float) kDefaultWidth` and applies it to every
      font size and layout dimension -- vector paths and JUCE Font rendering both
      stay resolution-sharp under this kind of scaling (unlike stretching a
      pre-rendered bitmap), which is what keeps typography crisp at any window size.
    - The heavy, continuously-animated part of the UI (FluidVisualizerComponent) is
      fully separated from the knobs/labels: the knobs are plain juce::Slider/
      ComboBox/ToggleButton Components (via a custom AeroDynamicsLookAndFeel) that
      JUCE already only repaints on interaction; the visualizer runs its own 60Hz
      Timer independently. Nothing here drives a global repaint-everything loop.
*/
class AeroDynamicsProAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AeroDynamicsProAudioProcessorEditor (AeroDynamicsProAudioProcessor&);
    ~AeroDynamicsProAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kDefaultWidth = 800;
    static constexpr int kDefaultHeight = 600;

    void showAbout();
    void hideAbout();

    AeroDynamicsProAudioProcessor& processorRef;
    AeroDynamicsLookAndFeel lookAndFeel;

    // Header.
    juce::Label wordmarkLabel;
    juce::TextButton aboutButton { "About" };

    // Visualization.
    FluidVisualizerComponent visualizer;

    // Controls.
    juce::Slider pressureSlider, viscositySlider, turbulenceSlider, flowRateSlider, mixSlider;
    juce::Label pressureLabel, viscosityLabel, turbulenceLabel, flowRateLabel, mixLabel, oversamplingLabel;
    juce::ComboBox oversamplingBox;
    juce::ToggleButton bypassButton { "Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> pressureAttachment, viscosityAttachment, turbulenceAttachment,
                                       flowRateAttachment, mixAttachment;
    std::unique_ptr<ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    std::unique_ptr<AboutPanel> aboutPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroDynamicsProAudioProcessorEditor)
};
