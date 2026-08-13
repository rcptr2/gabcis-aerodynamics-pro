/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    // Recursively checks every descendant Component has a strictly positive width
    // and height -- the concrete, deterministic form of Phase 5's "extreme corner
    // -drag resizing" edge case: whatever the trigger (mouse drag or a host/session
    // restoring a saved window size), the RESULT is the same resized()/layout call,
    // so this checks the actual thing that could break (a slot going negative or
    // zero-sized) without depending on fragile UI automation.
    void checkNoDegenerateBounds (const juce::Component& component, const juce::String& path)
    {
        INFO ("component path: " << path);
        REQUIRE (component.getWidth() > 0);
        REQUIRE (component.getHeight() > 0);

        for (int i = 0; i < component.getNumChildComponents(); ++i)
            if (auto* child = component.getChildComponent (i))
                checkNoDegenerateBounds (*child, path + " > " + child->getName());
    }
}

TEST_CASE ("AeroDynamicsProAudioProcessorEditor: minimum size (400x300) lays out with no degenerate bounds",
           "[PluginEditor]")
{
    AeroDynamicsProAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    REQUIRE (editor != nullptr);

    editor->setSize (400, 300); // the resize-limit minimum from PluginEditor.h
    checkNoDegenerateBounds (*editor, "editor@400x300");
}

TEST_CASE ("AeroDynamicsProAudioProcessorEditor: maximum size (1600x1200) lays out with no degenerate bounds",
           "[PluginEditor]")
{
    AeroDynamicsProAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    REQUIRE (editor != nullptr);

    editor->setSize (1600, 1200); // the resize-limit maximum from PluginEditor.h
    checkNoDegenerateBounds (*editor, "editor@1600x1200");
}

TEST_CASE ("AeroDynamicsProAudioProcessorEditor: the About panel also lays out cleanly at both extremes",
           "[PluginEditor]")
{
    for (const int size : { 400, 1600 })
    {
        AeroDynamicsProAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        REQUIRE (editor != nullptr);

        const auto height = size == 400 ? 300 : 1200;
        editor->setSize (size, height);

        // Simulate clicking About: find the button and trigger its callback path by
        // calling the same code the click handler calls, since the constrained test
        // environment doesn't have a running message loop to deliver a real click.
        for (int i = 0; i < editor->getNumChildComponents(); ++i)
            if (auto* button = dynamic_cast<juce::TextButton*> (editor->getChildComponent (i)))
                if (button->getButtonText() == "About")
                    button->triggerClick();

        checkNoDegenerateBounds (*editor, "editor-with-about@" + juce::String (size));
    }
}
