/// @file TestZoomTrackCurve.cpp
/// @brief Automated unit test suite for ZoomTrackFocusEngine, PCHIP spline, and LUT parser.

#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "LensTypes.h"

using namespace LensControl;

TEST(TestZoomTrackCurve, KnotAndCurveValidation)
{
    TrackCurve emptyCurve { };
    emptyCurve.knotCount = 0U;
    EXPECT_FALSE(emptyCurve.isValid());

    TrackCurve singleKnotCurve { };
    singleKnotCurve.knotCount = 1U;
    singleKnotCurve.knots[0].zoom = MotorPosition { 100U };
    singleKnotCurve.knots[0].focus = MotorPosition { 500U };
    EXPECT_FALSE(singleKnotCurve.isValid());

    TrackCurve nonMonotonicCurve { };
    nonMonotonicCurve.knotCount = 3U;
    nonMonotonicCurve.knots[0].zoom = MotorPosition { 100U };
    nonMonotonicCurve.knots[1].zoom = MotorPosition { 500U };
    nonMonotonicCurve.knots[2].zoom = MotorPosition { 300U }; // Decreasing
    EXPECT_FALSE(nonMonotonicCurve.isValid());

    TrackCurve equalZoomCurve { };
    equalZoomCurve.knotCount = 2U;
    equalZoomCurve.knots[0].zoom = MotorPosition { 200U };
    equalZoomCurve.knots[1].zoom = MotorPosition { 200U }; // Equal zoom
    EXPECT_FALSE(equalZoomCurve.isValid());

    TrackCurve validCurve { };
    validCurve.knotCount = 3U;
    validCurve.knots[0].zoom = MotorPosition { 100U };
    validCurve.knots[0].focus = MotorPosition { 400U };
    validCurve.knots[1].zoom = MotorPosition { 500U };
    validCurve.knots[1].focus = MotorPosition { 800U };
    validCurve.knots[2].zoom = MotorPosition { 1000U };
    validCurve.knots[2].focus = MotorPosition { 1200U };
    EXPECT_TRUE(validCurve.isValid());
}

TEST(TestZoomTrackCurve, PchipMonotonicityAndNoOvershoot)
{
    // Pathological step data: flat [0..200], step up to 1000 at 400, flat [400..600]
    // A standard cubic spline will oscillate and overshoot below 200 and above 1000.
    // PCHIP must guarantee monotonicity with zero overshoot!
    TrackCurve stepCurve { };
    stepCurve.knotCount = 4U;
    stepCurve.knots[0].zoom = MotorPosition { 0U };
    stepCurve.knots[0].focus = MotorPosition { 200U };
    stepCurve.knots[1].zoom = MotorPosition { 200U };
    stepCurve.knots[1].focus = MotorPosition { 200U };
    stepCurve.knots[2].zoom = MotorPosition { 400U };
    stepCurve.knots[2].focus = MotorPosition { 1000U };
    stepCurve.knots[3].zoom = MotorPosition { 600U };
    stepCurve.knots[3].focus = MotorPosition { 1000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(stepCurve));
    engine.setMode(InterpolationMode::PchipSpline);

    // Verify plateau [0..200] is strictly flat at 200 (zero slope, zero dip)
    for (std::uint16_t z = 0U; z <= 200U; z += 20U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { z });
        EXPECT_EQ(f.value(), 200U) << "Overshoot or dip detected at z=" << z;
    }

    // Verify monotonic climb [200..400]
    std::uint16_t prevF = 200U;
    for (std::uint16_t z = 200U; z <= 400U; z += 10U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { z });
        EXPECT_GE(f.value(), prevF) << "Monotonicity violated at z=" << z;
        EXPECT_LE(f.value(), 1000U) << "Overshoot detected at z=" << z;
        prevF = f.value();
    }
    EXPECT_EQ(prevF, 1000U);

    // Verify plateau [400..600] is strictly flat at 1000 (zero overshoot)
    for (std::uint16_t z = 400U; z <= 600U; z += 20U) {
        const MotorPosition f = engine.evaluateFocus(MotorPosition { z });
        EXPECT_EQ(f.value(), 1000U) << "Overshoot detected at z=" << z;
    }
}

