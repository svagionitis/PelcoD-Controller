/// @file TestPelcoDDevice.cpp
/// @brief Lifecycle and destruction-safety tests for PelcoD::PelcoDDevice (review finding C3).

#include "CapturingTransport.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "PelcoDFrame.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

/// @brief Builds a valid 7-byte pan-position response (0x59) for device address 1.
/// @return Encoded Pelco-D frame.
std::vector<std::uint8_t> panResponse()
{
    return PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x59U, 0x12U, 0x34U);
}

/// @brief A data callback captured before stop() must not reach the device after stop() returns.
/// @details Transports invoke a *copy* of the registered callback outside their lock, so
///          `setDataCallback(nullptr)` is not a barrier (C3b). The stale copy must be rejected.
TEST(PelcoDDeviceLifecycle, StaleRxCbIgnoredAfterStop)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::atomic<std::uint32_t> rxCount { 0U };
    static_cast<void>(device.addTrafficCallback(
        [&rxCount](bool, const std::vector<std::uint8_t>&) { rxCount.fetch_add(1U); }, false, true));

    ASSERT_TRUE(device.start());
    const auto staleRx = transport->lastDataCb();
    ASSERT_TRUE(staleRx);

    device.stop();
    staleRx(panResponse());

    EXPECT_EQ(rxCount.load(), 0U);
}

/// @brief A data callback from a previous session must be rejected after a restart.
/// @details The fresh session's callback must still deliver frames normally.
TEST(PelcoDDeviceLifecycle, StaleRxCbIgnoredAfterRestart)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::atomic<std::uint32_t> rxCount { 0U };
    static_cast<void>(device.addTrafficCallback(
        [&rxCount](bool, const std::vector<std::uint8_t>&) { rxCount.fetch_add(1U); }, false, true));

    ASSERT_TRUE(device.start());
    const auto session1Rx = transport->lastDataCb();
    device.stop();
    ASSERT_TRUE(device.start());

    session1Rx(panResponse());
    EXPECT_EQ(rxCount.load(), 0U);

    transport->lastDataCb()(panResponse());
    EXPECT_EQ(rxCount.load(), 1U);

    device.stop();
}

/// @brief A stale Disconnected notification from a previous session must not affect the new session.
TEST(PelcoDDeviceLifecycle, StaleStateCbIgnoredAfterRestart)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    const auto session1State = transport->lastStateCb();
    device.stop();
    ASSERT_TRUE(device.start());

    std::atomic<std::uint32_t> statusCount { 0U };
    static_cast<void>(
        device.addStatusCallback([&statusCount](const PelcoD::DeviceStatus&) { statusCount.fetch_add(1U); }));

    session1State(PelcoD::TransportState::Disconnected, "stale");

    EXPECT_TRUE(device.getStatus().connected);
    EXPECT_EQ(statusCount.load(), 0U);

    device.stop();
}

/// @brief The destructor must not return while an RX callback is still executing inside the device.
/// @details A status callback blocks on the RX thread; a second thread destroys the device. Before the
///          fix the destructor returned immediately and the RX thread then touched freed members (UAF).
TEST(PelcoDDeviceLifecycle, DtorWaitsForInFlightRx)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_unique<PelcoD::PelcoDDevice>(mock, std::uint8_t { 1U });
    ASSERT_TRUE(device->start());

    std::promise<void> enteredPromise {};
    auto entered = enteredPromise.get_future();
    std::promise<void> releasePromise {};
    const std::shared_future<void> release { releasePromise.get_future().share() };
    std::atomic<bool> first { true };

    static_cast<void>(device->addStatusCallback([&](const PelcoD::DeviceStatus&) {
        if (first.exchange(false)) {
            enteredPromise.set_value();
            static_cast<void>(release.wait_for(5s));
        }
    }));

    std::thread rxThread { [&mock] { mock->injectRxData(panResponse()); } };
    if (entered.wait_for(2s) != std::future_status::ready) {
        releasePromise.set_value();
        rxThread.join();
        FAIL() << "RX callback was never entered";
    }

    std::atomic<bool> destroyed { false };
    std::thread killer { [&device, &destroyed] {
        device.reset();
        destroyed.store(true);
    } };

    std::this_thread::sleep_for(150ms);
    EXPECT_FALSE(destroyed.load()) << "Destructor returned while an RX callback was in flight";

    releasePromise.set_value();
    rxThread.join();
    killer.join();
    EXPECT_TRUE(destroyed.load());
}

