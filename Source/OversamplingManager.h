/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

/** Wraps two pre-built `juce::dsp::Oversampling<float>` chains (4x and 8x, per the
    blueprint's Phase 1: "kotelezo a minimalisan 4x, de inkabb 8x oversampling") and
    lets the audio thread switch between them by index only -- both chains are
    constructed once in prepare() (message thread; `juce::dsp::Oversampling`
    allocates internally, so building a NEW one from the audio thread whenever the
    quality parameter changes would not be real-time-safe). Switching mid-stream
    just changes which already-built chain's pointer gets used, and resets it so no
    stale filter state from the previously-active chain leaks in.

    Filter type: half-band FIR equiripple, max quality. The blueprint stresses that
    skipped/insufficient oversampling would let aliasing destroy the turbulence
    effect's organic character (the nonlinear FluidEngine generates a wide harmonic
    spread) -- FIR equiripple gives the steepest stopband of JUCE's two oversampling
    filter types, at the cost of added latency, which is fine for a creative
    saturation/distortion effect (not a zero-latency monitoring path) and is reported
    to the host via setLatencySamples() regardless.
*/
class OversamplingManager
{
public:
    enum Quality
    {
        factor4x = 0,
        factor8x = 1,
        numQualities
    };

    void prepare (int numChannels, int maximumBlockSize);
    void reset() noexcept;

    /** Real-time safe: only switches which pre-built chain is active. */
    void setQuality (Quality newQuality) noexcept;
    Quality getQuality() const noexcept { return quality; }

    /** The current chain's oversampling factor as a plain multiplier (4 or 8). */
    int getFactor() const noexcept { return quality == factor8x ? 8 : 4; }

    juce::dsp::AudioBlock<float> processSamplesUp (const juce::dsp::AudioBlock<const float>& input) noexcept;
    void processSamplesDown (juce::dsp::AudioBlock<float>& output) noexcept;

    double getLatencySamples() const noexcept;

private:
    juce::dsp::Oversampling<float>& active() noexcept { return quality == factor8x ? os8x : os4x; }
    const juce::dsp::Oversampling<float>& active() const noexcept { return quality == factor8x ? os8x : os4x; }

    // factor = 2^N times oversampling: N=2 -> 4x, N=3 -> 8x.
    juce::dsp::Oversampling<float> os4x { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false };
    juce::dsp::Oversampling<float> os8x { 2, 3, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false };

    Quality quality = factor8x;
};
