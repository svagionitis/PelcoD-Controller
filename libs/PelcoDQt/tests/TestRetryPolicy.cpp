/// @file TestRetryPolicy.cpp
/// @brief Unit tests for command and query retries with configurable backoff.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "PelcoDFrame.h"
#include "QPelcoDDevice.h"
#include "RetryPolicy.h"
#include "TestHelpers.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace PelcoDTest;

// Custom transport for simulating dropped responses or transient transmit failures
class FlakyTransport final : public PelcoD::BaseTransport {
public:
    bool open() override
    {
        m_open.store(true);
        return true;
    }

    void close() override
    {
        m_open.store(false);
        stopReadThread();
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sentFrames.push_back(data);

        if (m_failSendCount > 0U) {
            --m_failSendCount;
            return false;
        }
        return m_open.load();
    }

    void inject(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

    void setFailSendCount(std::uint32_t count)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failSendCount = count;
    }

    [[nodiscard]] std::size_t getSentCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sentFrames.size();
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getSentFrames() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sentFrames;
    }

private:
    std::atomic<bool> m_open { false };
    mutable std::mutex m_mutex;
    std::uint32_t m_failSendCount { 0U };
    std::vector<std::vector<std::uint8_t>> m_sentFrames;
};

void testBackoffCalculations()
{
    PelcoD::RetryConfig config;
    config.initialBackoff = std::chrono::milliseconds(50);
    config.maxBackoff = std::chrono::milliseconds(500);

    // Attempt 0 should always be 0ms
    assert(PelcoD::calculateBackoffDelay(config, 0U).count() == 0);

    // Fixed strategy
    config.strategy = PelcoD::BackoffStrategy::Fixed;
    assert(PelcoD::calculateBackoffDelay(config, 1U).count() == 50);
    assert(PelcoD::calculateBackoffDelay(config, 2U).count() == 50);
    assert(PelcoD::calculateBackoffDelay(config, 5U).count() == 50);

    // Linear strategy
    config.strategy = PelcoD::BackoffStrategy::Linear;
    assert(PelcoD::calculateBackoffDelay(config, 1U).count() == 50);
    assert(PelcoD::calculateBackoffDelay(config, 2U).count() == 100);
    assert(PelcoD::calculateBackoffDelay(config, 3U).count() == 150);
    assert(PelcoD::calculateBackoffDelay(config, 10U).count() == 500); // Clamped at maxBackoff (500)
    assert(PelcoD::calculateBackoffDelay(config, 20U).count() == 500);

    // Exponential strategy
    config.strategy = PelcoD::BackoffStrategy::Exponential;
    config.backoffMultiplier = 2.0;
    assert(PelcoD::calculateBackoffDelay(config, 1U).count() == 50); // 50 * (2^0) = 50
    assert(PelcoD::calculateBackoffDelay(config, 2U).count() == 100); // 50 * (2^1) = 100
    assert(PelcoD::calculateBackoffDelay(config, 3U).count() == 200); // 50 * (2^2) = 200
    assert(PelcoD::calculateBackoffDelay(config, 4U).count() == 400); // 50 * (2^3) = 400
    assert(PelcoD::calculateBackoffDelay(config, 5U).count() == 500); // Clamped at 500 (800 -> 500)
}

void testQuerySucceedsOnRetry()
{
    auto transport = std::make_shared<FlakyTransport>();
    PelcoD::PelcoDDevice device(transport, 1U);
    device.setQueryTimeoutMs(60U); // 60ms timeout per attempt

    PelcoD::RetryConfig config;
    config.maxRetries = 2U;
    config.initialBackoff = std::chrono::milliseconds(30);
    config.strategy = PelcoD::BackoffStrategy::Fixed;
    device.setRetryConfig(config);

    std::atomic<int> retryCount { 0 };
    std::atomic<bool> timeoutFired { false };
    std::atomic<bool> querySuccess { false };

    device.addRetryCallback(
        [&](const std::string& tag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay) {
            if (tag == "QueryPan") {
                retryCount.fetch_add(1);
                assert(attempt == 1U);
                assert(maxRetries == 2U);
                assert(delay.count() == 30);
            }
        });

    device.addTimeoutCallback([&](const std::string&) { timeoutFired.store(true); });

    device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
        if (tag == "QueryPan" && success) {
            querySuccess.store(true);
        }
    });

    std::atomic<int> txCount { 0 };
    const auto panReply = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x13U, 0x88U);
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (isTx) {
            const int count = txCount.fetch_add(1);
            if (count == 1) {
                // Attempt 1 was transmitted; inject response now
                transport->inject(panReply);
            }
        }
    });

    assert(device.start());

    // Send query
    device.queryPan();

    // Wait for attempt 0 to timeout (60ms) and retry attempt 1 to succeed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    assert(retryCount.load() == 1);
    assert(querySuccess.load());
    assert(!timeoutFired.load());
    assert(device.getStatus().panCentidegrees == 5000U);

    device.stop();
}

