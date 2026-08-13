/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Dark, mystical/alchemical look for every control in the editor, per the Phase 4
    UI brief: deep obsidian background, dark glowing green at rest, warm glowing
    amber/gold when a control is driven hard, and everything typographic (labels,
    numeric text boxes, the value pointer line) drawn as razor-sharp flat colour --
    only the arcs/auras get the soft "glow" treatment (layered translucent strokes,
    the standard cheap fake-glow technique -- no blur/OpenGL needed).
*/
class AeroDynamicsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AeroDynamicsLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    static const juce::Colour backgroundColour;
    static const juce::Colour panelColour;
    static const juce::Colour idleGlowColour;
    static const juce::Colour activeGlowLow;
    static const juce::Colour activeGlowHigh;
    static const juce::Colour textColour;
    static const juce::Colour dimTextColour;

    /** The slider Component::setName() this LookAndFeel looks for to draw the
        Pressure-specific amber aura above 60% (see the Phase 4 UI brief's
        "Parameter-Reactive Styling" point). Set on the Pressure juce::Slider in
        PluginEditor.
    */
    static constexpr const char* pressureSliderName = "pressure";
};
