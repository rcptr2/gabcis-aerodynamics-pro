/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <array>

/** Single-channel, audio-rate solver for the NONLINEAR half of the 1D Burgers
    equation ("Fluid Engine") -- v0.6.0 OPERATOR-SPLITTING REDESIGN.

    Through v0.5.2 this class solved the whole equation
    `du/dt + (c + alpha*u)*du/dx = nu*d2u/dx2` on a single FDM grid, where `c`
    (kBaseFlowSpeed) was a constant background flow added specifically so
    Turbulence = 0 still transported signal across the grid (see v0.2.1). Live
    testing (Phase 5, three separate rounds) found that approach fundamentally
    flawed in two ways, not just mistunable:

      1. The FIRST-ORDER UPWIND discretization used for the `c` term has
         well-known, textbook numerical diffusion (a truncation-error artefact of
         upwind differencing, not something tunable away) -- every cell the signal
         crosses smears it a little, so ANY nonzero `c` meant the plugin could
         never be truly transparent even with Viscosity/Turbulence at 0%. Confirmed
         directly: a FabFilter Pro-Q spectrum with the plugin bypassed showed
         visibly more high-frequency content than with it engaged, even at all
         -params-zero, and Flow Rate alone (bigger grid = more cells = more
         accumulated smearing) measurably rolled off highs like a lowpass filter.
      2. Tying Flow Rate to the SAME grid that carries the nonlinear physics forced
         a lose-lose trade: a bigger grid (more Flow Rate "chamber" character) also
         meant more CPU (O(grid size) per sample, v0.5.1) and more numerical
         diffusion (point 1); a smaller grid meant less CPU/diffusion but too few
         cells for Viscosity/Turbulence to do anything audible (the original
         v0.5.1 bug).

    The fix: OPERATOR SPLITTING. The equation's linear part (`c`, the "how far does
    the wave travel" character) and its nonlinear part (`alpha*u` turbulence +
    `nu` viscosity, the "how does the wave break/smear" character) are solved by
    two SEPARATE, purpose-built stages instead of one shared grid:

      - Linear transport (`c`, now the Flow Rate knob's whole job) is a proper
        fractional-sample `juce::dsp::DelayLine` in PluginProcessor -- a delay line
        has ZERO numerical diffusion by construction (it just reads an
        interpolated past sample), completely eliminating problem 1's "always-on"
        smearing. See PluginProcessor.h/.cpp.
      - THIS class now solves only `du/dt + alpha*u*du/dx = nu*d2u/dx2` (no `c`
        term at all) on a small, FIXED-size grid (`kGridSize`, independent of Flow
        Rate) -- fixing problem 2: constant, small CPU cost regardless of Flow
        Rate, and Viscosity/Turbulence always have the same number of cells to
        work with no matter what Flow Rate is set to.

    Removing `c` also directly improves stability headroom (see setParameters()):
    with no constant velocity contribution, Viscosity alone (Turbulence = 0) can
    now safely reach nu ~= 0.455, close to the classical 0.5 diffusion limit --
    compared to 0.25-0.29 when `c` was eating into the same coupled-stability
    budget through v0.5.1/v0.5.2.

    Still solved via upwind advection + central diffusion, still hard-clamped to
    [-kMaxAmplitude, +kMaxAmplitude] every step as a defence-in-depth safety net
    (see the coupled von Neumann analysis in setParameters()' comment for where the
    analytic bound itself comes from).

    TWO further corrections made DURING this same redesign, both measured, not
    assumed (see setParameters() and Tests/FluidEngineTests.cpp):

      - Diffusion is a SMOOTHING process, not a transport one -- it attenuates a
        signal roughly exponentially with the number of cells it crosses, not
        "carries" it. With the old kGridSize=16 (kept from the pre-redesign grid's
        Flow-Rate-minimum) and no more `c` term to compensate, only ~0.4% of a
        default-settings (Viscosity/Turbulence 30%) sine's energy reached the
        outlet -- effectively still-silent, just at a different setting than the
        v0.5.2 bug. A grid-size sweep (see the project's changelog for the actual
        numbers) showed transmission collapses fast with size: kGridSize was
        SHRUNK to 4, restoring ~74% transmission at those same default settings.
      - The turbulence (nonlinear advection) term is self-referential --
        `velocity = alpha*u` -- so a grid cell sitting at EXACTLY 0 has velocity 0
        and the advection term vanishes regardless of alpha or its neighbours: a
        cell can never be "kick-started" by turbulence alone, only diffusion (which
        depends on NEIGHBOUR values, not the cell's own) can. Measured directly:
        Viscosity=0% + Turbulence=100% produced exact silence. Fixed with a small
        always-on diffusion floor (`kViscosityFloor`) added to viscosityCoeff
        regardless of the Viscosity knob's position -- small enough to leave the
        stability margin intact (verified: worst-case margin is still 8%), but
        enough that no cell can get permanently stuck at 0.
*/
class FluidEngine
{
public:
    /** Fixed grid size for the nonlinear (Viscosity/Turbulence) stage -- no longer
        tied to Flow Rate (that's the delay line's job now, see the class comment).
        Small deliberately: diffusion attenuates a signal roughly exponentially with
        the number of cells crossed (measured, see the class comment), so a small
        grid is what keeps the "shock formation" character audible rather than
        smoothed away to near-nothing before it reaches the outlet.
    */
    static constexpr int kGridSize = 4;

