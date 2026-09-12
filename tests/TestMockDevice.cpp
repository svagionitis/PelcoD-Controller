/// @file TestMockDevice.cpp
/// @brief End-to-end unit tests connecting PelcoDDevice to simulated MockPelcoDDevice.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "PelcoDFrame.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class ControlledTransport final : public PelcoD::ITransport {
public:
    bool open() override
    {
        m_open = true;
        return true;
    }

    void close() override
    {
        m_open = false;
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sentFrames.push_back(data);
        return m_open.load();
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dataCallback = std::move(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stateCallback = std::move(callback);
    }

    void inject(const std::vector<std::uint8_t>& data)
    {
        DataReceivedCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = m_dataCallback;
        }
        if (callback) {
            callback(data);
        }
    }

private:
    std::atomic<bool> m_open { false };
    mutable std::mutex m_mutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;
    std::vector<std::vector<std::uint8_t>> m_sentFrames;
};

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

    // 10. Query General (18-byte model name response)
    device.queryGeneral();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(device.getInfo().modelName == mock->getInternalState().modelName);

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

void testBurstTelemetryReception()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> rxPacketCount { 0 };
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (!isTx) {
            rxPacketCount.fetch_add(1);
        }
    });

    assert(device.start());

    // Inject 3 consecutive 7-byte telemetry frames (21 bytes total) into mock RX stream simultaneously
    // Pan response (Opcode 0x59): Pan = 100.00 deg = 10000 = 0x2710
    const auto f1 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x27U, 0x10U);
    // Tilt response (Opcode 0x5B): Tilt = 45.00 deg = 4500 = 0x1194
    const auto f2 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x11U, 0x94U);
    // Zoom response (Opcode 0x5D): Zoom = 1500 = 0x05DC
    const auto f3 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x05U, 0xDCU);

    std::vector<std::uint8_t> burst;
    burst.insert(burst.end(), f1.begin(), f1.end());
    burst.insert(burst.end(), f2.begin(), f2.end());
    burst.insert(burst.end(), f3.begin(), f3.end());
    assert(burst.size() == 21U);

    // Feed burst into device RX
    mock->injectRxData(burst);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // All 3 frames must have been individually parsed and dispatched
    assert(rxPacketCount.load() == 3);
    assert(device.getStatus().panCentidegrees == 10000U);
    assert(device.getStatus().tiltCentidegrees == 4500U);
    assert(device.getStatus().zoomPosition == 1500U);

    device.stop();
}

void testDynamicTelemetryPollingLifecycle()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> txPollQueryCount { 0 };
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        // Opcode in frame[3]: QueryPan (0x51), QueryTilt (0x53), QueryZoom (0x55)
        if (isTx && frame.size() == PelcoD::PelcoDFrame::StandardFrameSize) {
            const std::uint8_t op = frame[3];
            if (op == 0x51U || op == 0x53U || op == 0x55U) {
                txPollQueryCount.fetch_add(1);
            }
        }
    });

    // Start with polling disabled (the default)
    assert(!device.getTelemetryPolling());
    assert(device.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(txPollQueryCount.load() == 0);

    // Dynamically enable telemetry polling at 50ms interval while running
    device.setTelemetryPolling(true, 50U);
    assert(device.getTelemetryPolling());

    // Give time for at least 2 polling cycles (each cycle emits 3 queries: Pan, Tilt, Zoom)
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    assert(txPollQueryCount.load() >= 3);

    // Dynamically disable telemetry polling while running
    device.setTelemetryPolling(false);
    assert(!device.getTelemetryPolling());

    // Wait for in-flight queries from the active cycle to finish transmitting
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const int countAfterSettle = txPollQueryCount.load();

    // Ensure no additional queries are dispatched while polling is disabled
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(txPollQueryCount.load() == countAfterSettle);

    // Re-enable polling dynamically to verify repeat enable cycle
    device.setTelemetryPolling(true, 50U);
    assert(device.getTelemetryPolling());
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    assert(txPollQueryCount.load() > countAfterSettle);

    device.stop();

    // Verify polling configured before start()
    txPollQueryCount.store(0);
    PelcoD::PelcoDDevice devicePre(mock, 1U);
    devicePre.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        if (isTx && frame.size() == PelcoD::PelcoDFrame::StandardFrameSize) {
            const std::uint8_t op = frame[3];
            if (op == 0x51U || op == 0x53U || op == 0x55U) {
                txPollQueryCount.fetch_add(1);
            }
        }
    });

    devicePre.setTelemetryPolling(true, 50U);
    assert(devicePre.getTelemetryPolling());
    assert(devicePre.start());

    // Duplicate start() call must safely return true without terminating or leaking threads
    assert(devicePre.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    assert(txPollQueryCount.load() >= 3);

    devicePre.stop();
}

