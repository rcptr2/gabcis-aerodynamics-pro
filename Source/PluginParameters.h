/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** APVTS parameter IDs and layout for Gabci's AeroDynamics, per the blueprint's
    section 3 ("Fobb Parameterek") plus the Phase 1 oversampling-quality control.
*/
namespace ParamIDs
{
    inline constexpr const char* pressure       = "pressure";
    inline constexpr const char* viscosity      = "viscosity";
    inline constexpr const char* turbulence     = "turbulence";
    inline constexpr const char* flowRate       = "flowRate";
    inline constexpr const char* oversampling   = "oversampling";
    inline constexpr const char* mix            = "mix";
    inline constexpr const char* bypass         = "bypass";
}

/** Index values for the `oversampling` AudioParameterChoice, in declaration order --
    mirrors OversamplingManager::Quality so the two can be cast directly.
*/
namespace OversamplingOption
{
    enum Index
    {
        factor4x = 0,
        factor8x = 1
    };

    inline const juce::StringArray choices { "4x", "8x" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
