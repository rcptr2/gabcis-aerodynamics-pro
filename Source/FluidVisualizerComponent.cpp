/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "FluidVisualizerComponent.h"

#include <cmath>

#include "AeroDynamicsLookAndFeel.h"

FluidVisualizerComponent::FluidVisualizerComponent (VisualizationPublisher& publisherToUse,
                                                      std::atomic<float>* turbulenceParamToUse)
    : publisher (publisherToUse), turbulenceParam (turbulenceParamToUse)
{
    startTimerHz (kFrameRateHz);
}

FluidVisualizerComponent::~FluidVisualizerComponent()
{
    stopTimer();
}

void FluidVisualizerComponent::resized()
{
}

void FluidVisualizerComponent::paint (juce::Graphics& g)
{
    // The whole point of the low-res buffer: this is the only per-frame cost that
    // scales with the component's actual (possibly maximised) on-screen size --
    // one upscale blit, linear-filtered, not a redraw of the simulation itself.
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (lowResBuffer, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void FluidVisualizerComponent::timerCallback()
{
   #if JUCE_DEBUG
    const auto startMs = juce::Time::getMillisecondCounterHiRes();
    renderLowResBuffer();
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - startMs;

    if (elapsedMs > kFrameBudgetMs)
        DBG ("FluidVisualizerComponent: renderLowResBuffer() took " << elapsedMs
             << " ms, over the " << kFrameBudgetMs << " ms budget");

    rollingMaxMs = juce::jmax (rollingMaxMs, elapsedMs);
    if (++framesSinceReport >= kFrameRateHz)
    {
        DBG ("FluidVisualizerComponent: renderLowResBuffer() max over the last "
             << framesSinceReport << " frames: " << rollingMaxMs << " ms");
        rollingMaxMs = 0.0;
        framesSinceReport = 0;
    }
   #else
    renderLowResBuffer();
   #endif

    repaint();
}

void FluidVisualizerComponent::renderLowResBuffer()
{
    juce::Graphics g (lowResBuffer);
    g.fillAll (AeroDynamicsLookAndFeel::backgroundColour);

    const auto turbulence01 = juce::jlimit (0.0f, 1.0f, turbulenceParam->load() / 100.0f);
    const auto snapshot = publisher.read();

    // Background UV-warp (Phase 4 brief: "Turbulence Background UV-Warping"): a
    // sparse dot grid whose position is offset by cheap sin/cos math driven by
    // Turbulence and a slow running animation phase -- a "warping energy field"
    // impression at near-zero cost (a few dozen dots, no per-pixel work).
    constexpr int dotsX = 20;
    constexpr int dotsY = 12;

    for (int iy = 0; iy < dotsY; ++iy)
    {
        for (int ix = 0; ix < dotsX; ++ix)
        {
            const float baseX = (float) ix / (float) (dotsX - 1) * (float) kBufferWidth;
            const float baseY = (float) iy / (float) (dotsY - 1) * (float) kBufferHeight;

            const float warpAmount = turbulence01 * 6.0f;
            const float offsetX = warpAmount * std::sin (baseY * 0.15f + animationPhase);
            const float offsetY = warpAmount * std::cos (baseX * 0.15f + animationPhase * 0.8f);

            g.setColour (AeroDynamicsLookAndFeel::idleGlowColour.withAlpha (0.10f));
            g.fillEllipse (baseX + offsetX - 0.75f, baseY + offsetY - 0.75f, 1.5f, 1.5f);
        }
    }

    // Fluid ribbons: the actual FluidEngine grid state (via the lock-free
    // publisher), drawn as glowing streamlines. L/R get a small vertical offset for
    // a subtle stereo shimmer; each ribbon's own peak amplitude this frame (not a
    // fixed colour) drives the idle-green -> active-amber blend, so the
    // visualization directly reflects how hard the fluid is being driven.
    const auto drawRibbon = [&] (const std::array<float, (size_t) FluidEngine::kGridSize>& grid,
                                  float verticalOffset, float alphaMultiplier)
    {
        constexpr auto n = FluidEngine::kGridSize; // v0.6.0: fixed-size grid, no more active-length concept

        juce::Path ribbon;
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) (n - 1) * (float) kBufferWidth;
            const float amplitude01 = juce::jlimit (-1.0f, 1.0f, grid[(size_t) i] / FluidEngine::kMaxAmplitude);
            const float y = (float) kBufferHeight * 0.5f + verticalOffset - amplitude01 * (float) kBufferHeight * 0.38f;

            if (i == 0)
                ribbon.startNewSubPath (x, y);
            else
                ribbon.lineTo (x, y);

            peak = juce::jmax (peak, std::abs (amplitude01));
        }

        const auto colour = AeroDynamicsLookAndFeel::idleGlowColour.interpolatedWith (
            AeroDynamicsLookAndFeel::activeGlowHigh, peak);

        // Cheap fake-glow again: wide translucent pass, then a crisp narrow one.
        g.setColour (colour.withAlpha (0.30f * alphaMultiplier));
        g.strokePath (ribbon, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (0.85f * alphaMultiplier));
        g.strokePath (ribbon, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawRibbon (snapshot.gridL, -6.0f, 1.0f);
    drawRibbon (snapshot.gridR, 6.0f, 0.85f);

    animationPhase += 0.03f;
    if (animationPhase > juce::MathConstants<float>::twoPi * 1000.0f)
        animationPhase = 0.0f; // avoid unbounded growth over very long sessions
}
