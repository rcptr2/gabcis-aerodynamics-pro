/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DcBlocker.h"
#include "FluidEngine.h"
#include "OversamplingManager.h"
#include "PluginParameters.h"
#include "VisualizationPublisher.h"

//==============================================================================
/** Main plugin processor for Gabci's AeroDynamics.

    Bus layout: one stereo Main input/output bus only (no sidechain -- the blueprint
    has no sidechain-driven feature). Mono is intentionally unsupported: both
    OversamplingManager and the per-channel FluidEngine array are fixed at 2
    channels, matching this processor's own stereo-only design.

    Signal path per block: dry copy saved -> oversample up (4x/8x, selectable) ->
    per channel, at the oversampled rate: Pressure gain -> FluidEngine (the
    Viscosity/Turbulence-driven NONLINEAR shaping, a small fixed-size grid, see
    FluidEngine.h's v0.6.0 operator-splitting redesign) -> flowRateDelayLine (the
    LINEAR "chamber length" transport Flow Rate now controls directly, a proper
    zero-smearing fractional delay line) -> oversample down -> per-channel DcBlocker
    on the wet signal -> the saved dry copy is run through a fractional-sample delay
    line (dryLatencyCompensation) set to exactly the oversampling chain's current
    latency, so it lines up in time with the (inherently latent) wet signal -> the
    two are crossfaded (linear law) driven by the Mix parameter (Bypass forces the
    crossfade fully dry).

    Why Flow Rate is a delay line, not part of the FDM grid (v0.6.0 change): through
    v0.5.2, Flow Rate controlled the FDM grid's own size, and the grid's constant
    background-flow term (needed so Turbulence=0 didn't produce silence) used
    first-order upwind differencing -- which has inherent, textbook numerical
    diffusion, so the plugin was never fully transparent even with every knob at
    zero, confirmed directly with a spectrum analyser. A delay line has zero
    numerical diffusion by construction, eliminating that "always-on" smearing, and
    also decouples Flow Rate's CPU cost and Viscosity/Turbulence's audibility from
    each other entirely (FluidEngine's grid is now fixed-size, independent of Flow
    Rate).

    Why the dry signal needs its own delay line: the oversampling filters delay the
    wet path by a few tens of samples (see OversamplingManager's measured 59.5/64.25
    -sample latencies). setLatencySamples() tells the HOST to compensate across
    tracks, but does nothing for the dry/wet mix happening INSIDE this processBlock()
    -- without explicitly delaying the dry copy by the same amount here, mixing it
    against the delayed wet signal would comb-filter/phase-cancel.

    Latency reporting: the oversampling filters' latency changes with the
    Oversampling quality parameter, so it cannot be set once in prepareToPlay() and
    forgotten. processBlock() detects a quality change (audio thread -- cheap index
    compare only) and calls notifyLatencyChanged(), a private juce::AsyncUpdater hook
    that calls the actual setLatencySamples() on the message thread, the same pattern
    already proven in the sibling SpectralCarve Pro project for its own
    runtime-variable latency. The same quality-change detection also re-points
    dryLatencyCompensation's delay time at the new latency value.
*/
class AeroDynamicsProAudioProcessor final : public juce::AudioProcessor,
                                             private juce::AsyncUpdater
{
public:
    AeroDynamicsProAudioProcessor();
    ~AeroDynamicsProAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Phase 4's FluidVisualizerComponent reads from this (lock-free, see
        VisualizationPublisher.h) -- published once per processBlock() from the
        audio thread, read from the editor's Timer on the UI thread.
    */
    VisualizationPublisher& getVisualizationPublisher() noexcept { return visualizationPublisher; }

private:
    void notifyLatencyChanged();
    void handleAsyncUpdate() override;

    std::atomic<float>* pressureParam = nullptr;
    std::atomic<float>* viscosityParam = nullptr;
    std::atomic<float>* turbulenceParam = nullptr;
    std::atomic<float>* flowRateParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    OversamplingManager oversampling;
    std::array<FluidEngine, 2> fluidEngines;
    std::array<DcBlocker, 2> dcBlockers;

    juce::AudioBuffer<float> dryBuffer;

    // Delays the saved dry copy by exactly the oversampling chain's current latency
    // (fractional -- see the class-level comment) so it lines up with the wet signal
    // before the Mix crossfade. Max size has headroom above the largest currently
    // measured latency (64.25 samples at 8x); re-pointed at the new latency whenever
    // the Oversampling quality parameter changes (see processBlock()).
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryLatencyCompensation { 128 };

    // Effective Dry/Wet amount in [0, 1] after folding in Bypass (which forces this
    // to 0, fully dry, regardless of the Mix parameter). Smoothed to avoid a click on
    // either Mix automation or a Bypass toggle. Linear law (see processBlock()).
    juce::SmoothedValue<float> mixSmoothed;

    // Flow Rate's own delay line (v0.6.0 -- see the class-level comment for why this
    // replaced the old FDM-grid-size approach). Max size depends on the actual host
    // sample rate, so it is sized in prepareToPlay() rather than at compile time
    // (unlike dryLatencyCompensation's fixed bound, which only ever needs to cover
    // the oversampling filters' own -- host-rate-independent -- latency).
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> flowRateDelayLines;
    static constexpr float kMaxFlowRateDelayMs = 25.0f; // widened from 15ms (v0.6.1): more audible "chamber" character

    // A delay-time change is a continuous, not structural, change now that Flow Rate
    // is a delay line rather than an FDM grid size -- still smoothed (avoids a
    // pitch-shift "zipper" artefact from abrupt delay-time jumps), at the
    // oversampled rate, once per oversampled sample (see processBlock()).
    juce::SmoothedValue<float> flowRateSmoothed;

    VisualizationPublisher visualizationPublisher;

    int lastReportedOversamplingIndex = -1;

    // Cached so processBlock() can convert Flow Rate's 0-100% into an actual delay
    // sample count matching whatever oversampling factor is currently active (kept
    // in milliseconds terms, not samples, so a quality switch doesn't change the
    // knob's musical meaning). Updated in prepareToPlay() and whenever the
    // Oversampling quality parameter changes.
    double currentOversampledRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroDynamicsProAudioProcessor)
};
