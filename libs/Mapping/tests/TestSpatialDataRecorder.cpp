/// @file TestSpatialDataRecorder.cpp
/// @brief Unit tests for SpatialDataRecorder telemetry ingest, frustum computation, and decimation.

#include "SpatialDataRecorder.h"
#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestSpatialDataRecorder, IngestWithExplicitCorners) {
    SpatialDataRecorder recorder;
    EXPECT_TRUE(recorder.empty());
    EXPECT_EQ(recorder.size(), 0U);

    Klv::UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 32.7157;
    msg.sensorLongitudeDeg = -117.1611;
    msg.sensorTrueAltitudeM = 1500.0;
    msg.platformHeadingDeg = 90.0;
    msg.platformPitchDeg = 2.0;
    msg.platformRollDeg = -1.0;
    msg.platformTailNumber = "HAWK-01";
    msg.missionId = "RECON-1";

    Klv::FrustumCorners corners {};
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

    const auto& tracks = recorder.trackPoints();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].timestampUs, 1728043200000000ULL);
    EXPECT_DOUBLE_EQ(tracks[0].position.latitudeDeg, 32.7157);
    EXPECT_DOUBLE_EQ(tracks[0].position.longitudeDeg, -117.1611);
    EXPECT_DOUBLE_EQ(tracks[0].position.altitudeM, 1500.0);
    EXPECT_EQ(tracks[0].tailNumber, "HAWK-01");
    EXPECT_EQ(tracks[0].missionId, "RECON-1");

    const auto& frustums = recorder.frustums();
    ASSERT_EQ(frustums.size(), 1U);
    EXPECT_TRUE(frustums[0].valid);
    EXPECT_TRUE(frustums[0].hasTargetCenter);
    EXPECT_DOUBLE_EQ(frustums[0].apex.latitudeDeg, 32.7157);
    EXPECT_DOUBLE_EQ(frustums[0].targetCenter.latitudeDeg, 32.715);
    EXPECT_DOUBLE_EQ(frustums[0].targetCenter.altitudeM, 50.0);
    EXPECT_DOUBLE_EQ(frustums[0].base[0].latitudeDeg, 32.720); // TL
}

TEST(TestSpatialDataRecorder, IngestWithGeodesyFrustumFallback) {
    SpatialDataRecorder recorder;

    Klv::UasDatalinkMessage msg {};
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
    const auto& frustums = recorder.frustums();
    ASSERT_EQ(frustums.size(), 1U);
    EXPECT_TRUE(frustums[0].valid);
    // Looking North, top corners should have higher latitude than bottom corners
    EXPECT_GT(frustums[0].base[0].latitudeDeg, frustums[0].base[3].latitudeDeg);
    // Right corners should have greater longitude than left corners
    EXPECT_GT(frustums[0].base[1].longitudeDeg, frustums[0].base[0].longitudeDeg);
}

TEST(TestSpatialDataRecorder, UniformTimeDecimation) {
    SpatialDataRecorder recorder;

    // Push 10 frames at 100ms intervals (10 Hz)
    for (std::size_t i = 0; i < 10; ++i) {
        Klv::UasDatalinkMessage msg {};
        msg.precisionTimeStampUs = 1000000ULL + i * 100000ULL;
        msg.sensorLatitudeDeg = 32.7157 + (static_cast<double>(i) * 0.0001);
        msg.sensorLongitudeDeg = -117.1611;
        msg.sensorTrueAltitudeM = 1000.0;
        recorder.addTelemetryFrame(msg);
    }

    EXPECT_EQ(recorder.size(), 10U);

    // Decimate to 500ms intervals
    DecimationConfig cfg {};
    cfg.mode = DecimationMode::UniformTime;
    cfg.intervalSec = 0.5;

    recorder.decimate(cfg);

    // Initial frame (0.0s), 0.5s, 0.9s (last kept), total 3 frames
    EXPECT_EQ(recorder.size(), 3U);
    EXPECT_EQ(recorder.trackPoints().front().timestampUs, 1000000ULL);
    EXPECT_EQ(recorder.trackPoints().back().timestampUs, 1900000ULL);
}

TEST(TestSpatialDataRecorder, DistanceThresholdDecimation) {
    SpatialDataRecorder recorder;

    // Frame 0 at origin
    Klv::UasDatalinkMessage msg0 {};
    msg0.precisionTimeStampUs = 1000000ULL;
    msg0.sensorLatitudeDeg = 0.0;
    msg0.sensorLongitudeDeg = 0.0;
    msg0.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg0);

    // Frame 1: moved ~1 meter North
    Klv::UasDatalinkMessage msg1 {};
    msg1.precisionTimeStampUs = 1100000ULL;
    msg1.sensorLatitudeDeg = 0.00001; // ~1.1 meters
    msg1.sensorLongitudeDeg = 0.0;
    msg1.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg1);

    // Frame 2: moved ~50 meters North
    Klv::UasDatalinkMessage msg2 {};
    msg2.precisionTimeStampUs = 1200000ULL;
    msg2.sensorLatitudeDeg = 0.0005; // ~55 meters
    msg2.sensorLongitudeDeg = 0.0;
    msg2.sensorTrueAltitudeM = 100.0;
    recorder.addTelemetryFrame(msg2);

    DecimationConfig cfg {};
    cfg.mode = DecimationMode::DistanceThreshold;
    cfg.distanceDeltaM = 10.0; // Filter out moves < 10m

    recorder.decimate(cfg);

    // msg1 should be filtered out, keeping msg0 and msg2 (first and last)
    EXPECT_EQ(recorder.size(), 2U);
}
