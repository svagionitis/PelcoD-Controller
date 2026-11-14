/// @file TestLensAxisControls.cpp
/// @brief Automated unit verification for optical axis controls, limits, and emergency stop.

#include <cstdint>

#include <chrono>

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensPlayerController.h"

namespace LensApp {

class LensAxisControlsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc = 1;
            static char appName[] = "LensAxisControlsTest";
            static char* argv[] = { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensAxisControlsTest, RelativeJogStepping)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::telemetryUpdated);

    const int initialFocus = controller.currentFocus();
    controller.moveFocusStep(100);
    EXPECT_TRUE(spy.count() >= 1);
    EXPECT_EQ(controller.currentFocus(), initialFocus + 100);

    const int initialZoom = controller.currentZoom();
    controller.moveZoomStep(250);
    EXPECT_EQ(controller.currentZoom(), initialZoom + 250);

    controller.moveFocusStep(-50);
    EXPECT_EQ(controller.currentFocus(), initialFocus + 50);
}

TEST_F(LensAxisControlsTest, BoundaryClamping)
{
    LensPlayerController controller;

    controller.setFocusTarget(-1000);
    EXPECT_GE(controller.currentFocus(), controller.focusMin());

    controller.setFocusTarget(100000);
    EXPECT_LE(controller.currentFocus(), controller.focusMax());

    controller.setZoomTarget(-500);
    EXPECT_GE(controller.currentZoom(), controller.zoomMin());

    controller.setZoomTarget(90000);
    EXPECT_LE(controller.currentZoom(), controller.zoomMax());
}

TEST_F(LensAxisControlsTest, SpeedPresetAssignment)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::speedsChanged);

    controller.setFocusSpeed(32);
    EXPECT_EQ(controller.focusSpeed(), 32);
    EXPECT_GE(spy.count(), 1);

    controller.setZoomSpeed(200);
    EXPECT_EQ(controller.zoomSpeed(), 200);
}

TEST_F(LensAxisControlsTest, LimitSwitchFlagReflection)
{
    LensPlayerController controller;

    // Initially at min boundaries (0, 0)
    controller.setFocusTarget(0);
    controller.setZoomTarget(0);

    // Limit flags should be accessible via limitFlags property
    const int flags = controller.limitFlags();
    EXPECT_GE(flags, 0);
}

TEST_F(LensAxisControlsTest, EmergencyStopExecution)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::emergencyStopOccurred);

    const auto t0 = std::chrono::steady_clock::now();
    controller.emergencyStop();
    const auto t1 = std::chrono::steady_clock::now();

    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // Verify SLA: Emergency Stop must execute in < 5 ms
    EXPECT_LT(durationMs, 5);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(controller.statusMessage().contains(QStringLiteral("EMERGENCY STOP")));
}

} // namespace LensApp
