/// @file TestLensTelemetryAndPresets.cpp
/// @brief Automated unit and concurrency tests for LensTelemetryRingBuffer and LensPresetManager.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariantList>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensPlayerController.h"
#include "LensPresetManager.h"
#include "LensTelemetryRingBuffer.h"

namespace LensApp {

class LensTelemetryAndPresetsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestLensTelemetryAndPresets";
            static char* argv[] = { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensTelemetryAndPresetsTest, RingBufferEmptyState)
{
    LensTelemetryRingBuffer ring;
    EXPECT_EQ(ring.capacity(), 300);
    EXPECT_EQ(ring.count(), 0);
    EXPECT_DOUBLE_EQ(ring.latestMetricScore(), 0.0);
    EXPECT_DOUBLE_EQ(ring.minMetricScore(), 0.0);
    EXPECT_DOUBLE_EQ(ring.maxMetricScore(), 0.0);
    EXPECT_DOUBLE_EQ(ring.avgMetricScore(), 0.0);
    EXPECT_EQ(ring.latestMetricLatencyUs(), 0);
    EXPECT_EQ(ring.avgMetricLatencyUs(), 0);
    EXPECT_EQ(ring.maxMetricLatencyUs(), 0);
    EXPECT_EQ(ring.latestLoopLatencyUs(), 0);
    EXPECT_EQ(ring.avgLoopLatencyUs(), 0);

    const QVariantList history = ring.getMetricHistory();
    EXPECT_TRUE(history.isEmpty());
}

TEST_F(LensTelemetryAndPresetsTest, RingBufferPushAndStatistics)
{
    LensTelemetryRingBuffer ring;
    QSignalSpy spy(&ring, &LensTelemetryRingBuffer::historyUpdated);

    // Push 3 known samples
    TelemetrySample s1 { };
    s1.timestampNs = 1000000000ULL;
    s1.frameIndex = 1U;
    s1.channel = 0U;
    s1.focusPosition = 1000U;
    s1.zoomPosition = 500U;
    s1.metricScore = 10.0;
    s1.metricLatencyUs = 200U;
    s1.loopLatencyUs = 1000U;
    ring.appendSample(s1);

    TelemetrySample s2 { };
    s2.timestampNs = 1033333333ULL;
    s2.frameIndex = 2U;
    s2.channel = 0U;
    s2.focusPosition = 1100U;
    s2.zoomPosition = 500U;
    s2.metricScore = 30.0;
    s2.metricLatencyUs = 400U;
    s2.loopLatencyUs = 2000U;
    ring.appendSample(s2);

    TelemetrySample s3 { };
    s3.timestampNs = 1066666666ULL;
    s3.frameIndex = 3U;
    s3.channel = 0U;
    s3.focusPosition = 1200U;
    s3.zoomPosition = 500U;
    s3.metricScore = 20.0;
    s3.metricLatencyUs = 300U;
    s3.loopLatencyUs = 1500U;
    ring.appendSample(s3);

    EXPECT_EQ(spy.count(), 3);
    EXPECT_EQ(ring.count(), 3);
    EXPECT_DOUBLE_EQ(ring.latestMetricScore(), 20.0);
    EXPECT_DOUBLE_EQ(ring.minMetricScore(), 10.0);
    EXPECT_DOUBLE_EQ(ring.maxMetricScore(), 30.0);
    EXPECT_DOUBLE_EQ(ring.avgMetricScore(), 20.0);
    EXPECT_EQ(ring.latestMetricLatencyUs(), 300);
    EXPECT_EQ(ring.maxMetricLatencyUs(), 400);
    EXPECT_EQ(ring.avgMetricLatencyUs(), 300);
    EXPECT_EQ(ring.latestLoopLatencyUs(), 1500);
    EXPECT_EQ(ring.avgLoopLatencyUs(), 1500);

    // Check normalized metric history
    const QVariantList history = ring.getMetricHistory();
    EXPECT_EQ(history.size(), 3);
    // Min is 10 (-> 0.0), Max is 30 (-> 1.0), Mid is 20 (-> 0.5)
    EXPECT_NEAR(history.at(0).toDouble(), 0.0, 1e-4);
    EXPECT_NEAR(history.at(1).toDouble(), 1.0, 1e-4);
    EXPECT_NEAR(history.at(2).toDouble(), 0.5, 1e-4);
}

TEST_F(LensTelemetryAndPresetsTest, RingBufferWrapAroundOverflow)
{
    LensTelemetryRingBuffer ring;

    // Push 350 samples into capacity-300 ring
    for (std::uint32_t i = 1U; i <= 350U; ++i) {
        TelemetrySample s { };
        s.timestampNs = static_cast<std::uint64_t>(i) * 33333333ULL;
        s.frameIndex = i;
        s.channel = 0U;
        s.focusPosition = static_cast<std::uint16_t>(i * 10U);
        s.zoomPosition = 1000U;
        s.metricScore = static_cast<double>(i);
        s.metricLatencyUs = i;
        s.loopLatencyUs = i * 2U;
        ring.appendSample(s);
    }

    EXPECT_EQ(ring.count(), 300);
    EXPECT_EQ(ring.capacity(), 300);

    // Latest sample is 350
    EXPECT_DOUBLE_EQ(ring.latestMetricScore(), 350.0);

    // The oldest 50 samples (1..50) were overwritten. Current range is 51..350.
    EXPECT_DOUBLE_EQ(ring.minMetricScore(), 51.0);
    EXPECT_DOUBLE_EQ(ring.maxMetricScore(), 350.0);

    // Sum from 51 to 350 = 300 * (51 + 350) / 2 = 60150. Avg = 60150 / 300 = 200.5
    EXPECT_DOUBLE_EQ(ring.avgMetricScore(), 200.5);

    EXPECT_EQ(ring.latestMetricLatencyUs(), 350);
    EXPECT_EQ(ring.maxMetricLatencyUs(), 350);
    EXPECT_EQ(ring.avgMetricLatencyUs(), 200);

    // Test clear()
    ring.clear();
    EXPECT_EQ(ring.count(), 0);
    EXPECT_DOUBLE_EQ(ring.latestMetricScore(), 0.0);
}

TEST_F(LensTelemetryAndPresetsTest, RingBufferCsvExport)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString csvPath = tempDir.path() + QStringLiteral("/test_telemetry.csv");

