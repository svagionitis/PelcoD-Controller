/// @file TestUdpTransport.cpp
/// @brief Unit tests for PelcoD::UdpTransport socket lifecycle, loopback messaging, and error paths.

#include "TestHelpers.h"
#include "UdpTransport.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace Transport;

/// @brief Verify UdpTransport constructors, accessors, and configuration setters.
TEST(UdpTransportTest, AccessorsAndDefaults)
{
    UdpTransport transport("127.0.0.1", 8080U, 8081U);

    EXPECT_EQ(transport.getHost(), "127.0.0.1");
    EXPECT_EQ(transport.getPort(), 8080U);
    EXPECT_EQ(transport.getLocalPort(), 8081U);
    EXPECT_FALSE(transport.isOpen());

    transport.setHost("192.168.1.100");
    EXPECT_EQ(transport.getHost(), "192.168.1.100");

    transport.setPort(9000U);
    EXPECT_EQ(transport.getPort(), 9000U);

    transport.setLocalPort(9001U);
    EXPECT_EQ(transport.getLocalPort(), 9001U);

    // Closing an un-opened transport must be a safe no-op
    transport.close();
    EXPECT_FALSE(transport.isOpen());

    // sendData on closed transport must return false
    const std::vector<std::uint8_t> frame { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    EXPECT_FALSE(transport.sendData(frame));
}

/// @brief Verify full-duplex loopback communication between two UDP transport peers.
TEST(UdpTransportTest, LoopbackCommunication)
{
    constexpr std::uint16_t portA { 28881U };
    constexpr std::uint16_t portB { 28882U };

    UdpTransport peerA("127.0.0.1", portB, portA);
    UdpTransport peerB("127.0.0.1", portA, portB);

    std::mutex mtxA;
    std::condition_variable cvA;
    std::vector<std::uint8_t> receivedA;

    std::mutex mtxB;
    std::condition_variable cvB;
    std::vector<std::uint8_t> receivedB;

    peerA.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::scoped_lock lock(mtxA);
        receivedA.insert(receivedA.end(), data.begin(), data.end());
        cvA.notify_all();
    });

    peerB.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::scoped_lock lock(mtxB);
        receivedB.insert(receivedB.end(), data.begin(), data.end());
        cvB.notify_all();
    });

    ASSERT_TRUE(peerB.open()) << "Peer B failed to open UDP socket";
    ASSERT_TRUE(peerA.open()) << "Peer A failed to open UDP socket";
    EXPECT_TRUE(peerA.isOpen());
    EXPECT_TRUE(peerB.isOpen());

    // Give socket listener threads a brief instant to enter poll
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send Pelco-D Pan Left frame from Peer A to Peer B
    const std::vector<std::uint8_t> frameAtoB { 0xFF, 0x01, 0x00, 0x04, 0x3F, 0x00, 0x44 };
    ASSERT_TRUE(peerA.sendData(frameAtoB)) << "Peer A failed to send datagram";

    {
        std::unique_lock<std::mutex> lock(mtxB);
        const bool received
            = cvB.wait_for(lock, std::chrono::seconds(2), [&]() { return receivedB.size() >= frameAtoB.size(); });
        ASSERT_TRUE(received) << "Peer B timed out waiting for datagram from Peer A";
        EXPECT_EQ(receivedB, frameAtoB) << "Peer B received payload does not match sent frame";
    }

    // Send response Pelco-D frame from Peer B back to Peer A
    const std::vector<std::uint8_t> frameBtoA { 0xFF, 0x01, 0x00, 0x59, 0x00, 0x64, 0xBE };
    ASSERT_TRUE(peerB.sendData(frameBtoA)) << "Peer B failed to send datagram";

    {
        std::unique_lock<std::mutex> lock(mtxA);
        const bool received
            = cvA.wait_for(lock, std::chrono::seconds(2), [&]() { return receivedA.size() >= frameBtoA.size(); });
        ASSERT_TRUE(received) << "Peer A timed out waiting for datagram from Peer B";
        EXPECT_EQ(receivedA, frameBtoA) << "Peer A received payload does not match sent frame";
    }

    // Shutdown and verify state transition
    peerA.close();
    peerB.close();

    EXPECT_FALSE(peerA.isOpen());
    EXPECT_FALSE(peerB.isOpen());
    EXPECT_FALSE(peerA.sendData(frameAtoB));
    EXPECT_FALSE(peerB.sendData(frameBtoA));
}

/// @brief Verify invalid host or configuration failure paths.
TEST(UdpTransportTest, ErrorHandling)
{
    // Empty host address
    {
        UdpTransport transport("", 9000U);
        EXPECT_FALSE(transport.open());
        EXPECT_FALSE(transport.isOpen());
    }

    // Zero destination port
    {
        UdpTransport transport("127.0.0.1", 0U);
        EXPECT_FALSE(transport.open());
        EXPECT_FALSE(transport.isOpen());
    }

    // Unresolvable hostname
    {
        UdpTransport transport("invalid.domain.that.cannot.exist.test", 9000U);
        EXPECT_FALSE(transport.open());
        EXPECT_FALSE(transport.isOpen());
    }
}

} // namespace
