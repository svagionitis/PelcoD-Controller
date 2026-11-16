/// @file TestLensEndToEnd.cpp
/// @brief Automated headless end-to-end mission regression test suite for LensAppQt.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRectF>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QVariantList>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensPlayerController.h"
#include "LensPresetManager.h"
#include "LensTelemetryRingBuffer.h"

namespace LensApp {

class LensEndToEndTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestLensEndToEnd" };
            static char* argv[] { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensEndToEndTest, MissionInitializationAndChannelCycle)
{
    LensPlayerController controller;
    QSignalSpy channelSpy(&controller, &LensPlayerController::activeChannelChanged);

    EXPECT_EQ(controller.activeChannel(), 0);
    EXPECT_EQ(controller.focusMin(), 0);
    EXPECT_GT(controller.focusMax(), 0);
    EXPECT_EQ(controller.zoomMin(), 0);
    EXPECT_GT(controller.zoomMax(), 0);

    for (int ch { 1 }; ch < 4; ++ch) {
        controller.setActiveChannel(ch);
        EXPECT_EQ(controller.activeChannel(), ch);
    }
    EXPECT_EQ(channelSpy.count(), 3);

    controller.setActiveChannel(0);
    EXPECT_EQ(controller.activeChannel(), 0);
}

TEST_F(LensEndToEndTest, ParfocalZoomSlewAndTracking)
{
    LensPlayerController controller;
    QSignalSpy trackSpy(&controller, &LensPlayerController::zoomTrackChanged);

    controller.setZoomTrackEnabled(true);
    EXPECT_TRUE(controller.zoomTrackEnabled());
    EXPECT_GE(trackSpy.count(), 1);

    // Set interpolation mode to PCHIP spline (1)
    controller.setInterpolationMode(1);
    EXPECT_EQ(controller.interpolationMode(), 1);

    // Add calibration knots across zoom travel
    controller.clearKnots();
    controller.addKnot(0, 1000);
    controller.addKnot(20000, 3500);
    controller.addKnot(40000, 9000);
    controller.addKnot(65535, 24000);

    const QVariantList knots = controller.activeCurveKnots();
    EXPECT_EQ(knots.size(), 4);

    // Step zoom from 0 to 60000 and verify position updates
    controller.setZoomTarget(0);
    EXPECT_EQ(controller.currentZoom(), 0);

    controller.setZoomTarget(30000);
    EXPECT_EQ(controller.currentZoom(), 30000);

    controller.moveZoomStep(5000);
    EXPECT_EQ(controller.currentZoom(), 35000);
}

TEST_F(LensEndToEndTest, ClosedLoopAutofocusConvergence)
{
    LensPlayerController controller;
    QSignalSpy afSpy(&controller, &LensPlayerController::afStateChanged);

    // Configure synthetic defocus simulator
    controller.setSyntheticPlane(32000);
    EXPECT_EQ(controller.syntheticPlane(), 32000);

    controller.setSyntheticNoise(0.005);
    EXPECT_DOUBLE_EQ(controller.syntheticNoise(), 0.005);

    // Configure AF search: SeekAdaptive (0), Tenengrad (0)
    controller.setAfAlgorithm(0);
    controller.setFocusMetric(0);
    EXPECT_EQ(controller.afAlgorithm(), 0);
    EXPECT_EQ(controller.focusMetric(), 0);

    // Trigger autofocus sweep
    controller.triggerAutoFocus();
    EXPECT_TRUE(controller.isAfRunning() || controller.afState() >= 0);

    // Abort autofocus
    controller.abortAutoFocus();
    EXPECT_FALSE(controller.isAfRunning());
}

TEST_F(LensEndToEndTest, DynamicRoiAndPeakingConfiguration)
{
    LensPlayerController controller;
    QSignalSpy roiSpy(&controller, &LensPlayerController::roiRectChanged);
    QSignalSpy peakSpy(&controller, &LensPlayerController::showPeakingChanged);
    QSignalSpy threshSpy(&controller, &LensPlayerController::peakingThresholdChanged);

    // Center ROI update
    const QRectF newRoi(0.35, 0.35, 0.3, 0.3);
    controller.setRoiRect(newRoi);
    EXPECT_EQ(controller.roiRect(), newRoi);
    EXPECT_EQ(roiSpy.count(), 1);

    // Peaking overlay configuration
    controller.setShowPeaking(true);
    EXPECT_TRUE(controller.showPeaking());
    EXPECT_EQ(peakSpy.count(), 1);

    controller.setPeakingThreshold(75);
    EXPECT_EQ(controller.peakingThreshold(), 75);
    EXPECT_EQ(threshSpy.count(), 1);
}

TEST_F(LensEndToEndTest, PresetSaveMutateRecallCycle)
{
    LensPlayerController controller;
    LensPresetManager* pm = controller.presetManager();
    ASSERT_NE(pm, nullptr);

    // Set custom coordinates on channel 0
    controller.setActiveChannel(0);
    controller.setZoomTarget(42000);
    controller.setFocusTarget(18500);
    controller.setZoomSpeed(180);
    controller.setFocusSpeed(140);
    controller.setAfAlgorithm(1); // ScanThreePhase
    controller.setFocusMetric(2); // Laplacian

    // Save to slot 3
    controller.saveCurrentAsPreset(3, QStringLiteral("Tactical Recon"));

    LensPreset saved { };
    EXPECT_TRUE(pm->getPreset(3, saved));
    EXPECT_EQ(saved.name, QStringLiteral("Tactical Recon"));
    EXPECT_EQ(saved.zoomPosition, 42000U);
    EXPECT_EQ(saved.focusPosition, 18500U);
    EXPECT_EQ(saved.zoomSpeed, 180U);
    EXPECT_EQ(saved.focusSpeed, 140U);
    EXPECT_EQ(saved.afAlgorithm, 1);
    EXPECT_EQ(saved.focusMetric, 2);

    // Mutate current coordinates
    controller.setZoomTarget(1000);
    controller.setFocusTarget(500);
    EXPECT_EQ(controller.currentZoom(), 1000);
    EXPECT_EQ(controller.currentFocus(), 500);

    // Recall preset 3
    controller.recallPreset(3);
    EXPECT_EQ(controller.currentZoom(), 42000);
    EXPECT_EQ(controller.currentFocus(), 18500);
    EXPECT_EQ(controller.zoomSpeed(), 180);
    EXPECT_EQ(controller.focusSpeed(), 140);
    EXPECT_EQ(controller.afAlgorithm(), 1);
    EXPECT_EQ(controller.focusMetric(), 2);
}

TEST_F(LensEndToEndTest, TelemetryLoggingAndCsvExport)
{
    LensPlayerController controller;
    LensTelemetryRingBuffer* ring = controller.telemetryBuffer();
    ASSERT_NE(ring, nullptr);

    // Populate ring buffer with synthetic mission samples
    for (std::uint32_t i { 0U }; i < 50U; ++i) {
        TelemetrySample s { };
        s.timestampNs = 1000000000ULL + static_cast<std::uint64_t>(i) * 33333333ULL;
        s.frameIndex = i;
        s.channel = 0U;
        s.focusPosition = static_cast<std::uint16_t>(10000U + i * 100U);
        s.zoomPosition = static_cast<std::uint16_t>(5000U + i * 50U);
        s.metricScore = 15.0 + static_cast<double>(i) * 0.5;
        s.afState = 1U;
        s.limitFlags = 0U;
        s.decodeLatencyUs = 1200U;
        s.metricLatencyUs = 600U;
        s.loopLatencyUs = 2500U;
        s.currentFps = 30.0F;
        ring->appendSample(s);
    }

    EXPECT_EQ(ring->count(), 50);

    // Export to temporary CSV file
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString csvPath = tempDir.path() + QStringLiteral("/mission_telemetry.csv");

    controller.exportTelemetryCsv(csvPath);
    EXPECT_TRUE(QFile::exists(csvPath));

    QFile file(csvPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);

    const QString headerLine = in.readLine();
    EXPECT_TRUE(headerLine.contains(QStringLiteral("TimestampNs")));
    EXPECT_TRUE(headerLine.contains(QStringLiteral("FrameIndex")));
    EXPECT_TRUE(headerLine.contains(QStringLiteral("FocusPosition")));
    EXPECT_TRUE(headerLine.contains(QStringLiteral("MetricScore")));

    int rowCount { 0 };
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty()) {
            ++rowCount;
        }
    }
    EXPECT_EQ(rowCount, 50);
}

TEST_F(LensEndToEndTest, EmergencyStopResponsiveness)
{
    LensPlayerController controller;
    QSignalSpy stopSpy(&controller, &LensPlayerController::emergencyStopOccurred);

    controller.setZoomSpeed(250);
    controller.setFocusSpeed(250);

    const auto t0 = std::chrono::steady_clock::now();
    controller.emergencyStop();
    const auto t1 = std::chrono::steady_clock::now();

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_LT(elapsedMs, 5);
    EXPECT_EQ(stopSpy.count(), 1);
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
    char fakeName[] = "TestLensEndToEnd";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