void testQueryExhaustionTriggersTimeout()
{
    auto transport = std::make_shared<FlakyTransport>();
    PelcoD::PelcoDDevice device(transport, 1U);
    device.setQueryTimeoutMs(50U);

    PelcoD::RetryConfig config;
    config.maxRetries = 2U;
    config.initialBackoff = std::chrono::milliseconds(20);
    config.strategy = PelcoD::BackoffStrategy::Linear;
    device.setRetryConfig(config);

    std::atomic<int> retryAttempts { 0 };
    std::atomic<bool> timeoutFired { false };
    std::atomic<bool> queryFailed { false };

    device.addRetryCallback(
        [&](const std::string& tag, [[maybe_unused]] std::uint32_t attempt, [[maybe_unused]] std::uint32_t maxRetries,
            [[maybe_unused]] std::chrono::milliseconds delay) {
            if (tag == "QueryTilt") {
                retryAttempts.fetch_add(1);
            }
        });

    device.addTimeoutCallback([&](const std::string& tag) {
        if (tag == "QueryTilt") {
            timeoutFired.store(true);
        }
    });

    device.addQueryCompletedCallback([&](const std::string& tag, bool success, const PelcoD::DeviceStatus&) {
        if (tag == "QueryTilt" && !success) {
            queryFailed.store(true);
        }
    });

    assert(device.start());

    device.queryTilt();

    // Total wait: attempt 0 (50ms) + retry 1 backoff (20ms) + attempt 1 (50ms) + retry 2 backoff (40ms) + attempt 2
    // (50ms) = ~210ms
    std::this_thread::sleep_for(std::chrono::milliseconds(320));

    assert(retryAttempts.load() == 2);
    assert(timeoutFired.load());
    assert(queryFailed.load());

    device.stop();
}

void testPriorityInversionAvoidance()
{
    auto transport = std::make_shared<FlakyTransport>();
    PelcoD::PelcoDDevice device(transport, 1U);
    device.setQueryTimeoutMs(40U);

    // Long backoff on retry (300ms)
    PelcoD::RetryConfig config;
    config.maxRetries = 1U;
    config.initialBackoff = std::chrono::milliseconds(300);
    config.strategy = PelcoD::BackoffStrategy::Fixed;
    device.setRetryConfig(config);

    std::atomic<bool> stopMotionSent { false };
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        if (isTx && frame.size() == 7U && frame[2] == 0x00U && frame[3] == 0x00U) {
            // Pelco-D stop motion command
            stopMotionSent.store(true);
        }
    });

    assert(device.start());

    // 1. Dispatch query (will fail attempt 0 at 40ms and schedule retry for +300ms)
    device.queryPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // At this point, queryPan is waiting in backoff until ~360ms.
    // 2. Dispatch urgent stopMotion now
    const auto stopStart = std::chrono::steady_clock::now();
    device.stopMotion();

    // stopMotion MUST be transmitted immediately without waiting for the 300ms backoff
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto elapsedMs
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stopStart).count();

    assert(stopMotionSent.load());
    assert(elapsedMs < 200); // Definitely didn't block on 300ms backoff

    device.stop();
}

void testTransportErrorRetry()
{
    auto transport = std::make_shared<FlakyTransport>();
    // Make first sendData fail, then succeed
    transport->setFailSendCount(1U);

    PelcoD::PelcoDDevice device(transport, 1U);
    device.setQueryTimeoutMs(100U);

    PelcoD::RetryConfig config;
    config.maxRetries = 2U;
    config.initialBackoff = std::chrono::milliseconds(25);
    config.strategy = PelcoD::BackoffStrategy::Fixed;
    config.retryOnTransportError = true;
    device.setRetryConfig(config);

    std::atomic<int> retriesFired { 0 };
    device.addRetryCallback([&](const std::string&, std::uint32_t attempt, [[maybe_unused]] std::uint32_t max,
                                [[maybe_unused]] std::chrono::milliseconds delay) {
        if (attempt == 1U) {
            retriesFired.fetch_add(1);
        }
    });

    assert(device.start());

    device.panLeft(0x20U);

    // Initial send failed, retry after 25ms should succeed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(retriesFired.load() == 1);
    assert(transport->getSentCount() >= 2U);

    device.stop();
}

void testQPelcoDDeviceRetrySignals(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    auto transport = std::make_shared<FlakyTransport>();
    PelcoDQt::QPelcoDDevice qdevice(transport, 1U);
    qdevice.setQueryTimeoutMs(50);

    qdevice.setRetryConfig(2, 25, 500, 2.0, 0); // maxRetries=2, initial=25ms, Fixed
    const auto cfg = qdevice.retryConfig();
    assert(cfg.maxRetries == 2U);
    assert(cfg.initialBackoff.count() == 25);
    assert(cfg.strategy == PelcoD::BackoffStrategy::Fixed);

    QSignalSpy retrySpy(&qdevice, &PelcoDQt::QPelcoDDevice::queryRetryAttempted);
    assert(retrySpy.isValid());

    assert(qdevice.connectDevice());

    qdevice.queryPan();

    // Allow time for timeout (50ms) + retry signal invocation
    const auto start = std::chrono::steady_clock::now();
    while (retrySpy.count() == 0
        && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            < 300) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(retrySpy.count() >= 1);
    const QList<QVariant> args = retrySpy.takeFirst();
    assert(args.at(0).toString() == "QueryPan");
    assert(args.at(1).toInt() == 1);
    assert(args.at(2).toInt() == 2);
    assert(args.at(3).toInt() == 25);

    qdevice.disconnectDevice();
}

int main(int argc, char* argv[])
{
    initTestHarness();

    std::cout << "[TestRetryPolicy] Starting backoff calculation tests...\n";
    testBackoffCalculations();

    std::cout << "[TestRetryPolicy] Starting query retry success tests...\n";
    testQuerySucceedsOnRetry();

    std::cout << "[TestRetryPolicy] Starting query retry exhaustion tests...\n";
    testQueryExhaustionTriggersTimeout();

    std::cout << "[TestRetryPolicy] Starting priority inversion avoidance tests...\n";
    testPriorityInversionAvoidance();

    std::cout << "[TestRetryPolicy] Starting transport error retry tests...\n";
    testTransportErrorRetry();

    std::cout << "[TestRetryPolicy] Starting Qt retry integration tests...\n";
    testQPelcoDDeviceRetrySignals(argc, argv);

    std::cout << "[TestRetryPolicy] All retry policy tests passed successfully!\n";
    return 0;
}
