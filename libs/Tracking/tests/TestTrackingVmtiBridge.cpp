/// @file TestTrackingVmtiBridge.cpp
/// @brief Unit tests for TrackingVmtiBridge and PtzAutoTracker VMTI ingestion.

#include "PtzAutoTracker.h"
#include "PtzSphericalEstimator.h"
#include "TrackingVmtiBridge.h"
#include <gtest/gtest.h>

using namespace Tracking;

TEST(TestTrackingVmtiBridge, BuildTargetPackFromState) {
    CameraIntrinsics intrinsics;
    intrinsics.imageWidth = 1920;
    intrinsics.imageHeight = 1080;
    intrinsics.fx0 = 1000.0;
    intrinsics.fy0 = 1000.0;
    intrinsics.cx = 960.0;
    intrinsics.cy = 540.0;

    SphericalTargetState state;
    state.azimuthRad = 0.0;
    state.elevationRad = 0.0;
    state.azimuthVelocityRadPerSec = 0.05; // Moving
    state.elevationVelocityRadPerSec = 0.0;
    state.locked = true;
    state.mahalanobisDistance = 1.0;
    state.ce90Meters = 4.2;
    state.le90Meters = 2.1;

    const Klv::GeoPoint3D platformPos { 37.7749, -122.4194, 500.0 };

    const Klv::VTargetPack pack = TrackingVmtiBridge::buildTargetPack(
        state, intrinsics, 101U, 0.0, 0.0, 1.0, 1000.0, platformPos);

    EXPECT_EQ(pack.targetId, 101U);
    ASSERT_TRUE(pack.centroid.has_value());
    // Principal point should project to col 961, row 541 (1-indexed)
    EXPECT_EQ(pack.centroid->col, 961U);
    EXPECT_EQ(pack.centroid->row, 541U);

    ASSERT_TRUE(pack.confidence.has_value());
    EXPECT_GT(*pack.confidence, 80U);

    ASSERT_TRUE(pack.detectionStatus.has_value());
    EXPECT_EQ(*pack.detectionStatus, 1U); // Moving

    ASSERT_TRUE(pack.targetCe90M.has_value());
    EXPECT_NEAR(*pack.targetCe90M, 4.2, 1e-4);

    ASSERT_TRUE(pack.targetLe90M.has_value());
    EXPECT_NEAR(*pack.targetLe90M, 2.1, 1e-4);

    ASSERT_TRUE(pack.targetLocation.has_value());
    // Target is 1000m North of platform
    EXPECT_GT(pack.targetLocation->latitudeDeg, platformPos.latitudeDeg);
    EXPECT_NEAR(pack.targetLocation->longitudeDeg, platformPos.longitudeDeg, 1e-4);
}

TEST(TestTrackingVmtiBridge, ExtractPixelDetectionFromCentroidAndBox) {
    // 1. Centroid extraction
    Klv::VTargetPack packCentroid;
    packCentroid.targetId = 1U;
    packCentroid.centroid = Klv::PixelCoord { 501U, 401U };

    double u = 0.0;
    double v = 0.0;
    EXPECT_TRUE(TrackingVmtiBridge::extractPixelDetection(packCentroid, u, v));
    EXPECT_NEAR(u, 500.0, 1e-6);
    EXPECT_NEAR(v, 400.0, 1e-6);

    // 2. Bounding box extraction
    Klv::VTargetPack packBox;
    packBox.targetId = 2U;
    Klv::PixelBoundingBox box;
    box.topLeft = Klv::PixelCoord { 101U, 201U };
    box.bottomRight = Klv::PixelCoord { 301U, 401U };
    packBox.boundingBox = box;

    EXPECT_TRUE(TrackingVmtiBridge::extractPixelDetection(packBox, u, v));
    EXPECT_NEAR(u, 200.0, 1e-6);
    EXPECT_NEAR(v, 300.0, 1e-6);

    // 3. Empty pack returns false
    Klv::VTargetPack emptyPack;
    emptyPack.targetId = 3U;
    EXPECT_FALSE(TrackingVmtiBridge::extractPixelDetection(emptyPack, u, v));
}

TEST(TestTrackingVmtiBridge, PtzAutoTrackerUpdateFromVmti) {
    PtzAutoTracker tracker;
    tracker.setDeadbands(0.01, 0.01);

    // Target to the right of frame center: col 1441 in a 1920x1080 frame (+0.5 normalized error)
    Klv::VTargetPack rightPack;
    rightPack.targetId = 10U;
    rightPack.centroid = Klv::PixelCoord { 1441U, 541U };
    rightPack.confidence = static_cast<std::uint8_t>(90U);

    const auto cmdRight = tracker.updateFromVmti(rightPack, 1920U, 1080U, 0.033, 1.0);
    EXPECT_EQ(cmdRight.state, PtzAutoTracker::TrackingState::Tracking);
    EXPECT_EQ(cmdRight.panDirection, 1); // Pan Right
    EXPECT_GT(cmdRight.panSpeed, 0);

    // Target to the left of frame center: col 481 in a 1920x1080 frame (-0.5 normalized error)
    Klv::VTargetPack leftPack;
    leftPack.targetId = 11U;
    leftPack.centroid = Klv::PixelCoord { 481U, 541U };
    leftPack.confidence = static_cast<std::uint8_t>(90U);

    const auto cmdLeft = tracker.updateFromVmti(leftPack, 1920U, 1080U, 0.033, 1.0);
    EXPECT_EQ(cmdLeft.panDirection, -1); // Pan Left
    EXPECT_GT(cmdLeft.panSpeed, 0);
}
