/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <atomic>

#include <juce_gui_basics/juce_gui_basics.h>

#include "VisualizationPublisher.h"

/** Phase 4's "Kármán-örvény"-style fluid visualization, per the UI brief's
    Low-Res FBO Offscreen Renderer point: the actual simulation-reactive drawing
    happens into a small (kBufferWidth x kBufferHeight) `juce::Image`, redrawn by a
    60Hz juce::Timer, then upscaled into this component's full (arbitrarily large,
    window-resize-driven) bounds with linear resampling in paint(). This keeps the
    heavy per-tick drawing work at a fixed, tiny cost regardless of how large the
    user maximizes the plugin window -- the brief's whole point ("preserving
    CPU/GPU resources... even on low-end integrated GPUs"). Deliberately a plain
    software juce::Image, not a real OpenGL FBO/context: same CPU/GPU-cost win, no
    GPU-driver-compatibility risk on weak integrated graphics.

    Data source: the FluidEngine grids live on the audio thread. This component
    reads a lock-free snapshot from a VisualizationPublisher (owned by the
    processor) each timer tick -- never touches the audio thread's own state
    directly. Pressure/Turbulence (needed for the reactive styling) are read
    directly from the APVTS raw parameter pointers, which is safe from any thread.
*/
class FluidVisualizerComponent : public juce::Component,
                                  private juce::Timer
{
public:
    FluidVisualizerComponent (VisualizationPublisher& publisherToUse,
                               std::atomic<float>* turbulenceParamToUse);
    ~FluidVisualizerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderLowResBuffer();

    static constexpr int kBufferWidth = 200;
    static constexpr int kBufferHeight = 120;
    static constexpr int kFrameRateHz = 60;

    VisualizationPublisher& publisher;
    std::atomic<float>* turbulenceParam;

    juce::Image lowResBuffer { juce::Image::ARGB, kBufferWidth, kBufferHeight, true };
    float animationPhase = 0.0f;

   #if JUCE_DEBUG
    // Phase 5 performance check (Debug builds only): renderLowResBuffer() is timed
    // every frame. Any single frame over kFrameBudgetMs logs immediately; the
    // rolling max over each ~1-second window also gets logged, so a live run shows
    // real sustained numbers, not just an isolated worst-case spike.
    static constexpr double kFrameBudgetMs = 5.0;
    double rollingMaxMs = 0.0;
    int framesSinceReport = 0;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidVisualizerComponent)
};
