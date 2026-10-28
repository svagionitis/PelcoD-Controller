/// @file TestSpatialDataRecorder.cpp
/// @brief Unit tests for SpatialDataRecorder telemetry ingest, frustum computation, and decimation.

#include "SpatialDataRecorder.h"

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "KlvTypes.h"
#include "SpatialExportTypes.h"
#include "VmtiTypes.h"

using namespace Mapping;

TEST(TestSpatialDataRecorder, IngestWithExplicitCorners)
{
    SpatialDataRecorder recorder { };
    EXPECT_TRUE(recorder.empty());
    EXPECT_EQ(recorder.size(), 0U);

    Klv::UasDatalinkMessage msg { };
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 32.7157;
    msg.sensorLongitudeDeg = -117.1611;
    msg.sensorTrueAltitudeM = 1500.0;
    msg.platformHeadingDeg = 90.0;
    msg.platformPitchDeg = 2.0;
    msg.platformRollDeg = -1.0;
    msg.platformTailNumber = "HAWK-01";
    msg.missionId = "RECON-1";

    Klv::FrustumCorners corners { };
    corners.topLeft = { 32.720, -117.150 };
    corners.topRight = { 32.720, -117.140 };
    corners.bottomRight = { 32.710, -117.140 };
    corners.bottomLeft = { 32.710, -117.150 };
    msg.cornerCoordinates = corners;
    msg.frameCenterLatDeg = 32.715;
    msg.frameCenterLonDeg = -117.145;
    msg.frameCenterElevM = 50.0;

    recorder.addTelemetryFrame(msg);

    EXPECT_FALSE(recorder.empty());
    EXPECT_EQ(recorder.size(), 1U);

    const auto& tracks { recorder.trackPoints() };
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].timestampUs, 1728043200000000ULL);
    EXPECT_DOUBLE_EQ(tracks[0].position.latitudeDeg, 32.7157);
    EXPECT_DOUBLE_EQ(tracks[0].position.longitudeDeg, -117.1611);
    EXPECT_DOUBLE_EQ(tracks[0].position.altitudeM, 1500.0);
    EXPECT_EQ(tracks[0].tailNumber, "HAWK-01");
    EXPECT_EQ(tracks[0].missionId, "RECON-1");

    const auto& frustums { recorder.frustums() };
    ASSERT_EQ(frustums.size(), 1U);
    EXPECT_TRUE(frustums[0].valid);
    EXPECT_TRUE(frustums[0].hasTargetCenter);
    EXPECT_DOUBLE_EQ(frustums[0].apex.latitudeDeg, 32.7157);
    EXPECT_DOUBLE_EQ(frustums[0].targetCenter.latitudeDeg, 32.715);
    EXPECT_DOUBLE_EQ(frustums[0].targetCenter.altitudeM, 50.0);
    EXPECT_DOUBLE_EQ(frustums[0].base[0].latitudeDeg, 32.720); // TL
}

TEST(TestSpatialDataRecorder, IngestWithGeodesyFrustumFallback)
{
    SpatialDataRecorder recorder { };

    Klv::UasDatalinkMessage msg { };
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 32.7157;
    msg.sensorLongitudeDeg = -117.1611;
    msg.sensorTrueAltitudeM = 1000.0;
    msg.platformHeadingDeg = 0.0; // North
    msg.sensorRelAzimuthDeg = 0.0;
    msg.sensorRelElevationDeg = -45.0; // Looking down 45 deg
    msg.sensorHfovDeg = 30.0;
    msg.sensorVfovDeg = 20.0;
    // cornerCoordinates intentionally left empty to trigger KlvGeodesy fallback

    recorder.addTelemetryFrame(msg);

    ASSERT_EQ(recorder.size(), 1U);
    const auto& frustums { recorder.frustums() };
    ASSERT_EQ(frustums.size(), 1U);
    EXPECT_TRUE(frustums[0].valid);
    // Looking North, top corners should have higher latitude than bottom corners
    EXPECT_GT(frustums[0].base[0].latitudeDeg, frustums[0].base[3].latitudeDeg);
    // Right corners should have greater longitude than left corners
    EXPECT_GT(frustums[0].base[1].longitudeDeg, frustums[0].base[0].longitudeDeg);
}

