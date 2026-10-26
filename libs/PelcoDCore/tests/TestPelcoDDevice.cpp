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
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
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

    std::promise<void> enteredPromise { };
    auto entered = enteredPromise.get_future();
    std::promise<void> releasePromise { };
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

    static_cast<void>(device.panLeft(0x20U));
    std::this_thread::sleep_for(15ms); // Allow worker thread to attempt and fail panLeft

    // Operator commands stop
    static_cast<void>(device.stopMotion());

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

    static_cast<void>(device.panLeft(0x20U));
    std::this_thread::sleep_for(15ms);

    // Operator changes direction to right
    static_cast<void>(device.panRight(0x20U));

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
    mutable std::mutex m_mutex { };
    DataReceivedCallback m_dataCb { };
    StateChangedCallback m_stateCb { };
    std::vector<std::thread> m_threads { };
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
        static_cast<void>(device.queryPan());
        static_cast<void>(device.queryTilt());
        static_cast<void>(device.queryZoom());
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(150ms);
    device.stop();

    EXPECT_GT(completedQueries.load(), 0U);
    EXPECT_FALSE(negativeDurationObserved.load())
        << "m_querySentTime was clobbered by concurrent query or read racily!";
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

    static_cast<void>(
        device.addQueryLatencyCallback([&](const std::string& tag, std::chrono::microseconds durationUs, bool success) {
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
        static_cast<void>(device.queryPan());
        std::this_thread::sleep_for(25ms);
    }

    device.stop();

    EXPECT_GT(timeoutCount.load(), 0U);
    EXPECT_FALSE(emptyTagObserved.load()) << "Timeout fired with empty queryTag due to TOCTOU race!";
    EXPECT_FALSE(invalidLatencyObserved.load()) << "Timeout latency was negative or invalid!";
}

/// @brief Telemetry dispatch must not revert connected=false on transport disconnect (review finding H2).
/// @details Verifies that when frames arrive concurrently with transport disconnection,
///          markDisconnected()'s update is never overwritten by dispatchFrame's snapshot write-back.
TEST(PelcoDDeviceConcurrency, DisconnectRxRaceSafety)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    ASSERT_TRUE(device.getStatus().connected);

    const auto frame = panResponse();

    for (std::uint32_t run { 0U }; run < 50U; ++run) {
        ASSERT_TRUE(transport->open());
        const auto dataCb = transport->lastDataCb();
        const auto stateCb = transport->lastStateCb();
        ASSERT_TRUE(dataCb);
        ASSERT_TRUE(stateCb);

        std::atomic<bool> stopWorker { false };
        std::thread rxWorker([&] {
            while (!stopWorker.load()) {
                dataCb(frame);
                std::this_thread::yield();
            }
        });

        std::this_thread::sleep_for(1ms);

        stateCb(PelcoD::TransportState::Disconnected, "link down");

        stopWorker.store(true);
        rxWorker.join();

        ASSERT_FALSE(device.getStatus().connected)
            << "Run " << run << ": dispatchFrame reverted connected to true after transport disconnect!";

        device.stop();
        ASSERT_TRUE(device.start());
    }

    device.stop();
}

/// @brief Telemetry dispatch must not revert setAddress() updates (review finding H2).
/// @details Verifies that an in-flight frame for an older address cannot overwrite m_status.address
///          after setAddress() has updated it.
TEST(PelcoDDeviceConcurrency, SetAddressRxRaceSafety)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    const auto frameAddr1 = panResponse();

    for (std::uint32_t run { 0U }; run < 50U; ++run) {
        device.setAddress(1U);
        ASSERT_EQ(device.getStatus().address, 1U);

        std::atomic<bool> stopWorker { false };
        std::thread rxWorker([&] {
            while (!stopWorker.load()) {
                dataCb(frameAddr1);
                std::this_thread::yield();
            }
        });

        std::this_thread::sleep_for(1ms);

        device.setAddress(2U);

        stopWorker.store(true);
        rxWorker.join();

        ASSERT_EQ(device.getStatus().address, 2U)
            << "Run " << run << ": dispatchFrame reverted address to 1 after setAddress(2)!";
    }

    device.stop();
}

