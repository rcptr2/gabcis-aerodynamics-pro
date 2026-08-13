/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#pragma once

#include <array>
#include <mutex>

#include "FluidEngine.h"

/** Single-producer/single-consumer snapshot publisher: the audio thread calls
    publish() once per processBlock() with the FluidEngine grids' current state, the
    UI thread's visualizer Timer calls read() to get the latest one for drawing.

    Implementation: std::mutex + try_lock() on the publish (audio-thread) side, not
    a hand-rolled lock-free scheme. Two earlier lock-free attempts both turned out
    to be genuinely broken, each one only caught by a real multi-threaded Catch2
    stress test, never by inspection:

      1. A double-buffer (writer picks the "other" of 2 buffers, flips an atomic
         index): unsafe under a fast writer, which can lap back around to the exact
         buffer the reader is still mid-copy of -- reproduced as a SIGABRT/heap
         -corruption crash.
      2. A seqlock (odd/even sequence counter + std::atomic_thread_fence around the
         copy, the textbook form): STILL produced an intermittent off-by-one-
         generation torn read under a sustained stress test (~1-in-5 runs), meaning
         the fence placement did not actually establish the happens-before
         relationship it was assumed to.

    Given this data is purely cosmetic (a visualizer, not anything audio-critical),
    the right call after two failed lock-free attempts is the provably-correct
    simple one: std::mutex. The audio-thread side uses try_lock(), which is
    guaranteed by the standard to never block -- on the rare occasion the UI thread
    holds the lock, publish() just skips that block's snapshot rather than waiting,
    so the audio thread can never stall on this. Skipping an occasional cosmetic
    frame at 60Hz is imperceptible; blocking the audio thread would not be
    acceptable at any frequency.
*/
class VisualizationPublisher
{
public:
    struct Snapshot
    {
        // v0.6.0: FluidEngine's grid is now fixed-size (no more Flow-Rate-dependent
        // active length), so there is no separate "active size" to publish anymore.
        std::array<float, (size_t) FluidEngine::kGridSize> gridL {};
        std::array<float, (size_t) FluidEngine::kGridSize> gridR {};
    };

    /** Audio thread only. Never blocks (try_lock() either succeeds immediately or
        this is a no-op) -- see the class-level comment for why.
    */
    void publish (const Snapshot& snapshot) noexcept
    {
        if (mutex.try_lock())
        {
            buffer = snapshot;
            mutex.unlock();
        }
    }

    /** UI thread only. May briefly block, bounded by the audio thread's tiny,
        allocation-free critical section above -- acceptable here since this never
        runs on the audio thread.
    */
    Snapshot read() const noexcept
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return buffer;
    }

private:
    mutable std::mutex mutex;
    Snapshot buffer {};
};
