#include <cstddef>
#include <cstdint>

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "KlvEncoder.h"
#include "KlvParser.h"
#include "VmtiEncoder.h"
#include "VmtiParser.h"
#include "VmtiTypes.h"

using namespace Klv;

TEST(VmtiTest, PixelCoordRowMajorConversion)
{
    // MISB ST 0903.6 Section 9.2.3.1 Example:
    // 3 rows by 4 columns.
    // Yellow pixel at position (Col=3, Row=2) in frameWidth 4 => pixel number 7.
    const PixelCoord coord { 3U, 2U };
    const std::uint32_t frameWidth = 4U;
    const std::uint32_t pixNum = coord.toPixelNumber(frameWidth);
    EXPECT_EQ(pixNum, 7U);

    const PixelCoord reconstructed = PixelCoord::fromPixelNumber(pixNum, frameWidth);
    EXPECT_EQ(reconstructed.col, 3U);
    EXPECT_EQ(reconstructed.row, 2U);

    // Corner cases
    const PixelCoord origin { 1U, 1U };
    EXPECT_EQ(origin.toPixelNumber(frameWidth), 1U);
    const PixelCoord reconstructedOrigin = PixelCoord::fromPixelNumber(1U, frameWidth);
    EXPECT_EQ(reconstructedOrigin.col, 1U);
    EXPECT_EQ(reconstructedOrigin.row, 1U);

    // HD 1920x1080 bottom-right pixel
    const PixelCoord hdEnd { 1920U, 1080U };
    const std::uint32_t hdEndNum = hdEnd.toPixelNumber(1920U);
    EXPECT_EQ(hdEndNum, 1920U * 1080U);
    const PixelCoord reconstructedHd = PixelCoord::fromPixelNumber(hdEndNum, 1920U);
    EXPECT_EQ(reconstructedHd.col, 1920U);
    EXPECT_EQ(reconstructedHd.row, 1080U);
}

TEST(VmtiTest, OffsetAndHaeScaling)
{
    // MISB ST 0903.6 Section 10.2.2.11 example:
    // 10.00 deg -> IMAPB(-19.2, 19.2, 3) = 0x3A6667
    const double latOffset = 10.0;
    const std::uint32_t scaledLat = VmtiEncoder::scaleOffset(latOffset);
    EXPECT_NEAR(static_cast<double>(scaledLat), static_cast<double>(0x3A6667U), 1.0);
    const double unscaledLat = VmtiParser::unscaleOffset(scaledLat);
    EXPECT_NEAR(unscaledLat, latOffset, 1e-4);

    // MISB ST 0903.6 Section 10.2.2.12 example:
    // 12.00 deg -> IMAPB(-19.2, 19.2, 3) = 0x3E6667
    const double lonOffset = 12.0;
    const std::uint32_t scaledLon = VmtiEncoder::scaleOffset(lonOffset);
    EXPECT_NEAR(static_cast<double>(scaledLon), static_cast<double>(0x3E6667U), 1.0);
    const double unscaledLon = VmtiParser::unscaleOffset(scaledLon);
    EXPECT_NEAR(unscaledLon, lonOffset, 1e-4);

    // MISB ST 0903.6 Section 10.2.2.13 example:
    // 10000 m HAE -> IMAPB(-900, 19000, 2) = 0x2A94
    const double hae = 10000.0;
    const std::uint16_t scaledHae = VmtiEncoder::scaleHae(hae);
    EXPECT_EQ(scaledHae, 0x2A94U);
    const double unscaledHae = VmtiParser::unscaleHae(scaledHae);
    EXPECT_NEAR(unscaledHae, hae, 0.5);
}

