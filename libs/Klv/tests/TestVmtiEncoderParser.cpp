#include "VmtiEncoder.h"
#include "VmtiParser.h"
#include "KlvEncoder.h"
#include "KlvParser.h"
#include "VmtiTypes.h"
#include <gtest/gtest.h>

using namespace Klv;

TEST(VmtiTest, PixelCoordRowMajorConversion) {
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

TEST(VmtiTest, OffsetAndHaeScaling) {
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

TEST(VmtiTest, StandaloneVmtiRoundTrip) {
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

TEST(VmtiTest, EmbeddedInMisb0601RoundTrip) {
    UasDatalinkMessage msg;
    msg.precisionTimeStampUs = 1695500000123456ULL;
    msg.missionId = "VMTI-AIR-TEST";
    msg.platformHeadingDeg = 90.0;
    msg.sensorLatitudeDeg = 38.0;
    msg.sensorLongitudeDeg = 24.0;
    msg.sensorTrueAltitudeM = 1000.0;
    msg.frameCenterLatDeg = 38.01;
    msg.frameCenterLonDeg = 24.01;

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