/// @brief Multi-threaded stress test ensuring zero lost updates across concurrent status mutations (H2).
/// @details Concurrently injects telemetry frames while modifying address and polling status,
///          verifying invariants and lack of TOCTOU clobbering.
TEST(PelcoDDeviceConcurrency, StressStatusDispatch)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    std::atomic<bool> stopStress { false };
    std::atomic<bool> raceDetected { false };

    std::thread t1([&] {
        std::uint16_t angle { 0U };
        while (!stopStress.load()) {
            const auto f = PelcoD::PelcoDFrame::createFrame(
                1U, 0x00U, 0x59U, static_cast<std::uint8_t>(angle >> 8U), static_cast<std::uint8_t>(angle & 0xFFU));
            dataCb(f);
            angle = static_cast<std::uint16_t>((angle + 10U) % 36000U);
            std::this_thread::yield();
        }
    });

    std::thread t2([&] {
        while (!stopStress.load()) {
            device.setAddress(2U);
            if (device.getAddress() == 2U && device.getStatus().address != 2U) {
                raceDetected.store(true);
            }
            device.setAddress(1U);
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(100ms);
    stopStress.store(true);

    t1.join();
    t2.join();
    device.stop();

    EXPECT_FALSE(raceDetected.load())
        << "dispatchFrame clobbered device address during concurrent setAddress operations!";
}

/// @brief Verify that transport send failure reports QueryCompleted(success=false) with default maxRetries=0 (M1).
TEST(PelcoDDeviceQueryOutcome, SendFailureReportsQueryCompletedWithZeroRetries)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::string completedTag { };
    bool completedSuccess { true };

    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            completedTag = tag;
            completedSuccess = success;
            queryCompletedPromise.set_value(success);
        }));

    // Transport will fail next send
    transport->setFailSendCount(1U);

    static_cast<void>(device.queryPan());

    const auto status = queryCompletedFuture.wait_for(500ms);
    ASSERT_EQ(status, std::future_status::ready) << "QueryCompletedCallback was never invoked on send failure!";
    EXPECT_EQ(completedTag, "QueryPan");
    EXPECT_FALSE(completedSuccess);

    device.stop();
}

/// @brief Verify that urgent pre-emption aborting an in-flight query reports QueryCompleted(success=false) (M1).
TEST(PelcoDDeviceQueryOutcome, UrgentPreemptionReportsQueryCompletedFalse)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::string completedTag { };
    bool completedSuccess { true };

    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            completedTag = tag;
            completedSuccess = success;
            queryCompletedPromise.set_value(success);
        }));

    // Send query (which sets awaitingResponse = true)
    static_cast<void>(device.queryPan());

    // Give worker time to send query and enter response wait
    std::this_thread::sleep_for(30ms);

    // Urgent command should pre-empt the query
    static_cast<void>(device.stopMotion());

    const auto status = queryCompletedFuture.wait_for(500ms);
    ASSERT_EQ(status, std::future_status::ready)
        << "QueryCompletedCallback was not invoked when query was pre-empted by Urgent command!";
    EXPECT_EQ(completedTag, "QueryPan");
    EXPECT_FALSE(completedSuccess);

    device.stop();
}

/// @brief Verify that enqueuing an Urgent command purges queued Low-priority queries and notifies failure (M1).
TEST(PelcoDDeviceQueryOutcome, UrgentEnqueuePurgesQueuedQueriesWithNotification)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::string completedTag { };
    bool completedSuccess { true };

    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            completedTag = tag;
            completedSuccess = success;
            queryCompletedPromise.set_value(success);
        }));

    static_cast<void>(device.queryPan());

    // Urgent command should purge the queued QueryPan from PacedCommandQueue
    static_cast<void>(device.stopMotion());

    const auto status = queryCompletedFuture.wait_for(500ms);
    ASSERT_EQ(status, std::future_status::ready)
        << "QueryCompletedCallback was not invoked when queued query was purged by Urgent command!";
    EXPECT_EQ(completedTag, "QueryPan");
    EXPECT_FALSE(completedSuccess);
}

/// @brief Verify that device stop() notifies failure for in-flight and queued queries (M1).
TEST(PelcoDDeviceQueryOutcome, DeviceStopNotifiesInFlightAndQueuedQueries)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::string completedTag { };
    bool completedSuccess { true };

    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            completedTag = tag;
            completedSuccess = success;
            queryCompletedPromise.set_value(success);
        }));

    // Dispatch query into response wait
    static_cast<void>(device.queryPan());
    std::this_thread::sleep_for(30ms);

    // Stop device
    device.stop();

    const auto status = queryCompletedFuture.wait_for(500ms);
    ASSERT_EQ(status, std::future_status::ready) << "QueryCompletedCallback was not invoked when query was stopped!";
    EXPECT_EQ(completedTag, "QueryPan");
    EXPECT_FALSE(completedSuccess);
}

/// @brief Verify that executeAsyncQuery rejects promptly on transport send failure (M1).
TEST(PelcoDDeviceQueryOutcome, AsyncQueryRejectsPromptlyOnSendFailure)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());

    // Configure transport to fail send
    transport->setFailSendCount(1U);

    const auto startTime = std::chrono::steady_clock::now();
    // 3000ms timeout configured, but failure should be prompt (< 200ms)
    auto future = device.queryPanAsync(3000ms);

    const auto status = future.wait_for(500ms);
    ASSERT_EQ(status, std::future_status::ready) << "Async query future did not resolve promptly on send failure!";

    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    EXPECT_LT(elapsed, 200) << "Async query waited for timeout instead of failing immediately!";

    EXPECT_THROW(future.get(), std::runtime_error);

    device.stop();
}

