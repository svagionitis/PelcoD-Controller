/// @file TestConnection.cpp
/// @brief Comprehensive tests for Connection, ScopedConnection, and callback lifecycle management.

#include "Connection.h"
#include "FujinonSX800Device.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "TestHelpers.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace PelcoDTest;

/// @brief Tests default state, copy, move, and idempotent disconnect of Connection.
void testBasicConnection()
{
    std::cout << "Testing basic Connection lifecycle..." << std::endl;

    PelcoD::Connection connDefault;
    assert(!connDefault.isConnected());
    connDefault.disconnect(); // Must be safe
    assert(!connDefault.isConnected());

    bool disconnected = false;
    {
        PelcoD::Connection conn([&disconnected]() { disconnected = true; });
        assert(conn.isConnected());
        conn.disconnect();
        assert(disconnected);
        assert(!conn.isConnected());

        // Second disconnect must be idempotent
        disconnected = false;
        conn.disconnect();
        assert(!disconnected);
    }

    // Move semantics
    bool movedDisconnected = false;
    {
        PelcoD::Connection c1([&movedDisconnected]() { movedDisconnected = true; });
        PelcoD::Connection c2 = std::move(c1);
        assert(c2.isConnected());
        c2.disconnect();
        assert(movedDisconnected);
        assert(!c2.isConnected());
    }
}

/// @brief Tests ScopedConnection RAII behavior, move semantics, and release().
void testScopedConnection()
{
    std::cout << "Testing ScopedConnection RAII lifecycle..." << std::endl;

    bool disconnected = false;
    {
        PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
        assert(scoped.isConnected());
    }
    // Must have automatically disconnected upon leaving scope
    assert(disconnected);

    // Manual disconnect early
    disconnected = false;
    {
        PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
        scoped.disconnect();
        assert(disconnected);
        assert(!scoped.isConnected());
        disconnected = false;
    }
    // Must not fire again on destruction
    assert(!disconnected);

    // Release ownership
    disconnected = false;
    {
        PelcoD::Connection released;
        {
            PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
            released = scoped.release();
            assert(!scoped.isConnected());
            assert(released.isConnected());
        }
        assert(!disconnected); // Scope exit did not disconnect
        released.disconnect();
        assert(disconnected);
    }

    // Move assignment
    bool disc1 = false;
    bool disc2 = false;
    {
        PelcoD::ScopedConnection s1(PelcoD::Connection([&disc1]() { disc1 = true; }));
        PelcoD::ScopedConnection s2(PelcoD::Connection([&disc2]() { disc2 = true; }));

        s1 = std::move(s2);
        // s1's previous connection should have been disconnected
        assert(disc1);
        assert(!disc2);
        assert(s1.isConnected());
    }
    assert(disc2);
}

template <typename Predicate> bool waitFor(Predicate pred, int timeoutMs = 2000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return pred();
}

/// @brief Tests selective callback disconnection on PelcoDDevice.
void testPelcoDDeviceSelectiveDisconnection()
{
    std::cout << "Testing PelcoDDevice selective callback disconnection..." << std::endl;

    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> countA { 0 };
    std::atomic<int> countB { 0 };

    PelcoD::Connection connA
        = device.addTrafficCallback([&countA](bool, const std::vector<std::uint8_t>&) { countA.fetch_add(1); });

    PelcoD::Connection connB
        = device.addTrafficCallback([&countB](bool, const std::vector<std::uint8_t>&) { countB.fetch_add(1); });

    assert(connA.isConnected());
    assert(connB.isConnected());
    assert(device.start());

    // Send a command to trigger traffic callbacks
    device.panLeft(0x20);

    bool receivedBoth = waitFor([&]() { return countA.load() >= 1 && countB.load() >= 1; }, 2000);
    assert(receivedBoth);

    // Disconnect A only
    connA.disconnect();
    assert(!connA.isConnected());
    assert(connB.isConnected());

    const int recordedA = countA.load();
    const int recordedB = countB.load();

    device.panRight(0x20);

    bool bIncremented = waitFor([&]() { return countB.load() > recordedB; }, 2000);
    assert(bIncremented);
    assert(countA.load() == recordedA); // A should NOT have fired again

    // Disconnect B
    connB.disconnect();
    assert(!connB.isConnected());

    device.stop();
}

/// @brief Tests that disconnecting after device destruction is completely safe (no crash/UB).
void testDisconnectAfterDeviceDestruction()
{
    std::cout << "Testing disconnection after PelcoDDevice destruction..." << std::endl;

    PelcoD::Connection orphanedTraffic;
    PelcoD::ScopedConnection orphanedScoped;

    {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        PelcoD::PelcoDDevice device(mock, 1U);

        orphanedTraffic = device.addTrafficCallback([](bool, const std::vector<std::uint8_t>&) {});
        orphanedScoped = device.addStatusCallback([](const PelcoD::DeviceStatus&) {});

        assert(orphanedTraffic.isConnected());
        assert(orphanedScoped.isConnected());
    } // device destroyed here

    // Must not crash or perform undefined behavior
    orphanedTraffic.disconnect();
    assert(!orphanedTraffic.isConnected());

    // orphanedScoped destructor will run at end of function without issue
}

/// @brief Tests FujinonSX800Device extended status callback and clearCallbacks override.
void testFujinonSX800DeviceConnection()
{
    std::cout << "Testing FujinonSX800Device Connection and clearCallbacks..." << std::endl;

    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::FujinonSX800Device fujinonDevice(mock, 1U);

    std::atomic<int> fujinonCount { 0 };
    PelcoD::Connection fujinonConn = fujinonDevice.addFujinonStatusCallback(
        [&fujinonCount](const PelcoD::FujinonStatus&) { fujinonCount.fetch_add(1); });

    assert(fujinonConn.isConnected());

    // Disconnect
    fujinonConn.disconnect();
    assert(!fujinonConn.isConnected());

    // Test clearCallbacks clears both base and Fujinon
    std::atomic<int> baseCount { 0 };
    std::atomic<int> fujCount2 { 0 };

    fujinonDevice.addTrafficCallback([&baseCount](bool, const std::vector<std::uint8_t>&) { baseCount.fetch_add(1); });
    fujinonDevice.addFujinonStatusCallback([&fujCount2](const PelcoD::FujinonStatus&) { fujCount2.fetch_add(1); });

    fujinonDevice.clearCallbacks();

    // Verify after clearCallbacks
    assert(fujinonDevice.start());
    fujinonDevice.panLeft(0x20);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(baseCount.load() == 0);
    assert(fujCount2.load() == 0);

    fujinonDevice.stop();
}

int main()
{
    std::cout << "=== Running TestConnection ===" << std::endl;
    testBasicConnection();
    testScopedConnection();
    testPelcoDDeviceSelectiveDisconnection();
    testDisconnectAfterDeviceDestruction();
    testFujinonSX800DeviceConnection();
    std::cout << "=== All TestConnection tests passed! ===" << std::endl;
    return 0;
}
