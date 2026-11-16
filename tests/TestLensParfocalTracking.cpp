/// @file TestLensParfocalTracking.cpp
/// @brief Unit tests for Parfocal Zoom-to-Focus calibration, PCHIP splines, and UI controller.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <gtest/gtest.h>

#include "LensConfigModel.h"
#include "LensController.h"
#include "LensPlayerController.h"
#include "LensTypes.h"
#include "MockLensDriver.h"
#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

namespace LensApp {

class LensParfocalTrackingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestLensParfocalTracking";
            static char* argv[] = { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensParfocalTrackingTest, PchipSplineMonotonicity)
{
    using namespace LensControl;

    TrackCurve curve { };
    curve.knotCount = 4U;
    curve.distanceMeters = 5.0F;

    // Step/plateau profile: 0..200 (flat), 200..400 (ramp), 400..600 (flat)
    curve.knots[0].zoom = MotorPosition { 0U };
    curve.knots[0].focus = MotorPosition { 200U };
    curve.knots[1].zoom = MotorPosition { 200U };
    curve.knots[1].focus = MotorPosition { 200U };
    curve.knots[2].zoom = MotorPosition { 400U };
    curve.knots[2].focus = MotorPosition { 1000U };
    curve.knots[3].zoom = MotorPosition { 600U };
    curve.knots[3].focus = MotorPosition { 1000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));
    engine.setMode(InterpolationMode::PchipSpline);

    // Verify plateau [0..200] is strictly flat at 200 (zero slope, zero dip)
    for (std::uint32_t z = 0U; z <= 200U; z += 20U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { static_cast<std::uint16_t>(z) });
        EXPECT_EQ(f.value(), 200U);
    }

    // Verify monotonic climb [200..400]
    std::uint16_t prevF = 200U;
    for (std::uint32_t z = 200U; z <= 400U; z += 10U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { static_cast<std::uint16_t>(z) });
        EXPECT_GE(f.value(), prevF);
        EXPECT_LE(f.value(), 1000U);
        prevF = f.value();
    }
    EXPECT_EQ(prevF, 1000U);

    // Verify plateau [400..600] is strictly flat at 1000 (zero overshoot)
    for (std::uint32_t z = 400U; z <= 600U; z += 20U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { static_cast<std::uint16_t>(z) });
        EXPECT_EQ(f.value(), 1000U);
    }
}

TEST_F(LensParfocalTrackingTest, PiecewiseLinearFallback)
{
    using namespace LensControl;

    TrackCurve curve { };
    curve.knotCount = 2U;
    curve.knots[0].zoom = MotorPosition { 0U };
    curve.knots[0].focus = MotorPosition { 1000U };
    curve.knots[1].zoom = MotorPosition { 10000U };
    curve.knots[1].focus = MotorPosition { 3000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));
    engine.setMode(InterpolationMode::PiecewiseLinear);

    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 0U }).value(), 1000U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 5000U }).value(), 2000U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 10000U }).value(), 3000U);
}

