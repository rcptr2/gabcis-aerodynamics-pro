/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "OversamplingManager.h"

void OversamplingManager::prepare (int numChannels, int maximumBlockSize)
{
    juce::ignoreUnused (numChannels); // both chains are fixed at 2 channels (stereo main bus)

    os4x.initProcessing ((size_t) maximumBlockSize);
    os8x.initProcessing ((size_t) maximumBlockSize);
    reset();
}

void OversamplingManager::reset() noexcept
{
    os4x.reset();
    os8x.reset();
}

void OversamplingManager::setQuality (Quality newQuality) noexcept
{
    if (newQuality == quality)
        return;

    quality = newQuality;
    active().reset();
}

juce::dsp::AudioBlock<float> OversamplingManager::processSamplesUp (const juce::dsp::AudioBlock<const float>& input) noexcept
{
    return active().processSamplesUp (input);
}

void OversamplingManager::processSamplesDown (juce::dsp::AudioBlock<float>& output) noexcept
{
    active().processSamplesDown (output);
}

double OversamplingManager::getLatencySamples() const noexcept
{
    return (double) active().getLatencyInSamples();
}
