/// @file TestBusScanner.cpp
/// @brief Unit tests for PelcoD::BusScanner range scanning, device discovery, and lifecycle controls.

#include "BusScanner.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include <BaseTransport.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

/// @brief Verify validation of invalid address ranges in scan configuration.
/// @details Ensures startScan returns false when start > end, start == 0, or end == 255.
TEST(BusScannerTest, RangeValidation)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    EXPECT_FALSE(scanner.isScanning());
    EXPECT_FALSE(scanner.isPaused());

    // Invalid: start > end
    PelcoD::ScanConfig cfg1;
    cfg1.startAddress = 10U;
    cfg1.endAddress = 5U;
    EXPECT_FALSE(scanner.startScan(cfg1));
    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);

    // Invalid: start == 0
    PelcoD::ScanConfig cfg2;
    cfg2.startAddress = 0U;
    cfg2.endAddress = 10U;
    EXPECT_FALSE(scanner.startScan(cfg2));

    // Invalid: end == 255 (valid range is 1-254)
    PelcoD::ScanConfig cfg3;
    cfg3.startAddress = 1U;
    cfg3.endAddress = 255U;
    EXPECT_FALSE(scanner.startScan(cfg3));
}

/// @brief Verify discovery of a mock device on an active address.
/// @details Probes addresses 1 through 4 with mock responding at address 3,
///          verifying that the device is detected, telemetry populated, and callbacks invoked.
TEST(BusScannerTest, SingleDeviceDiscovery)
{
    const std::uint8_t targetAddr = 3U;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(targetAddr);

    // Pre-set pan angle on mock device to 90.00 degrees (9000 centidegrees)
    PelcoD::MockDeviceState state = mock->getInternalState();
    state.panCentidegrees = 9000U;
    mock->setInternalState(state);

    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 4U;
    cfg.timeoutMs = 40U;
    cfg.interCommandDelayMs = 5U;

    std::atomic<bool> deviceFound { false };
    std::atomic<int> discoveredAddr { 0 };
    std::atomic<bool> hasPan { false };
    std::atomic<int> panAngle { 0 };

    scanner.setDeviceDiscoveredCallback([&](const PelcoD::DiscoveredDevice& dev) {
        deviceFound.store(true);
        discoveredAddr.store(dev.address);
        hasPan.store(dev.hasPanPosition);
        panAngle.store(dev.panCentidegrees);
    });

    std::atomic<bool> finished { false };
    std::atomic<std::size_t> totalFoundCount { 0 };
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>& devs) {
        finished.store(true);
        totalFoundCount.store(devs.size());
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    EXPECT_TRUE(scanner.isScanning());

    // Wait for scan to complete (max 2 seconds)
    const auto startTime = std::chrono::steady_clock::now();
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(2)) {
            FAIL() << "Scan timed out waiting for completion";
        }
    }

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    EXPECT_TRUE(finished.load());
    EXPECT_EQ(totalFoundCount.load(), 1U);

    const auto foundList = scanner.getDiscoveredDevices();
    ASSERT_EQ(foundList.size(), 1U);
    EXPECT_EQ(foundList[0].address, targetAddr);
    EXPECT_TRUE(foundList[0].hasPanPosition);
    EXPECT_EQ(foundList[0].panCentidegrees, 9000);

    EXPECT_TRUE(deviceFound.load());
    EXPECT_EQ(discoveredAddr.load(), targetAddr);
    EXPECT_TRUE(hasPan.load());
    EXPECT_EQ(panAngle.load(), 9000);
}

