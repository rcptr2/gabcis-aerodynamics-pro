/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "FluidEngine.h"

#include <algorithm>
#include <cmath>

void FluidEngine::reset() noexcept
{
    grid.fill (0.0f);
    nextGrid.fill (0.0f);
}

void FluidEngine::setParameters (float viscosity01, float turbulence01) noexcept
{
    viscosity01 = std::clamp (viscosity01, 0.0f, 1.0f);
    turbulence01 = std::clamp (turbulence01, 0.0f, 1.0f);

    // See the class-level comment in FluidEngine.h (v0.6.0 operator-splitting
    // redesign, v0.6.1 widened-turbulence-budget correction) for the coupled von
    // Neumann derivation these formulas come from, and for why kViscosityFloor is
    // always added regardless of the Viscosity knob (Turbulence alone cannot move
    // a grid cell starting at exactly 0).
    turbulenceCoeff = turbulence01 * (0.65f / kMaxAmplitude);
    const float nuMax = 0.455f - 0.325f * turbulence01;
    viscosityCoeff = viscosity01 * nuMax + kViscosityFloor;
}

float FluidEngine::processSample (float input) noexcept
{
    // Inlet boundary condition (Dirichlet): the driven input is soft-clipped into
    // the grid's hard amplitude bound before injection, so the boundary condition
    // itself can never be the thing that pushes a cell outside [-kMaxAmplitude,
    // +kMaxAmplitude] before the per-step clamp below even runs.
    const float driven = kMaxAmplitude * std::tanh (input / kMaxAmplitude);
    grid[0] = driven;

    // Interior cells: upwind (direction-of-flow) differencing for the
    // turbulenceCoeff*u advection term (no more constant `c` -- that is the
    // PluginProcessor-level delay line's job now, see FluidEngine.h), central
    // differencing for the linear diffusion term.
    for (int i = 1; i < kGridSize - 1; ++i)
    {
        const float ui = grid[(size_t) i];
        const float uPrev = grid[(size_t) (i - 1)];
        const float uNext = grid[(size_t) (i + 1)];

        const float velocity = turbulenceCoeff * ui;
        const float advection = velocity >= 0.0f ? velocity * (ui - uPrev) : velocity * (uNext - ui);
        const float diffusion = uNext - 2.0f * ui + uPrev;

        float updated = ui - advection + viscosityCoeff * diffusion;

        // Strict clamping / NaN scrub -- the safety net the blueprint's Phase 2
        // explicitly demands, on top of (not instead of) the analytic CFL-safe
        // coefficient scaling in setParameters().
        updated = std::isfinite (updated) ? std::clamp (updated, -kMaxAmplitude, kMaxAmplitude)
                                           : 0.0f;

        nextGrid[(size_t) i] = updated;
    }

    nextGrid[0] = driven;
    nextGrid[kGridSize - 1] = nextGrid[kGridSize - 2]; // outlet: zero-gradient / absorbing

    grid = nextGrid;

    return grid[kGridSize - 1];
}
