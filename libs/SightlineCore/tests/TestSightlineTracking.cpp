/// @file TestSightlineTracking.cpp
/// @brief Unit tests for Sightline tracking, motion, and MTI detection builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineClassificationBuilder.h"
#include "modules/SightlineClassificationParser.h"
#include "modules/SightlineDetectionBuilder.h"
#include "modules/SightlineDetectionParser.h"
#include "modules/SightlineTrackingBuilder.h"
#include "modules/SightlineTrackingParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify start tracking command serialization.
    TEST(TestSightlineTracking, BuildStartTracking)
    {
        MsgStartTracking msg {};
        msg.cameraIndex = 1U;
        msg.centerCol = 640U;
        msg.centerRow = 480U;
        msg.width = 80U;
        msg.height = 60U;
        msg.flags = 0x01U;
        msg.nearVal = 50U;
        msg.userTrackId = 3U;
        msg.framePts = 999999ULL;

        const auto pkt = SightlineTrackingBuilder::buildStartTracking(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::StartTracking);

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 21U);
        EXPECT_EQ(payload[0], 1U);
        EXPECT_EQ(payload[9], 0x01U);
        EXPECT_EQ(payload[12], 3U);

        // Verify facade equivalence
        const auto facadePkt = SightlineProtocolBuilder::buildStartTracking(msg);
        EXPECT_EQ(pkt, facadePkt);
    }

    /// @brief Verify stop tracking command serialization.
    TEST(TestSightlineTracking, BuildStopTracking)
    {
        MsgStopTracking msg {};
        msg.cameraIndex = 2U;
        msg.trackId = 5U;

        const auto pkt = SightlineTrackingBuilder::buildStopTracking(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::StopTracking);

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 4U);
        EXPECT_EQ(payload[0], 0U);
        EXPECT_EQ(payload[1], 0U);
        EXPECT_EQ(payload[2], 0U);
        EXPECT_EQ(payload[3], 2U);

        // Individual track stopping uses buildModifyTrackIndex
        MsgModifyTrackIndex modMsg {};
        modMsg.trackIndex = 5U;
        modMsg.flags = 0U; // Stop
        modMsg.cameraIndex = 2U;
        const auto modPkt = SightlineTrackingBuilder::buildModifyTrackIndex(modMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(modPkt), MessageId::ModifyTrackIndex);
        const auto modPayload = SightlineFraming::extractPayload(modPkt);
        ASSERT_EQ(modPayload.size(), 7U);
        EXPECT_EQ(modPayload[0], 5U);
        EXPECT_EQ(modPayload[1], 0U);
        EXPECT_EQ(modPayload[2], 2U);
    }

    /// @brief Verify tracking, motion and detection builder serialization.
    TEST(TestSightlineTracking, BuildTrackingMotion)
    {
        MsgNudgeTrackingCoordinate nudgeMsg {};
        nudgeMsg.cameraIndex = 0U;
        nudgeMsg.deltaCol = -2;
        nudgeMsg.deltaRow = 3;
        const auto nudgePkt = SightlineTrackingBuilder::buildNudgeTracking(nudgeMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(nudgePkt), MessageId::NudgeTrackingCoordinate);

        MsgCoordinateReportingMode repMsg {};
        repMsg.cameraIndex = 1U;
        repMsg.framePeriod = 2U;
        repMsg.reportingFlags = 0x07U;
        const auto repPkt = SightlineTrackingBuilder::buildSetReportingMode(repMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(repPkt), MessageId::CoordinateReportingMode);

        MsgStartTracking startMsg {};
        startMsg.cameraIndex = 0U;
        startMsg.centerCol = 320U;
        startMsg.centerRow = 240U;
        startMsg.width = 64U;
        startMsg.height = 48U;
        startMsg.flags = 0x01U;
        startMsg.nearVal = 100U;
        startMsg.userTrackId = 5U;
        startMsg.framePts = 12345678ULL;
        const auto startPkt = SightlineTrackingBuilder::buildStartTracking(startMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(startPkt), MessageId::StartTracking);
        const auto startPayload = SightlineFraming::extractPayload(startPkt);
        ASSERT_EQ(startPayload.size(), 21U);
        EXPECT_EQ(startPayload[0], 0U);
        EXPECT_EQ(startPayload[9], 0x01U);
        EXPECT_EQ(startPayload[12], 5U);

        MsgSetTrackingParameters trkParams {};
        trkParams.objectSize = 32U;
        trkParams.mode = 1U;
        trkParams.cameraIndex = 0U;
        trkParams.acquisitionSearchCol = 200U;
        trkParams.acquisitionSearchRow = 150U;
        trkParams.flags = 0x02U;
        const auto trkPkt = SightlineTrackingBuilder::buildSetTrackingParameters(trkParams);
        EXPECT_EQ(SightlineFraming::identifyMessage(trkPkt), MessageId::SetTrackingParameters);
        const auto trkPayload = SightlineFraming::extractPayload(trkPkt);
        ASSERT_EQ(trkPayload.size(), 16U);
        EXPECT_EQ(trkPayload[0], 32U);
        EXPECT_EQ(trkPayload[1], 1U);
        EXPECT_EQ(trkPayload[6], 0U); // cameraIndex
        EXPECT_EQ(trkPayload[15], 0x02U); // flags

        MsgDesignateSelectedTrackPrimary desMsg {};
        desMsg.cameraIndex = 0U;
        desMsg.trackId = 3U;
        const auto desPkt = SightlineTrackingBuilder::buildDesignatePrimary(desMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(desPkt), MessageId::DesignateSelectedTrackPrimary);

        MsgShiftSelectedTrack shiftMsg {};
        shiftMsg.cameraIndex = 0U;
        shiftMsg.trackId = 2U;
        shiftMsg.shiftCol = 5;
        shiftMsg.shiftRow = -4;
        const auto shiftPkt = SightlineTrackingBuilder::buildShiftSelectedTrack(shiftMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(shiftPkt), MessageId::ShiftSelectedTrack);

        MsgStopSelectedTrack stopMsg {};
        stopMsg.cameraIndex = 1U;
        stopMsg.trackId = 4U;
        const auto stopPkt = SightlineTrackingBuilder::buildStopSelectedTrack(stopMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(stopPkt), MessageId::StopSelectedTrack);

        MsgSetDetectionParameters detMsg {};
        detMsg.cameraIndex = 0U;
        detMsg.mode = DetectionMode::Drone;
        detMsg.threshold = 30U;
        detMsg.minTargetSize = 8U;
        detMsg.maxTargetSize = 150U;
        const auto detPkt = SightlineTrackingBuilder::buildSetDetectionParams(detMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(detPkt), MessageId::SetDetectionParameters);

        MsgCustomAIDetect aiMsg {};
        aiMsg.cameraIndex = 0U;
        aiMsg.modelId = 1U;
        aiMsg.confidenceThreshold = 75U;
        aiMsg.nmsThreshold = 50U;
        const auto aiPkt = SightlineTrackingBuilder::buildCustomAIDetect(aiMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(aiPkt), MessageId::CustomAIDetect);
    }

    /// @brief Verify tracking positions telemetry parsing (0x51).
    TEST(TestSightlineTracking, ParseTrackingPositions)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // byte 0: cameraIndex
        payload.push_back(1U); // byte 1: numTracks = 1

        // Track 0 (15-byte stride)
        payload.push_back(10U); // byte 0: trackId
        payload.push_back(0x40U); // bytes 1..2: col: 320
        payload.push_back(0x01U);
        payload.push_back(0xF0U); // bytes 3..4: row: 240
        payload.push_back(0x00U);
        payload.push_back(64U); // bytes 5..6: width: 64
        payload.push_back(0U);
        payload.push_back(48U); // bytes 7..8: height: 48
        payload.push_back(0U);
        payload.push_back(0x00U); // bytes 9..10: velCol8: 512 (2.0 pixels/frame)
        payload.push_back(0x02U);
        payload.push_back(0x00U); // bytes 11..12: velRow8: -256 (-1.0 pixel/frame)
        payload.push_back(0xFFU);
        payload.push_back(95U); // byte 13: confidence: 95
        payload.push_back(0x01U); // byte 14: flags: primary

        // Optional trailer (12 bytes: 8-byte timestamp + 4-byte frameId)
        const std::uint64_t ts { 1000000ULL };
        for (std::size_t i { 0U }; i < 8U; ++i) {
            payload.push_back(static_cast<std::uint8_t>((ts >> (i * 8U)) & 0xFFU));
        }

        const std::uint32_t fn { 42U };
        for (std::size_t i { 0U }; i < 4U; ++i) {
            payload.push_back(static_cast<std::uint8_t>((fn >> (i * 8U)) & 0xFFU));
        }

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPositions, payload);

        MsgTrackingPositions out {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingPositions(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.timestampUs, 1000000ULL);
        EXPECT_EQ(out.frameNumber, 42U);
        ASSERT_EQ(out.tracks.size(), 1U);
        EXPECT_EQ(out.tracks[0].trackId, 10U);
        EXPECT_DOUBLE_EQ(out.tracks[0].centerCol, 320.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].centerRow, 240.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].width, 64.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].height, 48.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].velocityCol, 2.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].velocityRow, -1.0);
        EXPECT_EQ(out.tracks[0].confidence, 95U);
        EXPECT_TRUE(out.tracks[0].isPrimary);

        // Verify facade equivalence
        MsgTrackingPositions facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingPositions(pkt, facadeOut));
        EXPECT_EQ(facadeOut.frameNumber, 42U);
    }

    /// @brief Verify single tracking position telemetry parsing (0x43).
    TEST(TestSightlineTracking, ParseTrackingPosition)
    {
        std::vector<std::uint8_t> payload {};
        // col: 320 (0x0140) (bytes 0..1)
        payload.push_back(0x40U);
        payload.push_back(0x01U);
        // row: 240 (0x00F0) (bytes 2..3)
        payload.push_back(0xF0U);
        payload.push_back(0x00U);
        // translationCol (sceneCol): 256 (bytes 4..5)
        payload.push_back(0x00U);
        payload.push_back(0x01U);
        // translationRow (sceneRow): 512 (bytes 6..7)
        payload.push_back(0x00U);
        payload.push_back(0x02U);
        // offsetCol: 0 (bytes 8..9)
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        // offsetRow: 0 (bytes 10..11)
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        // confidence: 99 (byte 12)
        payload.push_back(99U);
        // sceneConfidence: 80 (byte 13)
        payload.push_back(80U);
        // rotation: 1280 (= 10.0 deg, rotation / 128.0) (bytes 14..15)
        payload.push_back(0x00U);
        payload.push_back(0x05U);
        // cameraIndex: 0 (byte 16)
        payload.push_back(0U);
        // userTrackId: 1 (byte 17)
        payload.push_back(1U);
        // sceneColFrac8: 0 (byte 18)
        payload.push_back(0U);
        // sceneRowFrac8: 0 (byte 19)
        payload.push_back(0U);
        // sceneAngle7: 0 (bytes 20..21)
        payload.push_back(0U);
        payload.push_back(0U);
        // scale (sceneScale8): 256 (= 1.0, scale / 256.0) (bytes 22..23)
        payload.push_back(0x00U);
        payload.push_back(0x01U);

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPosition, payload);

        MsgTrackingPosition out {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingPosition(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_DOUBLE_EQ(out.col, 320.0);
        EXPECT_DOUBLE_EQ(out.row, 240.0);
        EXPECT_DOUBLE_EQ(out.translationCol, 256.0);
        EXPECT_DOUBLE_EQ(out.translationRow, 512.0);
        EXPECT_DOUBLE_EQ(out.rotationDeg, 10.0);
        EXPECT_DOUBLE_EQ(out.scale, 1.0);
        EXPECT_EQ(out.confidence, 99U);

        // Verify facade equivalence
        MsgTrackingPosition facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingPosition(pkt, facadeOut));
        EXPECT_DOUBLE_EQ(facadeOut.col, 320.0);
    }

    /// @brief Verify extended tracking positions with AI class telemetry parsing (0xA0).
    TEST(TestSightlineTracking, ParsePositionsExtended)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // byte 0: cameraIndex
        payload.push_back(1U); // byte 1: numTracks = 1

        // Track 0 (21-byte stride)
        payload.push_back(1U); // byte 0: trackId
        payload.push_back(0x64U); // bytes 1..2: col: 100
        payload.push_back(0x00U);
        payload.push_back(0x96U); // bytes 3..4: row: 150
        payload.push_back(0x00U);
        payload.push_back(32U); // bytes 5..6: width: 32
        payload.push_back(0U);
        payload.push_back(32U); // bytes 7..8: height: 32
        payload.push_back(0U);
        payload.push_back(0x00U); // bytes 9..10: velCol8: 512 (2.0 pixels/frame)
        payload.push_back(0x02U);
        payload.push_back(0x00U); // bytes 11..12: velRow8: -256 (-1.0 pixel/frame)
        payload.push_back(0xFFU);
        payload.push_back(90U); // byte 13: confidence: 90
        payload.push_back(0x01U); // byte 14: primary flag
        payload.push_back(3U); // byte 15: classifierLabel (e.g. Vehicle)
        payload.push_back(95U); // byte 16: classifierConf
        payload.push_back(1U); // byte 17: userTrackId
        payload.push_back(0U); // byte 18: trackColFrac8
        payload.push_back(0U); // byte 19: trackRowFrac8
        payload.push_back(0U); // byte 20: flags1

        // Optional trailer (12 bytes)
        const std::uint64_t ts { 2000000ULL };
        for (std::size_t i { 0U }; i < 8U; ++i) {
            payload.push_back(static_cast<std::uint8_t>((ts >> (i * 8U)) & 0xFFU));
        }

        const std::uint32_t fn { 100U };
        for (std::size_t i { 0U }; i < 4U; ++i) {
            payload.push_back(static_cast<std::uint8_t>((fn >> (i * 8U)) & 0xFFU));
        }

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPositionsExtended, payload);

        MsgTrackingPositionsExtended out {};
        ASSERT_TRUE(SightlineTrackingParser::parsePositionsExtended(pkt, out));
        EXPECT_EQ(out.timestampUs, 2000000ULL);
        EXPECT_EQ(out.frameNumber, 100U);
        ASSERT_EQ(out.tracks.size(), 1U);
        ASSERT_EQ(out.classIds.size(), 1U);
        EXPECT_EQ(out.tracks[0].trackId, 1U);
        EXPECT_DOUBLE_EQ(out.tracks[0].velocityCol, 2.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].velocityRow, -1.0);
        EXPECT_EQ(out.tracks[0].confidence, 90U);
        EXPECT_TRUE(out.tracks[0].isPrimary);
        EXPECT_EQ(out.classIds[0], 3U);

        // Verify facade equivalence
        MsgTrackingPositionsExtended facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parsePositionsExtended(pkt, facadeOut));
        EXPECT_EQ(facadeOut.frameNumber, 100U);
    }

    /// @brief Verify track trails query builder.
    TEST(TestSightlineTracking, TrackTrailsQuery)
    {
        const auto queryPkt = SightlineTrackingBuilder::buildGetTrackTrails(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetTrackTrails(0U));
    }

    /// @brief Verify VMTI configuration builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseVMTI)
    {
        MsgSetVMTI msg {};
        msg.cameraIndex = 1U;
        msg.enable = 1U;
        msg.sensitivity = 75U;
        msg.minTargetArea = 16U;
        msg.maxTargetArea = 1024U;
        msg.mode = 2U;

        const auto pkt = SightlineDetectionBuilder::buildSetVMTI(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetVMTI);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetVMTI(msg));

        MsgSetVMTI out {};
        ASSERT_TRUE(SightlineDetectionParser::parseVMTI(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.enable, 1U);
        EXPECT_EQ(out.sensitivity, 75U);
        EXPECT_EQ(out.minTargetArea, 16U);
        EXPECT_EQ(out.maxTargetArea, 1024U);
        EXPECT_EQ(out.mode, 2U);

        MsgSetVMTI facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVMTI(pkt, facadeOut));
        EXPECT_EQ(facadeOut.sensitivity, 75U);

        const auto queryPkt = SightlineDetectionBuilder::buildGetVMTI(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetVMTI(1U));
    }

    /// @brief Verify detection ROI builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseDetectionROI)
    {
        MsgDetectionROI msg {};
        msg.cameraIndex = 0U;
        msg.roiIndex = 2U;
        msg.roiType = 1U; // Exclusion
        msg.left = 100U;
        msg.top = 200U;
        msg.width = 400U;
        msg.height = 300U;

        const auto pkt = SightlineDetectionBuilder::buildSetDetectionROI(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetDetectionRegionOfInterestParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetDetectionROI(msg));

        MsgDetectionROI out {};
        ASSERT_TRUE(SightlineDetectionParser::parseDetectionROI(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.roiIndex, 2U);
        EXPECT_EQ(out.roiType, 1U);
        EXPECT_EQ(out.left, 100U);
        EXPECT_EQ(out.top, 200U);
        EXPECT_EQ(out.width, 400U);
        EXPECT_EQ(out.height, 300U);

        MsgDetectionROI facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDetectionROI(pkt, facadeOut));
        EXPECT_EQ(facadeOut.width, 400U);

        const auto queryPkt = SightlineDetectionBuilder::buildGetDetectionROI(0U, 2U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetDetectionROI(0U, 2U));
    }

    /// @brief Verify advanced detection parameters roundtrip.
    TEST(TestSightlineTracking, BuildAndParseAdvDetection)
    {
        MsgAdvancedDetectionParameters msg {};
        msg.cameraIndex = 1U;
        msg.minVelocity = 50U;
        msg.maxVelocity = 5000U;
        msg.persistenceFrames = 5U;
        msg.mergeDistance = 25U;

        const auto pkt = SightlineDetectionBuilder::buildSetAdvDetectionParams(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetAdvancedDetectionParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetAdvDetectionParams(msg));

        MsgAdvancedDetectionParameters out {};
        ASSERT_TRUE(SightlineDetectionParser::parseAdvDetectionParams(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.minVelocity, 50U);
        EXPECT_EQ(out.maxVelocity, 5000U);
        EXPECT_EQ(out.persistenceFrames, 5U);
        EXPECT_EQ(out.mergeDistance, 25U);

        MsgAdvancedDetectionParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseAdvDetectionParams(pkt, facadeOut));
        EXPECT_EQ(facadeOut.maxVelocity, 5000U);

        const auto queryPkt = SightlineDetectionBuilder::buildGetAdvDetectionParams(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetAdvDetectionParams(1U));
    }

    /// @brief Verify tracking gate pixel statistics parser and query builder.
    TEST(TestSightlineTracking, BuildAndParseTrackingPixelStats)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // cameraIndex
        payload.push_back(3U); // trackId
        SightlineFraming::appendU16Le(payload, 32768U); // meanIntensity
        SightlineFraming::appendU16Le(payload, 1024U); // stdDevIntensity
        payload.push_back(10U); // minIntensity
        payload.push_back(250U); // maxIntensity

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingBoxPixelStats, payload);

        MsgTrackingBoxPixelStats out {};
        ASSERT_TRUE(SightlineDetectionParser::parseTrackingPixelStats(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.trackId, 3U);
        EXPECT_EQ(out.meanIntensity, 32768U);
        EXPECT_EQ(out.stdDevIntensity, 1024U);
        EXPECT_EQ(out.minIntensity, 10U);
        EXPECT_EQ(out.maxIntensity, 250U);

        MsgTrackingBoxPixelStats facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingPixelStats(pkt, facadeOut));
        EXPECT_EQ(facadeOut.meanIntensity, 32768U);

        const auto queryPkt = SightlineDetectionBuilder::buildGetTrackingPixelStats(0U, 3U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetTrackingPixelStats(0U, 3U));
    }

    /// @brief Verify parsing of high-bit-depth (14-bit / 16-bit) tracking box pixel statistics.
    TEST(TestSightlineTracking, ParseHighBitDepthTrackingPixelStats)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(1U); // cameraIndex
        payload.push_back(2U); // trackId
        SightlineFraming::appendU16Le(payload, 19500U); // meanIntensity
        SightlineFraming::appendU16Le(payload, 300U);   // stdDevIntensity
        SightlineFraming::appendU16Le(payload, 18870U); // minIntensity (high-bit depth 16-bit)
        SightlineFraming::appendU16Le(payload, 20271U); // maxIntensity (high-bit depth 16-bit)

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingBoxPixelStats, payload);

        MsgTrackingBoxPixelStats out {};
        ASSERT_TRUE(SightlineDetectionParser::parseTrackingPixelStats(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.trackId, 2U);
        EXPECT_EQ(out.meanIntensity, 19500U);
        EXPECT_EQ(out.stdDevIntensity, 300U);
        EXPECT_EQ(out.minIntensity, static_cast<std::uint16_t>(18870U));
        EXPECT_EQ(out.maxIntensity, static_cast<std::uint16_t>(20271U));
    }


    /// @brief Verify automated detection snapshot command builder.
    TEST(TestSightlineTracking, BuildDoDetectSnapShot)
    {
        MsgDoDetectSnapShot msg {};
        msg.cameraIndex = 1U;
        msg.detectionIndex = 4U;

        const auto pkt = SightlineDetectionBuilder::buildDoDetectSnapShot(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::DoDetectSnapShot);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildDoDetectSnapShot(msg));

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 2U);
        EXPECT_EQ(payload[0], 1U);
        EXPECT_EQ(payload[1], 4U);
    }

    /// @brief Verify VMTI thumbnail chips builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseVMTIChips)
    {
        MsgVMTIChips msg {};
        msg.cameraIndex = 0U;
        msg.trackId = 7U;
        msg.chipIndex = 0U;
        msg.totalChips = 1U;
        msg.chipWidth = 32U;
        msg.chipHeight = 32U;
        msg.chipData = { 0xFFU, 0xD8U, 0xFFU, 0xE0U, 0x12U, 0x34U, 0x56U, 0x78U };

        const auto pkt = SightlineClassificationBuilder::buildVMTIChips(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::VMTIChips);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildVMTIChips(msg));

        MsgVMTIChips out {};
        ASSERT_TRUE(SightlineClassificationParser::parseVMTIChips(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.trackId, 7U);
        EXPECT_EQ(out.chipIndex, 0U);
        EXPECT_EQ(out.totalChips, 1U);
        EXPECT_EQ(out.chipWidth, 32U);
        EXPECT_EQ(out.chipHeight, 32U);
        EXPECT_EQ(out.chipData, msg.chipData);

        MsgVMTIChips facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVMTIChips(pkt, facadeOut));
        EXPECT_EQ(facadeOut.chipWidth, 32U);
    }

    /// @brief Verify VMTI fields configuration builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseVMTIFields)
    {
        MsgVMTIFields msg {};
        msg.cameraIndex = 1U;
        msg.enabledFieldsMask = 0x0000000FU;
        msg.reportRate = 2U;

        const auto pkt = SightlineClassificationBuilder::buildSetVMTIFields(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::VMTIFields);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetVMTIFields(msg));

        MsgVMTIFields out {};
        ASSERT_TRUE(SightlineClassificationParser::parseVMTIFields(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.enabledFieldsMask, 0x0000000FU);
        EXPECT_EQ(out.reportRate, 2U);

        MsgVMTIFields facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVMTIFields(pkt, facadeOut));
        EXPECT_EQ(facadeOut.reportRate, 2U);

        const auto queryPkt = SightlineClassificationBuilder::buildGetVMTIFields(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetVMTIFields(1U));
    }

    /// @brief Verify KLV class filter rules builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseKlvClassFilters)
    {
        MsgKlvClassFilters msg {};
        msg.cameraIndex = 0U;
        msg.classMask = 0x003FU;
        msg.minConfidence = 70U;

        const auto pkt = SightlineClassificationBuilder::buildSetKlvClassFilters(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::KlvClassFilters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetKlvClassFilters(msg));

        MsgKlvClassFilters out {};
        ASSERT_TRUE(SightlineClassificationParser::parseKlvClassFilters(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.classMask, 0x003FU);
        EXPECT_EQ(out.minConfidence, 70U);

        MsgKlvClassFilters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseKlvClassFilters(pkt, facadeOut));
        EXPECT_EQ(facadeOut.minConfidence, 70U);

        const auto queryPkt = SightlineClassificationBuilder::buildGetKlvClassFilters(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetKlvClassFilters(0U));
    }

    /// @brief Verify multi-class tracking telemetry builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseTrackingMultiClass)
    {
        MsgTrackingMultiClass msg {};
        msg.cameraIndex = 1U;
        msg.trackId = 5U;
        msg.primaryClass = 2U; // Person
        msg.confidence = 88U;
        msg.flags = 0x01U;

        const auto pkt = SightlineClassificationBuilder::buildTrackingMultiClass(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::TrackingMultiClass);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildTrackingMultiClass(msg));

        MsgTrackingMultiClass out {};
        ASSERT_TRUE(SightlineClassificationParser::parseTrackingMultiClass(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.trackId, 5U);
        EXPECT_EQ(out.primaryClass, 2U);
        EXPECT_EQ(out.confidence, 88U);
        EXPECT_EQ(out.flags, 0x01U);

        MsgTrackingMultiClass facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingMultiClass(pkt, facadeOut));
        EXPECT_EQ(facadeOut.confidence, 88U);
    }

    /// @brief Verify custom neural classifier builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseCustomClassifier)
    {
        MsgCustomClassifier msg {};
        msg.cameraIndex = 0U;
        msg.classifierType = 3U;
        msg.enable = 1U;
        msg.confidence = 65U;

        const auto pkt = SightlineClassificationBuilder::buildSetCustomClassifier(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::CustomClassifier);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetCustomClassifier(msg));

        MsgCustomClassifier out {};
        ASSERT_TRUE(SightlineClassificationParser::parseCustomClassifier(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.classifierType, 3U);
        EXPECT_EQ(out.enable, 1U);
        EXPECT_EQ(out.confidence, 65U);

        MsgCustomClassifier facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseCustomClassifier(pkt, facadeOut));
        EXPECT_EQ(facadeOut.classifierType, 3U);

        const auto queryPkt = SightlineClassificationBuilder::buildGetCustomClassifier(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetCustomClassifier(0U));
    }

    /// @brief Verify classifier parameters builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseClassifierParams)
    {
        MsgClassifierParameters msg {};
        msg.cameraIndex = 1U;
        msg.modelIndex = 2U;
        msg.nmsThreshold = 40U;
        msg.maxDetections = 50U;

        const auto pkt = SightlineClassificationBuilder::buildSetClassifierParams(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::ClassifierParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetClassifierParams(msg));

        MsgClassifierParameters out {};
        ASSERT_TRUE(SightlineClassificationParser::parseClassifierParams(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.modelIndex, 2U);
        EXPECT_EQ(out.nmsThreshold, 40U);
        EXPECT_EQ(out.maxDetections, 50U);

        MsgClassifierParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseClassifierParams(pkt, facadeOut));
        EXPECT_EQ(facadeOut.maxDetections, 50U);

        const auto queryPkt = SightlineClassificationBuilder::buildGetClassifierParams(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetClassifierParams(1U));
    }

    /// @brief Verify extended detection parameters roundtrip with Phase 1 enums and manual sensitivity.
    TEST(TestSightlineTracking, BuildAndParseDetectionParamsExt)
    {
        MsgSetDetectionParameters msg {};
        msg.cameraIndex = 2U;
        msg.detectionIndex = 1U;
        msg.mode = DetectionMode::Gas;
        msg.sensitivityMode = SensitivityMode::Manual;
        msg.threshold = 42U;
        msg.minTargetSize = 12U;
        msg.maxTargetSize = 350U;
        msg.bkgdThreshold = 18U;
        msg.watchFrames = 6U;
        msg.suspiciousScore = 80U;

        const auto pkt = SightlineDetectionBuilder::buildSetDetectionParams(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetDetectionParameters);

        MsgSetDetectionParameters out {};
        ASSERT_TRUE(SightlineDetectionParser::parseDetectionParams(pkt, out));
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.detectionIndex, 1U);
        EXPECT_EQ(out.mode, DetectionMode::Gas);
        EXPECT_EQ(out.sensitivityMode, SensitivityMode::Manual);
        EXPECT_EQ(out.threshold, 42U);
        EXPECT_EQ(out.minTargetSize, 12U);
        EXPECT_EQ(out.maxTargetSize, 350U);
        EXPECT_EQ(out.bkgdThreshold, 18U);
        EXPECT_EQ(out.watchFrames, 6U);
        EXPECT_EQ(out.suspiciousScore, 80U);
    }

    /// @brief Verify extended advanced detection parameters roundtrip for Staring, Aerial, Gas and Overlap.
    TEST(TestSightlineTracking, BuildAndParseAdvDetectionExt)
    {
        MsgAdvancedDetectionParameters msg {};
        msg.cameraIndex = 1U;
        msg.detectionIndex = 1U;
        msg.minVelocity = 100U;
        msg.maxVelocity = 8000U;
        msg.persistenceFrames = 4U;
        msg.mergeDistance = 15U;
        msg.hideOverlapTracks = false;
        msg.detectNearTrack = true;
        msg.averageTimeConstant = 15U;
        msg.edgePenalty = 70U;
        msg.nFramesBack = 8U;
        msg.useRegistration = true;
        msg.updateRate = 48U;
        msg.surroundSize = 35U;
        msg.blobDirection = BlobDirection::Bright;
        msg.use8BitImages = true;
        msg.gasAddOriginal = 200U;
        msg.gasColor = 2U;
        msg.aiIouThreshold = 55U;
        msg.enableMtd = true;
        msg.downsample = DetectionDownsample::Downsample2x;

        const auto pkt = SightlineDetectionBuilder::buildSetAdvDetectionParams(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetAdvancedDetectionParameters);

        MsgAdvancedDetectionParameters out {};
        ASSERT_TRUE(SightlineDetectionParser::parseAdvDetectionParams(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.detectionIndex, 1U);
        EXPECT_EQ(out.minVelocity, 100U);
        EXPECT_EQ(out.maxVelocity, 8000U);
        EXPECT_EQ(out.persistenceFrames, 4U);
        EXPECT_EQ(out.mergeDistance, 15U);
        EXPECT_FALSE(out.hideOverlapTracks);
        EXPECT_TRUE(out.detectNearTrack);
        EXPECT_EQ(out.averageTimeConstant, 15U);
        EXPECT_EQ(out.edgePenalty, 70U);
        EXPECT_EQ(out.nFramesBack, 8U);
        EXPECT_TRUE(out.useRegistration);
        EXPECT_EQ(out.updateRate, 48U);
        EXPECT_EQ(out.surroundSize, 35U);
        EXPECT_EQ(out.blobDirection, BlobDirection::Bright);
        EXPECT_TRUE(out.use8BitImages);
        EXPECT_EQ(out.gasAddOriginal, 200U);
        EXPECT_EQ(out.gasColor, 2U);
        EXPECT_EQ(out.aiIouThreshold, 55U);
        EXPECT_TRUE(out.enableMtd);
        EXPECT_EQ(out.downsample, DetectionDownsample::Downsample2x);
    }

    /// @brief Verify line and masked grid detection ROI geometries.
    TEST(TestSightlineTracking, BuildAndParseDetectionRoiExt)
    {
        MsgDetectionROI lineMsg {};
        lineMsg.cameraIndex = 0U;
        lineMsg.detectionIndex = 1U;
        lineMsg.roiIndex = 1U;
        lineMsg.geometryMode = RoiGeometryMode::DetectionLine;
        lineMsg.lineLeftX = 100U;
        lineMsg.lineLeftY = 250U;
        lineMsg.lineRightX = 500U;
        lineMsg.lineRightY = 350U;
        lineMsg.lineSide = LineReportSide::Above;

        const auto linePkt = SightlineDetectionBuilder::buildSetDetectionROI(lineMsg);
        MsgDetectionROI lineOut {};
        ASSERT_TRUE(SightlineDetectionParser::parseDetectionROI(linePkt, lineOut));
        EXPECT_EQ(lineOut.cameraIndex, 0U);
        EXPECT_EQ(lineOut.detectionIndex, 1U);
        EXPECT_EQ(lineOut.roiIndex, 1U);
        EXPECT_EQ(lineOut.geometryMode, RoiGeometryMode::DetectionLine);
        EXPECT_EQ(lineOut.lineLeftX, 100U);
        EXPECT_EQ(lineOut.lineLeftY, 250U);
        EXPECT_EQ(lineOut.lineRightX, 500U);
        EXPECT_EQ(lineOut.lineRightY, 350U);
        EXPECT_EQ(lineOut.lineSide, LineReportSide::Above);

        MsgDetectionROI gridMsg {};
        gridMsg.cameraIndex = 1U;
        gridMsg.detectionIndex = 0U;
        gridMsg.roiIndex = 0U;
        gridMsg.geometryMode = RoiGeometryMode::MaskedGrid;
        gridMsg.blocksWide = 16U;
        gridMsg.blocksHigh = 16U;
        gridMsg.gridMasks[0U] = 0xAAAAAAAAAAAAAAAAULL;
        gridMsg.gridMasks[1U] = 0x5555555555555555ULL;
        gridMsg.gridMasks[2U] = 0xFF00FF00FF00FF00ULL;
        gridMsg.gridMasks[3U] = 0x00FF00FF00FF00FFULL;
        gridMsg.showRegions = true;

        const auto gridPkt = SightlineDetectionBuilder::buildSetDetectionROI(gridMsg);
        MsgDetectionROI gridOut {};
        ASSERT_TRUE(SightlineDetectionParser::parseDetectionROI(gridPkt, gridOut));
        EXPECT_EQ(gridOut.geometryMode, RoiGeometryMode::MaskedGrid);
        EXPECT_EQ(gridOut.blocksWide, 16U);
        EXPECT_EQ(gridOut.blocksHigh, 16U);
        EXPECT_EQ(gridOut.gridMasks[0U], 0xAAAAAAAAAAAAAAAAULL);
        EXPECT_EQ(gridOut.gridMasks[1U], 0x5555555555555555ULL);
        EXPECT_EQ(gridOut.gridMasks[2U], 0xFF00FF00FF00FF00ULL);
        EXPECT_EQ(gridOut.gridMasks[3U], 0x00FF00FF00FF00FFULL);
        EXPECT_TRUE(gridOut.showRegions);
    }

    /// @brief Verify MTI reticle shape enum value for RectangleWithClass.
    TEST(TestSightlineTracking, ReticleTypeRectangleWithClass)
    {
        EXPECT_EQ(static_cast<std::uint8_t>(MtiReticleType::RectangleWithClass), 0x20U);
    }

    /// @brief Verify KLV metric dimension filters builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseKlvMetricFilters)
    {
        MsgKlvMetricFilters msg {};
        msg.cameraIndex = 1U;
        msg.minTargetWidthM = 0.8F;
        msg.maxTargetWidthM = 2.5F;
        msg.minTargetHeightM = 1.5F;
        msg.maxTargetHeightM = 4.2F;
        msg.filterAboveHorizon = true;
        msg.filterBelowHorizon = false;
        msg.minLatitude = 32.5;
        msg.maxLatitude = 35.8;
        msg.minLongitude = -117.2;
        msg.maxLongitude = -115.1;

        const auto setPkt = SightlineClassificationBuilder::buildSetKlvMetricFilters(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(setPkt), MessageId::KlvClassFilters);
        EXPECT_EQ(setPkt, SightlineProtocolBuilder::buildSetKlvMetricFilters(msg));

        MsgKlvMetricFilters out {};
        ASSERT_TRUE(SightlineClassificationParser::parseKlvMetricFilters(setPkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_NEAR(out.minTargetWidthM, 0.8F, 1e-4F);
        EXPECT_NEAR(out.maxTargetWidthM, 2.5F, 1e-4F);
        EXPECT_NEAR(out.minTargetHeightM, 1.5F, 1e-4F);
        EXPECT_NEAR(out.maxTargetHeightM, 4.2F, 1e-4F);
        EXPECT_TRUE(out.filterAboveHorizon);
        EXPECT_FALSE(out.filterBelowHorizon);
        EXPECT_NEAR(out.minLatitude, 32.5, 1e-4);
        EXPECT_NEAR(out.maxLatitude, 35.8, 1e-4);
        EXPECT_NEAR(out.minLongitude, -117.2, 1e-4);
        EXPECT_NEAR(out.maxLongitude, -115.1, 1e-4);

        MsgKlvMetricFilters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseKlvMetricFilters(setPkt, facadeOut));
        EXPECT_EQ(facadeOut.cameraIndex, 1U);

        const auto getPkt = SightlineClassificationBuilder::buildGetKlvMetricFilters(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getPkt), MessageId::GetParameters);
        EXPECT_EQ(getPkt, SightlineProtocolBuilder::buildGetKlvMetricFilters(1U));
    }

    /// @brief Verify classifier configuration builder and parser roundtrip.
    TEST(TestSightlineTracking, BuildAndParseClassifierConfig)
    {
        MsgClassifierConfig msg {};
        msg.cameraIndex = 0U;
        msg.model = PretrainedClassifierModel::Drone;
        msg.maxPerFrame = 8U;
        msg.minDimensions = 16U;
        msg.droneReporting = DroneReportingMode::Detailed;
        msg.detectionPadding = 4U;
        msg.updateRate = 30U;
        msg.useNpu = true;
        msg.asyncExecution = true;
        msg.customModelName = "drone_v2.bin";

        const auto setPkt = SightlineClassificationBuilder::buildSetClassifierConfig(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(setPkt), MessageId::ClassifierParameters);
        EXPECT_EQ(setPkt, SightlineProtocolBuilder::buildSetClassifierConfig(msg));

        MsgClassifierConfig out {};
        ASSERT_TRUE(SightlineClassificationParser::parseClassifierConfig(setPkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.model, PretrainedClassifierModel::Drone);
        EXPECT_EQ(out.maxPerFrame, 8U);
        EXPECT_EQ(out.minDimensions, 16U);
        EXPECT_EQ(out.droneReporting, DroneReportingMode::Detailed);
        EXPECT_EQ(out.detectionPadding, 4U);
        EXPECT_EQ(out.updateRate, 30U);
        EXPECT_TRUE(out.useNpu);
        EXPECT_TRUE(out.asyncExecution);
        EXPECT_EQ(out.customModelName, "drone_v2.bin");

        MsgClassifierConfig facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseClassifierConfig(setPkt, facadeOut));
        EXPECT_EQ(facadeOut.customModelName, "drone_v2.bin");

        const auto getPkt = SightlineClassificationBuilder::buildGetClassifierConfig(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getPkt), MessageId::GetParameters);
        EXPECT_EQ(getPkt, SightlineProtocolBuilder::buildGetClassifierConfig(0U));
    }

    /// @brief Verify sub-pixel fractional coordinates extraction from 0x43.
    TEST(TestSightlineTracking, ParseSubpixelTrackingPosition)
    {
        std::vector<std::uint8_t> payload {};
        // col: 320 (bytes 0..1)
        payload.push_back(0x40U);
        payload.push_back(0x01U);
        // row: 240 (bytes 2..3)
        payload.push_back(0xF0U);
        payload.push_back(0x00U);
        // translationCol: 100 (bytes 4..5)
        payload.push_back(0x64U);
        payload.push_back(0x00U);
        // translationRow: 50 (bytes 6..7)
        payload.push_back(0x32U);
        payload.push_back(0x00U);
        // offsetCol: 0 (bytes 8..9)
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        // offsetRow: 0 (bytes 10..11)
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        // confidence: 80 (byte 12)
        payload.push_back(80U);
        // sceneConfidence: 90 (byte 13)
        payload.push_back(90U);
        // rotation: 0 (bytes 14..15)
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        // cameraIndex: 0 (byte 16)
        payload.push_back(0U);
        // userTrackId: 1 (byte 17)
        payload.push_back(1U);
        // targetColFrac8: 128 (0.50 px) (byte 18)
        payload.push_back(128U);
        // targetRowFrac8: 64 (0.25 px) (byte 19)
        payload.push_back(64U);
        // sceneColFrac8: 192 (0.75 px) (byte 20)
        payload.push_back(192U);
        // sceneRowFrac8: 32 (0.125 px) (byte 21)
        payload.push_back(32U);
        // scale: 256 (1.0) (bytes 22..23)
        payload.push_back(0x00U);
        payload.push_back(0x01U);

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPosition, payload);
        MsgTrackingPosition out {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingPosition(pkt, out));

        EXPECT_DOUBLE_EQ(out.col, 320.5);
        EXPECT_DOUBLE_EQ(out.row, 240.25);
        EXPECT_DOUBLE_EQ(out.translationCol, 100.75);
        EXPECT_DOUBLE_EQ(out.translationRow, 50.125);
        EXPECT_FALSE(out.isCoasting);
        EXPECT_EQ(out.confidence, 80U);
    }

    /// @brief Verify coasting state extraction from single tracking position (0x43).
    TEST(TestSightlineTracking, ParseCoastingStateSingleTrack)
    {
        std::vector<std::uint8_t> payload {};
        // col: 100, row: 150
        payload.push_back(0x64U); payload.push_back(0x00U);
        payload.push_back(0x96U); payload.push_back(0x00U);
        // translationCol: 0, translationRow: 0
        payload.push_back(0x00U); payload.push_back(0x00U);
        payload.push_back(0x00U); payload.push_back(0x00U);
        // offsetCol: 0, offsetRow: 0
        payload.push_back(0x00U); payload.push_back(0x00U);
        payload.push_back(0x00U); payload.push_back(0x00U);
        // confidence with coasting MSB bit 7 set: 0x80 | 75 = 203
        payload.push_back(0x80U | 75U);
        // sceneConfidence: 50
        payload.push_back(50U);
        // rotation: 0
        payload.push_back(0x00U); payload.push_back(0x00U);
        // cameraIndex: 0
        payload.push_back(0U);

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPosition, payload);
        MsgTrackingPosition out {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingPosition(pkt, out));

        EXPECT_TRUE(out.isCoasting);
        EXPECT_EQ(out.confidence, 75U);
    }

    /// @brief Verify coasting state extraction from multi-target tracking telemetry (0x51).
    TEST(TestSightlineTracking, ParseCoastingStateMultiTracks)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // cameraIndex
        payload.push_back(2U); // numTracks = 2

        // Track 0: locked (confidence: 92, isCoasting: false)
        payload.push_back(1U); // trackId
        payload.push_back(0x40U); payload.push_back(0x01U); // col 320
        payload.push_back(0xF0U); payload.push_back(0x00U); // row 240
        payload.push_back(64U); payload.push_back(0U);      // width 64
        payload.push_back(48U); payload.push_back(0U);      // height 48
        payload.push_back(0U); payload.push_back(0U);       // velCol
        payload.push_back(0U); payload.push_back(0U);       // velRow
        payload.push_back(92U);                              // confidence 92
        payload.push_back(0x01U);                            // primary flag

        // Track 1: occluded coasting (confidence: 0x80 | 60, isCoasting: true)
        payload.push_back(2U); // trackId
        payload.push_back(0x80U); payload.push_back(0x01U); // col 384
        payload.push_back(0x00U); payload.push_back(0x01U); // row 256
        payload.push_back(32U); payload.push_back(0U);      // width 32
        payload.push_back(32U); payload.push_back(0U);      // height 32
        payload.push_back(0U); payload.push_back(0U);       // velCol
        payload.push_back(0U); payload.push_back(0U);       // velRow
        payload.push_back(0x80U | 60U);                      // confidence 60 with coast bit
        payload.push_back(0x00U);                            // secondary flag

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPositions, payload);
        MsgTrackingPositions out {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingPositions(pkt, out));
        ASSERT_EQ(out.tracks.size(), 2U);

        EXPECT_FALSE(out.tracks[0].isCoasting);
        EXPECT_EQ(out.tracks[0].confidence, 92U);

        EXPECT_TRUE(out.tracks[1].isCoasting);
        EXPECT_EQ(out.tracks[1].confidence, 60U);
    }

    /// @brief Verify coasting state extraction from extended positions with AI classification (0xA0).
    TEST(TestSightlineTracking, ParseCoastingPositionsExtended)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // cameraIndex
        payload.push_back(1U); // numTracks = 1

        payload.push_back(5U); // trackId
        payload.push_back(0x00U); payload.push_back(0x01U); // col 256
        payload.push_back(0x80U); payload.push_back(0x00U); // row 128
        payload.push_back(50U); payload.push_back(0U);      // width
        payload.push_back(50U); payload.push_back(0U);      // height
        payload.push_back(0U); payload.push_back(0U);       // velCol
        payload.push_back(0U); payload.push_back(0U);       // velRow
        payload.push_back(0x80U | 68U);                      // confidence 68 with coast bit
        payload.push_back(0x01U);                            // primary
        payload.push_back(2U);                               // classId: vehicle
        for (std::size_t i { 0U }; i < 5U; ++i) {
            payload.push_back(0U);                           // padding to 21 bytes
        }

        const auto pkt = SightlineFraming::buildPacket(MessageId::TrackingPositionsExtended, payload);
        MsgTrackingPositionsExtended out {};
        ASSERT_TRUE(SightlineTrackingParser::parsePositionsExtended(pkt, out));
        ASSERT_EQ(out.tracks.size(), 1U);

        EXPECT_TRUE(out.tracks[0].isCoasting);
        EXPECT_EQ(out.tracks[0].confidence, 68U);
    }

    /// @brief Verify default parameters compliance with Sightline EAN-Target-Tracking specifications.
    TEST(TestSightlineTracking, TrackingParametersDefaultCompliance)
    {
        const MsgSetTrackingParameters params {};
        // Section 7.2: Default max misses is 45 frames (1.5 seconds at 30 fps)
        EXPECT_EQ(params.maxMisses, 45U);
        // Section 5.2: Default smoothing values are 5
        EXPECT_EQ(params.zoomSmoothing, 5U);
        EXPECT_EQ(params.rollSmoothing, 5U);
        // Section 4.9.1: Default maxPauseTime is 0
        EXPECT_EQ(params.maxPauseTime, 0U);
    }

    /// @brief Verify round-trip serialization of maxPauseTime in SetTrackingParameters.
    TEST(TestSightlineTracking, BuildAndParseMaxPauseTime)
    {
        MsgSetTrackingParameters params {};
        params.cameraIndex = 1U;
        params.objectSize = 48U;
        params.mode = static_cast<std::uint8_t>(TrackingMode::Drone);
        params.maxMisses = 60U;
        params.zoomSmoothing = 5U;
        params.rollSmoothing = 5U;
        params.maxTracks = 15U;
        params.maxPauseTime = 10U; // 10 seconds buffer

        const auto pkt = SightlineTrackingBuilder::buildSetTrackingParameters(params);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetTrackingParameters);

        MsgSetTrackingParameters parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseTrackingParameters(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_EQ(parsed.objectSize, 48U);
        EXPECT_EQ(parsed.mode, static_cast<std::uint8_t>(TrackingMode::Drone));
        EXPECT_EQ(parsed.maxMisses, 60U);
        EXPECT_EQ(parsed.maxPauseTime, 10U);
    }

    /// @brief Verify precision acquisition with MISB timestamp serialization (0x08).
    TEST(TestSightlineTracking, BuildAndParseStartPrecision)
    {
        const auto pkt = SightlineTrackingBuilder::buildStartPrecision(
            2U, 640U, 480U, 80U, 60U, 987654321ULL, 0x01U);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::StartTracking);

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 21U);

        MsgStartTracking parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseStartTracking(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 2U);
        EXPECT_EQ(parsed.centerCol, 640U);
        EXPECT_EQ(parsed.centerRow, 480U);
        EXPECT_EQ(parsed.width, 80U);
        EXPECT_EQ(parsed.height, 60U);
        EXPECT_EQ(parsed.flags, 0x01U);
        EXPECT_EQ(parsed.framePts, 987654321ULL);
    }

    /// @brief Verify full 10-byte ModifyTracking command with ModifyMode (0x05).
    TEST(TestSightlineTracking, BuildAndParseModifyTrackingFull)
    {
        const auto pkt = SightlineTrackingBuilder::buildModifyTrackingMode(
            1U, 500U, 300U, ModifyMode::DesignateNearOrNewPrimary, 4U, 32U, 32U);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::ModifyTracking);

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 10U);

        MsgModifyTracking parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseModifyTracking(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_EQ(parsed.col, 500U);
        EXPECT_EQ(parsed.row, 300U);
        EXPECT_EQ(parsed.width, 32U);
        EXPECT_EQ(parsed.height, 32U);
        EXPECT_EQ(parsed.trackId, 4U);
        EXPECT_EQ(parsed.mode, static_cast<std::uint8_t>(ModifyMode::DesignateNearOrNewPrimary));
    }

    /// @brief Verify forced coasting modes mapped to ModifyTrackIndex (0x17).
    TEST(TestSightlineTracking, BuildAndParseForcedCoastingAllModes)
    {
        // 1. None / Clear coasting
        {
            const auto pkt = SightlineTrackingBuilder::buildForcedCoasting(0U, 1U, ForcedCoastingMode::None);
            MsgModifyTrackIndex parsed {};
            ASSERT_TRUE(SightlineTrackingParser::parseModifyTrackIndex(pkt, parsed));
            EXPECT_EQ(parsed.trackIndex, 1U);
            EXPECT_EQ(parsed.flags, static_cast<std::uint8_t>(TrackIndexAction::CoastNone));
        }

        // 2. Freeze Updates
        {
            const auto pkt = SightlineTrackingBuilder::buildForcedCoasting(0U, 2U, ForcedCoastingMode::FreezeUpdates);
            MsgModifyTrackIndex parsed {};
            ASSERT_TRUE(SightlineTrackingParser::parseModifyTrackIndex(pkt, parsed));
            EXPECT_EQ(parsed.trackIndex, 2U);
            EXPECT_EQ(parsed.flags, static_cast<std::uint8_t>(TrackIndexAction::CoastFreezeUpdates));
        }

        // 3. Freeze Search
        {
            const auto pkt = SightlineTrackingBuilder::buildForcedCoasting(0U, 3U, ForcedCoastingMode::FreezeSearch);
            MsgModifyTrackIndex parsed {};
            ASSERT_TRUE(SightlineTrackingParser::parseModifyTrackIndex(pkt, parsed));
            EXPECT_EQ(parsed.trackIndex, 3U);
            EXPECT_EQ(parsed.flags, static_cast<std::uint8_t>(TrackIndexAction::CoastFreezeSearch));
        }

        // 4. Freeze Propagation
        {
            const auto pkt = SightlineTrackingBuilder::buildForcedCoasting(0U, 4U, ForcedCoastingMode::FreezePropagation);
            MsgModifyTrackIndex parsed {};
            ASSERT_TRUE(SightlineTrackingParser::parseModifyTrackIndex(pkt, parsed));
            EXPECT_EQ(parsed.trackIndex, 4U);
            EXPECT_EQ(parsed.flags, static_cast<std::uint8_t>(TrackIndexAction::CoastFreezePropagation));
        }
    }

    /// @brief Verify dynamic track box resizing with and without Acquisition Assist (0x17).
    TEST(TestSightlineTracking, BuildAndParseTrackResizing)
    {
        // Resize with Acquisition Assist (action 9)
        const auto pkt = SightlineTrackingBuilder::buildModifyTrackIndex(
            1U, 2U, TrackIndexAction::ResizeWithAcquisitionAssist, 80U, 60U);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::ModifyTrackIndex);

        MsgModifyTrackIndex parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseModifyTrackIndex(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_EQ(parsed.trackIndex, 2U);
        EXPECT_EQ(parsed.flags, 9U);
        EXPECT_EQ(parsed.width, 80U);
        EXPECT_EQ(parsed.height, 60U);
    }

    /// @brief Verify display-frame rotated nudge coordinates (0x0A).
    TEST(TestSightlineTracking, BuildAndParseNudgeDisplayRotated)
    {
        const auto pkt = SightlineTrackingBuilder::buildNudgeTrackingRotated(
            0U, -15, 25, NudgeCoordinateMode::DisplayCoordinates);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::NudgeTrackingCoordinate);

        MsgNudgeTrackingCoordinate parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseNudgeTracking(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 0U);
        EXPECT_EQ(parsed.rotate, 1U);
        EXPECT_EQ(parsed.offsetCol, -15);
        EXPECT_EQ(parsed.offsetRow, 25);
    }

    /// @brief Verify track shift relative to centroid (0x33).
    TEST(TestSightlineTracking, BuildAndParseShiftSelectedTrack)
    {
        MsgShiftSelectedTrack msg {};
        msg.cameraIndex = 1U;
        msg.trackId = 3U;
        msg.shiftCol = -10;
        msg.shiftRow = 20;

        const auto pkt = SightlineTrackingBuilder::buildShiftSelectedTrack(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::ShiftSelectedTrack);

        MsgShiftSelectedTrack parsed {};
        ASSERT_TRUE(SightlineTrackingParser::parseShiftSelectedTrack(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_EQ(parsed.trackId, 3U);
        EXPECT_EQ(parsed.shiftCol, -10);
        EXPECT_EQ(parsed.shiftRow, 20);
    }

    /// @brief Verify TrackCoordinate normalized conversions for closed-loop PTZ auto-tracking.
    TEST(TestSightlineTracking, TrackCoordinateNormalizedConversions)
    {
        TrackCoordinate track {};
        track.trackId = 1U;
        track.centerCol = 960.0;
        track.centerRow = 540.0;
        track.width = 192.0;
        track.height = 108.0;
        track.velocityCol = 48.0;
        track.velocityRow = -27.0;
        track.confidence = 90U;
        track.isPrimary = true;
        track.isCoasting = false;

        constexpr double frameW = 1920.0;
        constexpr double frameH = 1080.0;

        // Centered target: error should be 0.0
        EXPECT_NEAR(track.normalizedErrorX(frameW), 0.0, 1e-6);
        EXPECT_NEAR(track.normalizedErrorY(frameH), 0.0, 1e-6);

        // Normalized height: 108 / 1080 = 0.10
        EXPECT_NEAR(track.normalizedHeight(frameH), 0.10, 1e-6);

        // Normalized velocity:
        // velocityCol / (frameW * 0.5) * 30 Hz = 48 / 960 * 30 = 0.05 * 30 = 1.5 units/sec
        EXPECT_NEAR(track.normalizedVelocityX(frameW, 30.0), 1.5, 1e-6);
        // velocityRow / (frameH * 0.5) * 30 Hz = -27 / 540 * 30 = -0.05 * 30 = -1.5 units/sec
        EXPECT_NEAR(track.normalizedVelocityY(frameH, 30.0), -1.5, 1e-6);

        // Off-center target: Col = 1440 (+0.5), Row = 270 (-0.5)
        track.centerCol = 1440.0;
        track.centerRow = 270.0;
        EXPECT_NEAR(track.normalizedErrorX(frameW), 0.5, 1e-6);
        EXPECT_NEAR(track.normalizedErrorY(frameH), -0.5, 1e-6);
    }

} // namespace
} // namespace Sightline
