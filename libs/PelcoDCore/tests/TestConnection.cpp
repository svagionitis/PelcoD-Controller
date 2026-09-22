/// @file TestConnection.cpp
/// @brief Google Test suite for Connection, ScopedConnection, and callback lifecycle management.

#include "Connection.h"
#include "FujinonSX800Device.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "TestHelpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace PelcoDTest;

namespace {

template <typename Predicate> [[nodiscard]] bool waitFor(Predicate pred, int timeoutMs = 2000)
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

/// @brief Verify basic Connection construction, disconnection idempotence, and move semantics.
/// @details Checks default construction, calling disconnect on empty connection, disconnecting active
///          connection, idempotent secondary disconnect, and move assignment.
TEST(ConnectionTest, BasicConnectionLifecycle)
{
    PelcoD::Connection connDefault;
    EXPECT_FALSE(connDefault.isConnected());
    connDefault.disconnect(); // Must be safe
    EXPECT_FALSE(connDefault.isConnected());

    bool disconnected { false };
    {
        PelcoD::Connection conn([&disconnected]() { disconnected = true; });
        EXPECT_TRUE(conn.isConnected());
        conn.disconnect();
        EXPECT_TRUE(disconnected);
        EXPECT_FALSE(conn.isConnected());

        // Second disconnect must be idempotent
        disconnected = false;
        conn.disconnect();
        EXPECT_FALSE(disconnected);
    }

    // Move semantics
    bool movedDisconnected { false };
    {
        PelcoD::Connection c1([&movedDisconnected]() { movedDisconnected = true; });
        PelcoD::Connection c2 = std::move(c1);
        EXPECT_TRUE(c2.isConnected());
        c2.disconnect();
        EXPECT_TRUE(movedDisconnected);
        EXPECT_FALSE(c2.isConnected());
    }
}

/// @brief Verify ScopedConnection RAII automatic disconnection upon destruction.
/// @details Validates automatic disconnect on scope exit, manual disconnect before scope exit,
///          ownership release via release(), and move assignment between scoped connections.
TEST(ConnectionTest, ScopedConnectionLifecycle)
{
    bool disconnected { false };
    {
        PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
        EXPECT_TRUE(scoped.isConnected());
    }
    // Must have automatically disconnected upon leaving scope
    EXPECT_TRUE(disconnected);

    // Manual disconnect early
    disconnected = false;
    {
        PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
        scoped.disconnect();
        EXPECT_TRUE(disconnected);
        EXPECT_FALSE(scoped.isConnected());
        disconnected = false;
    }
    // Must not fire again on destruction
    EXPECT_FALSE(disconnected);

    // Release ownership
    disconnected = false;
    {
        PelcoD::Connection released;
        {
            PelcoD::ScopedConnection scoped(PelcoD::Connection([&disconnected]() { disconnected = true; }));
            released = scoped.release();
            EXPECT_FALSE(scoped.isConnected());
            EXPECT_TRUE(released.isConnected());
        }
        EXPECT_FALSE(disconnected); // Scope exit did not disconnect
        released.disconnect();
        EXPECT_TRUE(disconnected);
    }

    // Move assignment
    bool disc1 { false };
    bool disc2 { false };
    {
        PelcoD::ScopedConnection s1(PelcoD::Connection([&disc1]() { disc1 = true; }));
        PelcoD::ScopedConnection s2(PelcoD::Connection([&disc2]() { disc2 = true; }));

        s1 = std::move(s2);
        // s1's previous connection should have been disconnected
        EXPECT_TRUE(disc1);
        EXPECT_FALSE(disc2);
        EXPECT_TRUE(s1.isConnected());
    }
    EXPECT_TRUE(disc2);
}

/// @brief Verify ScopedConnectionList container lifecycle, addition, and bulk disconnection.
/// @details Tests adding connections via operator+= and add(), disconnectAll(), clear(),
///          and move semantics.
TEST(ConnectionTest, ScopedConnectionList)
{
    PelcoD::ScopedConnectionList list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0U);

    bool disc1 { false };
    bool disc2 { false };
    list += PelcoD::Connection([&disc1] { disc1 = true; });
    list.add(PelcoD::Connection([&disc2] { disc2 = true; }));

    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.size(), 2U);
    EXPECT_FALSE(disc1);
    EXPECT_FALSE(disc2);

    list.disconnectAll();
    EXPECT_TRUE(disc1);
    EXPECT_TRUE(disc2);
    EXPECT_EQ(list.size(), 2U);

    list.clear();
    EXPECT_TRUE(list.empty());

    // Move semantics & auto-disconnect on destruction
    bool disc3 { false };
    {
        PelcoD::ScopedConnectionList listA;
        listA += PelcoD::Connection([&disc3] { disc3 = true; });
        PelcoD::ScopedConnectionList listB = std::move(listA);
        EXPECT_FALSE(disc3);
    }
    EXPECT_TRUE(disc3);
}