TEST(VmtiTest, StandaloneVmtiRoundTrip)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1695500000999999ULL;
    original.systemName = "AI-TRACKER-0903";
    original.version = 6U;
    original.totalTargetsDetected = 2U;
    original.numTargetsReported = 2U;
    original.frameWidth = 1920U;
    original.frameHeight = 1080U;

    VTargetPack target1;
    target1.targetId = 42U;
    target1.centroid = PixelCoord { 960U, 540U };
    target1.boundingBox = PixelBoundingBox { PixelCoord { 920U, 500U }, PixelCoord { 1000U, 580U } };
    target1.priority = static_cast<std::uint8_t>(1U);
    target1.confidence = static_cast<std::uint8_t>(94U);
    target1.history = static_cast<std::uint16_t>(120U);
    target1.percentagePixels = static_cast<std::uint8_t>(85U);
    target1.colorRgb = std::array<std::uint8_t, 3> { 0xFF, 0x00, 0x00 };
    target1.targetIntensity = 310.15F; // 37.0 C in Kelvin
    target1.locationOffsetDeg = GeoPoint2D { 0.005, -0.008 };
    target1.heightAboveEllipsoidM = 125.0;
    target1.detectionStatus = static_cast<std::uint8_t>(1U); // Active-Moving
    original.targets.push_back(target1);

    VTargetPack target2;
    target2.targetId = 108U;
    target2.centroid = PixelCoord { 300U, 400U };
    target2.confidence = static_cast<std::uint8_t>(88U);
    target2.targetLocation = GeoPoint3D { 37.7749, -122.4194, 250.0 };
    target2.detectionStatus = static_cast<std::uint8_t>(2U); // Active-Stopped
    original.targets.push_back(target2);

    const std::vector<std::uint8_t> encoded = VmtiEncoder::encode(original, true);
    ASSERT_FALSE(encoded.empty());

    EXPECT_TRUE(VmtiParser::isVmti(encoded.data(), encoded.size()));

    VmtiLocalSet decoded;
    const KlvStatus status = VmtiParser::parse(encoded.data(), encoded.size(), decoded, true);
    EXPECT_EQ(status, KlvStatus::Success);

    ASSERT_TRUE(decoded.precisionTimeStampUs.has_value());
    EXPECT_EQ(*decoded.precisionTimeStampUs, *original.precisionTimeStampUs);
    ASSERT_TRUE(decoded.systemName.has_value());
    EXPECT_EQ(*decoded.systemName, *original.systemName);
    EXPECT_EQ(decoded.version, original.version);
    ASSERT_TRUE(decoded.totalTargetsDetected.has_value());
    EXPECT_EQ(*decoded.totalTargetsDetected, 2U);
    ASSERT_TRUE(decoded.numTargetsReported.has_value());
    EXPECT_EQ(*decoded.numTargetsReported, 2U);
    EXPECT_EQ(decoded.frameWidth, 1920U);
    EXPECT_EQ(decoded.frameHeight, 1080U);

    ASSERT_EQ(decoded.targets.size(), 2U);

    const auto& decT1 = decoded.targets[0];
    EXPECT_EQ(decT1.targetId, 42U);
    ASSERT_TRUE(decT1.centroid.has_value());
    EXPECT_EQ(decT1.centroid->col, 960U);
    EXPECT_EQ(decT1.centroid->row, 540U);
    ASSERT_TRUE(decT1.boundingBox.has_value());
    EXPECT_EQ(decT1.boundingBox->topLeft.col, 920U);
    EXPECT_EQ(decT1.boundingBox->topLeft.row, 500U);
    EXPECT_EQ(decT1.boundingBox->bottomRight.col, 1000U);
    EXPECT_EQ(decT1.boundingBox->bottomRight.row, 580U);
    ASSERT_TRUE(decT1.confidence.has_value());
    EXPECT_EQ(*decT1.confidence, 94U);
    ASSERT_TRUE(decT1.priority.has_value());
    EXPECT_EQ(*decT1.priority, 1U);
    ASSERT_TRUE(decT1.history.has_value());
    EXPECT_EQ(*decT1.history, 120U);
    ASSERT_TRUE(decT1.colorRgb.has_value());
    EXPECT_EQ((*decT1.colorRgb)[0], 0xFF);
    EXPECT_EQ((*decT1.colorRgb)[1], 0x00);
    EXPECT_EQ((*decT1.colorRgb)[2], 0x00);
    ASSERT_TRUE(decT1.targetIntensity.has_value());
    EXPECT_NEAR(*decT1.targetIntensity, 310.15F, 1e-4F);
    ASSERT_TRUE(decT1.locationOffsetDeg.has_value());
    EXPECT_NEAR(decT1.locationOffsetDeg->latitudeDeg, 0.005, 1e-4);
    EXPECT_NEAR(decT1.locationOffsetDeg->longitudeDeg, -0.008, 1e-4);
    ASSERT_TRUE(decT1.heightAboveEllipsoidM.has_value());
    EXPECT_NEAR(*decT1.heightAboveEllipsoidM, 125.0, 1.0);
    ASSERT_TRUE(decT1.detectionStatus.has_value());
    EXPECT_EQ(*decT1.detectionStatus, 1U);

    const auto& decT2 = decoded.targets[1];
    EXPECT_EQ(decT2.targetId, 108U);
    ASSERT_TRUE(decT2.centroid.has_value());
    EXPECT_EQ(decT2.centroid->col, 300U);
    EXPECT_EQ(decT2.centroid->row, 400U);
    ASSERT_TRUE(decT2.targetLocation.has_value());
    EXPECT_NEAR(decT2.targetLocation->latitudeDeg, 37.7749, 1e-5);
    EXPECT_NEAR(decT2.targetLocation->longitudeDeg, -122.4194, 1e-5);
    EXPECT_NEAR(decT2.targetLocation->altitudeM, 250.0, 1.0);
    ASSERT_TRUE(decT2.detectionStatus.has_value());
    EXPECT_EQ(*decT2.detectionStatus, 2U);
}