    /** Always added to viscosityCoeff regardless of the Viscosity knob -- without
        this, Turbulence alone (Viscosity at its literal 0%) cannot move a cell
        that starts at exactly 0 at all (see the class comment's second measured
        correction). Small enough that the worst-case combined stability margin
        (Viscosity and Turbulence both at 100%) stays a comfortable ~10% -- see
        setParameters().
    */
    static constexpr float kViscosityFloor = 0.02f;

    /** Hard amplitude clamp applied to every grid cell after every step. This is both
        the safety net against numerical blow-up AND the analytic bound that
        setParameters() derives the CFL-safe turbulenceCoeff ceiling from -- the two
        are the same number by design, not a coincidence.
    */
    static constexpr float kMaxAmplitude = 4.0f;

    FluidEngine() = default;

    void reset() noexcept;

    /** @param viscosity01    0-1, maps to the diffusion coefficient nu in
                              [kViscosityFloor, nuMax(turbulence01) + kViscosityFloor],
                              where `nuMax(t) = 0.455 - 0.325*t`. Derived directly
                              from a coupled von Neumann stability analysis of THIS
                              (now `c`-free) scheme: the amplification factor at the
                              grid's Nyquist mode (k=pi, checkerboard) is
                              `z(pi) = 1 - 2*v - 4*nu` where `v = alpha*u` -- NOTE
                              the factor of 2 on v and 4 on nu, both required (a
                              live-tested v0.6.1 patch proposal that dropped them,
                              using `v + 2*nu` instead of `2*v + 4*nu`, was caught
                              and corrected here before it shipped). Solving
                              `|z(pi)| <= 0.9` for nu at the worst case
                              `v = turbulenceCoeff_max * kMaxAmplitude = 0.65` gives
                              nu <= 0.15, i.e. 0.455 - 0.325*1 = 0.13, plus
                              kViscosityFloor's 0.02 = 0.15 exactly at the margin --
                              verified against a numerical sweep over k and over the
                              full Viscosity x Turbulence x sign(u) grid, not just
                              this one closed-form point (same methodology as the
                              v0.5.2/v0.6.0 derivations).
        @param turbulence01   0-1, maps to the turbulence coefficient (alpha) in
                              [0, 0.65 / kMaxAmplitude], i.e. the worst-case velocity
                              contribution `alpha*u` at u = +-kMaxAmplitude is 0.65
                              (widened from v0.6.0's 0.5 -- live testing found
                              Turbulence's audible effect too subtle at typical,
                              non-Pressure-driven signal levels).
    */
    void setParameters (float viscosity01, float turbulence01) noexcept;

    /** Runs one FDM timestep: injects `input` as the inlet boundary condition and
        returns the outlet reading. Must be called once per sample at whatever rate
        the engine was prepared for (the oversampled rate, in normal use).
    */
    float processSample (float input) noexcept;

    /** Read-only access to the current grid state, for the Phase 4 visualizer to
        snapshot from the audio thread (see VisualizationPublisher). Cheap (returns a
        reference, no copy) -- the caller is expected to copy what it needs into a
        publisher immediately, not hold onto this reference past the current call.
    */
    const std::array<float, kGridSize>& getGrid() const noexcept { return grid; }

private:
    std::array<float, kGridSize> grid {};
    std::array<float, kGridSize> nextGrid {};

    float viscosityCoeff = 0.0f;
    float turbulenceCoeff = 0.0f;
};