TEST_F(LensParfocalTrackingTest, MultiDistanceVergence)
{
    using namespace LensControl;

    ZoomTrackFocusEngine engine;

    TrackCurve curveNear { };
    curveNear.distanceMeters = 5.0F; // V = 0.20
    curveNear.knotCount = 2U;
    curveNear.knots[0].zoom = MotorPosition { 0U };
    curveNear.knots[0].focus = MotorPosition { 1000U };
    curveNear.knots[1].zoom = MotorPosition { 10000U };
    curveNear.knots[1].focus = MotorPosition { 2000U };
    EXPECT_TRUE(engine.addCurve(curveNear));

    TrackCurve curveFar { };
    curveFar.distanceMeters = 100.0F; // V = 0.01
    curveFar.knotCount = 2U;
    curveFar.knots[0].zoom = MotorPosition { 0U };
    curveFar.knots[0].focus = MotorPosition { 500U };
    curveFar.knots[1].zoom = MotorPosition { 10000U };
    curveFar.knots[1].focus = MotorPosition { 1000U };
    EXPECT_TRUE(engine.addCurve(curveFar));

    // Direct near evaluation
    EXPECT_EQ(engine.evaluateDistance(MotorPosition { 0U }, 5.0F).value(), 1000U);
    EXPECT_EQ(engine.evaluateDistance(MotorPosition { 10000U }, 5.0F).value(), 2000U);

    // Direct far evaluation
    EXPECT_EQ(engine.evaluateDistance(MotorPosition { 0U }, 100.0F).value(), 500U);
    EXPECT_EQ(engine.evaluateDistance(MotorPosition { 10000U }, 100.0F).value(), 1000U);

    // Intermediate distance (10.0m)
    const std::uint16_t fMid0 = engine.evaluateDistance(MotorPosition { 0U }, 10.0F).value();
    const std::uint16_t fMid10k = engine.evaluateDistance(MotorPosition { 10000U }, 10.0F).value();

    EXPECT_GT(fMid0, 500U);
    EXPECT_LT(fMid0, 1000U);
    EXPECT_GT(fMid10k, 1000U);
    EXPECT_LT(fMid10k, 2000U);
}

TEST_F(LensParfocalTrackingTest, SerializationRoundTrip)
{
    using namespace LensControl;

    TrackCurve originalCurve { };
    originalCurve.distanceMeters = 25.0F;
    originalCurve.knotCount = 5U;
    originalCurve.knots[0] = { MotorPosition { 0U }, MotorPosition { 1200U }, 0.0F };
    originalCurve.knots[1] = { MotorPosition { 5000U }, MotorPosition { 2400U }, 0.0F };
    originalCurve.knots[2] = { MotorPosition { 12000U }, MotorPosition { 3600U }, 0.0F };
    originalCurve.knots[3] = { MotorPosition { 25000U }, MotorPosition { 4800U }, 0.0F };
    originalCurve.knots[4] = { MotorPosition { 65535U }, MotorPosition { 6000U }, 0.0F };

    ZoomTrackFocusEngine saveEngine;
    EXPECT_TRUE(saveEngine.addCurve(originalCurve));

    const std::string text = saveEngine.serializeString();
    EXPECT_FALSE(text.empty());

    ZoomTrackFocusEngine loadEngine;
    const TrackCurveError err = loadEngine.parseString(text);
    EXPECT_EQ(err, TrackCurveError::None);
    EXPECT_EQ(loadEngine.curveCount(), 1U);

    const TrackCurve* parsed = loadEngine.getCurve(0U);
    ASSERT_NE(parsed, nullptr);
    EXPECT_TRUE(parsed->isValid());
    EXPECT_EQ(parsed->knotCount, originalCurve.knotCount);

    for (std::size_t i = 0U; i < originalCurve.knotCount; ++i) {
        EXPECT_EQ(parsed->knots[i].zoom.value(), originalCurve.knots[i].zoom.value());
        EXPECT_EQ(parsed->knots[i].focus.value(), originalCurve.knots[i].focus.value());
    }
}

TEST_F(LensParfocalTrackingTest, ControllerInitialParfocalState)
{
    LensPlayerController controller;

    EXPECT_FALSE(controller.zoomTrackEnabled());
    EXPECT_EQ(controller.activeCurveIndex(), 0);
    EXPECT_FLOAT_EQ(static_cast<float>(controller.activeCurveDistance()), 0.0F);
    EXPECT_FALSE(controller.isCalibrating());
    EXPECT_EQ(controller.calibStep(), 0);
    EXPECT_EQ(controller.calibTotalSteps(), 0);
    EXPECT_DOUBLE_EQ(controller.calibProgress(), 0.0);
}

