/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Modal-style overlay carrying the project's attribution/signature -- click
    anywhere to close. Matches the sibling projects' own About panel convention.
*/
class AboutPanel : public juce::Component
{
public:
    AboutPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClose;

private:
    juce::Label titleLabel, versionLabel, attributionLabel, testedLabel, closeHintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};
