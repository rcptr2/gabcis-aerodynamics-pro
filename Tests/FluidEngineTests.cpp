/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "FluidEngine.h"

TEST_CASE ("FluidEngine: silence stays silence", "[FluidEngine]")
{
    FluidEngine engine;
    engine.reset();
    engine.setParameters (0.5f, 0.5f);

    for (int i = 0; i < 1000; ++i)
    {
        const auto out = engine.processSample (0.0f);
        REQUIRE (std::isfinite (out));
        REQUIRE (out == 0.0f);
    }
}

TEST_CASE ("FluidEngine: worst-case parameters never produce NaN/Inf and stay within kMaxAmplitude",
           "[FluidEngine][stability]")
{
    // Max viscosity, max turbulence, driven by full-scale content designed to be as
    // numerically hostile as possible: an alternating +/-1 square wave (largest
    // possible sample-to-sample derivative) pre-scaled well beyond kMaxAmplitude to
    // also exercise the inlet's tanh soft-clip.
    FluidEngine engine;
    engine.reset();
    engine.setParameters (1.0f, 1.0f);

    bool sawNonzero = false;

    for (int i = 0; i < 200000; ++i)
    {
        const float input = (i % 2 == 0 ? 1.0f : -1.0f) * FluidEngine::kMaxAmplitude * 10.0f;
        const auto out = engine.processSample (input);

        INFO ("sample index " << i << ", output " << out);
        REQUIRE (std::isfinite (out));
        REQUIRE (std::abs (out) <= FluidEngine::kMaxAmplitude);

        if (out != 0.0f)
            sawNonzero = true;
    }

    REQUIRE (sawNonzero);
}

TEST_CASE ("FluidEngine: worst-case parameters survive white noise at 10x headroom", "[FluidEngine][stability]")
{
    FluidEngine engine;
    engine.reset();
    engine.setParameters (1.0f, 1.0f);

    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> dist (-FluidEngine::kMaxAmplitude * 10.0f, FluidEngine::kMaxAmplitude * 10.0f);

    for (int i = 0; i < 200000; ++i)
    {
        const auto out = engine.processSample (dist (rng));
        REQUIRE (std::isfinite (out));
        REQUIRE (std::abs (out) <= FluidEngine::kMaxAmplitude);
    }
}

TEST_CASE ("FluidEngine: parameters changing live mid-stream stays stable", "[FluidEngine][stability]")
{
    // Viscosity/Turbulence can be automated by the host while audio is flowing.
    // Sweep both across their full range every sample, driven by hostile noise, and
    // confirm no instability results.
    FluidEngine engine;
    engine.reset();

    std::mt19937 rng (999);
    std::uniform_real_distribution<float> dist (-FluidEngine::kMaxAmplitude * 10.0f, FluidEngine::kMaxAmplitude * 10.0f);

    for (int i = 0; i < 100000; ++i)
    {
        const float viscosity01 = 0.5f + 0.5f * std::sin ((float) i * 0.01f);
        const float turbulence01 = 0.5f + 0.5f * std::cos ((float) i * 0.013f);
        engine.setParameters (viscosity01, turbulence01);

        const auto out = engine.processSample (dist (rng));
        REQUIRE (std::isfinite (out));
        REQUIRE (std::abs (out) <= FluidEngine::kMaxAmplitude);
    }
}

TEST_CASE ("FluidEngine: the turbulence parameter measurably changes the output", "[FluidEngine][behaviour]")
{
    // Sanity check that Turbulence actually does something nonlinear, isolated from
    // Viscosity's own smoothing/attenuation by comparing the two engines' outputs to
    // EACH OTHER (both share the same Viscosity), not each to the instantaneous
    // input.
    constexpr int numSamples = 4000;
    constexpr float freq = 220.0f;
    constexpr float sampleRate = 44100.0f;

    FluidEngine lowTurbulence;
    lowTurbulence.reset();
    lowTurbulence.setParameters (0.3f, 0.0f);

    FluidEngine highTurbulence;
    highTurbulence.reset();
    highTurbulence.setParameters (0.3f, 1.0f);

    double interEngineDiffEnergy = 0.0;
    double lowOutputEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float input = 0.5f * std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) i / sampleRate);

        const auto lowOut = lowTurbulence.processSample (input);
        const auto highOut = highTurbulence.processSample (input);

        REQUIRE (std::isfinite (lowOut));
        REQUIRE (std::isfinite (highOut));

        interEngineDiffEnergy += (double) (highOut - lowOut) * (highOut - lowOut);
        lowOutputEnergy += (double) lowOut * lowOut;
    }

    // Measured (v0.6.0, kGridSize=4, these exact settings/signal): ~11.3% --
    // 8% is a safe floor below that, comfortably above float rounding noise.
    REQUIRE (interEngineDiffEnergy > lowOutputEnergy * 0.08);
}