/// @brief Unsolicited alarm frames must not be dropped while a query is awaiting response (M2).
TEST(PelcoDDeviceUnsolicited, AlarmProcessedWhileQueryPending)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    std::promise<PelcoD::DeviceStatus> statusPromise;
    auto statusFuture = statusPromise.get_future();
    std::atomic<bool> statusReceived { false };

    static_cast<void>(device.addStatusCallback([&](const PelcoD::DeviceStatus& s) {
        if (s.alarms != 0U && !statusReceived.exchange(true)) {
            statusPromise.set_value(s);
        }
    }));

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::atomic<bool> queryCompletedReceived { false };
    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            if (tag == "QueryPan" && !queryCompletedReceived.exchange(true)) {
                queryCompletedPromise.set_value(success);
            }
        }));

    // Trigger QueryPan; worker loop sends query and enters response wait (m_awaitingResponse = true)
    static_cast<void>(device.queryPan());
    std::this_thread::sleep_for(30ms);

    // Deliver unsolicited 4-byte general response carrying alarm (addr=1, alarms=0x03, cksm=0x04)
    const std::vector<std::uint8_t> alarmFrame { 0xFFU, 0x01U, 0x03U, 0x04U };
    dataCb(alarmFrame);

    // Alarm frame should update status and trigger callback even though QueryPan is pending
    const auto statusRes = statusFuture.wait_for(300ms);
    EXPECT_EQ(statusRes, std::future_status::ready) << "Unsolicited alarm was dropped while query was pending (M2)!";
    if (statusRes == std::future_status::ready) {
        EXPECT_EQ(statusFuture.get().alarms, 0x03U);
    }

    // Now deliver the matching pan response
    dataCb(panResponse());

    const auto queryRes = queryCompletedFuture.wait_for(300ms);
    EXPECT_EQ(queryRes, std::future_status::ready);
    if (queryRes == std::future_status::ready) {
        EXPECT_TRUE(queryCompletedFuture.get());
    }

    device.stop();
}

/// @brief Unsolicited command ACK responses must not be dropped while a query is pending (M2).
TEST(PelcoDDeviceUnsolicited, CommandAckProcessedWhileQueryPending)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    std::promise<PelcoD::DeviceStatus> ackPromise;
    auto ackFuture = ackPromise.get_future();
    std::atomic<bool> ackReceived { false };

    static_cast<void>(device.addStatusCallback([&](const PelcoD::DeviceStatus& s) {
        if (s.lastAckOk && !ackReceived.exchange(true)) {
            ackPromise.set_value(s);
        }
    }));

    std::promise<bool> queryCompletedPromise;
    auto queryCompletedFuture = queryCompletedPromise.get_future();
    std::atomic<bool> queryCompletedReceived { false };
    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            if (tag == "QueryPan" && !queryCompletedReceived.exchange(true)) {
                queryCompletedPromise.set_value(success);
            }
        }));

    static_cast<void>(device.queryPan());
    std::this_thread::sleep_for(30ms);

    // Standard Extended ACK (cmd2=0x01 StandardExtended, data1=0x04 echo opcode, data2=0x01 ACK)
    const auto ackFrame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x01U, 0x04U, 0x01U);
    dataCb(ackFrame);

    const auto ackRes = ackFuture.wait_for(300ms);
    EXPECT_EQ(ackRes, std::future_status::ready) << "Unsolicited ACK frame was dropped while query was pending (M2)!";
    if (ackRes == std::future_status::ready) {
        const auto status = ackFuture.get();
        EXPECT_TRUE(status.lastAckOk);
        EXPECT_EQ(status.lastAckOpcode, 0x04U);
    }

    dataCb(panResponse());

    const auto queryRes = queryCompletedFuture.wait_for(300ms);
    EXPECT_EQ(queryRes, std::future_status::ready);
    if (queryRes == std::future_status::ready) {
        EXPECT_TRUE(queryCompletedFuture.get());
    }

    device.stop();
}

/// @brief A user status callback throwing an exception must not crash or prevent other callbacks (M3).
TEST(PelcoDDeviceExceptionSafety, ThrowingStatusCallbackDoesNotCrash)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::promise<PelcoD::DeviceStatus> secondCbPromise;
    auto secondCbFuture = secondCbPromise.get_future();
    std::atomic<bool> secondCbFired { false };

    // Register throwing callback first
    static_cast<void>(device.addStatusCallback(
        [](const PelcoD::DeviceStatus&) { throw std::runtime_error("Simulated user StatusCallback failure"); }));

    // Register second callback to verify multicast continues
    static_cast<void>(device.addStatusCallback([&](const PelcoD::DeviceStatus& s) {
        if (!secondCbFired.exchange(true)) {
            secondCbPromise.set_value(s);
        }
    }));

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    // Feed a pan response to trigger status callbacks
    dataCb(panResponse());

    const auto res = secondCbFuture.wait_for(300ms);
    EXPECT_EQ(res, std::future_status::ready)
        << "Second status callback was not invoked after first callback threw (M3)!";

    device.stop();
}