void testClearCallbacks()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> trafficCount { 0 };
    std::atomic<int> statusCount { 0 };

    device.addTrafficCallback([&](bool, const std::vector<std::uint8_t>&) { trafficCount.fetch_add(1); });
    device.addStatusCallback([&](const PelcoD::DeviceStatus&) { statusCount.fetch_add(1); });

    assert(device.start());
    device.panRight(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    assert(trafficCount.load() > 0);

    const int savedTraffic = trafficCount.load();
    const int savedStatus = statusCount.load();

    device.clearCallbacks();

    device.panLeft(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // No callbacks should fire after clearCallbacks()
    assert(trafficCount.load() == savedTraffic);
    assert(statusCount.load() == savedStatus);

    device.stop();
}

void testReentrantStart()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> trafficCount { 0 };
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (isTx) {
            trafficCount.fetch_add(1);
        }
    });

    // 1. First start should succeed
    assert(device.start());
    assert(device.getStatus().connected);

    // 2. Re-entrant sequential start calls while running must be idempotent
    assert(device.start());
    assert(device.start());
    assert(device.start());

    // Verify commands still process normally after multiple re-entrant start calls
    device.panRight(30);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    assert(trafficCount.load() >= 1);

    // 3. Stop and verify idempotent stop calls
    device.stop();
    assert(!device.getStatus().connected);
    device.stop(); // duplicate stop

    // 4. Restart cycle after stop
    assert(device.start());
    assert(device.getStatus().connected);
    assert(device.start()); // re-entrant again
    device.stop();

    // 5. Concurrent multi-threaded start() stress test
    PelcoD::PelcoDDevice concurrentDevice(mock, 1U);
    std::atomic<int> successCount { 0 };
    constexpr int threadCount = 8;
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&]() {
            if (concurrentDevice.start()) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    assert(successCount.load() == threadCount);
    assert(concurrentDevice.getStatus().connected);
    concurrentDevice.stop();
}

void testSharedBusDeviceFiltering()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> rxTrafficCount { 0 };
    std::atomic<int> statusUpdateCount { 0 };

    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>&) {
        if (!isTx) {
            rxTrafficCount.fetch_add(1);
        }
    });

    device.addStatusCallback([&](const PelcoD::DeviceStatus&) {
        statusUpdateCount.fetch_add(1);
    });

    assert(device.start());

    // Inject a Pan response for Device 2 (address 2, pan = 5000 = 0x1388)
    const auto fDev2Pan = PelcoD::PelcoDFrame::createFrame(0x02U, 0x00U, 0x59U, 0x13U, 0x88U);
    mock->injectRxData(fDev2Pan);

    // Inject a 4-byte general response for Device 2 (address 2, alarms = 0x05)
    const std::vector<std::uint8_t> fDev2Gen { 0xFFU, 0x02U, 0x05U, 0x07U };
    mock->injectRxData(fDev2Gen);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Traffic callback must see both frames (bus sniffer)
    assert(rxTrafficCount.load() == 2);

    // Device 1 status must NOT be modified by Device 2's packets
    assert(device.getStatus().address == 1U);
    assert(device.getStatus().panCentidegrees == 0U);
    assert(device.getStatus().alarms == 0U);
    assert(statusUpdateCount.load() == 0);

    device.stop();
}

void testQueryResponseCorrelation()
{
    auto transport = std::make_shared<ControlledTransport>();
    PelcoD::PelcoDDevice device(transport, 1U);
    device.setQueryTimeoutMs(150U);

    std::atomic<bool> panUpdated { false };
    std::atomic<bool> timedOut { false };
    device.addStatusCallback([&](const PelcoD::DeviceStatus& status) {
        if (status.panCentidegrees == 8500U) {
            panUpdated.store(true);
        }
    });
    device.addTimeoutCallback([&](const std::string& tag) {
        if (tag == "QueryPan") {
            timedOut.store(true);
        }
    });

    assert(device.start());

    device.queryPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    // Inject an interleaved 4-byte general response for Device 1 (e.g. motion/alarm acknowledgment)
    const std::vector<std::uint8_t> fGen { 0xFFU, 0x01U, 0x00U, 0x01U };
    transport->inject(fGen);

    // Inject a valid response for another device.
    const auto fOtherDevicePan = PelcoD::PelcoDFrame::createFrame(0x02U, 0x00U, 0x59U, 0x13U, 0x88U);
    transport->inject(fOtherDevicePan);

    // Inject a mismatched 7-byte Tilt response for Device 1 (opcode 0x5B, tilt = 2000)
    const auto fTilt = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x07U, 0xD0U);
    transport->inject(fTilt);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(!panUpdated.load());
    assert(device.getStatus().address == 1U);
    assert(device.getStatus().panCentidegrees == 0U);
    assert(device.getStatus().tiltCentidegrees == 0U);

    // Inject the expected 7-byte Pan response for Device 1 (opcode 0x59, pan = 8500 = 0x2134)
    const auto fPan = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x21U, 0x34U);
    transport->inject(fPan);

    for (int attempt = 0; attempt < 20 && !panUpdated.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(panUpdated.load());
    assert(device.getStatus().panCentidegrees == 8500U);

    // A mismatched response must not satisfy a subsequent query.
    panUpdated.store(false);
    timedOut.store(false);
    device.queryPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    transport->inject(fTilt);
    for (int attempt = 0; attempt < 30 && !timedOut.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(timedOut.load());

    device.stop();
}

int main()
{
    std::cout << "[TestMockDevice] Running tests..." << std::endl;
    testMockDeviceEndToEnd();
    testCopyOnWriteCallbacks();
    testBurstTelemetryReception();
    testDynamicTelemetryPollingLifecycle();
    testClearCallbacks();
    testReentrantStart();
    testSharedBusDeviceFiltering();
    testQueryResponseCorrelation();
    std::cout << "[TestMockDevice] All tests passed successfully." << std::endl;
    return 0;
}
