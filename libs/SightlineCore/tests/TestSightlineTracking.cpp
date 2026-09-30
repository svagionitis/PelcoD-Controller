/// @file TestSightlineTracking.cpp
/// @brief Unit tests for Sightline tracking, motion, and MTI detection builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
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

} // namespace
} // namespace Sightline
