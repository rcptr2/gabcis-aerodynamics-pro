/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include "DcBlocker.h"

#include <cmath>
#include <numbers>

namespace
{
    constexpr double kCutoffHz = 10.0;
}

void DcBlocker::prepare (double sampleRate) noexcept
{
    r = (float) (1.0 - (2.0 * std::numbers::pi * kCutoffHz / sampleRate));
    reset();
}

void DcBlocker::reset() noexcept
{
    xPrev = 0.0f;
    yPrev = 0.0f;
}

float DcBlocker::processSample (float input) noexcept
{
    const float output = input - xPrev + r * yPrev;
    xPrev = input;
    yPrev = output;
    return output;
}
