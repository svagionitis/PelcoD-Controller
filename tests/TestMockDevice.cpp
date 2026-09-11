/// @file TestMockDevice.cpp
/// @brief End-to-end unit tests connecting PelcoDDevice to simulated MockPelcoDDevice.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

void testMockDeviceEndToEnd()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<bool> statusReceived { false };
    std::atomic<bool> trafficReceived { false };

    device.addStatusCallback([&](const PelcoD::DeviceStatus& status) {
        if (status.panCentidegrees > 0U || status.tiltCentidegrees > 0U) {
            statusReceived.store(true);
        }
    });

    device.addTrafficCallback([&]([[maybe_unused]] bool isTx, const std::vector<std::uint8_t>& frame) {
        if (!frame.empty()) {
            trafficReceived.store(true);
        }
    });

    assert(device.start());
    assert(device.isConnected());

    // 1. Send Pan Right command
    device.panRight(0x20U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify mock device internal pan moved
    const auto state1 = mock->getInternalState();
    assert(state1.panCentidegrees > 0U);
    assert(trafficReceived.load());

    // 2. Set Preset 1 at this position
    device.setPreset(1U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto state2 = mock->getInternalState();
    assert(state2.presets.find(1U) != state2.presets.end());
    assert(state2.presets.at(1U).pan == state1.panCentidegrees);

    // 3. Move elsewhere (Tilt Up)
    device.tiltUp(0x10U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. Go To Zero Pan
    device.zeroPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(mock->getInternalState().panCentidegrees == 0U);

    // 5. Recall Preset 1
    device.goToPreset(1U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(mock->getInternalState().panCentidegrees == state1.panCentidegrees);

    // 6. Query Pan
    device.queryPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Check that device received pan in status callback
    assert(statusReceived.load());
    assert(device.getStatus().panCentidegrees == state1.panCentidegrees);

    // 7. Set Zero Position (Calibration opcode 0x49)
    device.panRight(0x20U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(mock->getInternalState().panCentidegrees > 0U);
    device.setZeroPosition();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(mock->getInternalState().panCentidegrees == 0U);

    // 8. Set and Query Magnification (0x5F / 0x61)
    device.setMagnification(250U);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(mock->getInternalState().magnification == 250U);
    device.queryMagnification();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(device.getStatus().magnification == 250U);

    // 9. Query Diagnostics (0x6F -> 0x71)
    device.queryDiagnostics();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(device.getStatus().diagnosticTemp == mock->getInternalState().diagnosticTemp);
    assert(device.getStatus().diagnosticSensorId == mock->getInternalState().diagnosticSensorId);

    device.stop();
    assert(!device.isConnected());
}

void testCopyOnWriteCallbacks()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> cb1Count { 0 };
    std::atomic<int> cb2Count { 0 };
    std::atomic<int> cb3Count { 0 };

    device.addTrafficCallback(
        [&]([[maybe_unused]] bool isTx, const std::vector<std::uint8_t>&) { cb1Count.fetch_add(1); });

    assert(device.start());

    // Send command to trigger TX traffic callback
    device.panRight(0x10U);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(cb1Count.load() >= 1);

    // Dynamically register second callback (COW creates a new snapshot)
    device.addTrafficCallback(
        [&]([[maybe_unused]] bool isTx, const std::vector<std::uint8_t>&) { cb2Count.fetch_add(1); });

    const int cb1Snap = cb1Count.load();
    device.tiltUp(0x10U);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Both cb1 and cb2 must have received the new packet
    assert(cb1Count.load() > cb1Snap);
    assert(cb2Count.load() >= 1);

    // Register third callback while active
    device.addTrafficCallback(
        [&]([[maybe_unused]] bool isTx, const std::vector<std::uint8_t>&) { cb3Count.fetch_add(1); });

    device.stopMotion();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(cb3Count.load() >= 1);

    device.stop();
}

int main()
{
    std::cout << "[TestMockDevice] Running tests..." << std::endl;
    testMockDeviceEndToEnd();
    testCopyOnWriteCallbacks();
    std::cout << "[TestMockDevice] All tests passed successfully." << std::endl;
    return 0;
}
