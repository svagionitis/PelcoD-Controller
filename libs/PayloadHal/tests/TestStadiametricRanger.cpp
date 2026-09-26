/// @file TestStadiametricRanger.cpp
/// @brief Comprehensive unit tests for Passive Stadiametric & Kinematic Triangulation Range Estimator.

#include "StadiametricRanger.h"
#include "IPayload.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace PayloadHal;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

} // namespace

TEST(TestStadiametricRanger, TargetProfilesAndDimensions)
{
    StadiametricRanger ranger;

    // Default profile is MainBattleTank
    EXPECT_EQ(ranger.targetProfile(), TargetClassProfile::MainBattleTank);
    auto mbtDims = ranger.targetDimensions();
    EXPECT_DOUBLE_EQ(mbtDims.heightMeters, 2.4);
    EXPECT_DOUBLE_EQ(mbtDims.widthMeters, 3.5);
    EXPECT_DOUBLE_EQ(mbtDims.lengthMeters, 7.0);

    // Switch to HumanPersonnel
    ranger.setTargetProfile(TargetClassProfile::HumanPersonnel);
    EXPECT_EQ(ranger.targetProfile(), TargetClassProfile::HumanPersonnel);
    auto humanDims = ranger.targetDimensions();
    EXPECT_DOUBLE_EQ(humanDims.heightMeters, 1.8);
    EXPECT_DOUBLE_EQ(humanDims.widthMeters, 0.5);

    // Switch to PatrolVessel
    ranger.setTargetProfile(TargetClassProfile::PatrolVessel);
    auto vesselDims = ranger.targetDimensions();
    EXPECT_DOUBLE_EQ(vesselDims.lengthMeters, 25.0);

    // Custom dimensions
    TargetPhysicalDimensions custom { 4.0, 3.0, 10.0 };
    ranger.setCustomTargetDimensions(custom);
    EXPECT_EQ(ranger.targetProfile(), TargetClassProfile::Custom);
    auto actualCustom = ranger.targetDimensions();
    EXPECT_DOUBLE_EQ(actualCustom.widthMeters, 4.0);
    EXPECT_DOUBLE_EQ(actualCustom.heightMeters, 3.0);
    EXPECT_DOUBLE_EQ(actualCustom.lengthMeters, 10.0);
}

TEST(TestStadiametricRanger, StadiametricHeightRanging)
{
    StadiametricRanger ranger;
    ranger.setTargetProfile(TargetClassProfile::HumanPersonnel); // Height = 1.8 m

    OpticalFrameCalibration calib {};
    calib.verticalFovDeg = 20.0;
    calib.horizontalFovDeg = 30.0;
    calib.frameWidthPx = 1920;
    calib.frameHeightPx = 1080;
    ranger.setOpticalCalibration(calib);

    // Synthetic target at known distance R_true = 500 meters
    // Target height = 1.8 m
    // Subtended angle theta_v = 2 * atan(H / (2 * R))
    const double expectedThetaV = 2.0 * std::atan(1.8 / (2.0 * 500.0));
    const double tanHalfVfov = std::tan((20.0 * 0.5) * kDegToRad);
    const double expectedNormH = std::tan(expectedThetaV * 0.5) / tanHalfVfov;

    BoundingBoxDetection bbox {};
    bbox.normX = 0.5;
    bbox.normY = 0.5;
    bbox.normWidth = 0.02;
    bbox.normHeight = expectedNormH;
    bbox.pixelJitter1Sigma = 1.0;

    auto est = ranger.estimateStadiametricRange(bbox, StadiametricDimensionMode::Height);

    EXPECT_TRUE(est.valid);
    EXPECT_NEAR(est.slantRangeMeters, 500.0, 0.1);
    EXPECT_GT(est.subtendedAngleMrad, 0.0);
    EXPECT_GT(est.rangeUncertaintyMeters, 0.0);
    EXPECT_EQ(est.usedDimension, StadiametricDimensionMode::Height);
}

