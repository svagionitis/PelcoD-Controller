/// @file TestUdpTransport.cpp
/// @brief Unit tests for PelcoD::UdpTransport socket lifecycle, loopback messaging, and error paths.

#include "TestHelpers.h"
#include "UdpTransport.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/// @brief Verify UdpTransport constructors, accessors, and configuration setters.
static void testAccessorsAndDefaults()
{
    PelcoD::UdpTransport transport("127.0.0.1", 8080U, 8081U);

    assert(transport.getHost() == "127.0.0.1");
    assert(transport.getPort() == 8080U);
    assert(transport.getLocalPort() == 8081U);
    assert(!transport.isOpen());

    transport.setHost("192.168.1.100");
    assert(transport.getHost() == "192.168.1.100");

    transport.setPort(9000U);
    assert(transport.getPort() == 9000U);

    transport.setLocalPort(9001U);
    assert(transport.getLocalPort() == 9001U);

    // Closing an un-opened transport must be a safe no-op
    transport.close();
    assert(!transport.isOpen());

    // sendData on closed transport must return false
    const std::vector<std::uint8_t> frame { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    assert(!transport.sendData(frame));

    std::cout << "  testAccessorsAndDefaults: PASSED\n";
}

/// @brief Verify full-duplex loopback communication between two UDP transport peers.
static void testLoopbackCommunication()
{
    constexpr std::uint16_t portA { 28881U };
    constexpr std::uint16_t portB { 28882U };

    PelcoD::UdpTransport peerA("127.0.0.1", portB, portA);
    PelcoD::UdpTransport peerB("127.0.0.1", portA, portB);

    std::mutex mtxA;
    std::condition_variable cvA;
    std::vector<std::uint8_t> receivedA;

    std::mutex mtxB;
    std::condition_variable cvB;
    std::vector<std::uint8_t> receivedB;

    peerA.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mtxA);
        receivedA.insert(receivedA.end(), data.begin(), data.end());
        cvA.notify_all();
    });

    peerB.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mtxB);
        receivedB.insert(receivedB.end(), data.begin(), data.end());
        cvB.notify_all();
    });

    assert(peerB.open() && "Peer B failed to open UDP socket");
    assert(peerA.open() && "Peer A failed to open UDP socket");
    assert(peerA.isOpen());
    assert(peerB.isOpen());

    // Give socket listener threads a brief instant to enter poll
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send Pelco-D Pan Left frame from Peer A to Peer B
    const std::vector<std::uint8_t> frameAtoB { 0xFF, 0x01, 0x00, 0x04, 0x3F, 0x00, 0x44 };
    assert(peerA.sendData(frameAtoB) && "Peer A failed to send datagram");

    {
        std::unique_lock<std::mutex> lock(mtxB);
        const bool received = cvB.wait_for(lock, std::chrono::seconds(2), [&]() {
            return receivedB.size() >= frameAtoB.size();
        });
        assert(received && "Peer B timed out waiting for datagram from Peer A");
        assert(receivedB == frameAtoB && "Peer B received payload does not match sent frame");
    }

    // Send response Pelco-D frame from Peer B back to Peer A
    const std::vector<std::uint8_t> frameBtoA { 0xFF, 0x01, 0x00, 0x59, 0x00, 0x64, 0xBE };
    assert(peerB.sendData(frameBtoA) && "Peer B failed to send datagram");

    {
        std::unique_lock<std::mutex> lock(mtxA);
        const bool received = cvA.wait_for(lock, std::chrono::seconds(2), [&]() {
            return receivedA.size() >= frameBtoA.size();
        });
        assert(received && "Peer A timed out waiting for datagram from Peer B");
        assert(receivedA == frameBtoA && "Peer A received payload does not match sent frame");
    }

    // Shutdown and verify state transition
    peerA.close();
    peerB.close();

    assert(!peerA.isOpen());
    assert(!peerB.isOpen());
    assert(!peerA.sendData(frameAtoB));
    assert(!peerB.sendData(frameBtoA));

    std::cout << "  testLoopbackCommunication: PASSED\n";
}

/// @brief Verify invalid host or configuration failure paths.
static void testErrorHandling()
{
    // Empty host address
    {
        PelcoD::UdpTransport transport("", 9000U);
        assert(!transport.open());
        assert(!transport.isOpen());
    }

    // Zero destination port
    {
        PelcoD::UdpTransport transport("127.0.0.1", 0U);
        assert(!transport.open());
        assert(!transport.isOpen());
    }

    // Unresolvable hostname
    {
        PelcoD::UdpTransport transport("invalid.domain.that.cannot.exist.test", 9000U);
        assert(!transport.open());
        assert(!transport.isOpen());
    }

    std::cout << "  testErrorHandling: PASSED\n";
}

int main()
{
    PelcoDTest::initTestHarness();

    std::cout << "[TestUdpTransport] Running...\n";
    testAccessorsAndDefaults();
    testLoopbackCommunication();
    testErrorHandling();
    std::cout << "[TestUdpTransport] All tests passed.\n";
    return 0;
}