/// @brief Verify progress callbacks are reported for each address.
/// @details Checks scanProgressCallback firing with correct current address and scanned count for all targets.
TEST(BusScannerTest, ProgressCallbacks)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 3U;
    cfg.timeoutMs = 30U;
    cfg.interCommandDelayMs = 5U;

    std::mutex cbMutex;
    std::vector<std::uint8_t> reportedAddresses;
    std::vector<std::size_t> reportedCounts;
    std::atomic<std::size_t> callbackCount { 0 };

    scanner.setScanProgressCallback([&](std::uint8_t current, std::size_t scanned, std::size_t /*total*/) {
        std::scoped_lock lock(cbMutex);
        reportedAddresses.push_back(current);
        reportedCounts.push_back(scanned);
        callbackCount.fetch_add(1);
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning() || callbackCount.load() < 3U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::scoped_lock lock(cbMutex);
    ASSERT_EQ(reportedAddresses.size(), 3U);
    EXPECT_EQ(reportedAddresses[0], 1U);
    EXPECT_EQ(reportedAddresses[1], 2U);
    EXPECT_EQ(reportedAddresses[2], 3U);

    ASSERT_EQ(reportedCounts.size(), 3U);
    EXPECT_EQ(reportedCounts[0], 1U);
    EXPECT_EQ(reportedCounts[1], 2U);
    EXPECT_EQ(reportedCounts[2], 3U);
}

/// @brief Verify stopScan aborts an active scan immediately.
/// @details Initiates scan over 20 addresses, calls stopScan after first response, and verifies prompt return to Idle.
TEST(BusScannerTest, StopScan)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(200U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 20U;
    cfg.timeoutMs = 150U;
    cfg.interCommandDelayMs = 10U;

    std::atomic<std::size_t> scannedCount { 0 };
    scanner.setScanProgressCallback(
        [&](std::uint8_t /*curr*/, std::size_t scanned, std::size_t /*tot*/) { scannedCount.store(scanned); });

    ASSERT_TRUE(scanner.startScan(cfg));
    // Wait until at least 1 address is scanned
    while (scannedCount.load() < 1U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    scanner.stopScan();

    // Scanner should return to Idle quickly
    const auto stopStart = std::chrono::steady_clock::now();
    while (scanner.getState() != PelcoD::ScanState::Idle) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - stopStart > std::chrono::milliseconds(500)) {
            FAIL() << "stopScan did not terminate in expected timeframe";
        }
    }

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    // Should have stopped well before reaching address 20
    EXPECT_LT(scannedCount.load(), 20U);
}

/// @brief Verify pause and resume operations.
/// @details Checks scan pausing at an intermediate state and resuming successfully to completion.
TEST(BusScannerTest, PauseResume)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 5U;
    cfg.timeoutMs = 50U;
    cfg.interCommandDelayMs = 5U;

    ASSERT_TRUE(scanner.startScan(cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    scanner.pauseScan();
    EXPECT_TRUE(scanner.isPaused());
    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Paused);

    // Sleep while paused
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_TRUE(scanner.isPaused());

    // Resume
    scanner.resumeScan();
    EXPECT_FALSE(scanner.isPaused());

    while (scanner.isScanning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
}

/// @brief Verify stopping an active scan while it is paused immediately wakes the condition variable.
/// @details Calls stopScan while scan is paused, verifying prompt unblocking and worker join.
TEST(BusScannerTest, StopWhilePaused)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 50U;
    cfg.timeoutMs = 100U;
    cfg.interCommandDelayMs = 10U;

    ASSERT_TRUE(scanner.startScan(cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    scanner.pauseScan();
    EXPECT_TRUE(scanner.isPaused());
    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Paused);

    // Call stopScan while paused - must immediately notify m_pauseCv and join worker
    const auto stopStart = std::chrono::steady_clock::now();
    scanner.stopScan();
    const auto stopDuration
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stopStart);

    EXPECT_FALSE(scanner.isScanning());
    EXPECT_FALSE(scanner.isPaused());
    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    EXPECT_LT(stopDuration, std::chrono::milliseconds(100)) << "stopScan while paused took too long!";
}

