/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "PluginParameters.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Pressure: the "wind tunnel" inlet drive, 0-36 dB pre-gain applied before the
    // signal is injected into the FluidEngine's Dirichlet boundary cell.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pressure, 1 },
        "Pressure",
        juce::NormalisableRange<float> (0.0f, 36.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Viscosity: 0-100%, maps to FluidEngine's diffusion coefficient nu (linearly,
    // internally scaled to stay under the parabolic CFL limit -- see FluidEngine.h).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::viscosity, 1 },
        "Viscosity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Turbulence: 0-100%, maps to FluidEngine's nonlinear convection coefficient
    // (internally scaled to stay under the hyperbolic/CFL limit).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::turbulence, 1 },
        "Turbulence",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Flow Rate: 0-100%, maps to a fractional-sample delay line AFTER the
    // nonlinear FluidEngine stage (v0.6.0+ operator splitting -- see
    // PluginProcessor.h/.cpp), not the FDM grid itself. Pure delay, zero
    // numerical diffusion by construction.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowRate, 1 },
        "Flow Rate",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Oversampling quality: 4x (lower latency/CPU) or 8x (default -- the blueprint's
    // preferred setting; see OversamplingManager.h for the FIR-equiripple rationale).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::oversampling, 1 },
        "Oversampling",
        OversamplingOption::choices,
        OversamplingOption::factor8x));

    // Mix: 0-100% Dry/Wet, default 100% (fully wet) per the blueprint's Phase 3 spec.
    // Equal-power crossfade against the (latency-compensated) dry signal -- see
    // PluginProcessor::processBlock().
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false));

    return { params.begin(), params.end() };
}