/// @brief Verify selective callback deregistration on PelcoDDevice without affecting others.
/// @details Registers two traffic callbacks, disconnects one, and verifies only the remaining callback
///          continues to receive events.
TEST(ConnectionTest, PelcoDDeviceSelectiveDisconnection)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::atomic<int> countA { 0 };
    std::atomic<int> countB { 0 };

    PelcoD::Connection connA
        = device.addTrafficCallback([&countA](bool, const std::vector<std::uint8_t>&) { countA.fetch_add(1); });

    PelcoD::Connection connB
        = device.addTrafficCallback([&countB](bool, const std::vector<std::uint8_t>&) { countB.fetch_add(1); });

    EXPECT_TRUE(connA.isConnected());
    EXPECT_TRUE(connB.isConnected());
    ASSERT_TRUE(device.start());

    // Send a command to trigger traffic callbacks
    device.panLeft(0x20);

    const bool receivedBoth = waitFor([&]() { return countA.load() >= 1 && countB.load() >= 1; }, 2000);
    EXPECT_TRUE(receivedBoth);

    // Disconnect A only
    connA.disconnect();
    EXPECT_FALSE(connA.isConnected());
    EXPECT_TRUE(connB.isConnected());

    const int recordedA = countA.load();
    const int recordedB = countB.load();

    device.panRight(0x20);

    const bool bIncremented = waitFor([&]() { return countB.load() > recordedB; }, 2000);
    EXPECT_TRUE(bIncremented);
    EXPECT_EQ(countA.load(), recordedA); // A should NOT have fired again

    // Disconnect B
    connB.disconnect();
    EXPECT_FALSE(connB.isConnected());

    device.stop();
}

/// @brief Verify safe disconnect invocation after the underlying PelcoDDevice is destroyed.
/// @details Ensures orphaned Connection objects can be safely disconnected or destroyed
///          without crashing or accessing invalid memory.
TEST(ConnectionTest, DisconnectAfterDeviceDestruction)
{
    PelcoD::Connection orphanedTraffic;
    PelcoD::ScopedConnection orphanedScoped;

    {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        PelcoD::PelcoDDevice device(mock, 1U);

        orphanedTraffic = device.addTrafficCallback([](bool, const std::vector<std::uint8_t>&) {});
        orphanedScoped = device.addStatusCallback([](const PelcoD::DeviceStatus&) {});

        EXPECT_TRUE(orphanedTraffic.isConnected());
        EXPECT_TRUE(orphanedScoped.isConnected());
    } // device destroyed here

    // Must not crash or perform undefined behavior
    orphanedTraffic.disconnect();
    EXPECT_FALSE(orphanedTraffic.isConnected());
}

/// @brief Verify callback lifecycle and clearCallbacks on FujinonSX800Device.
/// @details Tests that Fujinon status callbacks and base traffic callbacks are registered and cleanly
///          cleared by clearCallbacks().
TEST(ConnectionTest, FujinonSX800DeviceConnection)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::FujinonSX800Device fujinonDevice(mock, 1U);

    std::atomic<int> fujinonCount { 0 };
    PelcoD::Connection fujinonConn = fujinonDevice.addFujinonStatusCallback(
        [&fujinonCount](const PelcoD::FujinonStatus&) { fujinonCount.fetch_add(1); });

    EXPECT_TRUE(fujinonConn.isConnected());

    // Disconnect
    fujinonConn.disconnect();
    EXPECT_FALSE(fujinonConn.isConnected());

    // Test clearCallbacks clears both base and Fujinon
    std::atomic<int> baseCount { 0 };
    std::atomic<int> fujCount2 { 0 };

    fujinonDevice.addTrafficCallback([&baseCount](bool, const std::vector<std::uint8_t>&) { baseCount.fetch_add(1); });
    fujinonDevice.addFujinonStatusCallback([&fujCount2](const PelcoD::FujinonStatus&) { fujCount2.fetch_add(1); });

    fujinonDevice.clearCallbacks();

    // Verify after clearCallbacks
    ASSERT_TRUE(fujinonDevice.start());
    fujinonDevice.panLeft(0x20);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(baseCount.load(), 0);
    EXPECT_EQ(fujCount2.load(), 0);

    fujinonDevice.stop();
}