/// @brief A user traffic callback throwing an exception must not crash worker or RX threads (M3).
TEST(PelcoDDeviceExceptionSafety, ThrowingTrafficCallbackDoesNotCrash)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::promise<bool> secondRxTrafficPromise;
    auto secondRxTrafficFuture = secondRxTrafficPromise.get_future();
    std::atomic<bool> secondRxFired { false };

    // Register throwing RX traffic callback
    static_cast<void>(device.addTrafficCallback(
        [](bool, const std::vector<std::uint8_t>&) {
            throw std::runtime_error("Simulated user TrafficCallback failure");
        },
        false, true));

    // Register second callback
    static_cast<void>(device.addTrafficCallback(
        [&](bool isTx, const std::vector<std::uint8_t>&) {
            if (!isTx && !secondRxFired.exchange(true)) {
                secondRxTrafficPromise.set_value(true);
            }
        },
        false, true));

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    dataCb(panResponse());

    const auto res = secondRxTrafficFuture.wait_for(300ms);
    EXPECT_EQ(res, std::future_status::ready)
        << "Second traffic callback was not invoked after first callback threw (M3)!";

    device.stop();
}

/// @brief A user query completed or latency callback throwing an exception must not crash (M3).
TEST(PelcoDDeviceExceptionSafety, ThrowingQueryCallbackDoesNotCrash)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::promise<bool> secondQueryCompletedPromise;
    auto secondQueryCompletedFuture = secondQueryCompletedPromise.get_future();
    std::atomic<bool> secondQueryFired { false };

    // Register throwing query callbacks
    static_cast<void>(device.addQueryCompletedCallback([](const std::string&, bool, const PelcoD::DeviceStatus&) {
        throw std::runtime_error("Simulated user QueryCompletedCallback failure");
    }));

    static_cast<void>(device.addQueryLatencyCallback([](const std::string&, std::chrono::microseconds, bool) {
        throw std::runtime_error("Simulated user QueryLatencyCallback failure");
    }));

    // Register second callback
    static_cast<void>(
        device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
            if (tag == "QueryPan" && !secondQueryFired.exchange(true)) {
                secondQueryCompletedPromise.set_value(success);
            }
        }));

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    static_cast<void>(device.queryPan());
    std::this_thread::sleep_for(30ms);

    dataCb(panResponse());

    const auto res = secondQueryCompletedFuture.wait_for(300ms);
    EXPECT_EQ(res, std::future_status::ready)
        << "Second query completed callback was not invoked after first callback threw (M3)!";
    if (res == std::future_status::ready) {
        EXPECT_TRUE(secondQueryCompletedFuture.get());
    }

    device.stop();
}

/// @brief Verify that zero or negative timeout does not leak callbacks or hang indefinitely (M4).
TEST(PelcoDDeviceAsync, ZeroOrNegativeTimeoutDoesNotLeak)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    device.setQueryTimeoutMs(50U); // 50ms fallback

    ASSERT_TRUE(device.start());
    const auto dataCb = transport->lastDataCb();
    ASSERT_TRUE(dataCb);

    // Test with 0ms (should adopt 50ms fallback)
    auto futureZero = device.queryPanAsync(0ms);
    std::this_thread::sleep_for(15ms);
    dataCb(panResponse());

    const auto resZero = futureZero.wait_for(200ms);
    ASSERT_EQ(resZero, std::future_status::ready);
    EXPECT_GT(futureZero.get(), 0U);

    // Test with negative timeout (should also adopt 50ms fallback and timeout cleanly without response)
    auto futureNeg = device.queryPanAsync(-10ms);
    const auto resNeg = futureNeg.wait_for(200ms);
    ASSERT_EQ(resNeg, std::future_status::ready);
    EXPECT_THROW(futureNeg.get(), std::runtime_error);

    device.stop();
}

/// @brief Verify that queryStatusAsync() queries and populates Pan, Tilt, and Zoom (M4).
TEST(PelcoDDeviceAsync, FullStatusAsyncQueriesPanTiltZoom)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(static_cast<std::uint8_t>(1U));
    // Pre-populate mock state with non-zero pan, tilt, zoom
    PelcoD::MockDeviceState state;
    state.panCentidegrees = 12000U;
    state.tiltCentidegrees = 4500U;
    state.zoomPosition = 350U;
    mock->setInternalState(state);

    PelcoD::PelcoDDevice device { mock, 1U };
    ASSERT_TRUE(device.start());

    auto statusFuture = device.queryStatusAsync(1000ms);
    const auto res = statusFuture.wait_for(500ms);
    ASSERT_EQ(res, std::future_status::ready) << "queryStatusAsync() future did not resolve with full status (M4)!";

    const auto status = statusFuture.get();
    EXPECT_TRUE(status.connected);
    EXPECT_EQ(status.panCentidegrees, 12000U);
    EXPECT_EQ(status.tiltCentidegrees, 4500U);
    EXPECT_EQ(status.zoomPosition, 350U);

    device.stop();
}

