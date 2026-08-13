/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

/** Single-channel 1st-order high-pass DC blocker, per the blueprint's Phase 3 spec
    ("DC filter (egypolusu felulateresztu 10 Hz-en)"): the FluidEngine's asymmetric
    wave-breaking (upwind advection reacting differently to positive vs. negative
    grid values once Turbulence pushes the effective velocity's sign around) can
    leave a DC offset on the wet signal, which this removes before the Dry/Wet mix.

    Classic one-pole DC-blocking filter (Julius O. Smith's standard form):
    y[n] = x[n] - x[n-1] + R*y[n-1], R just under 1. Mathematically stable for any
    0 <= R < 1 (the single pole sits strictly inside the unit circle); R is derived
    from the fixed 10 Hz cutoff and the actual sample rate in prepare(), not
    hardcoded, so it stays correct at 44.1kHz, 48kHz, 96kHz etc.
*/
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    float processSample (float input) noexcept;

private:
    float r = 0.995f;
    float xPrev = 0.0f;
    float yPrev = 0.0f;
};