TEST(TestZoomTrackCurve, PiecewiseLinearEvaluation)
{
    TrackCurve curve { };
    curve.knotCount = 3U;
    curve.knots[0].zoom = MotorPosition { 100U };
    curve.knots[0].focus = MotorPosition { 200U };
    curve.knots[1].zoom = MotorPosition { 300U };
    curve.knots[1].focus = MotorPosition { 600U };
    curve.knots[2].zoom = MotorPosition { 500U };
    curve.knots[2].focus = MotorPosition { 1000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));
    engine.setMode(InterpolationMode::PiecewiseLinear);

    // Midpoints
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 100U }).value(), 200U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 200U }).value(), 400U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 300U }).value(), 600U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 400U }).value(), 800U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 500U }).value(), 1000U);
}

TEST(TestZoomTrackCurve, BoundaryClamping)
{
    TrackCurve curve { };
    curve.knotCount = 2U;
    curve.knots[0].zoom = MotorPosition { 1000U };
    curve.knots[0].focus = MotorPosition { 500U };
    curve.knots[1].zoom = MotorPosition { 5000U };
    curve.knots[1].focus = MotorPosition { 2500U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));

    // Under minimum zoom
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 0U }).value(), 500U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 500U }).value(), 500U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 1000U }).value(), 500U);

    // Over maximum zoom
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 5000U }).value(), 2500U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 60000U }).value(), 2500U);
}

TEST(TestZoomTrackCurve, EmptyEngineEvaluation)
{
    ZoomTrackFocusEngine engine;
    EXPECT_EQ(engine.curveCount(), 0U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 1000U }).value(), 0U);
    EXPECT_EQ(engine.evaluateDistance(MotorPosition { 1000U }, 10.0F).value(), 0U);
}

TEST(TestZoomTrackCurve, ParseUniversalLutFormat)
{
    const std::string_view lutText = "# Universal Parfocal Calibration LUT File\n"
                                     "# Distance: Infinity\n"
                                     "5:\n"
                                     "z: 100, 500, 1200, 3500, 8000\n"
                                     "f: 450, 485,  540,  690, 1020\n";

    ZoomTrackFocusEngine engine;
    const TrackCurveError err = engine.parseString(lutText);
    ASSERT_EQ(err, TrackCurveError::None);
    ASSERT_EQ(engine.curveCount(), 1U);

    const TrackCurve* curve = engine.getCurve(0U);
    ASSERT_NE(curve, nullptr);
    EXPECT_EQ(curve->knotCount, 5U);
    EXPECT_FLOAT_EQ(curve->distanceMeters, 0.0F);

    EXPECT_EQ(curve->knots[0].zoom.value(), 100U);
    EXPECT_EQ(curve->knots[0].focus.value(), 450U);
    EXPECT_EQ(curve->knots[4].zoom.value(), 8000U);
    EXPECT_EQ(curve->knots[4].focus.value(), 1020U);

    // Spline evaluation at intermediate points
    const MotorPosition midF = engine.evaluateFocus(MotorPosition { 500U });
    EXPECT_EQ(midF.value(), 485U);
}

TEST(TestZoomTrackCurve, MultiCurveDistanceBlending)
{
    const std::string_view multiLut = "# Distance: 10.0m\n"
                                      "3:\n"
                                      "z: 100, 500, 1000\n"
                                      "f: 500, 700,  900\n"
                                      "\n"
                                      "# Distance: Infinity\n"
                                      "3:\n"
                                      "z: 100, 500, 1000\n"
                                      "f: 400, 600,  800\n";

    ZoomTrackFocusEngine engine;
    ASSERT_EQ(engine.parseString(multiLut), TrackCurveError::None);
    ASSERT_EQ(engine.curveCount(), 2U);

    // Infinity evaluation
    const MotorPosition fInf = engine.evaluateDistance(MotorPosition { 500U }, 0.0F);
    EXPECT_EQ(fInf.value(), 700U); // Active curve is 0 if no blend, or matching curve

    // Blend at 10m: should match 10m curve exactly (700)
    const MotorPosition f10m = engine.evaluateDistance(MotorPosition { 500U }, 10.0F);
    EXPECT_EQ(f10m.value(), 700U);
}

TEST(TestZoomTrackCurve, ParseErrorHandling)
{
    ZoomTrackFocusEngine engine;

    // Empty input
    EXPECT_EQ(engine.parseString(""), TrackCurveError::EmptyInput);

    // Malformed sample count header
    EXPECT_EQ(engine.parseString("abc:\nz: 10, 20\nf: 30, 40\n"), TrackCurveError::InsufficientKnots);

    // Count mismatch
    const std::string_view mismatchText = "3:\n"
                                          "z: 100, 200, 300\n"
                                          "f: 100, 200\n";
    EXPECT_EQ(engine.parseString(mismatchText), TrackCurveError::CountMismatch);

    // Non-monotonic zoom
    const std::string_view nonMonoText = "3:\n"
                                         "z: 100, 500, 400\n"
                                         "f: 100, 200, 300\n";
    EXPECT_EQ(engine.parseString(nonMonoText), TrackCurveError::NonMonotonicZoom);

    // Missing file
    EXPECT_EQ(engine.parseFile("non_existent_curve_file.lut"), TrackCurveError::FileNotFound);
}