    LensTelemetryRingBuffer ring;

    TelemetrySample s1 { };
    s1.timestampNs = 5000000000ULL;
    s1.frameIndex = 42U;
    s1.channel = 0U;
    s1.focusPosition = 25000U;
    s1.zoomPosition = 12000U;
    s1.metricScore = 88.5;
    s1.afState = 3U;
    s1.limitFlags = 0U;
    s1.decodeLatencyUs = 1200U;
    s1.metricLatencyUs = 750U;
    s1.loopLatencyUs = 2500U;
    s1.currentFps = 59.9F;
    ring.appendSample(s1);

    EXPECT_TRUE(ring.exportCsv(csvPath));

    QFile file(csvPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    const QString header = in.readLine();
    EXPECT_TRUE(header.startsWith(QStringLiteral("TimestampNs,FrameIndex,Channel")));

    const QString dataLine = in.readLine();
    EXPECT_TRUE(dataLine.contains(QStringLiteral("5000000000,42,0,25000,12000,88.5,3,0,1200,750,2500")));
    file.close();
}

TEST_F(LensTelemetryAndPresetsTest, PresetManagerFactoryDefaults)
{
    LensPresetManager manager;
    EXPECT_EQ(manager.activeChannel(), 0);

    const QVariantList list = manager.currentChannelPresets();
    EXPECT_EQ(list.size(), 8);

    // Preset 1: Wide Surveillance
    LensPreset p1 { };
    EXPECT_TRUE(manager.getPreset(1, p1));
    EXPECT_EQ(p1.id, 1);
    EXPECT_EQ(p1.channel, 0);
    EXPECT_EQ(p1.name, QStringLiteral("Wide Surveillance"));
    EXPECT_EQ(p1.zoomPosition, 1000U);
    EXPECT_EQ(p1.focusPosition, 4200U);
    EXPECT_EQ(p1.focusSpeed, 200U);
    EXPECT_EQ(p1.zoomSpeed, 200U);
    EXPECT_TRUE(p1.isValid);

    // Preset 4: Extreme Spotter
    LensPreset p4 { };
    EXPECT_TRUE(manager.getPreset(4, p4));
    EXPECT_EQ(p4.id, 4);
    EXPECT_EQ(p4.name, QStringLiteral("Extreme Spotter"));
    EXPECT_EQ(p4.zoomPosition, 62000U);
    EXPECT_EQ(p4.focusPosition, 8600U);
    EXPECT_TRUE(p4.isValid);

    // Preset 5: Unassigned slot
    LensPreset p5 { };
    EXPECT_FALSE(manager.getPreset(5, p5));
}

TEST_F(LensTelemetryAndPresetsTest, PresetManagerSaveAndRecall)
{
    LensPresetManager manager;
    QSignalSpy spy(&manager, &LensPresetManager::presetsChanged);

    EXPECT_TRUE(manager.savePreset(5, QStringLiteral("Perimeter Sentry"), 32000, 15000, 180, 180, 1, 0));

    EXPECT_GE(spy.count(), 1);

    LensPreset saved { };
    EXPECT_TRUE(manager.getPreset(5, saved));
    EXPECT_EQ(saved.id, 5);
    EXPECT_EQ(saved.name, QStringLiteral("Perimeter Sentry"));
    EXPECT_EQ(saved.focusPosition, 32000U);
    EXPECT_EQ(saved.zoomPosition, 15000U);
    EXPECT_EQ(saved.focusSpeed, 180U);
    EXPECT_EQ(saved.zoomSpeed, 180U);
    EXPECT_TRUE(saved.isValid);

    // Delete preset 5
    EXPECT_TRUE(manager.deletePreset(5));
    LensPreset deleted { };
    EXPECT_FALSE(manager.getPreset(5, deleted));
}

TEST_F(LensTelemetryAndPresetsTest, PresetManagerChannelIsolation)
{
    LensPresetManager manager;

    // Mutate Channel 0 Preset 1
    EXPECT_TRUE(manager.savePreset(1, QStringLiteral("Custom Ch0"), 5000, 2000, 100, 100, 1, 0));

    // Switch to Channel 1
    manager.setActiveChannel(1);
    EXPECT_EQ(manager.activeChannel(), 1);

    // Verify Channel 1 Preset 1 retains its own factory default
    LensPreset p1Ch1 { };
    EXPECT_TRUE(manager.getPreset(1, p1Ch1));
    EXPECT_EQ(p1Ch1.channel, 1);
    EXPECT_EQ(p1Ch1.name, QStringLiteral("Wide Surveillance"));
    EXPECT_EQ(p1Ch1.focusPosition, 4200U);

    // Switch back to Channel 0 and verify custom name persists
    manager.setActiveChannel(0);
    LensPreset p1Ch0 { };
    EXPECT_TRUE(manager.getPreset(1, p1Ch0));
    EXPECT_EQ(p1Ch0.name, QStringLiteral("Custom Ch0"));
}

TEST_F(LensTelemetryAndPresetsTest, PresetJsonRoundTrip)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString jsonPath = tempDir.path() + QStringLiteral("/presets.json");

