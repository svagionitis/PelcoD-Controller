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
        detMsg.mode = 2U;
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

} // namespace
} // namespace Sightline
