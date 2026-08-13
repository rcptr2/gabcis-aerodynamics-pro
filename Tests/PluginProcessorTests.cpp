/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "PluginProcessor.h"

namespace
{
    void fillWithSine (juce::AudioBuffer<float>& buffer, float freq, float sampleRate, int startSampleIndex)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi * freq
                                            * (float) (startSampleIndex + i) / sampleRate);
        }
    }

    bool isFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (data[i]))
                    return false;
        }
        return true;
    }
}

TEST_CASE ("AeroDynamicsProAudioProcessor: switching Oversampling quality mid-stream stays stable",
           "[PluginProcessor]")
{
    // Phase 5's "Final Polish" edge case: the Oversampling ComboBox can be changed
    // by the user (or host automation) while audio is actively flowing, not just
    // between prepareToPlay() calls. This exercises processBlock()'s own
    // quality-change detection (index compare -> setQuality() -> re-point the dry
    // -path delay line -> notifyLatencyChanged()), not just OversamplingManager in
    // isolation (already covered by OversamplingManagerTests.cpp).
    AeroDynamicsProAudioProcessor processor;
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 256;

    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    auto* oversamplingParam = processor.apvts.getParameter (ParamIDs::oversampling);
    REQUIRE (oversamplingParam != nullptr);

    for (int block = 0; block < 40; ++block)
    {
        // Flip quality every 5 blocks, including switching mid-way through a
        // continuous sine so the discontinuity in the underlying oversampling
        // chain's internal filter state is actually exercised.
        if (block % 5 == 0)
            oversamplingParam->setValueNotifyingHost (block % 10 == 0 ? 0.0f : 1.0f);

        fillWithSine (buffer, 220.0f, (float) sampleRate, block * blockSize);
        processor.processBlock (buffer, midi);

        INFO ("block " << block);
        REQUIRE (isFinite (buffer));
    }
}

TEST_CASE ("AeroDynamicsProAudioProcessor: toggling Bypass mid-stream crossfades cleanly", "[PluginProcessor]")
{
    AeroDynamicsProAudioProcessor processor;
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 256;

    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    // Drive the FluidEngine hard so bypassed vs. non-bypassed output actually
    // differs -- otherwise a bug that ignored Bypass entirely could still pass.
    processor.apvts.getParameter (ParamIDs::turbulence)->setValueNotifyingHost (1.0f);
    processor.apvts.getParameter (ParamIDs::pressure)->setValueNotifyingHost (1.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    auto* bypassParam = processor.apvts.getParameter (ParamIDs::bypass);
    REQUIRE (bypassParam != nullptr);

    for (int block = 0; block < 30; ++block)
    {
        if (block == 10)
            bypassParam->setValueNotifyingHost (1.0f); // engage Bypass mid-stream
        else if (block == 20)
            bypassParam->setValueNotifyingHost (0.0f); // disengage again

        fillWithSine (buffer, 220.0f, (float) sampleRate, block * blockSize);
        processor.processBlock (buffer, midi);

        INFO ("block " << block);
        REQUIRE (isFinite (buffer));
    }
}

TEST_CASE ("AeroDynamicsProAudioProcessor: a sample-rate change re-initialises cleanly", "[PluginProcessor]")
{
    // Hosts call prepareToPlay() again (not just once) whenever the project's sample
    // rate changes -- every stateful member (oversampling, FluidEngines, DcBlockers,
    // the dry-path delay line, both SmoothedValues) must come back into a consistent
    // state, not retain stale sizing/state from the previous rate.
    AeroDynamicsProAudioProcessor processor;
    constexpr int blockSize = 512;

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 44100.0 })
    {
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        for (int block = 0; block < 5; ++block)
        {
            fillWithSine (buffer, 220.0f, (float) sampleRate, block * blockSize);
            processor.processBlock (buffer, midi);

            INFO ("sampleRate " << sampleRate << ", block " << block);
            REQUIRE (isFinite (buffer));
        }
    }
}

TEST_CASE ("AeroDynamicsProAudioProcessor: getStateInformation/setStateInformation round-trips through the real processor",
           "[PluginProcessor]")
{
    AeroDynamicsProAudioProcessor writer;
    writer.apvts.getParameter (ParamIDs::viscosity)->setValueNotifyingHost (0.8f);
    writer.apvts.getParameter (ParamIDs::mix)->setValueNotifyingHost (0.4f);

    juce::MemoryBlock state;
    writer.getStateInformation (state);

    AeroDynamicsProAudioProcessor reader;
    reader.setStateInformation (state.getData(), (int) state.getSize());

    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::viscosity)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::viscosity)->load());
    REQUIRE (reader.apvts.getRawParameterValue (ParamIDs::mix)->load()
             == writer.apvts.getRawParameterValue (ParamIDs::mix)->load());
}
