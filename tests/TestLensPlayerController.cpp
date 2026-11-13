/// @file TestLensPlayerController.cpp
/// @brief Tests for LensPlayerController, LensWorker, and MockLensDriver integration.

#include <cstdint>

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensPlayerController.h"
#include "LensVideoQuickItem.h"
#include "PresentationTypes.h"

namespace LensApp {

class LensPlayerControllerTest : public ::testing::Test { };

TEST_F(LensPlayerControllerTest, InitialChannelState)
{
    LensPlayerController controller;

    EXPECT_EQ(controller.activeChannel(), 0);
    EXPECT_EQ(controller.lensMode(), 0);
    EXPECT_EQ(controller.focusMin(), 0);
    EXPECT_EQ(controller.focusMax(), 65535);
    EXPECT_EQ(controller.zoomMin(), 0);
    EXPECT_EQ(controller.zoomMax(), 65535);
    EXPECT_FALSE(controller.isAfRunning());
    EXPECT_NE(controller.configModel(), nullptr);
}

TEST_F(LensPlayerControllerTest, ConfigModelDefaults)
{
    LensPlayerController controller;
    auto* model = controller.configModel();
    ASSERT_NE(model, nullptr);

    EXPECT_EQ(model->defaultZoomSpeed(), 127);
    EXPECT_EQ(model->defaultFocusSpeed(), 127);
    EXPECT_EQ(model->microStepSize(), 50);
    EXPECT_EQ(model->channel0Name(), QStringLiteral("Ch 0: EO Wide"));
    EXPECT_EQ(model->channel1Name(), QStringLiteral("Ch 1: Spotter Tele"));
    EXPECT_EQ(model->channel2Name(), QStringLiteral("Ch 2: Thermal LWIR"));
    EXPECT_EQ(model->channel3Name(), QStringLiteral("Ch 3: SWIR Optical"));

    model->setDefaultZoomSpeed(200);
    EXPECT_EQ(model->defaultZoomSpeed(), 200);

    model->resetDefaults();
    EXPECT_EQ(model->defaultZoomSpeed(), 127);
}

TEST_F(LensPlayerControllerTest, ChannelSwitching)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::activeChannelChanged);

    controller.setActiveChannel(2);
    EXPECT_EQ(controller.activeChannel(), 2);
    EXPECT_EQ(spy.count(), 1);

    // Clamp out-of-range
    controller.setActiveChannel(99);
    EXPECT_EQ(controller.activeChannel(), 3);
}

TEST_F(LensPlayerControllerTest, LensModeSwitching)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::lensModeChanged);

    controller.setLensMode(1);
    EXPECT_EQ(controller.lensMode(), 1);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(LensPlayerControllerTest, MoveStepActuation)
{
    LensPlayerController controller;

    // Initial positions should be 0
    QTest::qWait(80);

    controller.moveFocusStep(100);
    controller.moveZoomStep(150);

    // Allow worker tick to process movement in MockLensDriver
    QTest::qWait(120);

    EXPECT_EQ(controller.currentFocus(), 100);
    EXPECT_EQ(controller.currentZoom(), 150);
}

TEST_F(LensPlayerControllerTest, AbsoluteTargetActuation)
{
    LensPlayerController controller;

    controller.setFocusTarget(1200);
    controller.setZoomTarget(3400);

    QTest::qWait(120);

    EXPECT_EQ(controller.currentFocus(), 1200);
    EXPECT_EQ(controller.currentZoom(), 3400);
}

TEST_F(LensPlayerControllerTest, EmergencyStop)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::statusMessageChanged);

    controller.emergencyStop();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(controller.statusMessage(), QStringLiteral("EMERGENCY STOP EXECUTED"));
}

TEST_F(LensPlayerControllerTest, VideoQuickItemProperties)
{
    LensVideoQuickItem item;
    LensPlayerController controller;

    item.setController(&controller);
    EXPECT_EQ(item.controller(), &controller);

    EXPECT_TRUE(item.showGrid());
    item.setShowGrid(false);
    EXPECT_FALSE(item.showGrid());

    EXPECT_TRUE(item.showReticle());
    item.setShowReticle(false);
    EXPECT_FALSE(item.showReticle());

    EXPECT_TRUE(item.showRoi());
    item.setShowRoi(false);
    EXPECT_FALSE(item.showRoi());
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
    char fakeName[] = "TestLensPlayerController";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
