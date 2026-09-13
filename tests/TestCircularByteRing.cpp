/// @file TestCircularByteRing.cpp
/// @brief Unit and concurrent stress tests for SPSC lock-free CircularByteRing.

#include "CircularByteRing.h"
#include "TestHelpers.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

void testBasicOperations()
{
    PelcoD::CircularByteRing<1024U> ring;
    assert(ring.capacity() == 1024U);
    assert(ring.availableRead() == 0U);
    assert(ring.availableWrite() == 1024U);

    const std::vector<std::uint8_t> testData { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U };
    assert(ring.writeExact(testData.data(), testData.size()));
    assert(ring.availableRead() == 5U);
    assert(ring.availableWrite() == 1024U - 5U);

    // Peek without advance
    std::vector<std::uint8_t> peekBuf(5U, 0x00U);
    assert(ring.peekBytes(peekBuf.data(), 5U));
    assert(peekBuf == testData);
    assert(ring.availableRead() == 5U);

    // Delimiter search
    assert(ring.findByte(0x03U) == 2U);
    assert(ring.findByte(0xFFU) == decltype(ring)::npos);

    // Read and advance
    std::vector<std::uint8_t> readBuf(5U, 0x00U);
    assert(ring.readExact(readBuf.data(), 5U));
    assert(readBuf == testData);
    assert(ring.availableRead() == 0U);
}

void testWrapAround()
{
    PelcoD::CircularByteRing<16U> ring; // Small buffer to force multiple wraps
    std::vector<std::uint8_t> src(10U);
    std::iota(src.begin(), src.end(), 100U);

    // Write 10 bytes
    assert(ring.writeExact(src.data(), 10U));
    assert(ring.availableRead() == 10U);

    // Read 8 bytes
    std::vector<std::uint8_t> dst(8U);
    assert(ring.readExact(dst.data(), 8U));
    assert(ring.availableRead() == 2U);

    // Write 10 bytes again, forcing wrap around tail
    std::vector<std::uint8_t> src2(10U);
    std::iota(src2.begin(), src2.end(), 200U);
    assert(ring.writeExact(src2.data(), 10U));
    assert(ring.availableRead() == 12U);

    // Read all 12 bytes
    std::vector<std::uint8_t> finalDst(12U);
    assert(ring.readExact(finalDst.data(), 12U));
    assert(ring.availableRead() == 0U);

    // Check first 2 remaining from src
    assert(finalDst[0] == src[8]);
    assert(finalDst[1] == src[9]);
    // Check 10 bytes from src2
    for (std::size_t i { 0U }; i < 10U; ++i) {
        assert(finalDst[2U + i] == src2[i]);
    }
}

void testConcurrentSPSC()
{
    PelcoD::CircularByteRing<4096U> ring;
    constexpr std::size_t TotalBytes = 200000U;
    std::atomic<bool> producerDone { false };

    // Producer thread
    std::thread producer([&] {
        std::size_t sent { 0U };
        while (sent < TotalBytes) {
            const std::uint8_t b = static_cast<std::uint8_t>(sent & 0xFFU);
            if (ring.writeExact(&b, 1U)) {
                ++sent;
            } else {
                std::this_thread::yield();
            }
        }
        producerDone.store(true);
    });

    // Consumer thread
    std::thread consumer([&] {
        std::size_t received { 0U };
        while (received < TotalBytes) {
            std::uint8_t b { 0x00U };
            if (ring.readExact(&b, 1U)) {
                const std::uint8_t expected = static_cast<std::uint8_t>(received & 0xFFU);
                assert(b == expected);
                ++received;
            } else {
                if (producerDone.load() && ring.availableRead() == 0U) {
                    break;
                }
                std::this_thread::yield();
            }
        }
        assert(received == TotalBytes);
    });

    producer.join();
    consumer.join();
}

int main()
{
    PelcoDTest::initTestHarness();
    std::cout << "[TestCircularByteRing] Running tests..." << std::endl;
    testBasicOperations();
    testWrapAround();
    testConcurrentSPSC();
    std::cout << "[TestCircularByteRing] All tests passed successfully." << std::endl;
    return 0;
}