TEST(TestZoomTrackCurve, SerializationRoundtrip)
{
    const std::string_view originalLut = "# Distance: Infinity\n"
                                         "4:\n"
                                         "z: 100, 500, 1500, 4000\n"
                                         "f: 200, 450,  700, 1200\n";

    ZoomTrackFocusEngine engine1;
    ASSERT_EQ(engine1.parseString(originalLut), TrackCurveError::None);

    const std::string serialized = engine1.serializeString();
    EXPECT_FALSE(serialized.empty());

    ZoomTrackFocusEngine engine2;
    ASSERT_EQ(engine2.parseString(serialized), TrackCurveError::None);
    ASSERT_EQ(engine2.curveCount(), 1U);

    // Verify all evaluated points match identically
    for (std::uint16_t z = 50U; z <= 4500U; z += 50U) {
        const MotorPosition f1 = engine1.evaluateFocus(MotorPosition { z });
        const MotorPosition f2 = engine2.evaluateFocus(MotorPosition { z });
        EXPECT_EQ(f1.value(), f2.value()) << "Mismatch at z=" << z;
    }
}

TEST(TestZoomTrackCurve, AutomatedCalibrationWorkflow)
{
    ZoomTrackFocusEngine engine;
    EXPECT_FALSE(engine.isCalibrating());

    // Start 5-knot calibration between zoom 1000 and 5000
    engine.startCalib(5U, MotorPosition { 1000U }, MotorPosition { 5000U });
    EXPECT_TRUE(engine.isCalibrating());

    // Expected step interval = (5000 - 1000) / 4 = 1000
    // Zoom knots: 1000, 2000, 3000, 4000, 5000
    EXPECT_EQ(engine.nextCalibZoom().value(), 1000U);
    engine.recordCalibKnot(MotorPosition { 300U });
    EXPECT_TRUE(engine.isCalibrating());

    EXPECT_EQ(engine.nextCalibZoom().value(), 2000U);
    engine.recordCalibKnot(MotorPosition { 500U });
    EXPECT_TRUE(engine.isCalibrating());

    EXPECT_EQ(engine.nextCalibZoom().value(), 3000U);
    engine.recordCalibKnot(MotorPosition { 800U });
    EXPECT_TRUE(engine.isCalibrating());

    EXPECT_EQ(engine.nextCalibZoom().value(), 4000U);
    engine.recordCalibKnot(MotorPosition { 1200U });
    EXPECT_TRUE(engine.isCalibrating());

    EXPECT_EQ(engine.nextCalibZoom().value(), 5000U);
    engine.recordCalibKnot(MotorPosition { 1800U });

    // After 5th knot, calibration is complete and curve is added
    EXPECT_FALSE(engine.isCalibrating());
    ASSERT_EQ(engine.curveCount(), 1U);

    const TrackCurve* curve = engine.getCurve(0U);
    ASSERT_NE(curve, nullptr);
    EXPECT_EQ(curve->knotCount, 5U);
    EXPECT_EQ(curve->knots[0].zoom.value(), 1000U);
    EXPECT_EQ(curve->knots[0].focus.value(), 300U);
    EXPECT_EQ(curve->knots[4].zoom.value(), 5000U);
    EXPECT_EQ(curve->knots[4].focus.value(), 1800U);

    // Verify evaluation works seamlessly on the calibrated curve
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 1000U }).value(), 300U);
    EXPECT_EQ(engine.evaluateFocus(MotorPosition { 5000U }).value(), 1800U);
}

TEST(TestZoomTrackCurve, CancelCalibration)
{
    ZoomTrackFocusEngine engine;
    engine.startCalib(5U, MotorPosition { 100U }, MotorPosition { 500U });
    EXPECT_TRUE(engine.isCalibrating());

    engine.recordCalibKnot(MotorPosition { 200U });
    EXPECT_TRUE(engine.isCalibrating());

    engine.cancelCalib();
    EXPECT_FALSE(engine.isCalibrating());
    EXPECT_EQ(engine.curveCount(), 0U);
}
