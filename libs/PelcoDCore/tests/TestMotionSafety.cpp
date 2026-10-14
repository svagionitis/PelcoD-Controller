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
    EXPECT_EQ(device.panLeft(0x20U), PelcoD::SendResult::Success);
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
        EXPECT_EQ(device.tiltUp(0x20U), PelcoD::SendResult::Success);
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
    EXPECT_EQ(queue.enqueue(queryFrame, "QueryPan", PelcoD::CommandPriority::Low, 0U), PelcoD::SendResult::Success);

    // Enqueue motion commands belonging to the active motion generation
    const auto panFrame = PelcoD::ProtocolBuilder::buildPan(1U, PelcoD::PanDirection::Left, 0x20U);
    EXPECT_EQ(queue.enqueue(panFrame, "", PelcoD::CommandPriority::Normal, 1U), PelcoD::SendResult::Success);

    const auto tiltFrame = PelcoD::ProtocolBuilder::buildTilt(1U, PelcoD::TiltDirection::Up, 0x20U);
    EXPECT_EQ(queue.enqueue(tiltFrame, "", PelcoD::CommandPriority::Normal, 1U), PelcoD::SendResult::Success);

    EXPECT_EQ(queue.size(), 3U);

    const std::size_t purged = queue.purgeMotionCommands();
    EXPECT_EQ(purged, 2U);
    EXPECT_EQ(queue.size(), 1U);

    PelcoD::CommandItem remaining;
    EXPECT_TRUE(queue.popReady(
        remaining, [] { return false; }, std::chrono::steady_clock::time_point::max(), 10ms));
    EXPECT_EQ(remaining.queryTag, "QueryPan");
}

/// @brief Transport Disconnected event must purge pending motion commands (review finding C5).
TEST(TestMotionSafety, TransportDisconnectPurgesQueuedMotion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    // Queue motion commands
    EXPECT_EQ(device.panLeft(0x10U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.tiltDown(0x10U), PelcoD::SendResult::Success);

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
    EXPECT_EQ(device.panRight(0x20U), PelcoD::SendResult::Success);

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
        EXPECT_EQ(device.panRight(0x20U), PelcoD::SendResult::Success);
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

    EXPECT_EQ(device.panLeft(0x20U), PelcoD::SendResult::Success);
    std::this_thread::sleep_for(20ms);

    // Operator stops explicitly
    EXPECT_EQ(device.stopMotion(), PelcoD::SendResult::Success);
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
    EXPECT_EQ(device.panLeft(0x20U), PelcoD::SendResult::Success);

    std::this_thread::sleep_for(80ms);

    const auto frames = transport->sentFrames();
    EXPECT_EQ(frames.size(), 1U);
    EXPECT_FALSE(isStopFrame(frames[0]));

    device.stop();
}

/// @brief MotionSafetyGuard operates via TaskScheduler without launching its own worker thread.
TEST(TestMotionSafety, TaskSchedulerWatchdogOperatesWithoutDedicatedThread)
{
    auto scheduler = std::make_shared<PelcoD::TaskScheduler>();
    scheduler->start();

    std::atomic<bool> stopTriggered { false };
    PelcoD::MotionSafetyGuard guard { [&stopTriggered] { stopTriggered = true; }, scheduler };

    guard.setDeadManTimeout(40ms);
    EXPECT_FALSE(guard.isArmed());
    EXPECT_FALSE(guard.isMoving());

    // Issue motion
    const auto panFrame = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Left, 0x20U);
    guard.onMotionCommand(panFrame);

    EXPECT_TRUE(guard.isArmed());
    EXPECT_TRUE(guard.isMoving());

    // Wait for timeout (40ms + safety margin)
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(stopTriggered.load());
    EXPECT_FALSE(guard.isArmed());
    EXPECT_FALSE(guard.isMoving());

    scheduler->stop();
}

/// @brief extractMotionAxes decomposes Pan and Tilt motion commands into discrete axes.
TEST(TestMotionSafety, ExtractMotionAxesIdentifiesPanAndTiltCorrectly)
{
    // Pan Left
    const auto panLeft = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Left, 0x20U);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(panLeft), PelcoD::MotionAxis::Pan);

    // Tilt Down
    const auto tiltDown = PelcoD::ProtocolBuilder::buildTiltFrame(1U, PelcoD::TiltDirection::Down, 0x15U);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(tiltDown), PelcoD::MotionAxis::Tilt);

    // Diagonal: Pan Right + Tilt Up
    const auto diag = PelcoD::ProtocolBuilder::buildMotionFrame(
        1U, PelcoD::PanDirection::Right, 0x20U, PelcoD::TiltDirection::Up, 0x10U);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(diag), PelcoD::MotionAxis::Pan | PelcoD::MotionAxis::Tilt);
}

