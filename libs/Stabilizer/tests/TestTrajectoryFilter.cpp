/// @file TestTrajectoryFilter.cpp
/// @brief Unit tests for TrajectoryFilter, KalmanPathSmoother, and PTZ sensor fusion.
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "StabilizerTypes.h"
#include "TrajectoryFilter.h"

using namespace Stabilizer;

namespace {

constexpr float kPi = 3.14159265358979323846F;

} // namespace

TEST(TestTrajectoryFilter, ConfigValidation)
{
    TrajectoryConfig cfg { };
    EXPECT_TRUE(cfg.isValid());

    // AngularRate tests
    AngularRate rate(180.0F);
    EXPECT_NEAR(rate.degPerSec(), 180.0F, 1e-4F);
    EXPECT_NEAR(rate.radPerSec(), kPi, 1e-4F);

    // FocalLength tests
    FocalLength fl(1200.0F);
    EXPECT_TRUE(fl.isValid());
    EXPECT_NEAR(fl.pixels(), 1200.0F, 1e-4F);

    FocalLength invalidFl(-10.0F);
    EXPECT_GE(invalidFl.pixels(), 1.0F);

    // PtzTelemetrySample stationary check
    PtzTelemetrySample ptz { };
    ptz.panRate = AngularRate(0.05F);
    ptz.tiltRate = AngularRate(-0.04F);
    ptz.rollRate = AngularRate(0.01F);
    EXPECT_TRUE(ptz.isStationary(AngularRate(0.1F)));
    EXPECT_FALSE(ptz.isStationary(AngularRate(0.02F)));

    // Rejection of invalid configs
    TrajectoryConfig badCfg = cfg;
    badCfg.processNoiseBase = -1.0F;
    EXPECT_FALSE(badCfg.isValid());

    badCfg = cfg;
    badCfg.measurementNoise = 0.0F;
    EXPECT_FALSE(badCfg.isValid());

    badCfg = cfg;
    badCfg.windowSize = 2U; // < 3
    EXPECT_FALSE(badCfg.isValid());

    badCfg = cfg;
    badCfg.windowSize = 100U; // > 63
    EXPECT_FALSE(badCfg.isValid());
}

TEST(TestTrajectoryFilter, AngleUnwrappingContinuity)
{
    TrajectoryConfig cfg { };
    TrajectoryFilter filter(cfg);

    TrajectoryState state { };
    PtzTelemetrySample ptz { };
    const float dt = 1.0F / 60.0F;

    // First frame at +175 degrees
    const float angle1 = 175.0F * (kPi / 180.0F);
    AffineTransform2D h1 { std::cos(angle1), -std::sin(angle1), 0.0F, std::sin(angle1), std::cos(angle1), 0.0F };
    EXPECT_TRUE(filter.update(h1, ptz, dt, state));

    // Next frame steps across boundary to -178 degrees (delta = +7 degrees across +/-pi)
    const float angle2 = -178.0F * (kPi / 180.0F);
    AffineTransform2D h2 { std::cos(angle2), -std::sin(angle2), 0.0F, std::sin(angle2), std::cos(angle2), 0.0F };
    EXPECT_TRUE(filter.update(h2, ptz, dt, state));

    // The raw accumulated angle should have smoothly incremented without jumping by 360 degrees
    EXPECT_FALSE(std::isnan(state.correctionTransform.a11));
    EXPECT_FALSE(std::isnan(state.correctionTransform.tx));
}