TEST(TestStadiametricRanger, StadiametricAspectAngleProjection)
{
    StadiametricRanger ranger;
    // MBT: width = 3.5, length = 7.0
    ranger.setTargetProfile(TargetClassProfile::MainBattleTank);

    OpticalFrameCalibration calib {};
    calib.horizontalFovDeg = 25.0;
    calib.verticalFovDeg = 15.0;
    calib.frameWidthPx = 1920;
    calib.frameHeightPx = 1080;
    ranger.setOpticalCalibration(calib);

    BoundingBoxDetection bbox {};
    bbox.normWidth = 0.04;
    bbox.normHeight = 0.02;

    // Pure head-on (aspect = 0 deg) -> projected width is pure width (3.5m)
    auto estHeadOn = ranger.estimateStadiametricRange(bbox, StadiametricDimensionMode::Width, 0.0);
    EXPECT_TRUE(estHeadOn.valid);

    // Pure broadside (aspect = 90 deg) -> projected width is length (7.0m)
    auto estBroadside = ranger.estimateStadiametricRange(bbox, StadiametricDimensionMode::Width, 90.0);
    EXPECT_TRUE(estBroadside.valid);

    // Broadside presents twice the dimension of head-on for same pixel width,
    // so range must be twice as large
    EXPECT_NEAR(estBroadside.slantRangeMeters / estHeadOn.slantRangeMeters, 7.0 / 3.5, 0.01);
}

TEST(TestStadiametricRanger, TwoPointTriangulationCollinearRejection)
{
    StadiametricRanger ranger;
    ranger.setMinimumConvergenceAngleDeg(2.0);

    // Target at [1000, 0, 0] (due North)
    // Platform flying directly toward target: P1 = [0, 0, 0], P2 = [200, 0, 0]
    // Azimuth for both is 0.0 degrees (collinear flight)
    ranger.addBearingObservation(0.0, 0.0, { 0.0, 0.0, 0.0 });
    ranger.addBearingObservation(0.0, 0.0, { 200.0, 0.0, 0.0 });

    auto est = ranger.computeTwoPointTriangulation();

    EXPECT_FALSE(est.valid);
    EXPECT_EQ(est.quality, TriangulationQuality::InsufficientBaseline);
}

TEST(TestStadiametricRanger, TwoPointTriangulationOrthogonalFlyby)
{
    StadiametricRanger ranger;
    ranger.setMinimumConvergenceAngleDeg(1.0);

    // Target stationary at [1000.0, 1000.0, 0.0]
    // Obs 1: Platform at [0.0, 1000.0, 0.0] -> Target is due North (az = 0 deg, el = 0 deg)
    // Range is 1000.0 m
    ranger.addBearingObservation(0.0, 0.0, { 0.0, 1000.0, 0.0 });

    // Obs 2: Platform at [1000.0, 0.0, 0.0] -> Target is due East (az = 90 deg, el = 0 deg)
    // Range is 1000.0 m
    // Convergence angle is 90 degrees (optimal GDOP)
    ranger.addBearingObservation(90.0, 0.0, { 1000.0, 0.0, 0.0 });

    auto est = ranger.computeTwoPointTriangulation();

    EXPECT_TRUE(est.valid);
    EXPECT_EQ(est.quality, TriangulationQuality::Optimal);
    EXPECT_NEAR(est.convergenceAngleDeg, 90.0, 0.1);
    EXPECT_NEAR(est.rayMissDistanceMeters, 0.0, 1e-4);
    EXPECT_NEAR(est.targetPositionNedMeters.x, 1000.0, 0.1);
    EXPECT_NEAR(est.targetPositionNedMeters.y, 1000.0, 0.1);
    EXPECT_NEAR(est.targetPositionNedMeters.z, 0.0, 0.1);
    EXPECT_NEAR(est.slantRangeFromLatestMeters, 1000.0, 0.1);
}

TEST(TestStadiametricRanger, BatchTriangulationLeastSquares)
{
    StadiametricRanger ranger(20);

    // Ground target at [1200.0, 800.0, 0.0]
    const Vector3D targetTrue { 1200.0, 800.0, 0.0 };

    // Platform flies along East-West trajectory at altitude 200 m (Z = -200 m)
    // X = 0 m, Y ranges from -500 m to +1500 m in 6 steps
    for (int i = 0; i <= 6; ++i) {
        const double posY = -500.0 + static_cast<double>(i) * 300.0;
        const Vector3D platPos { 0.0, posY, -200.0 };

        const Vector3D rel = { targetTrue.x - platPos.x, targetTrue.y - platPos.y, targetTrue.z - platPos.z };
        const double rangeHoriz = std::sqrt(rel.x * rel.x + rel.y * rel.y);
        const double azDeg = std::atan2(rel.y, rel.x) * (180.0 / kPi);
        const double elDeg = std::atan2(-rel.z, rangeHoriz) * (180.0 / kPi); // Z is down, so -rel.z is up

        ranger.addBearingObservation(azDeg, elDeg, platPos);
    }

    EXPECT_EQ(ranger.observationCount(), 7U);

    auto est = ranger.computeBatchTriangulation();

    EXPECT_TRUE(est.valid);
    EXPECT_EQ(est.observationsUsed, 7U);
    EXPECT_NEAR(est.targetPositionNedMeters.x, targetTrue.x, 1.0);
    EXPECT_NEAR(est.targetPositionNedMeters.y, targetTrue.y, 1.0);
    EXPECT_NEAR(est.targetPositionNedMeters.z, targetTrue.z, 1.0);
    EXPECT_NEAR(est.rayMissDistanceMeters, 0.0, 0.5);
    EXPECT_GT(est.slantRangeFromLatestMeters, 500.0);
}