/// @brief Verify multi-baud auto-discovery identifies device running at a non-default baud rate.
/// @details Tests scanning across multiple baud rates to locate a device communicating at 19200 baud.
TEST(BusScannerTest, MultiBaudDiscovery)
{
    const std::uint8_t targetAddr = 7U;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(targetAddr);

    // Configure mock device to only respond at 19200 baud
    PelcoD::MockDeviceState state = mock->getInternalState();
    state.baudRate = 19200U;
    state.filterByBaudRate = true;
    state.panCentidegrees = 4500U;
    mock->setInternalState(state);
    mock->setBaudRate(9600U); // pre-scan baud rate is 9600

    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 5U;
    cfg.endAddress = 8U;
    cfg.timeoutMs = 25U;
    cfg.interCommandDelayMs = 2U;
    cfg.baudSwitchDelayMs = 2U;
    cfg.baudRates = { 2400U, 4800U, 9600U, 19200U, 38400U };

    std::atomic<bool> deviceFound { false };
    std::atomic<int> discoveredAddr { 0 };
    std::atomic<std::uint32_t> discoveredBaud { 0U };
    std::atomic<int> panAngle { 0 };

    scanner.setDeviceDiscoveredCallback([&](const PelcoD::DiscoveredDevice& dev) {
        deviceFound.store(true);
        discoveredAddr.store(dev.address);
        discoveredBaud.store(dev.baudRate);
        panAngle.store(dev.panCentidegrees);
    });

    std::vector<std::uint32_t> baudsVisited;
    std::mutex baudMutex;
    scanner.setBaudRateChangedCallback([&](std::uint32_t baud) {
        std::scoped_lock lock(baudMutex);
        baudsVisited.push_back(baud);
    });

    std::atomic<bool> finished { false };
    std::atomic<std::size_t> totalFoundCount { 0 };
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>& devs) {
        finished.store(true);
        totalFoundCount.store(devs.size());
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    EXPECT_TRUE(scanner.isScanning());

    const auto startTime = std::chrono::steady_clock::now();
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(5)) {
            FAIL() << "Multi-baud scan timed out";
        }
    }

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    EXPECT_TRUE(finished.load());
    EXPECT_EQ(totalFoundCount.load(), 1U);
    EXPECT_TRUE(deviceFound.load());
    EXPECT_EQ(discoveredAddr.load(), targetAddr);
    EXPECT_EQ(discoveredBaud.load(), 19200U);
    EXPECT_EQ(panAngle.load(), 4500);

    const auto foundList = scanner.getDiscoveredDevices();
    ASSERT_EQ(foundList.size(), 1U);
    EXPECT_EQ(foundList[0].address, targetAddr);
    EXPECT_EQ(foundList[0].baudRate, 19200U);
    EXPECT_TRUE(foundList[0].hasPanPosition);

    // Verify all 5 baud rates were tested
    {
        std::scoped_lock lock(baudMutex);
        ASSERT_EQ(baudsVisited.size(), 5U);
        EXPECT_EQ(baudsVisited[0], 2400U);
        EXPECT_EQ(baudsVisited[1], 4800U);
        EXPECT_EQ(baudsVisited[2], 9600U);
        EXPECT_EQ(baudsVisited[3], 19200U);
        EXPECT_EQ(baudsVisited[4], 38400U);
    }

    // Pre-scan baud rate (9600U) must be restored after scan finishes
    EXPECT_EQ(mock->getBaudRate(), 9600U);
}