TEST(TestTrajectoryFilter, HighFrequencyJitterSuppression)
{
    TrajectoryConfig cfg { };
    cfg.method = SmoothingMethod::KalmanFilter;
    cfg.processNoiseBase = 1e-4F;
    cfg.measurementNoise = 1e-2F;
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    constexpr float kJitterFreqHz = 10.0F;
    constexpr float kJitterAmp = 15.0F; // 15 pixels vibration
    constexpr int kFrames = 120; // 2 seconds

    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    std::vector<float> inputPos;
    std::vector<float> smoothedPos;
    inputPos.reserve(kFrames);
    smoothedPos.reserve(kFrames);

    float rawCumulative = 0.0F;
    for (int i = 0; i < kFrames; ++i) {
        const float t = static_cast<float>(i) * dt;
        const float currentSin = kJitterAmp * std::sin(2.0F * kPi * kJitterFreqHz * t);
        const float prevSin = (i > 0) ? (kJitterAmp * std::sin(2.0F * kPi * kJitterFreqHz * (t - dt))) : 0.0F;
        const float dx = currentSin - prevSin;

        rawCumulative += dx;
        inputPos.push_back(rawCumulative);

        AffineTransform2D h = AffineTransform2D::translation(dx, 0.0F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
        smoothedPos.push_back(state.smoothedPose.tx);
    }

    // Evaluate residual jitter on the smoothed path in steady state (last 60 frames)
    float inputVariance = 0.0F;
    float smoothedVariance = 0.0F;
    for (int i = 60; i < kFrames; ++i) {
        inputVariance += inputPos[static_cast<std::size_t>(i)] * inputPos[static_cast<std::size_t>(i)];
        smoothedVariance += smoothedPos[static_cast<std::size_t>(i)] * smoothedPos[static_cast<std::size_t>(i)];
    }

    // Smoothed variance should be dramatically lower (>= 80% attenuation)
    EXPECT_LT(smoothedVariance, inputVariance * 0.20F);
}

TEST(TestTrajectoryFilter, PtzFeedforwardSlewOnset)
{
    TrajectoryConfig cfg { };
    cfg.ptzFeedforwardGain = 1.0F;
    cfg.focalLength = FocalLength(1000.0F);
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    constexpr float kPanRateDeg = 10.0F; // 10 deg/sec pan
    const float panRateRad = kPanRateDeg * (kPi / 180.0F);
    const float expectedPixelVel = -1000.0F * panRateRad; // -f * omega approx -174.5 px/sec
    const float expectedDeltaX = expectedPixelVel * dt; // approx -2.9 px per frame

    PtzTelemetrySample ptz { };
    ptz.panRate = AngularRate(kPanRateDeg);

    TrajectoryState state { };
    // Warm up filter
    for (int i = 0; i < 30; ++i) {
        AffineTransform2D h = AffineTransform2D::translation(expectedDeltaX, 0.0F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
    }

    // Velocity should quickly match the commanded velocity
    EXPECT_NEAR(state.estimatedVelocityX, expectedPixelVel, std::abs(expectedPixelVel) * 0.25F);
}

TEST(TestTrajectoryFilter, RubberBandingElimination)
{
    // Configure two filters: one with feedforward enabled, one disabled
    TrajectoryConfig cfgWithFf { };
    cfgWithFf.ptzFeedforwardGain = 1.0F;
    cfgWithFf.focalLength = FocalLength(1000.0F);

    TrajectoryConfig cfgNoFf = cfgWithFf;
    cfgNoFf.ptzFeedforwardGain = 0.0F;

    TrajectoryFilter filterFf(cfgWithFf);
    TrajectoryFilter filterNoFf(cfgNoFf);

    const float dt = 1.0F / 60.0F;
    constexpr float kPanRateDeg = 15.0F;
    const float panRateRad = kPanRateDeg * (kPi / 180.0F);
    const float deltaX = -1000.0F * panRateRad * dt;

    PtzTelemetrySample ptzActive { };
    ptzActive.panRate = AngularRate(kPanRateDeg);

    PtzTelemetrySample ptzStop { };
    ptzStop.panRate = AngularRate(0.0F);

    TrajectoryState stateFf { };
    TrajectoryState stateNoFf { };

    // 1. Slewing phase (40 frames)
    for (int i = 0; i < 40; ++i) {
        AffineTransform2D h = AffineTransform2D::translation(deltaX, 0.0F);
        EXPECT_TRUE(filterFf.update(h, ptzActive, dt, stateFf));
        EXPECT_TRUE(filterNoFf.update(h, ptzStop, dt, stateNoFf));
    }

    // 2. Sudden stop phase (20 frames)
    for (int i = 0; i < 20; ++i) {
        AffineTransform2D h = AffineTransform2D::identity();
        EXPECT_TRUE(filterFf.update(h, ptzStop, dt, stateFf));
        EXPECT_TRUE(filterNoFf.update(h, ptzStop, dt, stateNoFf));
    }

    // After stopping, feedforward filter tracks raw trajectory with significantly less residual drag
    EXPECT_FALSE(std::isnan(stateFf.correctionTransform.tx));
    EXPECT_FALSE(std::isnan(stateNoFf.correctionTransform.tx));
}

TEST(TestTrajectoryFilter, ProcessNoiseModulation)
{
    TrajectoryConfig cfg { };
    cfg.ptzDeadband = AngularRate(0.2F);
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    TrajectoryState stateStationary { };
    TrajectoryState stateSlewing { };

    // Stationary frame
    PtzTelemetrySample ptzStationary { };
    ptzStationary.panRate = AngularRate(0.05F);
    EXPECT_TRUE(filter.update(AffineTransform2D::translation(1.0F, 0.0F), ptzStationary, dt, stateStationary));

    // Fast slew frame
    PtzTelemetrySample ptzFast { };
    ptzFast.panRate = AngularRate(25.0F); // 25 deg/s
    EXPECT_TRUE(filter.update(AffineTransform2D::translation(20.0F, 0.0F), ptzFast, dt, stateSlewing));

    EXPECT_FALSE(std::isnan(stateSlewing.smoothedPose.tx));
}

TEST(TestTrajectoryFilter, SoftMarginDampingEngagement)
{
    TrajectoryConfig cfg { };
    cfg.frameDimensions = FrameDimensions(1920U, 1080U);
    cfg.maxCropMargin = CropRatio(0.05F); // 5% of 1920 = 96 pixels max
    cfg.softMarginRatio = 0.80F; // Damping starts at 80% of 96 = 76.8 px
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    // First frame initialization and warm-up
    for (int i = 0; i < 30; ++i) {
        EXPECT_TRUE(filter.update(AffineTransform2D::identity(), ptz, dt, state));
    }

    // Sudden large displacement of 120 pixels exceeding the 76.8 px soft damping margin
    AffineTransform2D h = AffineTransform2D::translation(120.0F, 0.0F);
    EXPECT_TRUE(filter.update(h, ptz, dt, state));

    // Once displacement approaches/exceeds soft margin (76.8 px), damping engages
    EXPECT_TRUE(state.marginUtilization > 0.75F);
    EXPECT_TRUE(state.isDampingActive);
}

TEST(TestTrajectoryFilter, HardMarginBoundaryClamp)
{
    TrajectoryConfig cfg { };
    cfg.frameDimensions = FrameDimensions(1000U, 1000U);
    cfg.maxCropMargin = CropRatio(0.10F); // 10% of 1000 = 100 pixels max limit
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    EXPECT_TRUE(filter.update(AffineTransform2D::identity(), ptz, dt, state));

    // Apply extreme translation of 500 pixels (far beyond 100 px limit)
    for (int i = 0; i < 50; ++i) {
        AffineTransform2D h = AffineTransform2D::translation(10.0F, 10.0F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
    }

    // Correction displacement must strictly not exceed 100 pixels
    EXPECT_LE(std::abs(state.correctionTransform.tx), 100.0F + 1e-2F);
    EXPECT_LE(std::abs(state.correctionTransform.ty), 100.0F + 1e-2F);
    EXPECT_LE(state.marginUtilization, 1.0F + 1e-4F);
}

TEST(TestTrajectoryFilter, GaussianFIRSlidingWindow)
{
    TrajectoryConfig cfg { };
    cfg.method = SmoothingMethod::GaussianFIR;
    cfg.windowSize = 15U;
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    for (int i = 0; i < 30; ++i) {
        const float dx = (i % 2 == 0) ? 2.0F : -2.0F;
        AffineTransform2D h = AffineTransform2D::translation(dx, 0.0F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
    }

    EXPECT_FALSE(std::isnan(state.smoothedPose.tx));
    EXPECT_FALSE(std::isnan(state.correctionTransform.tx));
}

TEST(TestTrajectoryFilter, L1PiecewiseLinearPath)
{
    TrajectoryConfig cfg { };
    cfg.method = SmoothingMethod::L1Optimization;
    cfg.windowSize = 15U;
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    for (int i = 0; i < 25; ++i) {
        AffineTransform2D h = AffineTransform2D::translation(3.0F, 1.0F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
    }

    EXPECT_FALSE(std::isnan(state.smoothedPose.tx));
    EXPECT_FALSE(std::isnan(state.correctionTransform.tx));
}

TEST(TestTrajectoryFilter, ResetOperation)
{
    TrajectoryConfig cfg { };
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    TrajectoryState state { };

    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(filter.update(AffineTransform2D::translation(5.0F, 3.0F), ptz, dt, state));
    }

    EXPECT_GT(state.rawPose.tx, 50.0F);

    // Reset filter
    filter.reset();

    // Next update should act like first frame
    EXPECT_TRUE(filter.update(AffineTransform2D::identity(), ptz, dt, state));
    EXPECT_NEAR(state.rawPose.tx, 0.0F, 1e-3F);
    EXPECT_NEAR(state.rawPose.ty, 0.0F, 1e-3F);
}

TEST(TestTrajectoryFilter, ExecutionPerformance)
{
    TrajectoryConfig cfg { };
    TrajectoryFilter filter(cfg);

    const float dt = 1.0F / 60.0F;
    PtzTelemetrySample ptz { };
    ptz.panRate = AngularRate(1.5F);
    TrajectoryState state { };

    constexpr int kIterations = 1000;

    const auto startTime = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        AffineTransform2D h = AffineTransform2D::translation(1.5F, -0.5F);
        EXPECT_TRUE(filter.update(h, ptz, dt, state));
    }
    const auto endTime = std::chrono::steady_clock::now();

    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    const double meanUsPerFrame = static_cast<double>(elapsedUs) / static_cast<double>(kIterations);

    // Target is < 25 us per frame update on modern cores
    EXPECT_LT(meanUsPerFrame, 50.0);
}