TEST(VmtiTest, EmbeddedInMisb0601RoundTrip)
{
    UasDatalinkMessage msg;
    msg.precisionTimeStampUs = 1695500000123456ULL;
    msg.missionId = "VMTI-AIR-TEST";
    msg.platformHeadingDeg = 90.0;
    msg.sensorLatitudeDeg = 38.0;
    msg.sensorLongitudeDeg = 24.0;
    msg.sensorTrueAltitudeM = 1000.0;
    msg.frameCenterLatDeg = 38.01;
    msg.frameCenterLonDeg = 24.01;
    msg.wavelengthBands = static_cast<std::uint8_t>(0x10U); // LWIR band (MISB ST 0601 Tag 95)

    VmtiLocalSet vmti;
    vmti.precisionTimeStampUs = msg.precisionTimeStampUs;
    vmti.systemName = "EMBEDDED_VMTI";
    vmti.totalTargetsDetected = 1U;
    vmti.numTargetsReported = 1U;
    vmti.frameWidth = 1920U;
    vmti.frameHeight = 1080U;

    VTargetPack target;
    target.targetId = 1U;
    target.centroid = PixelCoord { 640U, 480U };
    target.confidence = static_cast<std::uint8_t>(99U);
    target.locationOffsetDeg = GeoPoint2D { 0.001, 0.002 };
    vmti.targets.push_back(target);

    msg.vmti = vmti;

    const std::vector<std::uint8_t> buffer = KlvEncoder::encode(msg);
    ASSERT_FALSE(buffer.empty());

    UasDatalinkMessage parsedMsg;
    const KlvStatus parseStatus = KlvParser::parse(buffer.data(), buffer.size(), parsedMsg);
    EXPECT_EQ(parseStatus, KlvStatus::Success);

    ASSERT_TRUE(parsedMsg.wavelengthBands.has_value());
    EXPECT_EQ(*parsedMsg.wavelengthBands, 0x10U);

    ASSERT_TRUE(parsedMsg.vmti.has_value());
    EXPECT_EQ(parsedMsg.vmti->systemName.value_or(""), "EMBEDDED_VMTI");
    EXPECT_EQ(parsedMsg.vmti->frameWidth, 1920U);
    EXPECT_EQ(parsedMsg.vmti->frameHeight, 1080U);
    ASSERT_EQ(parsedMsg.vmti->targets.size(), 1U);
    EXPECT_EQ(parsedMsg.vmti->targets[0].targetId, 1U);
    ASSERT_TRUE(parsedMsg.vmti->targets[0].centroid.has_value());
    EXPECT_EQ(parsedMsg.vmti->targets[0].centroid->col, 640U);
    EXPECT_EQ(parsedMsg.vmti->targets[0].centroid->row, 480U);
    ASSERT_TRUE(parsedMsg.vmti->targets[0].confidence.has_value());
    EXPECT_EQ(*parsedMsg.vmti->targets[0].confidence, 99U);
}

