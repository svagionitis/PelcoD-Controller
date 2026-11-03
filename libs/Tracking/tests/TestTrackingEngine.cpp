/// @file TestTrackingEngine.cpp
/// @brief Unit tests for multi-target TrackingEngine, kinematics, lifecycle FSM, and VMTI generation.

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "Klv/VmtiEncoder.h"
#include "Klv/VmtiParser.h"
#include "Klv/VmtiTypes.h"
#include "PtzAutoTracker.h"
#include "PtzCameraModel.h"
#include "TrackingEngine.h"

using namespace Tracking;

namespace {

CameraIntrinsics createTestIntrinsics()
{
    CameraIntrinsics intrinsics;
    intrinsics.imageWidth = 1920;
    intrinsics.imageHeight = 1080;
    intrinsics.fx0 = 1000.0;
    intrinsics.fy0 = 1000.0;
    intrinsics.cx = 960.0;
    intrinsics.cy = 540.0;
    return intrinsics;
}

Klv::VTargetPack createDetection(std::uint32_t col, std::uint32_t row, std::uint32_t w = 40U, std::uint32_t h = 40U)
{
    Klv::VTargetPack det;
    det.targetId = 1U;
    det.centroid = Klv::PixelCoord { col, row };
    det.boundingBox = Klv::PixelBoundingBox { Klv::PixelCoord { col - (w / 2U), row - (h / 2U) },
        Klv::PixelCoord { col + (w / 2U), row + (h / 2U) } };
    det.confidence = 90U;
    return det;
}

} // namespace

TEST(TestTrackingEngine, TrackLifecycleTransitions)
{
    TrackingEngineConfig cfg;
    cfg.minHitsToConfirm = 3U;
    cfg.maxCoastingFrames = 3U;
    TrackingEngine engine(cfg);
    const auto intrinsics = createTestIntrinsics();

    // Frame 1: Initial detection -> New
    engine.updateDetections({ createDetection(960U, 540U) }, 1000000ULL, 0.033, intrinsics);
    EXPECT_EQ(engine.getTrackCount(), 1U);
    auto tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::New);

    // Frame 2: Second hit -> Still New
    engine.updateDetections({ createDetection(961U, 540U) }, 1033000ULL, 0.033, intrinsics);
    tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::New);

    // Frame 3: Third hit -> Promoted to Active
    engine.updateDetections({ createDetection(962U, 540U) }, 1066000ULL, 0.033, intrinsics);
    tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::Active);

    // Frame 4: Missing detection -> Transitions to Coasting
    engine.updateDetections({}, 1099000ULL, 0.033, intrinsics);
    tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::Coasting);

    // Frame 5: Detection reacquired -> Returns to Active
    engine.updateDetections({ createDetection(963U, 540U) }, 1132000ULL, 0.033, intrinsics);
    tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::Active);

    // Frame 6, 7, 8, 9: Missed consecutive detections exceeding maxCoastingFrames (3)
    engine.updateDetections({}, 1165000ULL, 0.033, intrinsics); // coast 1
    engine.updateDetections({}, 1198000ULL, 0.033, intrinsics); // coast 2
    engine.updateDetections({}, 1231000ULL, 0.033, intrinsics); // coast 3
    engine.updateDetections({}, 1264000ULL, 0.033, intrinsics); // coast 4 -> Dropped and pruned

    EXPECT_EQ(engine.getTrackCount(), 0U);
}

TEST(TestTrackingEngine, MultiTargetAssociationIntegrity)
{
    TrackingEngineConfig cfg;
    cfg.iouThreshold = 0.2;
    cfg.maxGatingDistancePx = 100.0;
    cfg.minHitsToConfirm = 2U;
    TrackingEngine engine(cfg);
    const auto intrinsics = createTestIntrinsics();

    // Two distant targets: Target A at (400, 500) and Target B at (1400, 500)
    std::vector<Klv::VTargetPack> frame1 = { createDetection(400U, 500U), createDetection(1400U, 500U) };
    engine.updateDetections(frame1, 1000000ULL, 0.033, intrinsics);
    ASSERT_EQ(engine.getTrackCount(), 2U);
    auto tracks = engine.getActiveTracks();
    const std::uint32_t idA = tracks[0].trackId;
    const std::uint32_t idB = tracks[1].trackId;
    EXPECT_NE(idA, idB);

    // Frame 2: Targets move slightly
    std::vector<Klv::VTargetPack> frame2 = { createDetection(405U, 502U), createDetection(1395U, 498U) };
    engine.updateDetections(frame2, 1033000ULL, 0.033, intrinsics);
    ASSERT_EQ(engine.getTrackCount(), 2U);
    tracks = engine.getActiveTracks();

    // Ensure IDs are preserved and both confirmed to Active
    EXPECT_EQ(tracks[0].trackId, idA);
    EXPECT_EQ(tracks[0].status, Klv::TrackStatus::Active);
    EXPECT_EQ(tracks[1].trackId, idB);
    EXPECT_EQ(tracks[1].status, Klv::TrackStatus::Active);
}