/// @brief Verify filtered traffic callbacks honoring direction and address filters.
/// @details Validates TX-only, RX-only, and address-specific traffic callback filters on PelcoDDevice.
TEST(ConnectionTest, FilteredTrafficCallbacks)
{
    auto transport = std::make_shared<ControlledTransport>();
    PelcoD::PelcoDDevice device(transport, 1U);

    std::atomic<int> txOnlyCount { 0 };
    std::atomic<int> rxOnlyCount { 0 };
    std::atomic<int> addr1Count { 0 };
    std::atomic<int> addr2Count { 0 };

    PelcoD::ScopedConnectionList conns;
    conns += device.addTrafficCallback(
        [&txOnlyCount](bool, const std::vector<std::uint8_t>&) { txOnlyCount.fetch_add(1); }, true, false);
    conns += device.addTrafficCallback(
        [&rxOnlyCount](bool, const std::vector<std::uint8_t>&) { rxOnlyCount.fetch_add(1); }, false, true);
    conns += device.addTrafficCallback(
        1U, [&addr1Count](bool, const std::vector<std::uint8_t>&) { addr1Count.fetch_add(1); });
    conns += device.addTrafficCallback(
        2U, [&addr2Count](bool, const std::vector<std::uint8_t>&) { addr2Count.fetch_add(1); });

    ASSERT_TRUE(device.start());

    // Send pan command (Address 1, TX)
    device.panLeft(0x20);

    const bool txReceived = waitFor([&] { return txOnlyCount.load() >= 1; }, 2000);
    EXPECT_TRUE(txReceived);
    EXPECT_EQ(rxOnlyCount.load(), 0); // RX only must NOT receive TX
    EXPECT_GE(addr1Count.load(), 1); // Addr 1 received TX
    EXPECT_EQ(addr2Count.load(), 0); // Addr 2 must NOT receive Addr 1

    // Inject RX frame for Address 2
    const std::vector<std::uint8_t> frameAddr2 = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
    transport->inject(frameAddr2);

    const bool rxReceived = waitFor([&] { return rxOnlyCount.load() >= 1; }, 2000);
    EXPECT_TRUE(rxReceived);
    EXPECT_GE(addr2Count.load(), 1); // Addr 2 received RX

    device.stop();
}

/// @brief Verify asynchronous query execution and future fulfillment or timeout failure.
/// @details Checks queryPanAsync and queryStatusAsync fulfilling futures on live device,
///          immediate exception on stopped device, and timeout failure on silent transport.
TEST(ConnectionTest, AsyncQueries)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    // Test disconnected device immediately fails
    auto failFut = device.queryPanAsync();
    EXPECT_THROW(failFut.get(), std::runtime_error);

    ASSERT_TRUE(device.start());

    // Send Pan Right to set non-zero pan
    device.panRight(0x20);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Query pan asynchronously
    auto panFut = device.queryPanAsync();
    ASSERT_TRUE(panFut.valid());
    const std::uint16_t pan = panFut.get();
    EXPECT_GT(pan, 0U);

    // Query status asynchronously
    auto statusFut = device.queryStatusAsync();
    ASSERT_TRUE(statusFut.valid());
    const auto status = statusFut.get();
    EXPECT_TRUE(status.connected);

    // Query timeout failure path via watchdog timeout (silent transport does not reply)
    auto silentTransport = std::make_shared<ControlledTransport>();
    PelcoD::PelcoDDevice silentDevice(silentTransport, 1U);
    ASSERT_TRUE(silentDevice.start());

    auto shortTimeoutFut = silentDevice.queryPanAsync(std::chrono::milliseconds(20));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_THROW(shortTimeoutFut.get(), std::runtime_error);

    silentDevice.stop();
    device.stop();
}

/// @brief Verify multiple concurrent connections and non-interfering disconnections.
/// @details Registers multiple connections and disconnects them in reverse/arbitrary order,
///          ensuring each invocation occurs independently.
TEST(ConnectionTest, MultipleConcurrentConnections)
{
    std::atomic<int> disconnectFlags { 0 };
    PelcoD::Connection c1([&]() { disconnectFlags.fetch_or(1); });
    PelcoD::Connection c2([&]() { disconnectFlags.fetch_or(2); });
    PelcoD::Connection c3([&]() { disconnectFlags.fetch_or(4); });

    EXPECT_TRUE(c1.isConnected());
    EXPECT_TRUE(c2.isConnected());
    EXPECT_TRUE(c3.isConnected());

    // Disconnect middle first
    c2.disconnect();
    EXPECT_EQ(disconnectFlags.load(), 2);
    EXPECT_FALSE(c2.isConnected());
    EXPECT_TRUE(c1.isConnected());
    EXPECT_TRUE(c3.isConnected());

    // Disconnect first
    c1.disconnect();
    EXPECT_EQ(disconnectFlags.load(), 3);
    EXPECT_FALSE(c1.isConnected());
    EXPECT_TRUE(c3.isConnected());

    // Disconnect last
    c3.disconnect();
    EXPECT_EQ(disconnectFlags.load(), 7);
    EXPECT_FALSE(c3.isConnected());
}

/// @brief Verify ScopedConnection assignment to rebind active connections.
/// @details Assigns a new Connection to an active ScopedConnection, ensuring the previous
///          connection is automatically disconnected and the new one takes over.
TEST(ConnectionTest, ConnectionRebind)
{
    bool firstDisconnected = false;
    bool secondDisconnected = false;

    {
        PelcoD::ScopedConnection scoped(PelcoD::Connection([&]() { firstDisconnected = true; }));
        EXPECT_TRUE(scoped.isConnected());
        EXPECT_FALSE(firstDisconnected);

        // Reassign new connection: previous must disconnect immediately
        scoped = PelcoD::Connection([&]() { secondDisconnected = true; });
        EXPECT_TRUE(firstDisconnected);
        EXPECT_FALSE(secondDisconnected);
        EXPECT_TRUE(scoped.isConnected());
    }

    // Leaving scope: second must now disconnect
    EXPECT_TRUE(secondDisconnected);
}

} // namespace