TEST(VmtiTest, VmtiParseWithTrailingPadding)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1695500000888888ULL;
    original.systemName = "PADDING_TEST";
    original.version = 6U;
    original.totalTargetsDetected = 1U;
    original.numTargetsReported = 1U;

    VTargetPack target;
    target.targetId = 7U;
    target.centroid = PixelCoord { 100U, 200U };
    original.targets.push_back(target);

    std::vector<std::uint8_t> packet = VmtiEncoder::encode(original, true);
    ASSERT_FALSE(packet.empty());

    // Append 64 bytes of TS padding (0xFF)
    packet.insert(packet.end(), 64U, 0xFFU);

    VmtiLocalSet decoded;
    const KlvStatus status = VmtiParser::parse(packet.data(), packet.size(), decoded, true);
    EXPECT_EQ(status, KlvStatus::Success);
    ASSERT_TRUE(decoded.systemName.has_value());
    EXPECT_EQ(*decoded.systemName, "PADDING_TEST");
    ASSERT_EQ(decoded.targets.size(), 1U);
    EXPECT_EQ(decoded.targets[0].targetId, 7U);
}

TEST(VmtiTest, VmtiExtendedTag17AndSensorTags)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1695500000777777ULL;
    original.systemName = "SENSOR_TAGS_TEST";
    original.sourceSensor = "EO-NARROW";
    original.horizontalFovDeg = 30.5;
    original.verticalFovDeg = 17.2;

    // Target 1 with 16-byte Tag 17 (Lat, Lon, HAE, CE90, LE90)
    VTargetPack target1;
    target1.targetId = 1U;
    target1.targetLocation = GeoPoint3D { 34.05, -118.25, 150.0 };
    target1.targetCe90M = 12.0;
    target1.targetLe90M = 8.0;
    original.targets.push_back(target1);

    // Target 2 with 14-byte Tag 17 (Lat, Lon, HAE, CE90 only)
    VTargetPack target2;
    target2.targetId = 2U;
    target2.targetLocation = GeoPoint3D { 34.10, -118.30, 200.0 };
    target2.targetCe90M = 25.0;
    original.targets.push_back(target2);

    const std::vector<std::uint8_t> packet = VmtiEncoder::encode(original, true);
    ASSERT_FALSE(packet.empty());

    VmtiLocalSet decoded;
    const KlvStatus status = VmtiParser::parse(packet.data(), packet.size(), decoded, true);
    EXPECT_EQ(status, KlvStatus::Success);

    ASSERT_TRUE(decoded.sourceSensor.has_value());
    EXPECT_EQ(*decoded.sourceSensor, "EO-NARROW");
    ASSERT_TRUE(decoded.horizontalFovDeg.has_value());
    EXPECT_NEAR(*decoded.horizontalFovDeg, 30.5, 0.01);
    ASSERT_TRUE(decoded.verticalFovDeg.has_value());
    EXPECT_NEAR(*decoded.verticalFovDeg, 17.2, 0.01);

    ASSERT_EQ(decoded.targets.size(), 2U);

    // Verify Target 1 (16 bytes)
    const auto& decT1 = decoded.targets[0];
    EXPECT_EQ(decT1.targetId, 1U);
    ASSERT_TRUE(decT1.targetLocation.has_value());
    EXPECT_NEAR(decT1.targetLocation->latitudeDeg, 34.05, 1e-4);
    EXPECT_NEAR(decT1.targetLocation->longitudeDeg, -118.25, 1e-4);
    EXPECT_NEAR(decT1.targetLocation->altitudeM, 150.0, 1.0);
    ASSERT_TRUE(decT1.targetCe90M.has_value());
    EXPECT_NEAR(*decT1.targetCe90M, 12.0, 0.5);
    ASSERT_TRUE(decT1.targetLe90M.has_value());
    EXPECT_NEAR(*decT1.targetLe90M, 8.0, 0.5);

    // Verify Target 2 (14 bytes)
    const auto& decT2 = decoded.targets[1];
    EXPECT_EQ(decT2.targetId, 2U);
    ASSERT_TRUE(decT2.targetLocation.has_value());
    EXPECT_NEAR(decT2.targetLocation->latitudeDeg, 34.10, 1e-4);
    EXPECT_NEAR(decT2.targetLocation->longitudeDeg, -118.30, 1e-4);
    EXPECT_NEAR(decT2.targetLocation->altitudeM, 200.0, 1.0);
    ASSERT_TRUE(decT2.targetCe90M.has_value());
    EXPECT_NEAR(*decT2.targetCe90M, 25.0, 0.5);
    EXPECT_FALSE(decT2.targetLe90M.has_value());
}