TEST(TestTrackingEngine, KinematicsAndHistoryTrailManagement)
{
    TrackingEngineConfig cfg;
    cfg.maxHistoryPoints = 5U; // Small history limit to test ring buffer
    cfg.minHitsToConfirm = 1U;
    TrackingEngine engine(cfg);
    const auto intrinsics = createTestIntrinsics();

    const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 };
    const double slantRangeMeters = 1000.0;

    // Ingest 8 sequential frames with moving target
    for (std::uint32_t i = 0U; i < 8U; ++i) {
        const std::uint32_t col = 960U + (i * 10U);
        const std::uint64_t ts = 1000000ULL + (static_cast<std::uint64_t>(i) * 33000ULL);
        engine.updateDetections(
            { createDetection(col, 540U) }, ts, 0.033, intrinsics, 0.0, 0.0, 1.0, slantRangeMeters, platform);
    }

    auto tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    const auto& trk = tracks[0];

    // Verify history buffer is capped at maxHistoryPoints (5)
    EXPECT_EQ(trk.historyTrail.size(), 5U);

    // Chronological order verified
    for (std::size_t i = 1U; i < trk.historyTrail.size(); ++i) {
        EXPECT_GT(trk.historyTrail[i].timestampUs, trk.historyTrail[i - 1U].timestampUs);
    }

    // Kinematics derived
    ASSERT_TRUE(trk.groundSpeedMps.has_value());
    EXPECT_GE(*trk.groundSpeedMps, 0.0);
    ASSERT_TRUE(trk.groundCourseDeg.has_value());
    EXPECT_GE(*trk.groundCourseDeg, 0.0);
    EXPECT_LT(*trk.groundCourseDeg, 360.0);
}

TEST(TestTrackingEngine, StanagTaxonomyAndVmtiComposite)
{
    TrackingEngine engine;
    const auto intrinsics = createTestIntrinsics();

    engine.updateDetections({ createDetection(960U, 540U) }, 1000000ULL, 0.033, intrinsics);
    auto tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    const std::uint32_t trackId = tracks[0].trackId;

    // Assign STANAG 1241 classification
    EXPECT_TRUE(
        engine.setTrackTaxonomy(trackId, Klv::StanagCategory::TrackedArmor, "Main Battle Tank", 94U, "SFGPUCA----"));

    const auto ontologies = engine.getOntologies();
    ASSERT_EQ(ontologies.size(), 1U);
    EXPECT_EQ(ontologies[0].ontologyId, trackId);
    EXPECT_EQ(ontologies[0].category, Klv::StanagCategory::TrackedArmor);
    EXPECT_EQ(ontologies[0].classLabel, "Main Battle Tank");
    EXPECT_EQ(ontologies[0].confidence, 94U);
    EXPECT_EQ(ontologies[0].sidcCode, "SFGPUCA----");

    // Build VMTI Local Set and verify composite encoding
    const Klv::VmtiLocalSet vmti = engine.buildVmtiSet(1000000ULL, 1920U, 1080U);
    EXPECT_EQ(vmti.targets.size(), 1U); // Tag 101
    EXPECT_EQ(vmti.tracks.size(), 1U); // Tag 102
    EXPECT_EQ(vmti.ontologies.size(), 1U); // Tag 103

    const std::vector<std::uint8_t> encoded = Klv::VmtiEncoder::encode(vmti, true);
    ASSERT_FALSE(encoded.empty());

    Klv::VmtiLocalSet decoded;
    EXPECT_EQ(Klv::VmtiParser::parse(encoded.data(), encoded.size(), decoded, true), Klv::KlvStatus::Success);
    EXPECT_EQ(decoded.targets.size(), 1U);
    EXPECT_EQ(decoded.tracks.size(), 1U);
    EXPECT_EQ(decoded.ontologies.size(), 1U);
    EXPECT_EQ(decoded.ontologies[0].classLabel, "Main Battle Tank");
}

TEST(TestTrackingEngine, PrimaryTrackPtzAutoFraming)
{
    TrackingEngine engine;
    const auto intrinsics = createTestIntrinsics();

    // Target to the right and above center: col 1440, row 300
    engine.updateDetections({ createDetection(1440U, 300U) }, 1000000ULL, 0.033, intrinsics);
    auto tracks = engine.getActiveTracks();
    ASSERT_EQ(tracks.size(), 1U);
    const std::uint32_t targetId = tracks[0].trackId;

    // No primary track set -> returns nullopt
    PtzAutoTracker tracker;
    EXPECT_FALSE(engine.computePtzCommand(tracker, 1920U, 1080U, 0.033, 1.0).has_value());

    // Designate primary track
    engine.setPrimaryTrack(targetId);
    EXPECT_EQ(engine.getPrimaryTrack(), targetId);

    const auto cmd = engine.computePtzCommand(tracker, 1920U, 1080U, 0.033, 1.0);
    ASSERT_TRUE(cmd.has_value());
    // Target is to the right (+error azimuth) -> Pan Right command (+1)
    EXPECT_EQ(cmd->panDirection, 1);
    // Target is above center (-error elevation in screen space) -> Tilt Up command (+1)
    EXPECT_EQ(cmd->tiltDirection, 1);

    // Clear primary track
    engine.clearPrimaryTrack();
    EXPECT_FALSE(engine.getPrimaryTrack().has_value());
    EXPECT_FALSE(engine.computePtzCommand(tracker, 1920U, 1080U, 0.033, 1.0).has_value());
}