/// @brief Verify that queryStatusAsync() rejects promptly if a sub-query fails (M4).
TEST(PelcoDDeviceAsync, FullStatusAsyncRejectsPromptlyOnSendFailure)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    ASSERT_TRUE(device.start());
    transport->setFailSendCount(1U); // Fail the first send (QueryPan)

    const auto startTime = std::chrono::steady_clock::now();
    auto statusFuture = device.queryStatusAsync(3000ms);

    const auto res = statusFuture.wait_for(500ms);
    ASSERT_EQ(res, std::future_status::ready)
        << "queryStatusAsync() future did not reject promptly on send failure (M4)!";

    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    EXPECT_LT(elapsed, 200) << "queryStatusAsync waited for overall timeout instead of failing promptly!";

    EXPECT_THROW(statusFuture.get(), std::runtime_error);

    device.stop();
}

/// @brief Verify that optical and camera mode setters update DeviceStatus immediately (M6).
TEST(PelcoDDeviceStateModel, OpticalSettersUpdateDeviceStatus)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    static_cast<void>(device.setAutoFocus(PelcoD::AutoMode::Auto));
    EXPECT_EQ(device.getStatus().autoFocus, PelcoD::AutoMode::Auto);

    static_cast<void>(device.setAutoIris(PelcoD::AutoMode::On));
    EXPECT_EQ(device.getStatus().autoIris, PelcoD::AutoMode::On);

    static_cast<void>(device.setAgc(PelcoD::AutoMode::Auto));
    EXPECT_EQ(device.getStatus().agc, PelcoD::AutoMode::Auto);

    static_cast<void>(device.setBacklightComp(PelcoD::SwitchState::On));
    EXPECT_EQ(device.getStatus().backlightComp, PelcoD::SwitchState::On);

    static_cast<void>(device.setAutoWhiteBalance(PelcoD::SwitchState::On));
    EXPECT_EQ(device.getStatus().autoWhiteBalance, PelcoD::SwitchState::On);

    device.stop();
}

/// @brief Verify that zoom and focus speeds saturate to 0x03 and update DeviceStatus (M6).
TEST(PelcoDDeviceStateModel, SpeedSettersSaturateAndStore)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    static_cast<void>(device.setZoomSpeed(2U));
    EXPECT_EQ(device.getStatus().zoomSpeed, 2U);

    static_cast<void>(device.setZoomSpeed(0xFFU)); // Must saturate to 0x03
    EXPECT_EQ(device.getStatus().zoomSpeed, 3U);

    static_cast<void>(device.setFocusSpeed(1U));
    EXPECT_EQ(device.getStatus().focusSpeed, 1U);

    static_cast<void>(device.setFocusSpeed(10U)); // Must saturate to 0x03
    EXPECT_EQ(device.getStatus().focusSpeed, 3U);

    device.stop();
}

/// @brief Verify that activePreset is tracked on goToPreset and cleared on clearPreset/manual motion (M6).
TEST(PelcoDDeviceStateModel, PresetLifecycleTracking)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    EXPECT_EQ(device.getStatus().activePreset, 0U);

    static_cast<void>(device.goToPreset(5U));
    EXPECT_EQ(device.getStatus().activePreset, 5U);

    static_cast<void>(device.goToPreset(12U, 0x20U));
    EXPECT_EQ(device.getStatus().activePreset, 12U);

    // clearPreset for different preset does not reset activePreset
    static_cast<void>(device.clearPreset(3U));
    EXPECT_EQ(device.getStatus().activePreset, 12U);

    // clearPreset for activePreset resets it to 0
    static_cast<void>(device.clearPreset(12U));
    EXPECT_EQ(device.getStatus().activePreset, 0U);

    // Manual motion resets activePreset to 0
    static_cast<void>(device.goToPreset(8U));
    EXPECT_EQ(device.getStatus().activePreset, 8U);
    static_cast<void>(device.panLeft(0x20U));
    EXPECT_EQ(device.getStatus().activePreset, 0U);

    device.stop();
}