TEST(VmtiTest, VTrackTag102KinematicsAndHistoryRoundTrip)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1700000000123456ULL;
    original.systemName = "TRACK-ORCHESTRATOR";
    original.version = 6U;
    original.frameWidth = 1920U;
    original.frameHeight = 1080U;

    VTrackPack track1;
    track1.trackId = 101U;
    track1.status = TrackStatus::Active;
    track1.confidence = 92U;
    track1.velocityEastMps = 15.5; // m/s
    track1.velocityNorthMps = -22.3; // m/s
    track1.velocityUpMps = 1.2; // m/s
    track1.groundCourseDeg = 145.2; // deg
    track1.groundSpeedMps = 27.15; // m/s
    track1.accelerationEastMps2 = 0.85; // m/s^2
    track1.accelerationNorthMps2 = -1.15; // m/s^2
    track1.trackStartTimeUs = 1699999000000000ULL;
    track1.trackDurationUs = 1000123U;

    TrackHistoryPoint pt1;
    pt1.timestampUs = 1699999900000000ULL;
    pt1.pixelLocation = PixelCoord { 500U, 400U };
    pt1.geodeticLocation = GeoPoint3D { 37.7749, -122.4194, 150.0 };
    track1.historyTrail.push_back(pt1);

    TrackHistoryPoint pt2;
    pt2.timestampUs = 1700000000000000ULL;
    pt2.pixelLocation = PixelCoord { 520U, 410U };
    pt2.geodeticLocation = GeoPoint3D { 37.7751, -122.4192, 151.0 };
    track1.historyTrail.push_back(pt2);

    original.tracks.push_back(track1);

    // Round-trip standalone packet
    const std::vector<std::uint8_t> packet = VmtiEncoder::encode(original, true);
    ASSERT_FALSE(packet.empty());

    VmtiLocalSet decoded;
    const KlvStatus status = VmtiParser::parse(packet.data(), packet.size(), decoded, true);
    EXPECT_EQ(status, KlvStatus::Success);

    ASSERT_EQ(decoded.tracks.size(), 1U);
    const auto& decTrack = decoded.tracks[0];
    EXPECT_EQ(decTrack.trackId, 101U);
    EXPECT_EQ(decTrack.status, TrackStatus::Active);
    EXPECT_EQ(decTrack.confidence, 92U);

    ASSERT_TRUE(decTrack.velocityEastMps.has_value());
    EXPECT_NEAR(*decTrack.velocityEastMps, 15.5, 0.05);

    ASSERT_TRUE(decTrack.velocityNorthMps.has_value());
    EXPECT_NEAR(*decTrack.velocityNorthMps, -22.3, 0.05);

    ASSERT_TRUE(decTrack.velocityUpMps.has_value());
    EXPECT_NEAR(*decTrack.velocityUpMps, 1.2, 0.05);

    ASSERT_TRUE(decTrack.groundCourseDeg.has_value());
    EXPECT_NEAR(*decTrack.groundCourseDeg, 145.2, 0.05);

    ASSERT_TRUE(decTrack.groundSpeedMps.has_value());
    EXPECT_NEAR(*decTrack.groundSpeedMps, 27.15, 0.05);

    ASSERT_TRUE(decTrack.accelerationEastMps2.has_value());
    EXPECT_NEAR(*decTrack.accelerationEastMps2, 0.85, 0.05);

    ASSERT_TRUE(decTrack.accelerationNorthMps2.has_value());
    EXPECT_NEAR(*decTrack.accelerationNorthMps2, -1.15, 0.05);

    ASSERT_TRUE(decTrack.trackStartTimeUs.has_value());
    EXPECT_EQ(*decTrack.trackStartTimeUs, 1699999000000000ULL);

    ASSERT_TRUE(decTrack.trackDurationUs.has_value());
    EXPECT_EQ(*decTrack.trackDurationUs, 1000123U);

    ASSERT_EQ(decTrack.historyTrail.size(), 2U);
    EXPECT_EQ(decTrack.historyTrail[0].timestampUs, 1699999900000000ULL);
    EXPECT_EQ(decTrack.historyTrail[0].pixelLocation.col, 500U);
    EXPECT_EQ(decTrack.historyTrail[0].pixelLocation.row, 400U);
    ASSERT_TRUE(decTrack.historyTrail[0].geodeticLocation.has_value());
    EXPECT_NEAR(decTrack.historyTrail[0].geodeticLocation->latitudeDeg, 37.7749, 1e-4);
    EXPECT_NEAR(decTrack.historyTrail[0].geodeticLocation->longitudeDeg, -122.4194, 1e-4);

    EXPECT_EQ(decTrack.historyTrail[1].pixelLocation.col, 520U);
    EXPECT_EQ(decTrack.historyTrail[1].pixelLocation.row, 410U);
}