TEST(TestSpatialDataRecorder, UniformTimeDecimation)
{
    SpatialDataRecorder recorder { };

    // Push 10 frames at 100ms intervals (10 Hz)
    for (std::size_t i { 0U }; i < 10U; ++i) {
        Klv::UasDatalinkMessage msg { };
        msg.precisionTimeStampUs = 1000000ULL + (i * 100000ULL);
        msg.sensorLatitudeDeg = 32.7157 + (static_cast<double>(i) * 0.0001);
        msg.sensorLongitudeDeg = -117.1611;
        msg.sensorTrueAltitudeM = 1000.0;
        recorder.addTelemetryFrame(msg);
    }

    EXPECT_EQ(recorder.size(), 10U);

    // Decimate to 500ms intervals
    DecimationConfig cfg { };
    cfg.mode = DecimationMode::UniformTime;
    cfg.intervalSec = 0.5;

    recorder.decimate(cfg);

    // Initial frame (0.0s), 0.5s, 0.9s (last kept), total 3 frames
    EXPECT_EQ(recorder.size(), 3U);
    EXPECT_EQ(recorder.trackPoints().front().timestampUs, 1000000ULL);
    EXPECT_EQ(recorder.trackPoints().back().timestampUs, 1900000ULL);
}

TEST(TestSpatialDataRecorder, DistanceThresholdDecimation)
{
    SpatialDataRecorder recorder { };

    // Frame 0 at origin
    Klv::UasDatalinkMessage msg0 { };
    msg0.precisionTimeStampUs = 1000000ULL;
    msg0.sensorLatitudeDeg = 0.0;
    msg0.sensorLongitudeDeg = 0.0;
    msg0.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg0);

    // Frame 1: moved ~1 meter North
    Klv::UasDatalinkMessage msg1 { };
    msg1.precisionTimeStampUs = 1100000ULL;
    msg1.sensorLatitudeDeg = 0.00001; // ~1.1 meters
    msg1.sensorLongitudeDeg = 0.0;
    msg1.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg1);

    // Frame 2: moved ~50 meters North
    Klv::UasDatalinkMessage msg2 { };
    msg2.precisionTimeStampUs = 1200000ULL;
    msg2.sensorLatitudeDeg = 0.0005; // ~55 meters
    msg2.sensorLongitudeDeg = 0.0;
    msg2.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg2);

    DecimationConfig cfg { };
    cfg.mode = DecimationMode::DistanceThreshold;
    cfg.distanceDeltaM = 10.0; // Filter out moves < 10m

    recorder.decimate(cfg);

    // msg1 should be filtered out, keeping msg0 and msg2 (first and last)
    EXPECT_EQ(recorder.size(), 2U);
}

TEST(TestSpatialDataRecorder, HeadingThresholdDecimationAcrossWrapBoundary)
{
    SpatialDataRecorder recorder { };

    // Frame 0: Heading 359.0 deg
    Klv::UasDatalinkMessage msg0 { };
    msg0.precisionTimeStampUs = 1000000ULL;
    msg0.sensorLatitudeDeg = 35.0;
    msg0.sensorLongitudeDeg = 25.0;
    msg0.platformHeadingDeg = 359.0;
    recorder.addTelemetryFrame(msg0);

    // Frame 1: Heading 1.0 deg (only 2.0 deg across North 360/0 wrap boundary -> must be filtered when threshold
    // is 5.0 deg)
    Klv::UasDatalinkMessage msg1 { };
    msg1.precisionTimeStampUs = 1100000ULL;
    msg1.sensorLatitudeDeg = 35.0001;
    msg1.sensorLongitudeDeg = 25.0;
    msg1.platformHeadingDeg = 1.0;
    recorder.addTelemetryFrame(msg1);

    // Frame 2: Heading 14.0 deg (15.0 deg change from Frame 0's 359.0 deg -> must be retained)
    Klv::UasDatalinkMessage msg2 { };
    msg2.precisionTimeStampUs = 1200000ULL;
    msg2.sensorLatitudeDeg = 35.0002;
    msg2.sensorLongitudeDeg = 25.0;
    msg2.platformHeadingDeg = 14.0;
    recorder.addTelemetryFrame(msg2);

    // Frame 3: Heading 15.0 deg (last point is always retained)
    Klv::UasDatalinkMessage msg3 { };
    msg3.precisionTimeStampUs = 1300000ULL;
    msg3.sensorLatitudeDeg = 35.0003;
    msg3.sensorLongitudeDeg = 25.0;
    msg3.platformHeadingDeg = 15.0;
    recorder.addTelemetryFrame(msg3);

    DecimationConfig cfg { };
    cfg.mode = DecimationMode::HeadingThreshold;
    cfg.angleDeltaDeg = 5.0;

    recorder.decimate(cfg);

    ASSERT_EQ(recorder.size(), 3U);
    EXPECT_DOUBLE_EQ(recorder.trackPoints()[0].headingDeg, 359.0);
    EXPECT_DOUBLE_EQ(recorder.trackPoints()[1].headingDeg, 14.0);
    EXPECT_DOUBLE_EQ(recorder.trackPoints()[2].headingDeg, 15.0);
}