/// @brief Verify that setting pan limits updates DeviceStatus centidegree fields (M6).
TEST(PelcoDDeviceStateModel, LimitSettersSynchronizeStatus)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    EXPECT_EQ(device.setManualLeftPanLimit(12500U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.getStatus().manualLeftLimitCentidegrees, 12500U);

    EXPECT_EQ(device.setManualRightPanLimit(24500U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.getStatus().manualRightLimitCentidegrees, 24500U);

    EXPECT_EQ(device.setScanLeftPanLimit(1000U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.getStatus().scanLeftLimitCentidegrees, 1000U);

    EXPECT_EQ(device.setScanRightPanLimit(35000U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.getStatus().scanRightLimitCentidegrees, 35000U);

    device.stop();
}

/// @brief Detects a callable setFocusPosition(std::uint16_t) member.
template <typename T, typename = void> struct HasFocusSetter : std::false_type { };
template <typename T>
struct HasFocusSetter<T, std::void_t<decltype(std::declval<T&>().setFocusPosition(std::uint16_t { }))>>
    : std::true_type { };

/// @brief Detects a callable setIrisPosition(std::uint16_t) member.
template <typename T, typename = void> struct HasIrisSetter : std::false_type { };
template <typename T>
struct HasIrisSetter<T, std::void_t<decltype(std::declval<T&>().setIrisPosition(std::uint16_t { }))>> : std::true_type {
};

/// @brief Detects a callable setSerialNumber(const std::string&) member.
template <typename T, typename = void> struct HasSerialSetter : std::false_type { };
template <typename T>
struct HasSerialSetter<T, std::void_t<decltype(std::declval<T&>().setSerialNumber(std::string { }))>> : std::true_type {
};

/// @brief PelcoDDevice must not expose command-style setters that transmit nothing (B3, B4).
/// @details setFocusPosition / setIrisPosition / setSerialNumber only mutated local telemetry yet
///          returned SendResult::Success, and FujinonSX800Device::setFocusPosition (a real command)
///          hid the non-virtual base method, so calls through PelcoDDevice& silently sent nothing.
TEST(PelcoDDeviceStateModel, NoLocalOnlyCommandSetters)
{
    EXPECT_FALSE(HasFocusSetter<PelcoD::PelcoDDevice>::value);
    EXPECT_FALSE(HasIrisSetter<PelcoD::PelcoDDevice>::value);
    EXPECT_FALSE(HasSerialSetter<PelcoD::PelcoDDevice>::value);
}

/// @brief Test transport that can be configured to throw on open() (M8).
class ThrowingOpenTransport final : public PelcoD::ITransport {
public:
    [[nodiscard]] bool open() override
    {
        if (m_shouldThrow) {
            throw std::runtime_error("Simulated transport open failure");
        }
        m_open.store(true);
        return true;
    }

    void close() override
    {
        m_open.store(false);
        m_closed = true;
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& /*data*/) override
    {
        return m_open.load();
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        m_hasDataCb = static_cast<bool>(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        m_hasStateCb = static_cast<bool>(callback);
    }

    bool m_shouldThrow { true };
    std::atomic<bool> m_open { false };
    bool m_closed { false };
    bool m_hasDataCb { false };
    bool m_hasStateCb { false };
};

/// @brief Transport reconnection must set status.connected to true and notify callbacks (M7).
TEST(PelcoDDeviceReconnect, TransportReconnectSetsConnectedTrueAndNotifiesCallbacks)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());
    EXPECT_TRUE(device.getStatus().connected);

    std::atomic<std::uint32_t> statusCount { 0U };
    std::atomic<bool> lastConnectedState { false };
    static_cast<void>(device.addStatusCallback([&](const PelcoD::DeviceStatus& s) {
        statusCount.fetch_add(1U);
        lastConnectedState.store(s.connected);
    }));

    const auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(stateCb);

    // Simulate link disconnection
    stateCb(PelcoD::TransportState::Disconnected, "link down");
    EXPECT_FALSE(device.getStatus().connected);
    EXPECT_EQ(statusCount.load(), 1U);
    EXPECT_FALSE(lastConnectedState.load());

    // Simulate link reconnection
    stateCb(PelcoD::TransportState::Connected, "link up");
    EXPECT_TRUE(device.getStatus().connected);
    EXPECT_EQ(statusCount.load(), 2U);
    EXPECT_TRUE(lastConnectedState.load());

    device.stop();
}

/// @brief Redundant Connected notifications must not fire duplicate status callbacks (M7).
TEST(PelcoDDeviceReconnect, RedundantConnectedEventDoesNotTriggerDuplicateStatusCallback)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    std::atomic<std::uint32_t> statusCount { 0U };
    static_cast<void>(device.addStatusCallback([&](const PelcoD::DeviceStatus&) { statusCount.fetch_add(1U); }));

    const auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(stateCb);

    // Initial state is already connected; redundant Connected event should be a no-op
    stateCb(PelcoD::TransportState::Connected, "already up");
    EXPECT_EQ(statusCount.load(), 0U);

    device.stop();
}

/// @brief Transport reconnect received while device is stopped must be ignored (M7).
TEST(PelcoDDeviceReconnect, TransportReconnectIgnoredWhenDeviceStopped)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    const auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(stateCb);

    device.stop();
    EXPECT_FALSE(device.getStatus().connected);

    // Late or unexpected transport connect while device is stopped
    stateCb(PelcoD::TransportState::Connected, "link up");
    EXPECT_FALSE(device.getStatus().connected);
    EXPECT_FALSE(device.isConnected());
}

/// @brief Transport reconnect clears partial framing remnants in the accumulator (M7).
TEST(PelcoDDeviceReconnect, TransportReconnectClearsRxAccumulator)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    const auto dataCb = transport->lastDataCb();
    const auto stateCb = transport->lastStateCb();
    ASSERT_TRUE(dataCb);
    ASSERT_TRUE(stateCb);

    // Inject partial junk data
    dataCb({ 0xFFU, 0x01U, 0x00U });

    // Disconnect and reconnect
    stateCb(PelcoD::TransportState::Disconnected, "link down");
    stateCb(PelcoD::TransportState::Connected, "link up");

    std::atomic<std::uint32_t> trafficCount { 0U };
    static_cast<void>(device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (!isTx) {
            trafficCount.fetch_add(1U);
        }
    }));

    // Inject a valid frame
    dataCb(panResponse());
    EXPECT_EQ(trafficCount.load(), 1U);

    device.stop();
}