TEST(TestStadiametricRanger, RayMissDistanceAndOutlierRejection)
{
    StadiametricRanger ranger;

    // Nominal intersecting rays
    ranger.addBearingObservation(0.0, 0.0, { 0.0, 500.0, 0.0 });
    ranger.addBearingObservation(90.0, 0.0, { 500.0, 0.0, 0.0 });

    auto estNominal = ranger.computeTwoPointTriangulation();
    EXPECT_TRUE(estNominal.valid);
    EXPECT_NEAR(estNominal.rayMissDistanceMeters, 0.0, 1e-4);

    // Clear and introduce skew rays (one ray at elevation 30 deg, one at elevation -30 deg)
    ranger.resetTriangulation();
    EXPECT_EQ(ranger.observationCount(), 0U);

    ranger.addBearingObservation(0.0, 30.0, { 0.0, 500.0, 0.0 });
    ranger.addBearingObservation(90.0, -30.0, { 500.0, 0.0, 0.0 });

    auto estSkew = ranger.computeTwoPointTriangulation();
    EXPECT_TRUE(estSkew.valid);
    // Because the rays are skew in 3D, the miss distance must be substantially non-zero
    EXPECT_GT(estSkew.rayMissDistanceMeters, 10.0);
}

TEST(TestStadiametricRanger, HybridStadiametricTriangulationFusion)
{
    StadiametricRanger ranger;
    ranger.setTargetProfile(TargetClassProfile::HumanPersonnel);

    OpticalFrameCalibration calib {};
    calib.verticalFovDeg = 20.0;
    calib.horizontalFovDeg = 30.0;
    calib.frameWidthPx = 1920;
    calib.frameHeightPx = 1080;
    ranger.setOpticalCalibration(calib);

    // Setup triangulation: 1000m target
    ranger.addBearingObservation(0.0, 0.0, { 0.0, 1000.0, 0.0 });
    ranger.addBearingObservation(90.0, 0.0, { 1000.0, 0.0, 0.0 });

    // Setup bounding box roughly matching 1000m
    const double expectedThetaV = 2.0 * std::atan(1.8 / (2.0 * 1020.0));
    const double tanHalfVfov = std::tan((20.0 * 0.5) * kDegToRad);
    BoundingBoxDetection bbox {};
    bbox.normHeight = std::tan(expectedThetaV * 0.5) / tanHalfVfov;
    bbox.normWidth = 0.01;
    bbox.pixelJitter1Sigma = 1.0;

    auto fused = ranger.computeFusedRange(bbox);

    EXPECT_TRUE(fused.valid);
    EXPECT_TRUE(fused.stadiametricAvailable);
    EXPECT_TRUE(fused.triangulationAvailable);
    EXPECT_GT(fused.confidence01, 0.5);
    EXPECT_NEAR(fused.estimatedRangeMeters, 1000.0, 25.0);
    // Combined uncertainty must be less than or equal to the smallest individual uncertainty
    EXPECT_LE(fused.rangeUncertaintyMeters, std::max(fused.stadiametric.rangeUncertaintyMeters, fused.triangulation.rangeUncertaintyMeters));
}

TEST(TestStadiametricRanger, TelemetryIngestFromIPayload)
{
    StadiametricRanger ranger;
    SimulatedPayload payload;
    payload.connect();

    // Slew PTU to 45 deg pan, -10 deg tilt
    payload.panTilt()->setAbsoluteAngles(45.0, -10.0);

    // Ingest telemetry
    ranger.updateFromPayload(payload, { 100.0, 200.0, -50.0 });

    EXPECT_EQ(ranger.observationCount(), 1U);
    EXPECT_GT(ranger.opticalCalibration().horizontalFovDeg, 0.0);
}

TEST(TestStadiametricRanger, SimulatedPayloadIntegration)
{
    SimulatedPayload payload;
    payload.connect();

    auto ranger = payload.stadiametricRanger();
    ASSERT_NE(ranger, nullptr);

    ranger->setTargetProfile(TargetClassProfile::ArmoredPersonnelCarrier);
    EXPECT_EQ(ranger->targetProfile(), TargetClassProfile::ArmoredPersonnelCarrier);
}
