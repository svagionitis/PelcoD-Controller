/// @file TestBusScanner.cpp
/// @brief Unit tests for PelcoD::BusScanner range scanning, device discovery, and lifecycle controls.

#include "BusScanner.h"
#include "MockPelcoDDevice.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

/// @brief Verify validation of invalid address ranges.
static void testRangeValidation()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    assert(scanner.getState() == PelcoD::ScanState::Idle);
    assert(!scanner.isScanning());
    assert(!scanner.isPaused());

    // Invalid: start > end
    PelcoD::ScanConfig cfg1;
    cfg1.startAddress = 10U;
    cfg1.endAddress = 5U;
    assert(!scanner.startScan(cfg1));
    assert(scanner.getState() == PelcoD::ScanState::Idle);

    // Invalid: start == 0
    PelcoD::ScanConfig cfg2;
    cfg2.startAddress = 0U;
    cfg2.endAddress = 10U;
    assert(!scanner.startScan(cfg2));

    // Invalid: end == 255 (valid range is 1-254)
    PelcoD::ScanConfig cfg3;
    cfg3.startAddress = 1U;
    cfg3.endAddress = 255U;
    assert(!scanner.startScan(cfg3));

    std::cout << "  testRangeValidation: PASSED\n";
}

/// @brief Verify discovery of a mock device on an active address.
static void testSingleDeviceDiscovery()
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

    assert(scanner.startScan(cfg));
    assert(scanner.isScanning());

    // Wait for scan to complete (max 2 seconds)
    const auto startTime = std::chrono::steady_clock::now();
    while (scanner.isScanning() || !finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(2)) {
            assert(false && "Scan timed out waiting for completion");
        }
    }

    assert(scanner.getState() == PelcoD::ScanState::Idle);
    assert(finished.load());
    assert(totalFoundCount.load() == 1U);

    const auto foundList = scanner.getDiscoveredDevices();
    assert(foundList.size() == 1U);
    assert(foundList[0].address == targetAddr);
    assert(foundList[0].hasPanPosition);
    assert(foundList[0].panCentidegrees == 9000);

    assert(deviceFound.load());
    assert(discoveredAddr.load() == targetAddr);
    assert(hasPan.load());
    assert(panAngle.load() == 9000);

    std::cout << "  testSingleDeviceDiscovery: PASSED\n";
}

/// @brief Verify progress callbacks are reported for each address.
static void testProgressCallbacks()
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
        std::lock_guard<std::mutex> lock(cbMutex);
        reportedAddresses.push_back(current);
        reportedCounts.push_back(scanned);
        callbackCount.fetch_add(1);
    });

    assert(scanner.startScan(cfg));
    while (scanner.isScanning() || callbackCount.load() < 3U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::lock_guard<std::mutex> lock(cbMutex);
    assert(reportedAddresses.size() == 3U);
    assert(reportedAddresses[0] == 1U);
    assert(reportedAddresses[1] == 2U);
    assert(reportedAddresses[2] == 3U);

    assert(reportedCounts.size() == 3U);
    assert(reportedCounts[0] == 1U);
    assert(reportedCounts[1] == 2U);
    assert(reportedCounts[2] == 3U);

    std::cout << "  testProgressCallbacks: PASSED\n";
}

/// @brief Verify stopScan aborts an active scan immediately.
static void testStopScan()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(200U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 20U;
    cfg.timeoutMs = 150U;
    cfg.interCommandDelayMs = 10U;

    std::atomic<std::size_t> scannedCount { 0 };
    scanner.setScanProgressCallback([&](std::uint8_t /*curr*/, std::size_t scanned, std::size_t /*tot*/) {
        scannedCount.store(scanned);
    });

    assert(scanner.startScan(cfg));
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
            assert(false && "stopScan did not terminate in expected timeframe");
        }
    }

    assert(scanner.getState() == PelcoD::ScanState::Idle);
    // Should have stopped well before reaching address 20
    assert(scannedCount.load() < 20U);

    std::cout << "  testStopScan: PASSED\n";
}

/// @brief Verify pause and resume operations.
static void testPauseResume()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::BusScanner scanner(mock);

    PelcoD::ScanConfig cfg;
    cfg.startAddress = 1U;
    cfg.endAddress = 5U;
    cfg.timeoutMs = 50U;
    cfg.interCommandDelayMs = 5U;

    assert(scanner.startScan(cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    scanner.pauseScan();
    assert(scanner.isPaused());
    assert(scanner.getState() == PelcoD::ScanState::Paused);

    // Sleep while paused
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(scanner.isPaused());

    // Resume
    scanner.resumeScan();
    assert(!scanner.isPaused());

    while (scanner.isScanning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(scanner.getState() == PelcoD::ScanState::Idle);

    std::cout << "  testPauseResume: PASSED\n";
}

int main()
{
    std::cout << "Running TestBusScanner...\n";

    testRangeValidation();
    testSingleDeviceDiscovery();
    testProgressCallbacks();
    testStopScan();
    testPauseResume();

    std::cout << "All TestBusScanner tests PASSED!\n";
    return 0;
}