    LensPresetManager managerA;
    EXPECT_TRUE(managerA.savePreset(5, QStringLiteral("Target Alpha"), 28000, 42000, 150, 150, 2, 1));
    EXPECT_TRUE(managerA.exportJson(jsonPath));

    LensPresetManager managerB;
    // Before import, slot 5 is invalid
    LensPreset before { };
    EXPECT_FALSE(managerB.getPreset(5, before));

    EXPECT_TRUE(managerB.importJson(jsonPath));

    LensPreset after { };
    EXPECT_TRUE(managerB.getPreset(5, after));
    EXPECT_EQ(after.name, QStringLiteral("Target Alpha"));
    EXPECT_EQ(after.focusPosition, 28000U);
    EXPECT_EQ(after.zoomPosition, 42000U);
    EXPECT_EQ(after.focusSpeed, 150U);
    EXPECT_EQ(after.zoomSpeed, 150U);
    EXPECT_EQ(after.afAlgorithm, 2);
    EXPECT_EQ(after.focusMetric, 1);
}

TEST_F(LensTelemetryAndPresetsTest, ControllerPresetAndTelemetryIntegration)
{
    LensPlayerController controller;

    EXPECT_NE(controller.telemetryBuffer(), nullptr);
    EXPECT_NE(controller.presetManager(), nullptr);

    // Test preset recall on controller
    controller.recallPreset(1);
    EXPECT_EQ(controller.focusSpeed(), 200);
    EXPECT_EQ(controller.zoomSpeed(), 200);

    // Test saving current position to preset slot 6
    controller.saveCurrentAsPreset(6, QStringLiteral("Quick Slot 6"));
    LensPreset p6 { };
    EXPECT_TRUE(controller.presetManager()->getPreset(6, p6));
    EXPECT_EQ(p6.name, QStringLiteral("Quick Slot 6"));

    // Test clearing telemetry buffer through controller
    controller.clearTelemetryBuffer();
    EXPECT_EQ(controller.telemetryBuffer()->count(), 0);
}

TEST_F(LensTelemetryAndPresetsTest, MultiThreadedRingBufferStress)
{
    LensTelemetryRingBuffer ring;
    std::atomic<bool> stopFlag { false };
    std::atomic<std::size_t> pushedCount { 0U };

    // Producer thread: pushes samples at ~200 Hz
    std::thread producer([&ring, &stopFlag, &pushedCount]() {
        std::uint32_t frame = 0U;
        while (!stopFlag.load(std::memory_order_relaxed)) {
            TelemetrySample s { };
            s.timestampNs = static_cast<std::uint64_t>(frame) * 5000000ULL;
            s.frameIndex = ++frame;
            s.focusPosition = static_cast<std::uint16_t>(frame % 65535U);
            s.zoomPosition = static_cast<std::uint16_t>((frame * 2U) % 65535U);
            s.metricScore = static_cast<double>(frame % 100U);
            s.metricLatencyUs = 500U;
            s.loopLatencyUs = 2000U;
            ring.appendSample(s);
            pushedCount.fetch_add(1U, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Consumer thread: reads statistics and history repeatedly
    for (int i = 0; i < 50; ++i) {
        const double latest = ring.latestMetricScore();
        (void)latest;
        const double avg = ring.avgMetricScore();
        (void)avg;
        const QVariantList hist = ring.getMetricHistory();
        EXPECT_LE(hist.size(), 300);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    stopFlag.store(true, std::memory_order_relaxed);
    producer.join();

    EXPECT_GT(pushedCount.load(), 50U);
    EXPECT_LE(ring.count(), 300);
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
    char fakeName[] = "TestLensTelemetryAndPresets";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