/// @brief Verify multi-baud progress reports correct total count and baud rates.
/// @details Checks callback progression through configured baud list.
TEST(BusScannerTest, MultiBaudProgressAndCount)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 2U; // 2 addresses
    cfg.timeoutMs = 15U;
    cfg.interCommandDelayMs = 2U;
    cfg.baudSwitchDelayMs = 2U;
    cfg.baudRates = { 2400U, 4800U, 9600U }; // 3 bauds -> total 6 probes

    std::mutex cbMutex;
    std::vector<std::size_t> reportedCounts;
    std::vector<std::size_t> reportedTotals;
    std::vector<std::uint32_t> reportedBauds;

    scanner.setMultiBaudProgressCallback(
        [&](std::uint32_t baud, std::uint8_t /*current*/, std::size_t scanned, std::size_t total) {
            std::scoped_lock lock(cbMutex);
            reportedBauds.push_back(baud);
            reportedCounts.push_back(scanned);
            reportedTotals.push_back(total);
        });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::scoped_lock lock(cbMutex);
    ASSERT_EQ(reportedCounts.size(), 6U);
    ASSERT_EQ(reportedTotals.size(), 6U);
    EXPECT_EQ(reportedTotals[0], 6U);
    EXPECT_EQ(reportedCounts.back(), 6U);

    // 2 probes at 2400, 2 at 4800, 2 at 9600
    ASSERT_EQ(reportedBauds.size(), 6U);
    EXPECT_EQ(reportedBauds[0], 2400U);
    EXPECT_EQ(reportedBauds[1], 2400U);
    EXPECT_EQ(reportedBauds[2], 4800U);
    EXPECT_EQ(reportedBauds[3], 4800U);
    EXPECT_EQ(reportedBauds[4], 9600U);
    EXPECT_EQ(reportedBauds[5], 9600U);
}

/// @brief Verify stopping an active multi-baud scan terminates immediately and restores baud rate.
/// @details Verifies early termination cleanly restores pre-scan baud rate.
TEST(BusScannerTest, MultiBaudStopScan)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    mock->setBaudRate(9600U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 50U;
    cfg.timeoutMs = 100U;
    cfg.interCommandDelayMs = 10U;
    cfg.baudRates = { 2400U, 4800U, 9600U, 19200U, 38400U };

    std::atomic<std::size_t> scannedCount { 0 };
    scanner.setScanProgressCallback(
        [&](std::uint8_t /*curr*/, std::size_t scanned, std::size_t /*tot*/) { scannedCount.store(scanned); });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scannedCount.load() < 2U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    scanner.stopScan();

    const auto stopStart = std::chrono::steady_clock::now();
    while (scanner.getState() != PelcoD::ScanState::Idle) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - stopStart > std::chrono::milliseconds(500)) {
            FAIL() << "stopScan did not terminate in expected timeframe";
        }
    }

    EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
    EXPECT_LT(scannedCount.load(), 250U);
    // Original baud rate (9600) must be restored
    EXPECT_EQ(mock->getBaudRate(), 9600U);
}

/// @brief Verify scanner behavior when transport returns no responses.
/// @details Probes an address range where no device is present, verifying scanner completes
///          with 0 discovered devices.
TEST(BusScannerTest, ScanTimeoutBehavior)
{
    // Mock device is configured on address 50
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(50U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 3U; // None of these match 50
    cfg.timeoutMs = 15U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> finished { false };
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>&) { finished.store(true); });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(scanner.getDiscoveredDevices().empty());
}

/// @class EchoingTransport
/// @brief Test transport simulating RS-485 transceiver local hardware echo and mock responses.
class EchoingTransport final : public Transport::BaseTransport {
public:
    EchoingTransport() = default;
    ~EchoingTransport() override = default;

    EchoingTransport(const EchoingTransport&) = delete;
    EchoingTransport& operator=(const EchoingTransport&) = delete;
    EchoingTransport(EchoingTransport&&) = delete;
    EchoingTransport& operator=(EchoingTransport&&) = delete;

    bool open() override
    {
        m_isOpen.store(true);
        return true;
    }

    void close() override
    {
        m_isOpen.store(false);
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_isOpen.load();
    }