/// @brief If transport open() throws an exception during start(), device rolls back cleanly (M8).
TEST(PelcoDDeviceStartExceptionSafety, StartTransportOpenThrowRollsBackCleanly)
{
    auto throwingTransport = std::make_shared<ThrowingOpenTransport>();
    PelcoD::PelcoDDevice device { throwingTransport, 1U };

    EXPECT_FALSE(device.start());
    EXPECT_FALSE(device.isConnected());
    EXPECT_FALSE(device.getStatus().connected);
    EXPECT_FALSE(throwingTransport->m_hasDataCb);
    EXPECT_FALSE(throwingTransport->m_hasStateCb);
}

/// @brief After a failed start(), a subsequent start() on the same device succeeds once transport recovers (M8).
TEST(PelcoDDeviceStartExceptionSafety, RestartAfterFailedStartSucceeds)
{
    auto transport = std::make_shared<ThrowingOpenTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    EXPECT_FALSE(device.start());
    EXPECT_FALSE(device.isConnected());
    EXPECT_FALSE(device.getStatus().connected);

    // Recover transport link
    transport->m_shouldThrow = false;
    EXPECT_TRUE(device.start());
    EXPECT_TRUE(device.isConnected());
    EXPECT_TRUE(device.getStatus().connected);

    device.stop();
    EXPECT_FALSE(device.isConnected());
}

/// @brief Verify M12: setPower enqueues standard Power On and Power Off frames.
TEST(PelcoDDeviceFacade, SetPowerEnqueuesCorrectFrames)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    static_cast<void>(device.setPower(true));
    static_cast<void>(device.setPower(false));

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);
    // Frame 0: Power On (Cmd1 = 0x88)
    EXPECT_EQ(frames[0].size(), 7U);
    EXPECT_EQ(frames[0][2], 0x88U);
    // Frame 1: Power Off (Cmd1 = 0x08)
    EXPECT_EQ(frames[1].size(), 7U);
    EXPECT_EQ(frames[1][2], 0x08U);
}

/// @brief Verify M12: setAutoScan enqueues standard Auto Scan and Manual Scan frames.
TEST(PelcoDDeviceFacade, SetAutoScanEnqueuesCorrectFrames)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    static_cast<void>(device.setAutoScan(true));
    static_cast<void>(device.setAutoScan(false));

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);
    // Frame 0: Auto Scan (Cmd1 = 0x90)
    EXPECT_EQ(frames[0].size(), 7U);
    EXPECT_EQ(frames[0][2], 0x90U);
    // Frame 1: Manual Scan (Cmd1 = 0x10, ScanSense::ManualScanOn)
    EXPECT_EQ(frames[1].size(), 7U);
    EXPECT_EQ(frames[1][2], 0x10U);
}

/// @brief Verify M12: combined PTZ + Zoom/Focus/Iris move command resets activePreset and enqueues full frame.
TEST(PelcoDDeviceFacade, CombinedMotionEnqueuesFullPayloadAndResetsActivePreset)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    ASSERT_TRUE(device.start());

    // Set an active preset first
    static_cast<void>(device.goToPreset(5U));
    EXPECT_EQ(device.getStatus().activePreset, 5U);

    static_cast<void>(device.move(PelcoD::PanDirection::Left, 0x20U, PelcoD::TiltDirection::Up, 0x15U,
        PelcoD::ZoomAction::Tele, PelcoD::FocusAction::Near, PelcoD::IrisAction::Open));

    EXPECT_EQ(device.getStatus().activePreset, 0U);

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    device.stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 2U);
    const auto& moveFrame = frames[1];
    ASSERT_EQ(moveFrame.size(), 7U);
    EXPECT_EQ(moveFrame[4], 0x20U); // Pan speed
    EXPECT_EQ(moveFrame[5], 0x15U); // Tilt speed
    // Cmd2 bitmask contains Pan Left (0x04) | Tilt Up (0x08) | Zoom Tele (0x20) = 0x2C
    EXPECT_EQ(moveFrame[3] & 0x2CU, 0x2CU);
    // Cmd1 bitmask contains Iris Open (0x02) | Focus Near (0x01) = 0x03
    EXPECT_EQ(moveFrame[2] & 0x03U, 0x03U);
}

/// @brief Verify that commands on stopped device return SendResult::TransportClosed (A3).
TEST(PelcoDDeviceStateModel, SendResultTransportClosedWhenStopped)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };
    // Device is not started
    EXPECT_EQ(device.panLeft(0x20U), PelcoD::SendResult::TransportClosed);
    EXPECT_EQ(device.stopMotion(), PelcoD::SendResult::TransportClosed);
    EXPECT_EQ(device.queryPan(), PelcoD::SendResult::TransportClosed);
    EXPECT_EQ(device.sendRawFrame({ 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 }), PelcoD::SendResult::TransportClosed);
}