TEST(VmtiTest, OntologyTag103TaxonomyRoundTrip)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1700000000999999ULL;
    original.systemName = "ATR-CLASSIFIER";
    original.version = 6U;

    OntologyLocalSet ont1;
    ont1.ontologyId = 1U;
    ont1.ontologyUri = "urn:nato:stanag:1241";
    ont1.category = StanagCategory::TrackedArmor;
    ont1.classLabel = "Main Battle Tank T-72";
    ont1.confidence = 95U;
    ont1.sidcCode = "SFGPUCA----";
    ont1.algorithmId = 42U;
    original.ontologies.push_back(ont1);

    OntologyLocalSet ont2;
    ont2.ontologyId = 2U;
    ont2.ontologyUri = "urn:nato:stanag:4676";
    ont2.category = StanagCategory::WheeledVehicle;
    ont2.classLabel = "Military Supply Truck";
    ont2.confidence = 88U;
    ont2.sidcCode = "SFGPUCV----";
    original.ontologies.push_back(ont2);

    const std::vector<std::uint8_t> packet = VmtiEncoder::encode(original, true);
    ASSERT_FALSE(packet.empty());

    VmtiLocalSet decoded;
    const KlvStatus status = VmtiParser::parse(packet.data(), packet.size(), decoded, true);
    EXPECT_EQ(status, KlvStatus::Success);

    ASSERT_EQ(decoded.ontologies.size(), 2U);

    EXPECT_EQ(decoded.ontologies[0].ontologyId, 1U);
    EXPECT_EQ(decoded.ontologies[0].ontologyUri, "urn:nato:stanag:1241");
    EXPECT_EQ(decoded.ontologies[0].category, StanagCategory::TrackedArmor);
    EXPECT_EQ(decoded.ontologies[0].classLabel, "Main Battle Tank T-72");
    EXPECT_EQ(decoded.ontologies[0].confidence, 95U);
    EXPECT_EQ(decoded.ontologies[0].sidcCode, "SFGPUCA----");
    ASSERT_TRUE(decoded.ontologies[0].algorithmId.has_value());
    EXPECT_EQ(*decoded.ontologies[0].algorithmId, 42U);

    EXPECT_EQ(decoded.ontologies[1].ontologyId, 2U);
    EXPECT_EQ(decoded.ontologies[1].ontologyUri, "urn:nato:stanag:4676");
    EXPECT_EQ(decoded.ontologies[1].category, StanagCategory::WheeledVehicle);
    EXPECT_EQ(decoded.ontologies[1].classLabel, "Military Supply Truck");
    EXPECT_EQ(decoded.ontologies[1].confidence, 88U);
    EXPECT_EQ(decoded.ontologies[1].sidcCode, "SFGPUCV----");
    EXPECT_FALSE(decoded.ontologies[1].algorithmId.has_value());
}