    bool sendData(const std::vector<std::uint8_t>& data) override
    {
        if (!m_isOpen.load()) {
            return false;
        }
        {
            std::scoped_lock lock(m_txMutex);
            m_sentPackets.push_back(data);
        }
        if (m_echoTx.load()) {
            // Simulate RS-485 transceiver local hardware echo
            invokeDataCallback(data);
        }
        std::function<std::vector<std::vector<std::uint8_t>>(const std::vector<std::uint8_t>&)> resp;
        {
            std::scoped_lock lock(m_txMutex);
            resp = m_responder;
        }
        if (resp) {
            const auto replies = resp(data);
            for (const auto& reply : replies) {
                invokeDataCallback(reply);
            }
        }
        return true;
    }

    void setEcho(bool enabled) noexcept
    {
        m_echoTx.store(enabled);
    }

    void setResponder(std::function<std::vector<std::vector<std::uint8_t>>(const std::vector<std::uint8_t>&)> responder)
    {
        std::scoped_lock lock(m_txMutex);
        m_responder = std::move(responder);
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getSentPackets() const
    {
        std::scoped_lock lock(m_txMutex);
        return m_sentPackets;
    }

    [[nodiscard]] bool hasDataCallback() const
    {
        std::scoped_lock lock(m_callbackMutex);
        return static_cast<bool>(m_dataCallback);
    }

private:
    std::atomic<bool> m_isOpen { true };
    std::atomic<bool> m_echoTx { true };
    mutable std::mutex m_txMutex;
    std::vector<std::vector<std::uint8_t>> m_sentPackets {};
    std::function<std::vector<std::vector<std::uint8_t>>(const std::vector<std::uint8_t>&)> m_responder {};
};

/// @brief Verify H7a: local RS-485 hardware echo of the query frame is not treated as device discovery.
TEST(BusScannerTest, EchoedProbeIgnoredAndNotDiscovered)
{
    auto transport = std::make_shared<EchoingTransport>();
    transport->setEcho(true); // Probe packets will be immediately echoed back to RX

    PelcoD::BusScanner scanner(transport);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 3U;
    cfg.timeoutMs = 25U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> finished { false };
    std::mutex discMutex;
    std::vector<PelcoD::DiscoveredDevice> discovered;
    scanner.setDeviceDiscoveredCallback([&](const PelcoD::DiscoveredDevice& dev) {
        std::scoped_lock lock(discMutex);
        discovered.push_back(dev);
    });
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>&) {
        finished.store(true);
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::scoped_lock lock(discMutex);
    EXPECT_TRUE(discovered.empty()) << "Echoed probes were falsely discovered as devices!";
    EXPECT_TRUE(scanner.getDiscoveredDevices().empty());
}

/// @brief Verify H7a: genuine response is discovered even if hardware echo precedes it.
TEST(BusScannerTest, ValidPanResponseDiscoveredDespiteEcho)
{
    auto transport = std::make_shared<EchoingTransport>();
    transport->setEcho(true); // Hardware echoes every probe

    // Configure responder: only address 2 responds with a genuine Pan response (0x59)
    transport->setResponder([](const std::vector<std::uint8_t>& query) -> std::vector<std::vector<std::uint8_t>> {
        if (query.size() == 7U && query[1] == 2U) {
            // Build valid Pan response: 9000 centidegrees (0x2328)
            const auto panResp = PelcoD::PelcoDFrame::createFrame(2U, 0x00U, 0x59U, 0x23U, 0x28U);
            return { panResp };
        }
        return {};
    });

    PelcoD::BusScanner scanner(transport);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 3U;
    cfg.timeoutMs = 40U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> finished { false };
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>&) {
        finished.store(true);
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto found = scanner.getDiscoveredDevices();
    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found[0].address, 2U);
    EXPECT_TRUE(found[0].hasPanPosition);
    EXPECT_EQ(found[0].panCentidegrees, 9000U);
}

