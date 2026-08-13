/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "AeroDynamicsLookAndFeel.h"

const juce::Colour AeroDynamicsLookAndFeel::backgroundColour { 0xff0a110d };
const juce::Colour AeroDynamicsLookAndFeel::panelColour { 0xff121f19 };
const juce::Colour AeroDynamicsLookAndFeel::idleGlowColour { 0xff2fbd77 };
const juce::Colour AeroDynamicsLookAndFeel::activeGlowLow { 0xffff9100 };
const juce::Colour AeroDynamicsLookAndFeel::activeGlowHigh { 0xffffb300 };
const juce::Colour AeroDynamicsLookAndFeel::textColour { 0xfff5ecd8 };
const juce::Colour AeroDynamicsLookAndFeel::dimTextColour { 0xff8fa79b };

AeroDynamicsLookAndFeel::AeroDynamicsLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, backgroundColour);
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textColour);
    setColour (juce::ComboBox::backgroundColourId, panelColour);
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, panelColour);
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, activeGlowLow.withAlpha (0.3f));
    setColour (juce::TextButton::buttonColourId, panelColour);
    setColour (juce::TextButton::textColourOffId, textColour);
}

void AeroDynamicsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPosProportional, float rotaryStartAngle,
                                                 float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineThickness = juce::jmax (2.0f, radius * 0.12f);
    const auto arcRadius = radius - lineThickness * 0.5f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // The Pressure knob's own amber aura (Phase 4 brief: "Pressure Glow (>60%)") --
    // drawn BEFORE the track/arc so it sits behind them, a soft radial bloom whose
    // alpha and radius both scale with how far past 0.6 the value is. The gradient's
    // falloff-to-transparent must land WITHIN (or just past) the knob's own visible
    // bounds -- JUCE clips this Graphics call to the slider component's rectangle,
    // so a gradient radius much larger than that (an earlier version went up to 2x
    // the knob's own radius) only ever shows its high-alpha CENTRE, which reads as a
    // flat, nearly-opaque fill instead of a glow -- reproduced live and reported as
    // a visual bug, not a subtle aura, at high Pressure values.
    if (slider.getName() == pressureSliderName && sliderPosProportional > 0.6f)
    {
        const auto glowAmount = (sliderPosProportional - 0.6f) / 0.4f; // 0 at 60%, 1 at 100%
        const auto glowRadius = radius * (1.05f + 0.35f * glowAmount);

        juce::ColourGradient aura (activeGlowHigh.withAlpha (0.35f * glowAmount), centre,
                                    activeGlowHigh.withAlpha (0.0f), centre.translated (glowRadius, 0.0f), true);
        g.setGradientFill (aura);
        g.fillEllipse (centre.x - glowRadius, centre.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (idleGlowColour.withAlpha (0.18f));
    g.strokePath (track, juce::PathStrokeType (lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value colour builds from the idle magical-green toward the active amber/gold
    // as the value rises, per the brief's "Active/Turbulent" colour spec.
    const auto valueColour = idleGlowColour.interpolatedWith (activeGlowHigh, sliderPosProportional);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);

    // Cheap fake-glow: a wider, translucent pass behind a crisp normal-width pass --
    // no blur/OpenGL, just layered strokes (same trick JUCE examples use).
    g.setColour (valueColour.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (lineThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (valueColour);
    g.strokePath (value, juce::PathStrokeType (lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value pointer and centre cap: flat, sharp, high-contrast -- never part of the
    // glow layer, per the brief's readability requirement.
    const auto pointerLength = arcRadius * 0.72f;
    juce::Point<float> pointerEnd (centre.x + pointerLength * std::sin (angle), centre.y - pointerLength * std::cos (angle));
    g.setColour (textColour);
    g.drawLine (juce::Line<float> (centre, pointerEnd), lineThickness * 0.55f);

    g.setColour (panelColour);
    g.fillEllipse (centre.x - radius * 0.28f, centre.y - radius * 0.28f, radius * 0.56f, radius * 0.56f);
}

void AeroDynamicsLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                 bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    const auto pillHeight = juce::jmin (bounds.getHeight(), 22.0f);
    const auto pill = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - pillHeight * 0.5f, 42.0f, pillHeight);

    g.setColour (button.getToggleState() ? activeGlowLow.withAlpha (0.85f) : panelColour.brighter (0.1f));
    g.fillRoundedRectangle (pill, pillHeight * 0.5f);

    const auto knobDiameter = pillHeight - 6.0f;
    const auto knobX = button.getToggleState() ? pill.getRight() - knobDiameter - 3.0f : pill.getX() + 3.0f;
    g.setColour (textColour);
    g.fillEllipse (knobX, pill.getY() + 3.0f, knobDiameter, knobDiameter);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (button.getButtonText(), bounds.withTrimmedLeft (pill.getWidth() + 10.0f),
               juce::Justification::centredLeft);
}

void AeroDynamicsLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                             int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (idleGlowColour.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.2f);

    const auto arrowBounds = juce::Rectangle<float> ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH).reduced (8.0f);
    juce::Path arrow;
    arrow.addTriangle (arrowBounds.getX(), arrowBounds.getY(),
                       arrowBounds.getRight(), arrowBounds.getY(),
                       arrowBounds.getCentreX(), arrowBounds.getBottom());
    g.setColour (dimTextColour);
    g.fillPath (arrow);
}