/// @brief stop() invoked from inside an RX-thread callback must neither deadlock nor terminate.
TEST(PelcoDDeviceLifecycle, StopFromRxCallbackIsSafe)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::PelcoDDevice device { mock, 1U };
    ASSERT_TRUE(device.start());

    std::atomic<bool> stopped { false };
    static_cast<void>(device.addStatusCallback([&device, &stopped](const PelcoD::DeviceStatus&) {
        if (!stopped.exchange(true)) {
            device.stop();
        }
    }));

    std::thread rxThread { [&mock] { mock->injectRxData(panResponse()); } };
    rxThread.join();

    EXPECT_TRUE(stopped.load());
    EXPECT_FALSE(device.isConnected());
}

/// @brief A retried motion command must not execute after stopMotion() (review finding C4).
/// @details If panLeft() transmission fails and schedules a backoff retry, a subsequent
///          stopMotion() must cancel the pending retry so motion does not resume.
TEST(PelcoDDeviceLifecycle, MotionRetryCancelledByStopMotion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::RetryConfig cfg;
    cfg.maxRetries = 2U;
    cfg.initialBackoff = 50ms;
    cfg.strategy = PelcoD::BackoffStrategy::Fixed;
    cfg.retryOnTransportError = true;
    device.setRetryConfig(cfg);

    // Fail the very first send (panLeft)
    transport->setFailSendCount(1U);
    ASSERT_TRUE(device.start());

    device.panLeft(0x20U);
    std::this_thread::sleep_for(15ms); // Allow worker thread to attempt and fail panLeft

    // Operator commands stop
    device.stopMotion();

    // Wait past the 50ms backoff interval
    std::this_thread::sleep_for(120ms);

    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);
    // Frame 0: panLeft (cmd2=0x04)
    EXPECT_EQ(frames[0][3], 0x04U);
    // Frame 1: stop (cmd1=0x00, cmd2=0x00)
    EXPECT_EQ(frames[1][2], 0x00U);
    EXPECT_EQ(frames[1][3], 0x00U);

    // There must NOT be any subsequent panLeft frame after stop!
    EXPECT_EQ(frames.size(), 2U) << "Retried panLeft executed after stopMotion!";
}

/// @brief A retried motion command must not execute after a newer direction command (review finding C4).
/// @details If panLeft() fails and is queued for retry, an immediate panRight() must
///          cancel the panLeft retry so the direction is not inverted later.
TEST(PelcoDDeviceLifecycle, MotionRetryCancelledByDirectionChange)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::RetryConfig cfg;
    cfg.maxRetries = 2U;
    cfg.initialBackoff = 50ms;
    cfg.strategy = PelcoD::BackoffStrategy::Fixed;
    cfg.retryOnTransportError = true;
    device.setRetryConfig(cfg);

    transport->setFailSendCount(1U);
    ASSERT_TRUE(device.start());

    device.panLeft(0x20U);
    std::this_thread::sleep_for(15ms);

    // Operator changes direction to right
    device.panRight(0x20U);

    std::this_thread::sleep_for(120ms);

    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);
    EXPECT_EQ(frames[0][3], 0x04U); // PanLeft
    EXPECT_EQ(frames[1][3], 0x02U); // PanRight

    EXPECT_EQ(frames.size(), 2U) << "Retried panLeft executed after panRight!";
}

} // namespace