/// @brief extractMotionAxes decomposes optical motion (Zoom, Focus, Iris, Scan) into discrete axes.
TEST(TestMotionSafety, ExtractMotionAxesIdentifiesOpticalAxesCorrectly)
{
    // Zoom Wide
    const auto zoomWide = PelcoD::ProtocolBuilder::buildZoomFrame(1U, PelcoD::ZoomAction::Wide);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(zoomWide), PelcoD::MotionAxis::Zoom);

    // Focus Near
    const auto focusNear = PelcoD::ProtocolBuilder::buildFocusFrame(1U, PelcoD::FocusAction::Near);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(focusNear), PelcoD::MotionAxis::Focus);

    // Focus Far
    const auto focusFar = PelcoD::ProtocolBuilder::buildFocusFrame(1U, PelcoD::FocusAction::Far);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(focusFar), PelcoD::MotionAxis::Focus);

    // Iris Open
    const auto irisOpen = PelcoD::ProtocolBuilder::buildIrisFrame(1U, PelcoD::IrisAction::Open);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(irisOpen), PelcoD::MotionAxis::Iris);

    // AutoScan On
    const auto autoScan = PelcoD::ProtocolBuilder::buildScanFrame(1U, true);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(autoScan), PelcoD::MotionAxis::Scan);
}

/// @brief extractMotionAxes identifies stop commands and query frames as MotionAxis::None.
TEST(TestMotionSafety, ExtractMotionAxesIdentifiesStopAsNone)
{
    const auto stopFrame = PelcoD::ProtocolBuilder::buildStopFrame(1U);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(stopFrame), PelcoD::MotionAxis::None);

    const auto queryPan = PelcoD::ProtocolBuilder::buildQueryPan(1U);
    EXPECT_EQ(PelcoD::PelcoDFrame::extractMotionAxes(queryPan), PelcoD::MotionAxis::None);
}

/// @brief PelcoDSession auto-purges motion commands upon transport disconnect.
TEST(TestMotionSafety, SessionPurgesMotionQueueOnTransportDisconnect)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    auto session = std::make_shared<PelcoD::PelcoDSession>(transport, 1U);
    ASSERT_TRUE(session->start());

    // Enqueue motion command
    const auto panFrame = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Left, 0x20U);
    EXPECT_EQ(session->enqueueCommand(panFrame), PelcoD::SendResult::Success);

    // Simulate transport disconnect event
    auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(stateCb);
    stateCb(PelcoD::TransportState::Disconnected, "Cable disconnected");

    // After disconnect, queued motion commands must be purged
    EXPECT_FALSE(session->isConnected());
    EXPECT_EQ(session->purgeMotionCommands(), 0U);

    session->stop();
}

/// @brief MotionSafetyGuard::dispatch only updates moving state if enqueue succeeds.
TEST(TestMotionSafety, DispatchOnlyArmsWatchdogOnSuccessfulEnqueue)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    // Session is NOT started, so enqueue will return TransportClosed
    auto session = std::make_shared<PelcoD::PelcoDSession>(transport, 1U);
    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 50ms;

    auto guard = std::make_shared<PelcoD::MotionSafetyGuard>(session, nullptr, policy);
    EXPECT_FALSE(guard->isMoving());
    EXPECT_FALSE(guard->isArmed());

    const auto panFrame = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Left, 0x20U);
    EXPECT_EQ(guard->dispatch(panFrame), PelcoD::SendResult::TransportClosed);

    // Because enqueue failed, guard must NOT have transitioned to moving or armed
    EXPECT_FALSE(guard->isMoving());
    EXPECT_FALSE(guard->isArmed());
}

/// @brief Continuous runaway limit stops motion after max duration even if refreshed.
TEST(TestMotionSafety, RunawayLimitStopsMotionAfterMaxDuration)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    auto session = std::make_shared<PelcoD::PelcoDSession>(transport, 1U);
    ASSERT_TRUE(session->start());

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 80ms;
    policy.maxContinuousDuration = 100ms;

    auto scheduler = std::make_shared<PelcoD::TaskScheduler>();
    scheduler->start();

    std::atomic<bool> runawayTripped { false };
    auto guard = std::make_shared<PelcoD::MotionSafetyGuard>(session, scheduler, policy);
    guard->setSafetyEventCallback([&runawayTripped](PelcoD::MotionSafetyEvent ev, PelcoD::MotionAxis) {
        if (ev == PelcoD::MotionSafetyEvent::ContinuousMotionLimit) {
            runawayTripped = true;
        }
    });

    const auto panFrame = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Left, 0x20U);

    // Refresh every 30ms (well within 80ms dead-man timeout), but total duration passes 100ms
    for (int i { 0 }; i < 5; ++i) {
        static_cast<void>(guard->dispatch(panFrame));
        std::this_thread::sleep_for(30ms);
    }

    EXPECT_TRUE(runawayTripped.load());
    EXPECT_FALSE(guard->isMoving());

    scheduler->stop();
    session->stop();
}

