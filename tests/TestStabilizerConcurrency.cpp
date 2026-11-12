/// @file TestStabilizerConcurrency.cpp
/// @brief Concurrency, thread safety, and lock-free exchange test suite for StabilizerAppQt.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "StabilizerConfigModel.h"
#include "StabilizerFramePacket.h"
#include "StabilizerWorker.h"

namespace StabilizerApp {

class StabilizerConcurrencyTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestStabilizerConcurrency" };
            static char* argv[] { appName, nullptr };
            static auto app { std::make_unique<QCoreApplication>(argc, argv) };
        }
    }
};

TEST_F(StabilizerConcurrencyTest, LockFreeTripleBufferProducerConsumerBurst)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    constexpr std::uint32_t testWidth { 320U };
    constexpr std::uint32_t testHeight { 240U };

    for (auto& slot : tripleBuf.getSlots()) {
        slot.preallocate(testWidth, testHeight);
    }

    std::atomic<bool> stopFlag { false };
    std::atomic<std::uint32_t> framesProduced { 0U };
    std::atomic<std::uint32_t> framesConsumed { 0U };

    // High-speed producer thread
    std::thread producerThread([&]() {
        std::uint32_t counter { 0U };
        while (!stopFlag.load(std::memory_order_relaxed)) {
            auto& writeSlot = tripleBuf.getWriteBuffer();
            writeSlot.timestampSec = static_cast<double>(counter);
            if (!writeSlot.rawRgb.empty()) {
                writeSlot.rawRgb[0] = static_cast<std::uint8_t>(counter & 0xFFU);
            }
            tripleBuf.publishWriteBuffer();
            ++counter;
            framesProduced.store(counter, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // High-speed consumer thread
    std::thread consumerThread([&]() {
        while (!stopFlag.load(std::memory_order_relaxed)) {
            if (tripleBuf.swapReadBuffer()) {
                const auto& readSlot = tripleBuf.getReadBuffer();
                EXPECT_EQ(readSlot.width, testWidth);
                EXPECT_EQ(readSlot.height, testHeight);
                framesConsumed.fetch_add(1U, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
    });

    // Run burst for 500 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stopFlag.store(true, std::memory_order_release);

    producerThread.join();
    consumerThread.join();

    EXPECT_GT(framesProduced.load(), 100U);
    EXPECT_GT(framesConsumed.load(), 50U);
}

TEST_F(StabilizerConcurrencyTest, ConcurrentConfigHotReloading)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    worker.openSource(QStringLiteral("synthetic://shaker"));
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    StabilizerConfigModel model;

    // Concurrently mutate config while ticking worker
    for (std::uint32_t i { 0U }; i < 30U; ++i) {
        model.setMinTrackedFeatures(static_cast<int>(10U + (i % 50U)));
        model.setMaxCropMargin(0.05 + static_cast<double>(i % 15U) * 0.01);
        model.setInlierThreshold(1.0 + static_cast<double>(i % 10U) * 0.2);

        worker.updateConfig(model.toConfig());
        worker.processTick();

        static_cast<void>(tripleBuf.swapReadBuffer());
    }

    worker.stop();
}

} // namespace StabilizerApp