/// @brief Verify that exhausted query retries trigger CommandDroppedCallback and record metric (A3).
TEST(PelcoDDeviceStateModel, CommandDroppedCallbackOnRetryExhaustion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    PelcoD::RetryConfig cfg;
    cfg.maxRetries = 1U;
    cfg.initialBackoff = std::chrono::milliseconds(10);
    device.setRetryConfig(cfg);
    device.setQueryTimeoutMs(20U);

    std::atomic<bool> droppedInvoked { false };
    PelcoD::SendResult droppedReason { PelcoD::SendResult::Success };
    device.addCommandDroppedCallback([&](const std::vector<std::uint8_t>&, PelcoD::SendResult reason) {
        droppedReason = reason;
        droppedInvoked = true;
    });

    ASSERT_TRUE(device.start());
    EXPECT_EQ(device.queryPan(), PelcoD::SendResult::Success);

    // Wait for timeout + retry + timeout
    const auto start = std::chrono::steady_clock::now();
    while (!droppedInvoked && (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(droppedInvoked);
    EXPECT_EQ(droppedReason, PelcoD::SendResult::RetriesExhausted);
    EXPECT_GE(device.getProtocolStats().commandRetriesExhausted, 1U);
    EXPECT_GE(device.getProtocolStats().commandsDropped, 1U);

    device.stop();
}

/// @brief Queue-capacity evictions must be counted as overflows, not retry exhaustion (B2).
/// @details The command-dropped handler previously incremented commandRetriesExhausted for every
///          drop, so a pure QueueFull eviction was misreported as an exhausted retry.
TEST(PelcoDDeviceStateModel, QueueOverflowNotCountedAsRetryExhaustion)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    PelcoD::PelcoDDevice device { transport, 1U };

    std::atomic<std::uint32_t> overflowDrops { 0U };
    auto conn = device.addCommandDroppedCallback(
        [&overflowDrops](const std::vector<std::uint8_t>&, PelcoD::SendResult reason) {
            if (reason == PelcoD::SendResult::QueueFull) {
                overflowDrops.fetch_add(1U);
            }
        });

    ASSERT_TRUE(device.start());

    // The worker paces at ~20 ms per frame, so flooding beyond capacity forces evictions.
    constexpr std::size_t floodCount { PelcoD::PacedCommandQueue::DefaultMaxCapacity + 32U };
    for (std::size_t i { 0U }; i < floodCount; ++i) {
        static_cast<void>(device.setPreset(1U));
    }

    const auto stats = device.getProtocolStats();
    device.stop();

    EXPECT_GE(overflowDrops.load(), 1U);
    EXPECT_GE(stats.queueOverflows, 1U);
    EXPECT_EQ(stats.commandRetriesExhausted, 0U);
    EXPECT_GE(stats.commandsDropped, 1U);
}

/// @brief Verify A4: PelcoDDevice::createVirtualChannel transparently multiplexes underlying transport.
TEST(PelcoDDeviceFacade, DeviceProvidesMultiplexedChannel)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::PelcoDDevice device { mock, 1U };

    auto auxChan = device.createVirtualChannel();
    ASSERT_NE(auxChan, nullptr);

    ASSERT_TRUE(device.start());
    EXPECT_TRUE(device.isConnected());

    std::atomic<int> auxRxCount { 0 };
    std::vector<std::uint8_t> auxRxData;
    auxChan->setDataCallback([&](const std::vector<std::uint8_t>& data) {
        auxRxData = data;
        auxRxCount.fetch_add(1);
    });

    std::atomic<int> deviceRxCount { 0 };
    auto conn = device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (!isTx) {
            deviceRxCount.fetch_add(1);
        }
    });

    // Send command via device
    EXPECT_EQ(device.panLeft(0x20), PelcoD::SendResult::Success);

    // Send probe via auxChan
    const std::vector<std::uint8_t> probeFrame = { 0xFF, 0x02, 0x00, 0x51, 0x00, 0x00, 0x53 };
    EXPECT_TRUE(auxChan->sendData(probeFrame));

    // Simulate incoming frame on underlying transport
    const std::vector<std::uint8_t> responseFrame = { 0xFF, 0x01, 0x00, 0x59, 0x00, 0x00, 0x5A };
    mock->injectRxData(responseFrame);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    while ((auxRxCount.load() < 1 || deviceRxCount.load() < 1) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_GE(auxRxCount.load(), 1);
    EXPECT_GE(deviceRxCount.load(), 1);
    EXPECT_EQ(auxRxData, responseFrame);

    // Close auxChan: device remains operational
    auxChan->close();
    EXPECT_FALSE(auxChan->isOpen());
    EXPECT_TRUE(device.isConnected());
    const int auxCountAtClose = auxRxCount.load();

    // Inject another frame: only device receives it
    const int deviceCountBeforeInject = deviceRxCount.load();
    mock->injectRxData(responseFrame);
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (deviceRxCount.load() <= deviceCountBeforeInject && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_GT(deviceRxCount.load(), deviceCountBeforeInject);
    EXPECT_EQ(auxRxCount.load(), auxCountAtClose); // auxChan received nothing after close

    device.stop();
}

} // namespace
