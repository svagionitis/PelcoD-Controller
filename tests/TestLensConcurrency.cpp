/// @file TestLensConcurrency.cpp
/// @brief Concurrency, thread safety, and lock-free exchange test suite for LensAppQt.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QString>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "LensConfigModel.h"
#include "LensFramePacket.h"
#include "LensPlayerController.h"
#include "LensPresetManager.h"
#include "LensTelemetryRingBuffer.h"

namespace LensApp {

class LensConcurrencyTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestLensConcurrency" };
            static char* argv[] { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensConcurrencyTest, LockFreeTripleBufferProducerConsumerBurst)
{
    Video::AtomicTripleBuffer<LensFramePacket> tripleBuf;
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
            writeSlot.frameId = counter;
            writeSlot.timestampSec = static_cast<double>(counter) * 0.016666;
            if (!writeSlot.rgbBuffer.empty()) {
                writeSlot.rgbBuffer[0] = static_cast<std::uint8_t>(counter & 0xFFU);
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

    // Run burst for 300 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stopFlag.store(true, std::memory_order_release);

    producerThread.join();
    consumerThread.join();

    EXPECT_GT(framesProduced.load(), 50U);
    EXPECT_GT(framesConsumed.load(), 20U);
}

TEST_F(LensConcurrencyTest, ConcurrentTelemetryRingBufferReadWrite)
{
    LensTelemetryRingBuffer ring;
    std::atomic<bool> stopFlag { false };
    std::atomic<std::uint32_t> samplesAppended { 0U };

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString csvPath = tempDir.path() + QStringLiteral("/concurrent_telemetry.csv");

    // Writer thread: pushes samples continuously
    std::thread writerThread([&]() {
        std::uint32_t counter { 0U };
        while (!stopFlag.load(std::memory_order_relaxed)) {
            TelemetrySample s { };
            s.timestampNs = static_cast<std::uint64_t>(counter) * 1000000ULL;
            s.frameIndex = counter;
            s.channel = 0U;
            s.focusPosition = static_cast<std::uint16_t>(counter % 65535U);
            s.zoomPosition = static_cast<std::uint16_t>((counter * 2U) % 65535U);
            s.metricScore = static_cast<double>(counter % 100U);
            ring.appendSample(s);
            ++counter;
            samplesAppended.store(counter, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Reader thread 1: queries statistics
    std::thread statsReader([&]() {
        while (!stopFlag.load(std::memory_order_relaxed)) {
            static_cast<void>(ring.latestMetricScore());
            static_cast<void>(ring.minMetricScore());
            static_cast<void>(ring.maxMetricScore());
            static_cast<void>(ring.avgMetricScore());
            std::this_thread::yield();
        }
    });

    // Reader thread 2: pulls history vectors
    std::thread historyReader([&]() {
        while (!stopFlag.load(std::memory_order_relaxed)) {
            const auto history = ring.getMetricHistory();
            static_cast<void>(history.size());
            std::this_thread::yield();
        }
    });

    // Reader thread 3: periodically triggers CSV export
    std::thread csvExporter([&]() {
        while (!stopFlag.load(std::memory_order_relaxed)) {
            static_cast<void>(ring.exportCsv(csvPath));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Run for 300 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stopFlag.store(true, std::memory_order_release);

    writerThread.join();
    statsReader.join();
    historyReader.join();
    csvExporter.join();

    EXPECT_GT(samplesAppended.load(), 100U);
    EXPECT_GT(ring.count(), 0);
}

TEST_F(LensConcurrencyTest, ConcurrentRingBufferStatisticalQueries)
{
    LensTelemetryRingBuffer ring;
    std::atomic<bool> stopFlag { false };
    std::atomic<std::uint32_t> writeCount { 0U };
    std::atomic<std::uint32_t> readCount { 0U };

    // 2 Writer threads
    std::vector<std::thread> writers;
    writers.reserve(2U);
    for (std::uint32_t w { 0U }; w < 2U; ++w) {
        writers.emplace_back([&, w]() {
            std::uint32_t i { 0U };
            while (!stopFlag.load(std::memory_order_relaxed)) {
                TelemetrySample s { };
                s.timestampNs = static_cast<std::uint64_t>(i) * 1000000ULL;
                s.frameIndex = i;
                s.channel = static_cast<std::uint8_t>(w);
                s.focusPosition = static_cast<std::uint16_t>(i % 65535U);
                s.metricScore = 10.0 + static_cast<double>(i % 50U);
                s.metricLatencyUs = 200U + (i % 300U);
                s.loopLatencyUs = 1000U + (i % 500U);
                ring.appendSample(s);
                ++i;
                writeCount.fetch_add(1U, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    // 2 Reader threads
    std::vector<std::thread> readers;
    readers.reserve(2U);
    for (std::uint32_t r { 0U }; r < 2U; ++r) {
        readers.emplace_back([&]() {
            while (!stopFlag.load(std::memory_order_relaxed)) {
                static_cast<void>(ring.latestMetricScore());
                static_cast<void>(ring.minMetricScore());
                static_cast<void>(ring.maxMetricScore());
                static_cast<void>(ring.avgMetricScore());
                static_cast<void>(ring.latestMetricLatencyUs());
                static_cast<void>(ring.avgMetricLatencyUs());
                readCount.fetch_add(1U, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    stopFlag.store(true, std::memory_order_release);

    for (auto& t : writers) {
        t.join();
    }
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_GT(writeCount.load(), 100U);
    EXPECT_GT(readCount.load(), 100U);
}

TEST_F(LensConcurrencyTest, ConcurrentControllerChannelSwitchingAndJogging)
{
    LensPlayerController controller;
    std::atomic<bool> stopFlag { false };

    // Thread: Query current positions concurrently
    std::thread queryThread([&]() {
        while (!stopFlag.load(std::memory_order_relaxed)) {
            static_cast<void>(controller.currentFocus());
            static_cast<void>(controller.currentZoom());
            std::this_thread::yield();
        }
    });

    // Main GUI thread: Issue jogs, channel switches, and pump Qt events
    const auto startTime = std::chrono::steady_clock::now();
    int ch { 0 };
    while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(250)) {
        controller.moveFocusStep(10);
        controller.moveZoomStep(20);
        controller.setActiveChannel(ch);
        ch = (ch + 1) % 4;
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    stopFlag.store(true, std::memory_order_release);
    queryThread.join();
}

} // namespace LensApp

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", "C:/Qt/6.6.1/msvc2019_64/plugins");
    }
    int fakeArgc = 1;
    char fakeName[] = "TestLensConcurrency";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