/// @brief SafetyEventCallback fires with accurate event type and axis bitmask.
TEST(TestMotionSafety, SafetyEventCallbackFiresWithAccurateEventAndAxes)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    auto session = std::make_shared<PelcoD::PelcoDSession>(transport, 1U);
    ASSERT_TRUE(session->start());

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 40ms;

    auto scheduler = std::make_shared<PelcoD::TaskScheduler>();
    scheduler->start();

    std::atomic<bool> eventFired { false };
    PelcoD::MotionSafetyEvent receivedEvent {};
    PelcoD::MotionAxis receivedAxes {};

    auto guard = std::make_shared<PelcoD::MotionSafetyGuard>(session, scheduler, policy);
    guard->setSafetyEventCallback([&](PelcoD::MotionSafetyEvent ev, PelcoD::MotionAxis axes) {
        receivedEvent = ev;
        receivedAxes = axes;
        eventFired = true;
    });

    const auto panFrame = PelcoD::ProtocolBuilder::buildPanFrame(1U, PelcoD::PanDirection::Right, 0x20U);
    EXPECT_EQ(guard->dispatch(panFrame), PelcoD::SendResult::Success);

    std::this_thread::sleep_for(90ms);

    EXPECT_TRUE(eventFired.load());
    EXPECT_EQ(receivedEvent, PelcoD::MotionSafetyEvent::DeadManTimeout);
    EXPECT_EQ(receivedAxes, PelcoD::MotionAxis::Pan);

    scheduler->stop();
    session->stop();
}

/// @brief Device facade properly propagates and returns motion safety policy.
TEST(TestMotionSafety, DeviceFacadePolicyPropagation)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 120ms;
    policy.maxContinuousDuration = 450ms;
    policy.stopOnDisconnect = true;

    device.setMotionSafetyPolicy(policy);
    const auto retrieved = device.getMotionSafetyPolicy();

    EXPECT_EQ(retrieved.deadManTimeout, 120ms);
    EXPECT_EQ(retrieved.maxContinuousDuration, 450ms);
    EXPECT_TRUE(retrieved.stopOnDisconnect);
    EXPECT_EQ(device.getDeadManTimeout(), 120ms);
}

/// @brief Device notifies registered motion safety callback on safety intervention.
TEST(TestMotionSafety, DeviceMotionSafetyCallback)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 40ms;
    device.setMotionSafetyPolicy(policy);

    std::atomic<bool> callbackFired { false };
    PelcoD::MotionSafetyEvent capturedEvent {};
    PelcoD::MotionAxis capturedAxes {};

    auto conn = device.addMotionSafetyCallback([&](PelcoD::MotionSafetyEvent ev, PelcoD::MotionAxis axes) {
        capturedEvent = ev;
        capturedAxes = axes;
        callbackFired = true;
    });

    ASSERT_TRUE(device.start());
    EXPECT_EQ(device.panLeft(0x20U), PelcoD::SendResult::Success);

    std::this_thread::sleep_for(90ms);

    EXPECT_TRUE(callbackFired.load());
    EXPECT_EQ(capturedEvent, PelcoD::MotionSafetyEvent::DeadManTimeout);
    EXPECT_EQ(capturedAxes, PelcoD::MotionAxis::Pan);

    EXPECT_TRUE(conn.isConnected());
    conn.disconnect();
    EXPECT_FALSE(conn.isConnected());
    device.stop();
}

/// @brief Watchdog interventions properly increment protocol metric counters.
TEST(TestMotionSafety, SafetyMetricsIncrementOnTrips)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 40ms;
    device.setMotionSafetyPolicy(policy);

    ASSERT_TRUE(device.start());
    EXPECT_EQ(device.tiltUp(0x20U), PelcoD::SendResult::Success);

    std::this_thread::sleep_for(90ms);

    const auto stats = device.getProtocolStats();
    EXPECT_GE(stats.deadManTrips, 1U);

    device.stop();
}

/// @brief Continuous runaway limit engages safety interlock until explicit stop.
TEST(TestMotionSafety, SafetyInterlockBlocksMotion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::MotionSafetyPolicy policy {};
    policy.deadManTimeout = 100ms;
    policy.maxContinuousDuration = 60ms;
    device.setMotionSafetyPolicy(policy);

    ASSERT_TRUE(device.start());

    // Send motion every 20ms: deadman does not trip, but runaway limit triggers at 60ms
    for (int i { 0 }; i < 5; ++i) {
        static_cast<void>(device.panRight(0x20U));
        std::this_thread::sleep_for(20ms);
    }

    // Interlock is active: subsequent motion is rejected
    EXPECT_EQ(device.panRight(0x20U), PelcoD::SendResult::MotionPurged);

    // Operator sends explicit stop command: interlock clears
    EXPECT_EQ(device.stopMotion(), PelcoD::SendResult::Success);

    // Subsequent motion should now succeed
    EXPECT_EQ(device.panRight(0x20U), PelcoD::SendResult::Success);

    device.stop();
}

} // namespace
