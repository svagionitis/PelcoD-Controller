/// @file TestPatrolController.cpp
/// @brief Unit tests for PelcoD::PatrolController sequencing, dwell timing, and state transitions.

#include "PatrolController.h"
#include "TestHelpers.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

/// @brief Verify CRUD operations on patrol steps and default state.
static void testStepManagement()
{
    PelcoD::PatrolController controller;

    assert(controller.stepCount() == 0U);
    assert(!controller.isRunning());
    assert(!controller.isPaused());
    assert(controller.getState() == PelcoD::PatrolState::Idle);

    // Starting an empty tour must safely fail
    assert(!controller.start());

    // Add steps
    controller.addStep(PelcoD::PatrolStep { 10U, 5U, "Gate", 0U });
    controller.addStep(PelcoD::PatrolStep { 20U, 10U, "Fence", 0U });
    assert(controller.stepCount() == 2U);

    // Insert step
    controller.insertStep(1U, PelcoD::PatrolStep { 15U, 7U, "Middle", 0U });
    assert(controller.stepCount() == 3U);

    const auto steps = controller.getSteps();
    assert(steps[0].presetId == 10U && steps[0].name == "Gate");
    assert(steps[1].presetId == 15U && steps[1].name == "Middle");
    assert(steps[2].presetId == 20U && steps[2].name == "Fence");

    // Update step
    PelcoD::PatrolStep updatedStep { 16U, 8U, "Updated Middle", 0U };
    assert(controller.setStep(1U, updatedStep));
    assert(controller.getSteps()[1].presetId == 16U);

    // Remove step
    assert(controller.removeStep(1U));
    assert(controller.stepCount() == 2U);
    assert(controller.getSteps()[1].presetId == 20U);

    // Clear steps
    controller.clearSteps();
    assert(controller.stepCount() == 0U);

    std::cout << "  testStepManagement: PASSED\n";
}

/// @brief Verify full sequence execution across presets with dwell expiration.
static void testTourExecutionAndAdvancement()
{
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::uint8_t> dispatchedPresets;
    bool finished = false;

    PelcoD::PatrolController controller([&](std::uint8_t presetId) {
        std::lock_guard<std::mutex> lock(mtx);
        dispatchedPresets.push_back(presetId);
    });

    controller.setTourFinishedCallback([&]() {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
        cv.notify_all();
    });

    controller.addStep(PelcoD::PatrolStep { 1U, 1U, "Preset 1", 0U });
    controller.addStep(PelcoD::PatrolStep { 2U, 1U, "Preset 2", 0U });
    controller.addStep(PelcoD::PatrolStep { 3U, 1U, "Preset 3", 0U });
    controller.setLoop(false);

    assert(controller.start());
    assert(controller.isRunning());

    {
        std::unique_lock<std::mutex> lock(mtx);
        const bool completed = cv.wait_for(lock, std::chrono::seconds(6), [&]() { return finished; });
        assert(completed && "Tour failed to finish in expected duration");
        assert(dispatchedPresets.size() == 3U);
        assert(dispatchedPresets[0] == 1U);
        assert(dispatchedPresets[1] == 2U);
        assert(dispatchedPresets[2] == 3U);
    }

    assert(controller.getState() == PelcoD::PatrolState::Idle);
    std::cout << "  testTourExecutionAndAdvancement: PASSED\n";
}

/// @brief Verify pause and resume preserving remaining dwell time.
static void testPauseAndResume()
{
    PelcoD::PatrolController controller;
    controller.addStep(PelcoD::PatrolStep { 5U, 4U, "Hold Step", 0U });

    assert(controller.start());
    assert(controller.isRunning());

    // Wait ~1.2s
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    controller.pause();
    assert(controller.isPaused());
    assert(controller.getState() == PelcoD::PatrolState::Paused);

    const auto pausedRemaining = controller.getRemainingDwellSeconds();
    assert(pausedRemaining <= 3U);

    // Wait another second while paused
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    assert(controller.getRemainingDwellSeconds() == pausedRemaining && "Dwell time ticked while paused");

    controller.resume();
    assert(controller.isRunning());

    controller.stop();
    assert(!controller.isRunning());
    assert(controller.getState() == PelcoD::PatrolState::Idle);

    std::cout << "  testPauseAndResume: PASSED\n";
}

/// @brief Verify manual step skipping via nextStep and previousStep.
static void testManualSkip()
{
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::uint8_t> dispatchedPresets;

    PelcoD::PatrolController controller([&](std::uint8_t presetId) {
        std::lock_guard<std::mutex> lock(mtx);
        dispatchedPresets.push_back(presetId);
        cv.notify_all();
    });

    controller.addStep(PelcoD::PatrolStep { 1U, 20U, "Step 1", 0U });
    controller.addStep(PelcoD::PatrolStep { 2U, 20U, "Step 2", 0U });
    controller.addStep(PelcoD::PatrolStep { 3U, 20U, "Step 3", 0U });

    assert(controller.start());

    // First step dispatched
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&]() { return !dispatchedPresets.empty(); });
        assert(dispatchedPresets.back() == 1U);
    }

    // Skip to next step
    controller.nextStep();
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&]() { return dispatchedPresets.size() >= 2U; });
        assert(dispatchedPresets.back() == 2U);
    }

    // Skip back to previous step
    controller.previousStep();
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&]() { return dispatchedPresets.size() >= 3U; });
        assert(dispatchedPresets.back() == 1U);
    }

    controller.stop();
    assert(!controller.isRunning());

    std::cout << "  testManualSkip: PASSED\n";
}

int main()
{
    PelcoDTest::initTestHarness();

    std::cout << "[TestPatrolController] Running...\n";
    testStepManagement();
    testTourExecutionAndAdvancement();
    testPauseAndResume();
    testManualSkip();
    std::cout << "[TestPatrolController] All tests passed.\n";
    return 0;
}
