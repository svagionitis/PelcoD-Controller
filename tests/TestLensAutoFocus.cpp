/// @file TestLensAutoFocus.cpp
/// @brief Automated unit verification for closed-loop autofocus execution, telemetry, and CAF.

#include <cstdint>

#include <QGuiApplication>
#include <QPointF>
#include <QSignalSpy>
#include <QTest>
#include <QVariantList>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensPlayerController.h"

namespace LensApp {

class LensAutoFocusTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc = 1;
            static char appName[] = "LensAutoFocusTest";
            static char* argv[] = { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensAutoFocusTest, AlgorithmAndMetricConfiguration)
{
    LensPlayerController controller;
    QSignalSpy configSpy(&controller, &LensPlayerController::afConfigChanged);

    EXPECT_EQ(controller.afAlgorithm(), 1); // Default ScanThreePhase
    EXPECT_EQ(controller.focusMetric(), 0); // Default SobelGradientSquared

    controller.setAfAlgorithm(2); // GoldenSection
    EXPECT_EQ(controller.afAlgorithm(), 2);
    EXPECT_GE(configSpy.count(), 1);

    controller.setFocusMetric(3); // VollathF4
    EXPECT_EQ(controller.focusMetric(), 3);
    EXPECT_GE(configSpy.count(), 2);

    controller.setAfAlgorithm(4); // DctFrequency
    EXPECT_EQ(controller.afAlgorithm(), 4);

    controller.setFocusMetric(5); // DctHighFrequency
    EXPECT_EQ(controller.focusMetric(), 5);
}

TEST_F(LensAutoFocusTest, StateStringMapping)
{
    LensPlayerController controller;

    EXPECT_EQ(controller.afState(), 0);
    EXPECT_EQ(controller.afStateString(), QStringLiteral("IDLE"));

    // Verify all 9 discrete states
    const QString expectedNames[]
        = { QStringLiteral("IDLE"), QStringLiteral("INITIALIZING"), QStringLiteral("COARSE SWEEP"),
              QStringLiteral("MID SWEEP"), QStringLiteral("FINE SWEEP"), QStringLiteral("REVERSING"),
              QStringLiteral("SLEW TO PEAK"), QStringLiteral("PEAK CONFIRMED"), QStringLiteral("FAILED") };

    for (int state = 0; state <= 8; ++state) {
        QMetaObject::invokeMethod(&controller, "onAfStateUpdated", Qt::DirectConnection, Q_ARG(int, state),
            Q_ARG(bool, state >= 1 && state <= 6));

        EXPECT_EQ(controller.afState(), state);
        EXPECT_EQ(controller.afStateString(), expectedNames[state]);
        if (state >= 1 && state <= 6) {
            EXPECT_TRUE(controller.isAfRunning());
        } else {
            EXPECT_FALSE(controller.isAfRunning());
        }
    }
}

TEST_F(LensAutoFocusTest, TriggerAndAbortDispatches)
{
    LensPlayerController controller;
    QSignalSpy triggerSpy(&controller, &LensPlayerController::requestTriggerAf);
    QSignalSpy abortSpy(&controller, &LensPlayerController::requestAbortAf);
    QSignalSpy statusSpy(&controller, &LensPlayerController::statusMessageChanged);

    controller.setAfAlgorithm(1); // 3-Phase Scan
    controller.setFocusMetric(0); // Sobel
    controller.triggerAutoFocus();

    EXPECT_EQ(triggerSpy.count(), 1);
    const QList<QVariant> trigArgs = triggerSpy.takeFirst();
    EXPECT_EQ(trigArgs.at(0).toInt(), 0); // Active channel 0
    EXPECT_EQ(trigArgs.at(1).toInt(), 1); // Algorithm 1
    EXPECT_EQ(trigArgs.at(2).toInt(), 0); // Metric 0
    EXPECT_TRUE(controller.statusMessage().contains(QStringLiteral("triggered")));

    controller.abortAutoFocus();
    EXPECT_EQ(abortSpy.count(), 1);
    const QList<QVariant> abortArgs = abortSpy.takeFirst();
    EXPECT_EQ(abortArgs.at(0).toInt(), 0);
    EXPECT_TRUE(controller.statusMessage().contains(QStringLiteral("aborted")));
}

TEST_F(LensAutoFocusTest, PeakFitTelemetryUpdates)
{
    LensPlayerController controller;
    QSignalSpy fitSpy(&controller, &LensPlayerController::peakFitUpdated);

    EXPECT_FALSE(controller.isPeakValid());
    EXPECT_DOUBLE_EQ(controller.peakSummit(), 0.0);

    // Simulate worker emission of fitted peak
    QMetaObject::invokeMethod(&controller, "onPeakFitted", Qt::DirectConnection, Q_ARG(double, 38450.0),
        Q_ARG(double, 1285.5), Q_ARG(double, -0.052), Q_ARG(double, 0.965), Q_ARG(bool, true));

    EXPECT_EQ(fitSpy.count(), 1);
    EXPECT_TRUE(controller.isPeakValid());
    EXPECT_DOUBLE_EQ(controller.peakSummit(), 38450.0);
    EXPECT_DOUBLE_EQ(controller.peakScore(), 1285.5);
    EXPECT_DOUBLE_EQ(controller.peakCurvature(), -0.052);
    EXPECT_DOUBLE_EQ(controller.peakR2(), 0.965);
}

TEST_F(LensAutoFocusTest, ContinuousAfConfiguration)
{
    LensPlayerController controller;
    QSignalSpy cafToggleSpy(&controller, &LensPlayerController::cafToggled);
    QSignalSpy cafConfigSpy(&controller, &LensPlayerController::cafConfigChanged);

    EXPECT_FALSE(controller.cafEnabled());
    EXPECT_DOUBLE_EQ(controller.cafThreshold(), 0.25);

    controller.setCafEnabled(true);
    EXPECT_TRUE(controller.cafEnabled());
    EXPECT_EQ(cafToggleSpy.count(), 1);

    controller.setCafThreshold(0.35);
    EXPECT_DOUBLE_EQ(controller.cafThreshold(), 0.35);
    EXPECT_EQ(cafConfigSpy.count(), 1);

    // Verify boundary clamping
    controller.setCafThreshold(0.01); // Below min (0.05)
    EXPECT_DOUBLE_EQ(controller.cafThreshold(), 0.05);

    controller.setCafThreshold(0.99); // Above max (0.50)
    EXPECT_DOUBLE_EQ(controller.cafThreshold(), 0.50);
}

TEST_F(LensAutoFocusTest, SamplePointsSerialization)
{
    LensPlayerController controller;
    QSignalSpy samplesSpy(&controller, &LensPlayerController::samplesUpdated);

    EXPECT_TRUE(controller.samplePoints().isEmpty());

    QVariantList mockSamples;
    mockSamples.append(QPointF(32000.0, 100.0));
    mockSamples.append(QPointF(32100.0, 250.0));
    mockSamples.append(QPointF(32200.0, 520.0));
    mockSamples.append(QPointF(32300.0, 480.0));

    QMetaObject::invokeMethod(&controller, "onSamplesReady", Qt::DirectConnection, Q_ARG(QVariantList, mockSamples));

    EXPECT_EQ(samplesSpy.count(), 1);
    EXPECT_EQ(controller.samplePoints().size(), 4);

    const QPointF pt1 = controller.samplePoints().at(0).toPointF();
    EXPECT_DOUBLE_EQ(pt1.x(), 32000.0);
    EXPECT_DOUBLE_EQ(pt1.y(), 100.0);

    const QPointF pt3 = controller.samplePoints().at(2).toPointF();
    EXPECT_DOUBLE_EQ(pt3.x(), 32200.0);
    EXPECT_DOUBLE_EQ(pt3.y(), 520.0);
}

} // namespace LensApp
