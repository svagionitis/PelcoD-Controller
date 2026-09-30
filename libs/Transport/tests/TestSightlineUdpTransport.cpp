/// @file TestSightlineUdpTransport.cpp
/// @brief Unit tests for Sightline dual-port UDP transport lifecycle, loopback and stats.

#include "SightlineUdpTransport.h"

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

/// @brief Verify constructors, getters, setters and default configuration.
TEST(SightlineUdpTransportTest, AccessorsAndDefaults)
{
    SightlineUdpTransport transport("127.0.0.1", 14001U, 14002U);

    EXPECT_EQ(transport.getHost(), "127.0.0.1");
    EXPECT_EQ(transport.getCommandPort(), 14001U);
    EXPECT_EQ(transport.getReplyPort(), 14002U);
    EXPECT_FALSE(transport.isOpen());

    transport.setHost("10.0.0.5");
    EXPECT_EQ(transport.getHost(), "10.0.0.5");

    transport.setCommandPort(14003U);
    EXPECT_EQ(transport.getCommandPort(), 14003U);

    transport.setReplyPort(14004U);
    EXPECT_EQ(transport.getReplyPort(), 14004U);

    transport.close();
    EXPECT_FALSE(transport.isOpen());

    const std::vector<std::uint8_t> data { 0x51U, 0xAC, 0x02U, 0x00U, 0x00U };
    EXPECT_FALSE(transport.sendData(data));
}

/// @brief Verify loopback communication between two dual-port transports.
TEST(SightlineUdpTransportTest, LoopbackCommunication)
{
    constexpr std::uint16_t portA { 29111U };
    constexpr std::uint16_t portB { 29112U };

    // Peer A transmits to portB, listens on portA
    SightlineUdpTransport peerA("127.0.0.1", portB, portA);
    // Peer B transmits to portA, listens on portB
    SightlineUdpTransport peerB("127.0.0.1", portA, portB);

    std::mutex mtxA;
    std::condition_variable cvA;
    std::vector<std::uint8_t> receivedA {};

    peerA.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mtxA);
        receivedA = data;
        cvA.notify_one();
    });

    std::mutex mtxB;
    std::condition_variable cvB;
    std::vector<std::uint8_t> receivedB {};

    peerB.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mtxB);
        receivedB = data;
        cvB.notify_one();
    });

    ASSERT_TRUE(peerA.open());
    ASSERT_TRUE(peerB.open());
    EXPECT_TRUE(peerA.isOpen());
    EXPECT_TRUE(peerB.isOpen());

    // Send from A to B
    const std::vector<std::uint8_t> msgA2B { 0x51U, 0xAC, 0x02U, 0x08U, 0xDDU };
    EXPECT_TRUE(peerA.sendData(msgA2B));

    {
        std::unique_lock<std::mutex> lock(mtxB);
        EXPECT_TRUE(cvB.wait_for(lock, std::chrono::milliseconds(500), [&]() { return !receivedB.empty(); }));
    }
    EXPECT_EQ(receivedB, msgA2B);

    // Send reply from B to A
    const std::vector<std::uint8_t> msgB2A { 0x51U, 0xAC, 0x03U, 0x40U, 0x01U, 0xEEU };
    EXPECT_TRUE(peerB.sendData(msgB2A));

    {
        std::unique_lock<std::mutex> lock(mtxA);
        EXPECT_TRUE(cvA.wait_for(lock, std::chrono::milliseconds(500), [&]() { return !receivedA.empty(); }));
    }
    EXPECT_EQ(receivedA, msgB2A);

    peerA.close();
    peerB.close();
    EXPECT_FALSE(peerA.isOpen());
    EXPECT_FALSE(peerB.isOpen());
}

/// @brief Verify statistics tracking on SightlineUdpTransport.
TEST(SightlineUdpTransportTest, StatsTracking)
{
    constexpr std::uint16_t portCmd { 29113U };
    constexpr std::uint16_t portReply { 29114U };

    SightlineUdpTransport transport("127.0.0.1", portCmd, portReply);
    ASSERT_TRUE(transport.open());

    const auto snap = transport.getStats();
    EXPECT_TRUE(snap.udp.has_value());

    transport.close();
}

} // namespace