TEST(TestSpatialDataRecorder, OutOfOrderTimestampsAndNegativeIntervalSafety)
{
    SpatialDataRecorder recorder { };

    // Frame 0: t = 2.0s
    Klv::UasDatalinkMessage msg0 { };
    msg0.precisionTimeStampUs = 2000000ULL;
    msg0.sensorLatitudeDeg = 32.0;
    msg0.sensorLongitudeDeg = -117.0;
    recorder.addTelemetryFrame(msg0);

    // Frame 1: out-of-order timestamp t = 1.5s (less than Frame 0!)
    Klv::UasDatalinkMessage msg1 { };
    msg1.precisionTimeStampUs = 1500000ULL;
    msg1.sensorLatitudeDeg = 32.0001;
    msg1.sensorLongitudeDeg = -117.0;
    recorder.addTelemetryFrame(msg1);

    // Frame 2: t = 2.1s (only 0.1s after Frame 0, which is < 0.5s interval)
    Klv::UasDatalinkMessage msg2 { };
    msg2.precisionTimeStampUs = 2100000ULL;
    msg2.sensorLatitudeDeg = 32.0002;
    msg2.sensorLongitudeDeg = -117.0;
    recorder.addTelemetryFrame(msg2);

    // Frame 3: t = 2.6s (0.6s after Frame 0)
    Klv::UasDatalinkMessage msg3 { };
    msg3.precisionTimeStampUs = 2600000ULL;
    msg3.sensorLatitudeDeg = 32.0003;
    msg3.sensorLongitudeDeg = -117.0;
    recorder.addTelemetryFrame(msg3);

    DecimationConfig cfg { };
    cfg.mode = DecimationMode::UniformTime;
    cfg.intervalSec = 0.5;

    recorder.decimate(cfg);

    // Frame 1 (out-of-order) and Frame 2 (+0.1s) must NOT wrap around uint64_t and be retained!
    // Only Frame 0 (first) and Frame 3 (last) should remain.
    EXPECT_EQ(recorder.size(), 2U);
    EXPECT_EQ(recorder.trackPoints().front().timestampUs, 2000000ULL);
    EXPECT_EQ(recorder.trackPoints().back().timestampUs, 2600000ULL);
}

TEST(TestSpatialDataRecorder, VmtiTargetRecordingAndOffsetResolution)
{
    SpatialDataRecorder recorder { };

    Klv::UasDatalinkMessage msg { };
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 37.9838;
    msg.sensorLongitudeDeg = 23.7275;
    msg.sensorTrueAltitudeM = 1200.0;
    msg.frameCenterLatDeg = 38.0000;
    msg.frameCenterLonDeg = 23.7500;
    msg.frameCenterElevM = 40.0;

    Klv::VmtiLocalSet vmti { };
    Klv::VTargetPack packAbs { };
    packAbs.targetId = 10U;
    packAbs.targetLocation = Klv::GeoPoint3D { 38.0050, 23.7550, 45.0 };
    packAbs.targetCe90M = 3.5;
    packAbs.confidence = 92U;
    packAbs.detectionStatus = 1U;
    vmti.targets.push_back(packAbs);

    Klv::VTargetPack packRel { };
    packRel.targetId = 20U;
    packRel.locationOffsetDeg = Klv::GeoPoint2D { -0.0020, 0.0030 };
    packRel.heightAboveEllipsoidM = 42.0;
    vmti.targets.push_back(packRel);

    msg.vmti = vmti;
    recorder.addTelemetryFrame(msg);

    const auto& targets { recorder.vmtiTargets() };
    ASSERT_EQ(targets.size(), 2U);

    EXPECT_EQ(targets[0].targetId, 10U);
    EXPECT_DOUBLE_EQ(targets[0].position.latitudeDeg, 38.0050);
    EXPECT_DOUBLE_EQ(targets[0].position.longitudeDeg, 23.7550);
    EXPECT_DOUBLE_EQ(targets[0].position.altitudeM, 45.0);
    EXPECT_DOUBLE_EQ(targets[0].ce90M, 3.5);
    EXPECT_EQ(targets[0].confidence, 92U);
    EXPECT_EQ(targets[0].status, 1U);

    EXPECT_EQ(targets[1].targetId, 20U);
    EXPECT_NEAR(targets[1].position.latitudeDeg, 37.9980, 1e-6);
    EXPECT_NEAR(targets[1].position.longitudeDeg, 23.7530, 1e-6);
    EXPECT_DOUBLE_EQ(targets[1].position.altitudeM, 42.0);
}
