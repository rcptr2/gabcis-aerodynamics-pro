/*
    Gabci's AeroDynamics
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by Gabor
    Tomori and Gemini, under Gabor Tomori's direction and with Gemini's review, in 2026.
*/

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

#include "VisualizationPublisher.h"

TEST_CASE ("VisualizationPublisher: single-threaded read-after-publish returns exactly what was published",
           "[VisualizationPublisher]")
{
    VisualizationPublisher publisher;

    VisualizationPublisher::Snapshot snapshot;
    snapshot.gridL.fill (1.5f);
    snapshot.gridR.fill (-2.5f);

    publisher.publish (snapshot);
    const auto readBack = publisher.read();

    REQUIRE (readBack.gridL[0] == 1.5f);
    REQUIRE (readBack.gridR[0] == -2.5f);
    REQUIRE (readBack.gridL[(size_t) FluidEngine::kGridSize - 1] == 1.5f);
}

TEST_CASE ("VisualizationPublisher: concurrent publish/read never observes a torn snapshot",
           "[VisualizationPublisher][stability]")
{
    // Real std::thread stress test (not just an argument on paper): one thread
    // publishes a strictly-increasing "generation" pattern as fast as possible, the
    // main thread reads concurrently and checks every value in the snapshot it got
    // back is internally consistent with a single generation -- if the double
    // -buffer's atomic hand-off had a race, this would observe gridL/gridR values
    // from two different generations mixed in the same snapshot.
    VisualizationPublisher publisher;
    std::atomic<bool> stop { false };

    std::thread producer ([&publisher, &stop]
    {
        int generation = 0;

        while (! stop.load (std::memory_order_relaxed))
        {
            VisualizationPublisher::Snapshot snapshot;
            snapshot.gridL.fill ((float) generation);
            snapshot.gridR.fill ((float) generation * 2.0f);

            publisher.publish (snapshot);
            ++generation;
        }
    });

    // A std::thread that is still joinable when its destructor runs calls
    // std::terminate() -- if any REQUIRE below throws (Catch2's default failure
    // mode), the stack unwinds past `producer` without this cleanup running first,
    // turning a normal test failure into a process-wide SIGABRT that masks the real
    // failure reason. This guard runs first during unwinding (destroyed before
    // `producer`, C++ destroys locals in reverse declaration order) and joins it
    // unconditionally, so a REQUIRE failure reports cleanly instead.
    struct JoinOnExit
    {
        std::thread& thread;
        std::atomic<bool>& stopFlag;
        ~JoinOnExit()
        {
            stopFlag.store (true, std::memory_order_relaxed);
            if (thread.joinable())
                thread.join();
        }
    } joinGuard { producer, stop };

    int readsChecked = 0;
    for (int i = 0; i < 200000; ++i)
    {
        const auto snapshot = publisher.read();

        const auto expectedGeneration = snapshot.gridL[0];
        for (const auto v : snapshot.gridL)
            REQUIRE (v == expectedGeneration);
        for (const auto v : snapshot.gridR)
            REQUIRE (v == expectedGeneration * 2.0f);

        ++readsChecked;
    }

    REQUIRE (readsChecked == 200000);
}
