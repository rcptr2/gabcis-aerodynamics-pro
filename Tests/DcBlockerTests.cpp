/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>

#include "DcBlocker.h"

TEST_CASE ("DcBlocker: a sustained DC offset decays to near-zero", "[DcBlocker]")
{
    DcBlocker blocker;
    blocker.prepare (44100.0);

    float last = 0.0f;
    for (int i = 0; i < 10000; ++i)
        last = blocker.processSample (1.0f);

    INFO ("output after 10000 samples of constant 1.0 input: " << last);
    REQUIRE (std::abs (last) < 0.001f);
}

TEST_CASE ("DcBlocker: audio-rate content passes through close to unattenuated", "[DcBlocker]")
{
    constexpr int numSamples = 8000;
    constexpr float freq = 1000.0f;
    constexpr float sampleRate = 44100.0f;

    DcBlocker blocker;
    blocker.prepare ((double) sampleRate);

    double inEnergy = 0.0;
    double outEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float input = std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) i / sampleRate);
        const auto out = blocker.processSample (input);

        REQUIRE (std::isfinite (out));

        // Skip the first 2000 samples (filter settling) when measuring energy.
        if (i >= 2000)
        {
            inEnergy += (double) input * input;
            outEnergy += (double) out * out;
        }
    }

    const auto ratio = outEnergy / inEnergy;
    INFO ("energy ratio (post-settling): " << ratio);
    REQUIRE (ratio > 0.98);
    REQUIRE (ratio < 1.02);
}

TEST_CASE ("DcBlocker: stable and finite under sustained worst-case random input", "[DcBlocker][stability]")
{
    DcBlocker blocker;
    blocker.prepare (44100.0);

    std::mt19937 rng (42);
    std::uniform_real_distribution<float> dist (-100.0f, 100.0f);

    for (int i = 0; i < 100000; ++i)
    {
        const auto out = blocker.processSample (dist (rng));
        REQUIRE (std::isfinite (out));
    }
}
