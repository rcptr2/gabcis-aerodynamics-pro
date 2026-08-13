/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "OversamplingManager.h"

TEST_CASE ("OversamplingManager: 4x and 8x chains report distinct, sane latency", "[OversamplingManager]")
{
    OversamplingManager manager;
    manager.prepare (2, 512);

    manager.setQuality (OversamplingManager::factor4x);
    const auto latency4x = manager.getLatencySamples();

    manager.setQuality (OversamplingManager::factor8x);
    const auto latency8x = manager.getLatencySamples();

    INFO ("4x latency (samples, base rate): " << latency4x);
    INFO ("8x latency (samples, base rate): " << latency8x);

    REQUIRE (latency4x > 0.0);
    REQUIRE (latency8x > 0.0);
    // 8x runs two cascaded half-band stages vs. 4x's one, so it must report more
    // latency, not less or equal -- confirms the two chains are actually distinct,
    // not silently sharing one filter configuration.
    REQUIRE (latency8x > latency4x);
}

TEST_CASE ("OversamplingManager: up/down round-trip is finite and roughly unity-gain for a sine", "[OversamplingManager]")
{
    constexpr int blockSize = 512;
    OversamplingManager manager;
    manager.prepare (2, blockSize);
    manager.setQuality (OversamplingManager::factor8x);

    juce::AudioBuffer<float> buffer (2, blockSize);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < blockSize; ++i)
            data[i] = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / 44100.0f);
    }

    for (int block = 0; block < 20; ++block) // let the FIR latency fully flush through
    {
        juce::dsp::AudioBlock<float> ioBlock (buffer);
        juce::dsp::AudioBlock<const float> inputBlock (ioBlock);
        auto up = manager.processSamplesUp (inputBlock);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* data = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                REQUIRE (std::isfinite (data[i]));
        }

        manager.processSamplesDown (ioBlock);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < blockSize; ++i)
                REQUIRE (std::isfinite (data[i]));
        }
    }

    // After the FIR latency has flushed through several blocks of a steady sine, the
    // round-tripped signal should sit close to its original +/-0.5 peak amplitude
    // (allowing headroom for FIR ripple/edge effects) -- confirms the up/down pair is
    // not silently attenuating or blowing up the signal.
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, blockSize));

    REQUIRE (peak > 0.3f);
    REQUIRE (peak < 0.7f);
}
