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

    const auto frames = transport->sentFrames();
    device.stop();

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

    const auto frames = transport->sentFrames();
    device.stop();

    ASSERT_GE(frames.size(), 2U);
    EXPECT_EQ(frames[0][3], 0x04U); // PanLeft
    EXPECT_EQ(frames[1][3], 0x02U); // PanRight

    EXPECT_EQ(frames.size(), 2U) << "Retried panLeft executed after panRight!";
}

/// @brief Transport double that asynchronously responds to query frames.
class AutoEchoTransport final : public PelcoD::ITransport {
public:
    AutoEchoTransport() = default;
    ~AutoEchoTransport() override
    {
        for (auto& th : m_threads) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

    [[nodiscard]] bool open() override
    {
        m_open.store(true);
        return true;
    }

    void close() override
    {
        m_open.store(false);
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        if (!m_open.load() || data.size() < 4U) {
            return false;
        }

        std::scoped_lock lock { m_mutex };
        auto cb = m_dataCb;
        if (cb && data.size() >= 7U) {
            std::vector<std::uint8_t> resp;
            if (data[3] == 0x51U) { // QueryPan
                resp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x59U, 0x12U, 0x34U);
            } else if (data[3] == 0x53U) { // QueryTilt
                resp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x5BU, 0x05U, 0x67U);
            } else if (data[3] == 0x55U) { // QueryZoom
                resp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x5DU, 0x02U, 0x00U);
            }
            if (!resp.empty()) {
                m_threads.emplace_back([cb = std::move(cb), resp = std::move(resp)]() mutable {
                    std::this_thread::yield();
                    cb(resp);
                });
            }
        }
        return true;
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        std::scoped_lock lock { m_mutex };
        m_dataCb = std::move(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::scoped_lock lock { m_mutex };
        m_stateCb = std::move(callback);
    }

private:
    std::atomic<bool> m_open { false };
    mutable std::mutex m_mutex {};
    DataReceivedCallback m_dataCb {};
    StateChangedCallback m_stateCb {};
    std::vector<std::thread> m_threads {};
};

/// @brief Concurrently queued queries must not clobber m_querySentTime or cause data races (review finding H1).
/// @details Verifies that when queries and responses interleave rapidly across worker and RX threads,
///          query latencies remain non-negative, valid, and free of data races.
TEST(PelcoDDeviceConcurrency, QuerySentTimeRaceSafety)
{
    auto transport = std::make_shared<AutoEchoTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::atomic<bool> negativeDurationObserved { false };
    std::atomic<std::uint32_t> completedQueries { 0U };

    static_cast<void>(device.addQueryLatencyCallback(
        [&](const std::string& /*tag*/, std::chrono::microseconds durationUs, bool success) {
            if (success) {
                completedQueries.fetch_add(1U);
                if (durationUs.count() < 0) {
                    negativeDurationObserved.store(true);
                }
            }
        }));

    ASSERT_TRUE(device.start());

    constexpr std::uint32_t kIterations { 30U };
    for (std::uint32_t i { 0U }; i < kIterations; ++i) {
        device.queryPan();
        device.queryTilt();
        device.queryZoom();
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(150ms);
    device.stop();

    EXPECT_GT(completedQueries.load(), 0U);
    EXPECT_FALSE(negativeDurationObserved.load()) << "m_querySentTime was clobbered by concurrent query or read racily!";
}

/// @brief When checkQueryTimeout races with query dispatch and responses, timeout callbacks must
///        never observe an empty tag or invalid duration (H1).
TEST(PelcoDDeviceConcurrency, QueryTimeoutRaceSafety)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    device.setQueryTimeoutMs(20U);

    std::atomic<bool> emptyTagObserved { false };
    std::atomic<bool> invalidLatencyObserved { false };
    std::atomic<std::uint32_t> timeoutCount { 0U };

    static_cast<void>(device.addTimeoutCallback([&](const std::string& tag) {
        timeoutCount.fetch_add(1U);
        if (tag.empty()) {
            emptyTagObserved.store(true);
        }
    }));

    static_cast<void>(device.addQueryLatencyCallback(
        [&](const std::string& tag, std::chrono::microseconds durationUs, bool success) {
            if (!success) {
                if (tag.empty()) {
                    emptyTagObserved.store(true);
                }
                if (durationUs.count() < 0) {
                    invalidLatencyObserved.store(true);
                }
            }
        }));

    ASSERT_TRUE(device.start());

    // Send queries and let them time out
    for (std::uint32_t i { 0U }; i < 10U; ++i) {
        device.queryPan();
        std::this_thread::sleep_for(25ms);
    }

    device.stop();

    EXPECT_GT(timeoutCount.load(), 0U);
    EXPECT_FALSE(emptyTagObserved.load()) << "Timeout fired with empty queryTag due to TOCTOU race!";
    EXPECT_FALSE(invalidLatencyObserved.load()) << "Timeout latency was negative or invalid!";
}

} // namespace