TEST(VmtiTest, CompositeMultiFrameTags101And102And103)
{
    VmtiLocalSet original;
    original.precisionTimeStampUs = 1700000001000000ULL;
    original.version = 6U;
    original.frameWidth = 1920U;
    original.frameHeight = 1080U;

    // Tag 101: Single-frame detection chip
    VTargetPack target;
    target.targetId = 7U;
    target.centroid = PixelCoord { 960U, 540U };
    target.priority = 1U;
    target.confidence = 90U;
    original.targets.push_back(target);

    // Tag 102: Persistent track with velocity
    VTrackPack track;
    track.trackId = 7U;
    track.status = TrackStatus::Active;
    track.confidence = 90U;
    track.velocityEastMps = 10.0;
    track.velocityNorthMps = 5.0;
    track.groundSpeedMps = 11.18;
    track.groundCourseDeg = 63.4;
    original.tracks.push_back(track);

    // Tag 103: STANAG taxonomy
    OntologyLocalSet ontology;
    ontology.ontologyId = 7U;
    ontology.category = StanagCategory::Dismount;
    ontology.classLabel = "Infantry Squad";
    ontology.confidence = 85U;
    original.ontologies.push_back(ontology);

    // Test both Standalone and Embedded
    const std::vector<std::uint8_t> standalone = VmtiEncoder::encode(original, true);
    VmtiLocalSet decodedStandalone;
    EXPECT_EQ(VmtiParser::parse(standalone.data(), standalone.size(), decodedStandalone, true), KlvStatus::Success);
    EXPECT_EQ(decodedStandalone.targets.size(), 1U);
    EXPECT_EQ(decodedStandalone.tracks.size(), 1U);
    EXPECT_EQ(decodedStandalone.ontologies.size(), 1U);

    const std::vector<std::uint8_t> embedded = VmtiEncoder::encode(original, false);
    VmtiLocalSet decodedEmbedded;
    EXPECT_EQ(VmtiParser::parse(embedded.data(), embedded.size(), decodedEmbedded, false), KlvStatus::Success);
    EXPECT_EQ(decodedEmbedded.targets.size(), 1U);
    EXPECT_EQ(decodedEmbedded.tracks.size(), 1U);
    EXPECT_EQ(decodedEmbedded.ontologies.size(), 1U);
}