TEST_F(LensParfocalTrackingTest, ControllerToggleZoomTrack)
{
    LensPlayerController controller;
    QSignalSpy spy(&controller, &LensPlayerController::zoomTrackChanged);

    controller.setZoomTrackEnabled(true);
    EXPECT_TRUE(controller.zoomTrackEnabled());
    EXPECT_EQ(spy.count(), 1);

    controller.setZoomTrackEnabled(false);
    EXPECT_FALSE(controller.zoomTrackEnabled());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(LensParfocalTrackingTest, ControllerKnotTableEditing)
{
    LensPlayerController controller;
    QSignalSpy knotSpy(&controller, &LensPlayerController::knotsUpdated);

    // Allow worker thread to spin up and load default curve
    QTest::qWait(150);

    const auto initialCount = knotSpy.count();

    // Knot manipulation - add knot at unique zoom position (e.g. 500)
    controller.addKnot(500, 2000);
    QTest::qWait(150);
    EXPECT_GT(knotSpy.count(), initialCount);
    EXPECT_FALSE(controller.activeCurveKnots().isEmpty());

    controller.updateKnot(0, 500, 2500);
    QTest::qWait(150);

    controller.removeKnot(0);
    QTest::qWait(150);

    controller.clearKnots();
    QTest::qWait(150);
    EXPECT_TRUE(controller.activeCurveKnots().isEmpty());
}

TEST_F(LensParfocalTrackingTest, WizardStartAndCancelFlow)
{
    LensPlayerController controller;
    QSignalSpy stateSpy(&controller, &LensPlayerController::calibStateChanged);

    controller.startCalibWizard(4, 1000, 10000, 25.0);
    EXPECT_TRUE(controller.isCalibrating());
    EXPECT_GE(stateSpy.count(), 1);

    controller.cancelCalibWizard();
    EXPECT_FALSE(controller.isCalibrating());
    EXPECT_GE(stateSpy.count(), 2);
}

TEST_F(LensParfocalTrackingTest, ControllerCurveSelectionAndDistance)
{
    LensPlayerController controller;
    QSignalSpy curveSpy(&controller, &LensPlayerController::activeCurveChanged);

    controller.setActiveCurveDistance(25.0);
    EXPECT_DOUBLE_EQ(controller.activeCurveDistance(), 25.0);
    EXPECT_EQ(curveSpy.count(), 1);

    controller.setActiveCurveIndex(1);
    EXPECT_EQ(controller.activeCurveIndex(), 1);
    EXPECT_EQ(curveSpy.count(), 2);
}

TEST_F(LensParfocalTrackingTest, LensControllerSaveAndEngine)
{
    using namespace LensControl;

    LensController lensCtrl;
    lensCtrl.setDriver(CameraIndex { 0U }, std::make_unique<MockLensDriver>());

    auto* engine = lensCtrl.zoomTrackEngine(CameraIndex { 0U });
    ASSERT_NE(engine, nullptr);

    EXPECT_FALSE(lensCtrl.isZoomTrackEnabled(CameraIndex { 0U }));
    lensCtrl.setZoomTrackFocus(CameraIndex { 0U }, true);
    EXPECT_TRUE(lensCtrl.isZoomTrackEnabled(CameraIndex { 0U }));

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString savePath = tempDir.path() + "/lens_ctrl_curves.dat";

    TrackCurve testCurve { };
    testCurve.distanceMeters = 15.0F;
    testCurve.knotCount = 2U;
    testCurve.knots[0] = { MotorPosition { 0U }, MotorPosition { 100U }, 0.0F };
    testCurve.knots[1] = { MotorPosition { 1000U }, MotorPosition { 200U }, 0.0F };
    EXPECT_TRUE(engine->addCurve(testCurve));

    EXPECT_TRUE(lensCtrl.saveTrackCurves(CameraIndex { 0U }, savePath.toStdString()));
    EXPECT_TRUE(QFile::exists(savePath));
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
    char fakeName[] = "TestLensParfocalTracking";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