/// @brief Verify H7b: transport data callback is detached on stop and destruction.
TEST(BusScannerTest, TransportCallbackDetachedOnStopAndDtor)
{
    auto transport = std::make_shared<EchoingTransport>();
    transport->setEcho(false);

    {
        PelcoD::BusScanner scanner(transport);
        PelcoD::ScanConfig cfg;
        cfg.startAddress = 1U;
        cfg.endAddress = 2U;
        cfg.timeoutMs = 50U;
        cfg.interCommandDelayMs = 2U;

        ASSERT_TRUE(scanner.startScan(cfg));
        scanner.stopScan();
        EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Idle);
        EXPECT_FALSE(transport->hasDataCallback()) << "Transport data callback not cleared on stopScan!";
    }

    // Scanner is now destroyed; sending incoming data must not access dangling callback
    EXPECT_FALSE(transport->hasDataCallback()) << "Transport data callback not cleared on dtor!";
    const auto dummyFrame = PelcoD::ProtocolBuilder::buildQueryPan(1U);
    EXPECT_NO_THROW({
        transport->sendData(dummyFrame);
    });
}

/// @brief Verify H7c: re-entrant access to scanner methods from state callback does not deadlock.
TEST(BusScannerTest, ReentrantStateCallbackDoesNotDeadlock)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 2U;
    cfg.timeoutMs = 20U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> stateCbInvoked { false };
    scanner.setScanStateChangedCallback([&](PelcoD::ScanState state) {
        if (state == PelcoD::ScanState::Scanning) {
            stateCbInvoked.store(true);
            // Re-entrant queries to scanner while in state callback
            EXPECT_TRUE(scanner.isScanning());
            EXPECT_EQ(scanner.getState(), PelcoD::ScanState::Scanning);
            EXPECT_TRUE(scanner.getDiscoveredDevices().empty());
        }
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    EXPECT_TRUE(stateCbInvoked.load());
    scanner.stopScan();
}

/// @brief Verify H7c: calling startScan from inside finished callback on worker thread returns false safely.
TEST(BusScannerTest, StartScanFromWorkerCbFailsCleanly)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 2U;
    cfg.timeoutMs = 15U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> restartAttempted { false };
    std::atomic<bool> restartResult { true };
    std::atomic<bool> finished { false };

    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>&) {
        restartAttempted.store(true);
        // Calling startScan from within the worker thread callback must safely return false
        // without calling m_worker.join() on itself (std::system_error / crash)
        restartResult.store(scanner.startScan(cfg));
        finished.store(true);
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (!finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(restartAttempted.load());
    EXPECT_FALSE(restartResult.load());
}

/// @brief Verify A4/H7b: TransportMultiplexer enables PelcoDDevice and BusScanner to share transport concurrently.
TEST(BusScannerTest, SharedTransportWithPelcoDDeviceViaMultiplexer)
{
    const std::uint8_t targetAddr = 2U;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(targetAddr);
    auto mux = std::make_shared<PelcoD::TransportMultiplexer>(mock);

    auto devChan = mux->createChannel();
    auto scanChan = mux->createChannel();

    PelcoD::PelcoDDevice device(devChan, targetAddr);
    ASSERT_TRUE(device.start());

    PelcoD::BusScanner scanner(scanChan);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 3U;
    cfg.timeoutMs = 40U;
    cfg.interCommandDelayMs = 2U;

    std::atomic<bool> scanFinished { false };
    scanner.setScanFinishedCallback([&](const std::vector<PelcoD::DiscoveredDevice>&) {
        scanFinished.store(true);
    });

    ASSERT_TRUE(scanner.startScan(cfg));
    while (scanner.isScanning() || !scanFinished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto discovered = scanner.getDiscoveredDevices();
    ASSERT_EQ(discovered.size(), 1U);
    EXPECT_EQ(discovered[0].address, targetAddr);

    // Verify PelcoDDevice continues normal operation without interference
    EXPECT_TRUE(device.isConnected());
    device.stop();
}

} // namespace

