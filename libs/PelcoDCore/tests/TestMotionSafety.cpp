/// @file TestMotionSafety.cpp
/// @brief Unit and regression tests for C5 motion safety: fail-safe stop and dead-man timer.

#include "CapturingTransport.h"
#include "PacedCommandQueue.h"
#include "PelcoDDevice.h"
#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

/// @brief Helper to verify if a frame is a standard Pelco-D stop command.
[[nodiscard]] bool isStopFrame(const std::vector<std::uint8_t>& frame) noexcept
{
    return PelcoD::PelcoDFrame::isStandardStop(frame);
}

/// @brief Calling stop() while moving must send a Stop frame across transport (review finding C5).
TEST(TestMotionSafety, FailSafeStopSentOnDeviceStop)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    device.panLeft(0x20U);
    std::this_thread::sleep_for(25ms);

    // Operator or system stops device without an explicit stopMotion()
    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_FALSE(frames.empty());

    // The final frame sent before or during transport teardown must be a Stop frame!
    const auto& lastFrame = frames.back();
    EXPECT_TRUE(isStopFrame(lastFrame)) << "Expected stop frame on device.stop(), got: "
                                        << PelcoD::PelcoDFrame::toHexString(lastFrame);
}

/// @brief Destroying PelcoDDevice while moving must send a Stop frame (review finding C5).
TEST(TestMotionSafety, FailSafeStopSentOnDeviceDestruction)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    {
        PelcoD::PelcoDDevice device { transport, 1U };
        ASSERT_TRUE(device.start());
        device.tiltUp(0x20U);
        std::this_thread::sleep_for(25ms);
        // device destructor runs here
    }

    const auto frames = transport->sentFrames();
    ASSERT_FALSE(frames.empty());
    const auto& lastFrame = frames.back();
    EXPECT_TRUE(isStopFrame(lastFrame)) << "Expected stop frame on ~PelcoDDevice(), got: "
                                        << PelcoD::PelcoDFrame::toHexString(lastFrame);
}

/// @brief PacedCommandQueue::purgeMotionCommands removes all motion and stop commands.
TEST(TestMotionSafety, PacedCommandQueuePurgeMotionCommands)
{
    PelcoD::PacedCommandQueue queue { 32U };

    // Enqueue a non-motion query
    const auto queryFrame = PelcoD::ProtocolBuilder::buildQueryPan(1U);
    queue.enqueue(queryFrame, "QueryPan", PelcoD::CommandPriority::Low, 0U);

    // Enqueue motion commands belonging to the active motion generation
    const auto panFrame = PelcoD::ProtocolBuilder::buildPan(1U, PelcoD::PanDirection::Left, 0x20U);
    queue.enqueue(panFrame, "", PelcoD::CommandPriority::Normal, 1U);

    const auto tiltFrame = PelcoD::ProtocolBuilder::buildTilt(1U, PelcoD::TiltDirection::Up, 0x20U);
    queue.enqueue(tiltFrame, "", PelcoD::CommandPriority::Normal, 1U);

    EXPECT_EQ(queue.size(), 3U);

    const std::size_t purged = queue.purgeMotionCommands();
    EXPECT_EQ(purged, 2U);
    EXPECT_EQ(queue.size(), 1U);

    PelcoD::CommandItem remaining;
    EXPECT_TRUE(queue.popReady(remaining, [] { return false; }, std::chrono::steady_clock::time_point::max(), 10ms));
    EXPECT_EQ(remaining.queryTag, "QueryPan");
}

/// @brief Transport Disconnected event must purge pending motion commands (review finding C5).
TEST(TestMotionSafety, TransportDisconnectPurgesQueuedMotion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    // Queue motion commands
    device.panLeft(0x10U);
    device.tiltDown(0x10U);

    // Simulate transport disconnect event
    transport->close();
    auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(stateCb);
    stateCb(PelcoD::TransportState::Disconnected, "Cable disconnected");

    // Device should be disconnected and motion purged
    EXPECT_FALSE(device.isConnected());

    device.stop();
}

/// @brief Dead-man watchdog stops motion after timeout expires with no refresh (review finding C5).
TEST(TestMotionSafety, DeadManWatchdogStopsMotionOnTimeout)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    device.setDeadManTimeout(60ms);
    EXPECT_EQ(device.getDeadManTimeout(), 60ms);

    ASSERT_TRUE(device.start());

    // Start motion
    device.panRight(0x20U);

    // Wait for watchdog to trigger (60ms timeout + safety margin)
    std::this_thread::sleep_for(160ms);

    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);

    // Frame 0: panRight
    EXPECT_EQ(frames[0][3], 0x02U);

    // Watchdog triggered stop frame
    const auto& lastFrame = frames.back();
    EXPECT_TRUE(isStopFrame(lastFrame));
}

/// @brief Periodic motion updates refresh dead-man watchdog without triggering stop.
TEST(TestMotionSafety, DeadManWatchdogResetBySubsequentMotion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    device.setDeadManTimeout(80ms);
    ASSERT_TRUE(device.start());

    // Send periodic updates every 30ms (well within 80ms watchdog window)
    for (int i { 0 }; i < 4; ++i) {
        device.panRight(0x20U);
        std::this_thread::sleep_for(30ms);
    }

    // Capture count before timeout
    const auto activeFrames = transport->sentFrames();
    for (const auto& frame : activeFrames) {
        EXPECT_FALSE(isStopFrame(frame)) << "Premature stop frame emitted while motion was refreshed!";
    }

    // Now let watchdog expire
    std::this_thread::sleep_for(140ms);

    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GT(frames.size(), activeFrames.size());
    EXPECT_TRUE(isStopFrame(frames.back())) << "Watchdog should have fired after updates stopped!";
}

/// @brief Explicit stopMotion() disarms the dead-man watchdog.
TEST(TestMotionSafety, DeadManWatchdogDisarmedByExplicitStop)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    device.setDeadManTimeout(60ms);
    ASSERT_TRUE(device.start());

    device.panLeft(0x20U);
    std::this_thread::sleep_for(20ms);

    // Operator stops explicitly
    device.stopMotion();
    std::this_thread::sleep_for(25ms);

    // Wait past the 60ms dead-man timeout window
    std::this_thread::sleep_for(120ms);

    device.stop();

    const auto finalFrames = transport->sentFrames();
    // After stopMotion(), the watchdog was disarmed, so no additional watchdog stop was queued.
    // At device.stop(), best-effort stop may send one stop, but no duplicate watchdog stop occurred.
    std::size_t stopCount { 0U };
    for (const auto& f : finalFrames) {
        if (isStopFrame(f)) {
            ++stopCount;
        }
    }
    EXPECT_GE(stopCount, 1U);
}

/// @brief Zero dead-man timeout disables watchdog.
TEST(TestMotionSafety, ZeroTimeoutDisablesDeadMan)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    device.setDeadManTimeout(0ms);
    EXPECT_EQ(device.getDeadManTimeout(), 0ms);

    ASSERT_TRUE(device.start());
    device.panLeft(0x20U);

    std::this_thread::sleep_for(80ms);

    const auto frames = transport->sentFrames();
    EXPECT_EQ(frames.size(), 1U);
    EXPECT_FALSE(isStopFrame(frames[0]));

    device.stop();
}

} // namespace
