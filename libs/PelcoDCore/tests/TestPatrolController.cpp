/// @file TestPatrolController.cpp
/// @brief Unit tests for PelcoD::PatrolController sequencing, dwell timing, and state transitions.

#include "PatrolController.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace {

/// @brief Verify CRUD operations on patrol steps and default state.
TEST(PatrolControllerTest, StepManagement)
{
    PelcoD::PatrolController controller;

    EXPECT_EQ(controller.stepCount(), 0U);
    EXPECT_FALSE(controller.isRunning());
    EXPECT_FALSE(controller.isPaused());
    EXPECT_EQ(controller.getState(), PelcoD::PatrolState::Idle);

    // Starting an empty tour must safely fail
    EXPECT_FALSE(controller.start());

    // Add steps
    controller.addStep(PelcoD::PatrolStep { 10U, 5U, "Gate", 0U });
    controller.addStep(PelcoD::PatrolStep { 20U, 10U, "Fence", 0U });
    EXPECT_EQ(controller.stepCount(), 2U);

    // Insert step
    controller.insertStep(1U, PelcoD::PatrolStep { 15U, 7U, "Middle", 0U });
    EXPECT_EQ(controller.stepCount(), 3U);

    const auto steps = controller.getSteps();
    ASSERT_EQ(steps.size(), 3U);
    EXPECT_EQ(steps[0].presetId, 10U);
    EXPECT_EQ(steps[0].name, "Gate");
    EXPECT_EQ(steps[1].presetId, 15U);
    EXPECT_EQ(steps[1].name, "Middle");
    EXPECT_EQ(steps[2].presetId, 20U);
    EXPECT_EQ(steps[2].name, "Fence");

    // Update step
    PelcoD::PatrolStep updatedStep { 16U, 8U, "Updated Middle", 0U };
    EXPECT_TRUE(controller.setStep(1U, updatedStep));
    EXPECT_EQ(controller.getSteps()[1].presetId, 16U);

    // Remove step
    EXPECT_TRUE(controller.removeStep(1U));
    EXPECT_EQ(controller.stepCount(), 2U);
    EXPECT_EQ(controller.getSteps()[1].presetId, 20U);

    // Clear steps
    controller.clearSteps();
    EXPECT_EQ(controller.stepCount(), 0U);
}

/// @brief Verify full sequence execution across presets with dwell expiration.
TEST(PatrolControllerTest, TourExecutionAndAdvancement)
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

    ASSERT_TRUE(controller.start());
    EXPECT_TRUE(controller.isRunning());

    {
        std::unique_lock<std::mutex> lock(mtx);
        const bool completed = cv.wait_for(lock, std::chrono::seconds(6), [&]() { return finished; });
        ASSERT_TRUE(completed) << "Tour failed to finish in expected duration";
        ASSERT_EQ(dispatchedPresets.size(), 3U);
        EXPECT_EQ(dispatchedPresets[0], 1U);
        EXPECT_EQ(dispatchedPresets[1], 2U);
        EXPECT_EQ(dispatchedPresets[2], 3U);
    }

    EXPECT_EQ(controller.getState(), PelcoD::PatrolState::Idle);
}

/// @brief Verify pause and resume preserving remaining dwell time.
TEST(PatrolControllerTest, PauseAndResume)
{
    PelcoD::PatrolController controller;
    controller.addStep(PelcoD::PatrolStep { 5U, 4U, "Hold Step", 0U });

    ASSERT_TRUE(controller.start());
    EXPECT_TRUE(controller.isRunning());

    // Wait ~1.2s
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    controller.pause();
    EXPECT_TRUE(controller.isPaused());
    EXPECT_EQ(controller.getState(), PelcoD::PatrolState::Paused);

    const auto pausedRemaining = controller.getRemainingDwellSeconds();
    EXPECT_LE(pausedRemaining, 3U);

    // Wait another second while paused
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(controller.getRemainingDwellSeconds(), pausedRemaining) << "Dwell time ticked while paused";

    controller.resume();
    EXPECT_TRUE(controller.isRunning());

    controller.stop();
    EXPECT_FALSE(controller.isRunning());
    EXPECT_EQ(controller.getState(), PelcoD::PatrolState::Idle);
}

/// @brief Verify manual step skipping via nextStep and previousStep.
TEST(PatrolControllerTest, ManualSkip)
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

    ASSERT_TRUE(controller.start());

    // First step dispatched
    {
        std::unique_lock<std::mutex> lock(mtx);
        bool ok = cv.wait_for(lock, std::chrono::seconds(1), [&]() { return !dispatchedPresets.empty(); });
        ASSERT_TRUE(ok);
        EXPECT_EQ(dispatchedPresets.back(), 1U);
    }

    // Skip to next step
    controller.nextStep();
    {
        std::unique_lock<std::mutex> lock(mtx);
        bool ok = cv.wait_for(lock, std::chrono::seconds(1), [&]() { return dispatchedPresets.size() >= 2U; });
        ASSERT_TRUE(ok);
        EXPECT_EQ(dispatchedPresets.back(), 2U);
    }

    // Skip back to previous step
    controller.previousStep();
    {
        std::unique_lock<std::mutex> lock(mtx);
        bool ok = cv.wait_for(lock, std::chrono::seconds(1), [&]() { return dispatchedPresets.size() >= 3U; });
        ASSERT_TRUE(ok);
        EXPECT_EQ(dispatchedPresets.back(), 1U);
    }

    controller.stop();
    EXPECT_FALSE(controller.isRunning());
}

/// @brief Verify background thread is NOT spawned until start() and terminates on stop().
TEST(PatrolControllerTest, LazyThreadLifecycle)
{
    PelcoD::PatrolController controller;
    EXPECT_FALSE(controller.isWorkerActive()) << "Worker thread spawned eagerly in constructor!";

    controller.addStep(PelcoD::PatrolStep { 1U, 5U, "Gate", 0U });
    EXPECT_FALSE(controller.isWorkerActive()) << "Worker thread spawned before start()!";

    ASSERT_TRUE(controller.start());
    EXPECT_TRUE(controller.isWorkerActive()) << "Worker thread not active after start()!";

    controller.stop();
    EXPECT_FALSE(controller.isWorkerActive()) << "Worker thread remained active after stop()!";

    // Restart sequence to verify thread can cleanly re-spawn
    ASSERT_TRUE(controller.start());
    EXPECT_TRUE(controller.isWorkerActive()) << "Worker thread not active after second start()!";

    controller.stop();
    EXPECT_FALSE(controller.isWorkerActive()) << "Worker thread active after second stop()!";
}

} // namespace