TEST_CASE ("FluidEngine: Turbulence alone (Viscosity at its literal 0%) still transports signal (regression)",
           "[FluidEngine][behaviour]")
{
    // Regression test for a bug found and measured during the v0.6.0 redesign: the
    // turbulence (nonlinear advection) term is self-referential (velocity =
    // turbulenceCoeff * u), so a grid cell sitting at EXACTLY 0 has zero velocity
    // and can never be "kick-started" by turbulence alone -- only diffusion, driven
    // by neighbouring cells rather than the cell's own value, can. Measured before
    // the kViscosityFloor fix: Viscosity=0%, Turbulence=100% produced exact silence
    // at the outlet, indistinguishable from a "broken" plugin.
    constexpr int numSamples = 4000;
    constexpr float freq = 220.0f;
    constexpr float sampleRate = 44100.0f;

    FluidEngine engine;
    engine.reset();
    engine.setParameters (0.0f, 1.0f); // Viscosity = 0%, Turbulence = 100%

    double inputEnergy = 0.0;
    double outputEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float input = 0.5f * std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) i / sampleRate);
        const auto out = engine.processSample (input);

        REQUIRE (std::isfinite (out));

        inputEnergy += (double) input * input;
        outputEnergy += (double) out * out;
    }

    INFO ("inputEnergy=" << inputEnergy << " outputEnergy=" << outputEnergy);
    REQUIRE (outputEnergy > inputEnergy * 0.01); // must NOT be the ~0.000% measured before the floor fix
}

TEST_CASE ("FluidEngine: default-ish settings transmit a substantial fraction of the input's energy (regression)",
           "[FluidEngine][behaviour]")
{
    // Regression test for the OTHER bug found during the v0.6.0 redesign: with the
    // old kGridSize=16 (kept over from the pre-redesign Flow-Rate-minimum grid) and
    // no more `c` transport term, diffusion's inherent attenuation-not-transport
    // nature meant only ~0.4% of a default-settings (Viscosity/Turbulence 30%)
    // sine's energy reached the outlet -- effectively still-silent, just at
    // different settings than the v0.5.2 bug. Shrinking kGridSize to 4 measured
    // ~74% transmission at the same settings.
    constexpr int numSamples = 4000;
    constexpr float freq = 220.0f;
    constexpr float sampleRate = 44100.0f;

    FluidEngine engine;
    engine.reset();
    engine.setParameters (0.3f, 0.3f); // matches PluginParameters.cpp's actual defaults

    double inputEnergy = 0.0;
    double outputEnergy = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float input = 0.5f * std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) i / sampleRate);
        const auto out = engine.processSample (input);

        REQUIRE (std::isfinite (out));

        inputEnergy += (double) input * input;
        outputEnergy += (double) out * out;
    }

    INFO ("inputEnergy=" << inputEnergy << " outputEnergy=" << outputEnergy);
    REQUIRE (outputEnergy > inputEnergy * 0.5); // measured ~74%; 50% is a safe, conservative floor
}

namespace
{
    // The signature of the v0.5.1/v0.5.2 combined-advection-diffusion instability
    // (see the class-level comment in FluidEngine.h): the grid's Nyquist spatial
    // mode (checkerboard: adjacent cells alternating sign) grows every step until
    // the hard clamp catches it, producing an outlet reading that alternates sign
    // sample-to-sample at close to the clamp amplitude. A lag-1 autocorrelation
    // close to -1 over a steady-state tail is that signature; a normal,
    // non-pathological signal's lag-1 autocorrelation sits well above that.
    double lag1Autocorrelation (const std::vector<float>& tail)
    {
        double energy = 0.0, laggedProduct = 0.0;
        for (size_t i = 0; i + 1 < tail.size(); ++i)
        {
            energy += (double) tail[i] * tail[i];
            laggedProduct += (double) tail[i] * tail[i + 1];
        }
        return energy > 0.0 ? laggedProduct / energy : 0.0;
    }
}

TEST_CASE ("FluidEngine: no checkerboard (Nyquist) instability at any Viscosity/Turbulence setting",
           "[FluidEngine][stability]")
{
    // Regression test for the v0.5.2 bug, re-verified against the v0.6.0 redesign's
    // new (c-free) coefficient formulas.
    struct Case
    {
        float viscosity01, turbulence01;
        const char* label;
    };

    const Case cases[] = {
        { 0.0f, 0.0f, "Viscosity=0, Turbulence=0 (floor-only diffusion)" },
        { 0.0f, 1.0f, "Viscosity=0, Turbulence=100% (floor must still be stable)" },
        { 1.0f, 0.0f, "Viscosity=100%, Turbulence=0" },
        { 1.0f, 1.0f, "Viscosity=100%, Turbulence=100% (worst case)" },
    };

    for (const auto& c : cases)
    {
        FluidEngine engine;
        engine.reset();
        engine.setParameters (c.viscosity01, c.turbulence01);

        constexpr int settleSamples = 2000;
        constexpr int tailSamples = 1000;
        constexpr float freq = 220.0f;
        constexpr float sampleRate = 44100.0f;

        for (int i = 0; i < settleSamples; ++i)
            engine.processSample (2.0f * std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) i / sampleRate));

        std::vector<float> tail;
        tail.reserve (tailSamples);
        for (int i = 0; i < tailSamples; ++i)
        {
            const auto sampleIndex = settleSamples + i;
            const auto out = engine.processSample (
                2.0f * std::sin (2.0f * std::numbers::pi_v<float> * freq * (float) sampleIndex / sampleRate));
            REQUIRE (std::isfinite (out));
            tail.push_back (out);
        }

        const auto autocorr = lag1Autocorrelation (tail);
        INFO (c.label << ": lag-1 autocorrelation = " << autocorr);
        REQUIRE (autocorr > -0.5);
    }
}
