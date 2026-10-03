/// @file TestSightlineVideoPipeline.cpp
/// @brief Unit tests for Sightline video capture, display, compression, and streaming builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineBlendingBuilder.h"
#include "modules/SightlineBlendingParser.h"
#include "modules/SightlineCaptureBuilder.h"
#include "modules/SightlineCaptureParser.h"
#include "modules/SightlineCompressionBuilder.h"
#include "modules/SightlineCompressionParser.h"
#include "modules/SightlineDisplayBuilder.h"
#include "modules/SightlineDisplayParser.h"
#include "modules/SightlineEnhancementBuilder.h"
#include "modules/SightlineEnhancementParser.h"
#include "modules/SightlineNetworkBuilder.h"
#include "modules/SightlineRecordingBuilder.h"
#include "modules/SightlineRecordingParser.h"
#include "modules/RecordingValidator.h"
#include "modules/RecordingRingBuffer.h"
#include "modules/StorageRetentionManager.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify video pipeline and H.264 streaming commands serialization.
    TEST(TestSightlineVideoPipeline, BuildVideoPipeline)
    {
        MsgSetVideoParameters vidMsg {};
        vidMsg.autoChop = 1U;
        vidMsg.chopTop = 16U;
        vidMsg.chopBottom = 16U;
        vidMsg.chopLeft = 24U;
        vidMsg.chopRight = 24U;
        vidMsg.deinterlace = 1U;
        vidMsg.autoReset = 1U;
        vidMsg.cameraIndex = 0U;
        const auto vidPkt = SightlineCaptureBuilder::buildSetVideoParameters(vidMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(vidPkt), MessageId::SetVideoParameters);
        const auto vidPayload = SightlineFraming::extractPayload(vidPkt);
        ASSERT_EQ(vidPayload.size(), 8U);
        EXPECT_EQ(vidPayload[0], 1U);
        EXPECT_EQ(vidPayload[1], 16U);
        EXPECT_EQ(vidPayload[2], 16U);
        EXPECT_EQ(vidPayload[3], 24U);
        EXPECT_EQ(vidPayload[4], 24U);
        EXPECT_EQ(vidPayload[5], 1U);
        EXPECT_EQ(vidPayload[6], 1U);
        EXPECT_EQ(vidPayload[7], 0U);

        // Verify facade equivalence
        const auto facadeVidPkt = SightlineProtocolBuilder::buildSetVideoParameters(vidMsg);
        EXPECT_EQ(vidPkt, facadeVidPkt);

        MsgSetVideoMode modeMsg {};
        modeMsg.cameraIndex = 0U;
        modeMsg.freeze = 0U;
        modeMsg.digitalZoom = 150U;
        modeMsg.mirror = 1U;
        modeMsg.flip = 0U;
        const auto modePkt = SightlineCaptureBuilder::buildSetVideoMode(modeMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetVideoMode);

        MsgSetVideoEnhancement enhMsg {};
        enhMsg.cameraIndex = 0U;
        enhMsg.contrast = 65U;
        enhMsg.brightness = 55U;
        enhMsg.sharpening = 10U;
        enhMsg.claheEnable = 1U;
        const auto enhPkt = SightlineEnhancementBuilder::buildSetVideoEnhance(enhMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(enhPkt), MessageId::SetVideoEnhancementParameters);

        MsgSetDisplayParameters dispMsg {};
        dispMsg.displayIndex = 0U;
        dispMsg.cameraIndex = 0U;
        dispMsg.xOffset = 100U;
        dispMsg.yOffset = 50U;
        dispMsg.displayWidth = 1280U;
        dispMsg.displayHeight = 720U;
        const auto dispPkt = SightlineDisplayBuilder::buildSetDisplayParams(dispMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(dispPkt), MessageId::SetDisplayParameters);

        MsgSetEthernetVideoParameters ethMsg {};
        ethMsg.quality = 85U;
        ethMsg.foveal = 10U;
        ethMsg.frameStep = 2U;
        ethMsg.frameSize = 5U;
        ethMsg.displayId = 0x0002U;
        ethMsg.customWide = 0U;
        ethMsg.customHigh = 0U;
        const auto ethPkt = SightlineNetworkBuilder::buildSetEthernetVideo(ethMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(ethPkt), MessageId::SetEthernetVideoParameters);
        const auto ethPayload = SightlineFraming::extractPayload(ethPkt);
        ASSERT_EQ(ethPayload.size(), 10U);
        EXPECT_EQ(ethPayload[0], 85U);
        EXPECT_EQ(ethPayload[1], 10U);
        EXPECT_EQ(ethPayload[2], 2U);
        EXPECT_EQ(ethPayload[3], 5U);
        EXPECT_EQ(ethPayload[4], 0x02U);
        EXPECT_EQ(ethPayload[5], 0x00U);

        MsgSetEthernetDisplayParameters dispEthMsg {};
        dispEthMsg.protocol = 1U;
        dispEthMsg.ipAddress = 0xE0000001U;
        dispEthMsg.port = 15004U;
        dispEthMsg.displayId = 0x0002U;
        dispEthMsg.maxPacket = 1400U;
        dispEthMsg.maxRawPacket = 1400U;
        const auto dispEthPkt = SightlineNetworkBuilder::buildSetEthernetDisplay(dispEthMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(dispEthPkt), MessageId::SetEthernetDisplayParameters);
        const auto dispEthPayload = SightlineFraming::extractPayload(dispEthPkt);
        ASSERT_EQ(dispEthPayload.size(), 13U);
        EXPECT_EQ(dispEthPayload[0], 1U);
        EXPECT_EQ(dispEthPayload[1], 0xE0U);
        EXPECT_EQ(dispEthPayload[2], 0x00U);
        EXPECT_EQ(dispEthPayload[3], 0x00U);
        EXPECT_EQ(dispEthPayload[4], 0x01U);

        MsgSetH264Parameters h264Msg {};
        h264Msg.targetBitrateBps = 8000000U;
        h264Msg.intraFrameInterval = 60U;
        h264Msg.lfDisableIdc = 0U;
        h264Msg.airMbPeriod = 20U;
        h264Msg.sliceRefreshRowNumber = 0U;
        h264Msg.flags = 0x12U;
        h264Msg.displayId = 0x0002U;
        h264Msg.minQp = 10U;
        h264Msg.maxQp = 40U;
        const auto h264Pkt = SightlineCompressionBuilder::buildSetH264Parameters(h264Msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(h264Pkt), MessageId::SetH264Parameters);
        const auto h264Payload = SightlineFraming::extractPayload(h264Pkt);
        ASSERT_EQ(h264Payload.size(), 13U);

        MsgSetSDRecordingParameters sdMsg {};
        sdMsg.recordingState = 1U;
        sdMsg.cameraIndex = 0U;
        sdMsg.filenamePrefix = "MISSION_01";
        const auto sdPkt = SightlineRecordingBuilder::buildSetSDRecording(sdMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(sdPkt), MessageId::SetSDRecordingParameters);

        MsgStreamingControl streamMsg {};
        streamMsg.streamIndex = 0U;
        streamMsg.action = 1U;
        const auto streamPkt = SightlineCompressionBuilder::buildStreamingControl(streamMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(streamPkt), MessageId::StreamingControl);
    }

    /// @brief Verify current video parameters deserialization (0x46).
    TEST(TestSightlineVideoPipeline, ParseVideoParameters)
    {
        std::vector<std::uint8_t> payload {
            1U, // autoChop
            16U, // chopTop
            16U, // chopBottom
            24U, // chopLeft
            24U, // chopRight
            1U, // deinterlace
            1U, // autoReset
            0U // cameraIndex
        };
        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentVideoParameters, payload);

        MsgSetVideoParameters out {};
        ASSERT_TRUE(SightlineCaptureParser::parseVideoParameters(pkt, out));
        EXPECT_EQ(out.autoChop, 1U);
        EXPECT_EQ(out.chopTop, 16U);
        EXPECT_EQ(out.chopBottom, 16U);
        EXPECT_EQ(out.chopLeft, 24U);
        EXPECT_EQ(out.chopRight, 24U);
        EXPECT_EQ(out.deinterlace, 1U);
        EXPECT_EQ(out.autoReset, 1U);
        EXPECT_EQ(out.cameraIndex, 0U);

        // Verify facade equivalence
        MsgSetVideoParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVideoParameters(pkt, facadeOut));
        EXPECT_EQ(facadeOut.autoChop, 1U);
    }

    /// @brief Verify current H.264 parameters deserialization (0x56).
    TEST(TestSightlineVideoPipeline, ParseH264Parameters)
    {
        std::vector<std::uint8_t> payload {};
        // targetBitrateBps = 4000000 (0x003D0900)
        payload.push_back(0x00U);
        payload.push_back(0x09U);
        payload.push_back(0x3DU);
        payload.push_back(0x00U);
        payload.push_back(30U); // intraFrameInterval
        payload.push_back(0U); // lfDisableIdc
        payload.push_back(20U); // airMbPeriod
        payload.push_back(0U); // sliceRefreshRowNumber
        payload.push_back(0x12U); // flags
        payload.push_back(0x02U); // displayId low
        payload.push_back(0x00U); // displayId high
        payload.push_back(10U); // minQp
        payload.push_back(40U); // maxQp

        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentH264Parameters, payload);

        MsgSetH264Parameters out {};
        ASSERT_TRUE(SightlineCompressionParser::parseH264Parameters(pkt, out));
        EXPECT_EQ(out.targetBitrateBps, 4000000U);
        EXPECT_EQ(out.intraFrameInterval, 30U);
        EXPECT_EQ(out.lfDisableIdc, 0U);
        EXPECT_EQ(out.airMbPeriod, 20U);
        EXPECT_EQ(out.sliceRefreshRowNumber, 0U);
        EXPECT_EQ(out.flags, 0x12U);
        EXPECT_EQ(out.displayId, 0x0002U);
        EXPECT_EQ(out.minQp, 10U);
        EXPECT_EQ(out.maxQp, 40U);

        // Verify facade equivalence
        MsgSetH264Parameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseH264Parameters(pkt, facadeOut));
        EXPECT_EQ(facadeOut.targetBitrateBps, 4000000U);
    }

    /// @brief Verify streaming query, 3D noise query, and snapshot parsing.
    TEST(TestSightlineVideoPipeline, StreamingNoiseAndSnapshotRoundTrip)
    {
        const auto getStreamPkt = SightlineCompressionBuilder::buildGetStreamingControl(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getStreamPkt), MessageId::GetParameters);
        EXPECT_EQ(getStreamPkt, SightlineProtocolBuilder::buildGetStreamingControl(1U));

        const auto getNoisePkt = SightlineEnhancementBuilder::buildGetNoise3D(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getNoisePkt), MessageId::GetParameters);
        EXPECT_EQ(getNoisePkt, SightlineProtocolBuilder::buildGetNoise3D(0U));

        const auto getSnapPkt = SightlineRecordingBuilder::buildGetSnapShot(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getSnapPkt), MessageId::GetSnapShot);
        EXPECT_EQ(getSnapPkt, SightlineProtocolBuilder::buildGetSnapShot(0U));

        // Snapshot parsing
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // cameraIndex
        payload.push_back(1U); // status: Success
        const std::string fn = "snap001.jpg";
        SightlineFraming::appendString(payload, fn);

        const auto snapPkt = SightlineFraming::buildPacket(MessageId::CurrentSnapShot, payload);
        MsgCurrentSnapShot snapOut {};
        ASSERT_TRUE(SightlineRecordingParser::parseSnapShot(snapPkt, snapOut));
        EXPECT_EQ(snapOut.cameraIndex, 0U);
        EXPECT_EQ(snapOut.status, 1U);
        EXPECT_EQ(snapOut.fileName, "snap001.jpg");

        MsgCurrentSnapShot facadeSnapOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseSnapShot(snapPkt, facadeSnapOut));
        EXPECT_EQ(facadeSnapOut.fileName, "snap001.jpg");
    }

    /// @brief Verify camera switch command serialization and parsing.
    TEST(TestSightlineVideoPipeline, BuildAndParseCameraSwitch)
    {
        MsgCameraSwitch msg {};
        msg.cameraIndex = 2U;
        msg.switchType = 1U; // Smooth dissolve
        msg.flags = 0x05U;

        const auto pkt = SightlineCaptureBuilder::buildCameraSwitch(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::CameraSwitch);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildCameraSwitch(msg));

        MsgCameraSwitch out {};
        ASSERT_TRUE(SightlineCaptureParser::parseCameraSwitch(pkt, out));
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.switchType, 1U);
        EXPECT_EQ(out.flags, 0x05U);

        MsgCameraSwitch facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseCameraSwitch(pkt, facadeOut));
        EXPECT_EQ(facadeOut.switchType, 1U);
    }

    /// @brief Verify advanced capture parameters serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseAdvCapture)
    {
        MsgAdvancedCaptureParameters msg {};
        msg.cameraIndex = 1U;
        msg.bitDepth = 12U;
        msg.laneCount = 4U;
        msg.pixelClockHz = 74250000U;
        msg.syncFlags = 0x03U;

        const auto pkt = SightlineCaptureBuilder::buildSetAdvCaptureParams(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::AdvancedCaptureParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetAdvCaptureParams(msg));

        MsgAdvancedCaptureParameters out {};
        ASSERT_TRUE(SightlineCaptureParser::parseAdvCaptureParams(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.bitDepth, 12U);
        EXPECT_EQ(out.laneCount, 4U);
        EXPECT_EQ(out.pixelClockHz, 74250000U);
        EXPECT_EQ(out.syncFlags, 0x03U);

        MsgAdvancedCaptureParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseAdvCaptureParams(pkt, facadeOut));
        EXPECT_EQ(facadeOut.pixelClockHz, 74250000U);

        const auto queryPkt = SightlineCaptureBuilder::buildGetAdvCaptureParams(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetAdvCaptureParams(1U));
    }

    /// @brief Verify digital video framing parser parameters serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseDigiVideoParser)
    {
        MsgDigitalVideoParserParameters msg {};
        msg.cameraIndex = 0U;
        msg.videoStandard = 2U; // BT.1120
        msg.embeddedSync = 1U;
        msg.clockEdge = 1U; // Falling edge
        msg.flags = 0x08U;

        const auto pkt = SightlineCaptureBuilder::buildSetDigiVideoParser(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::DigitalVideoParserParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetDigiVideoParser(msg));

        MsgDigitalVideoParserParameters out {};
        ASSERT_TRUE(SightlineCaptureParser::parseDigiVideoParser(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.videoStandard, 2U);
        EXPECT_EQ(out.embeddedSync, 1U);
        EXPECT_EQ(out.clockEdge, 1U);
        EXPECT_EQ(out.flags, 0x08U);

        MsgDigitalVideoParserParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDigiVideoParser(pkt, facadeOut));
        EXPECT_EQ(facadeOut.videoStandard, 2U);

        const auto queryPkt = SightlineCaptureBuilder::buildGetDigiVideoParser(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetDigiVideoParser(0U));
    }

    /// @brief Verify camera sensor capabilities query and reply parsing.
    TEST(TestSightlineVideoPipeline, BuildAndParseCameraCapabilities)
    {
        const auto queryPkt = SightlineCaptureBuilder::buildGetCameraCapabilities(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetCameraCapabilities(1U));

        std::vector<std::uint8_t> payload {};
        payload.push_back(1U); // cameraIndex
        SightlineFraming::appendU16Le(payload, 3840U); // maxWidth
        SightlineFraming::appendU16Le(payload, 2160U); // maxHeight
        payload.push_back(60U); // maxFrameRate
        payload.push_back(1U); // supportsZoom
        payload.push_back(0x10U); // flags

        const auto pkt = SightlineFraming::buildPacket(MessageId::CameraCapabilities, payload);

        MsgCameraCapabilities out {};
        ASSERT_TRUE(SightlineCaptureParser::parseCameraCapabilities(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.maxWidth, 3840U);
        EXPECT_EQ(out.maxHeight, 2160U);
        EXPECT_EQ(out.maxFrameRate, 60U);
        EXPECT_EQ(out.supportsZoom, 1U);
        EXPECT_EQ(out.flags, 0x10U);

        MsgCameraCapabilities facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseCameraCapabilities(pkt, facadeOut));
        EXPECT_EQ(facadeOut.maxWidth, 3840U);
    }

    /// @brief Verify 4-point projective homography calibration serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseFourAlignPoints)
    {
        MsgFourAlignPoints msg {};
        msg.cameraIndex = 0U;
        msg.warpIndex = 1U;
        msg.points[0] = { 10U, 20U, 12U, 22U };
        msg.points[1] = { 600U, 25U, 602U, 27U };
        msg.points[2] = { 610U, 450U, 612U, 452U };
        msg.points[3] = { 15U, 440U, 17U, 442U };

        const auto pkt = SightlineBlendingBuilder::buildFourAlignPoints(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::FourAlignPoints);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildFourAlignPoints(msg));

        MsgFourAlignPoints out {};
        ASSERT_TRUE(SightlineBlendingParser::parseFourAlignPoints(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.warpIndex, 1U);
        EXPECT_EQ(out.points[0].warpCol, 10U);
        EXPECT_EQ(out.points[0].warpRow, 20U);
        EXPECT_EQ(out.points[0].fixedCol, 12U);
        EXPECT_EQ(out.points[0].fixedRow, 22U);
        EXPECT_EQ(out.points[2].warpCol, 610U);
        EXPECT_EQ(out.points[2].fixedRow, 452U);

        MsgFourAlignPoints facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseFourAlignPoints(pkt, facadeOut));
        EXPECT_EQ(facadeOut.points[1].warpCol, 600U);

        const auto queryPkt = SightlineBlendingBuilder::buildGetFourAlignPoints(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetFourAlignPoints(0U));
    }

    /// @brief Verify fine-tune blend alignment serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseBlendAlign)
    {
        MsgBlendAlign msg {};
        msg.cameraIndex = 1U;
        msg.mode = 1U; // Feature-based auto
        msg.offsetX = -15;
        msg.offsetY = 8;
        msg.rotation = 120; // 1.2 degrees
        msg.scale = 1050U; // 1.05x

        const auto pkt = SightlineBlendingBuilder::buildSetBlendAlign(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::BlendAlign);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetBlendAlign(msg));

        MsgBlendAlign out {};
        ASSERT_TRUE(SightlineBlendingParser::parseBlendAlign(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.mode, 1U);
        EXPECT_EQ(out.offsetX, -15);
        EXPECT_EQ(out.offsetY, 8);
        EXPECT_EQ(out.rotation, 120);
        EXPECT_EQ(out.scale, 1050U);

        MsgBlendAlign facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseBlendAlign(pkt, facadeOut));
        EXPECT_EQ(facadeOut.scale, 1050U);

        const auto queryPkt = SightlineBlendingBuilder::buildGetBlendAlign(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetBlendAlign(1U));
    }

    /// @brief Verify video display routing serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseVideoDisplay)
    {
        MsgVideoDisplay msg {};
        msg.displayIndex = 0U;
        msg.cameraIndex = 1U;
        msg.aspectRatio = 2U; // 16:9
        msg.rotation = 1U; // 90 deg
        msg.mirror = 0U;

        const auto pkt = SightlineDisplayBuilder::buildSetVideoDisplay(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::VideoDisplay);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetVideoDisplay(msg));

        MsgVideoDisplay out {};
        ASSERT_TRUE(SightlineDisplayParser::parseVideoDisplay(pkt, out));
        EXPECT_EQ(out.displayIndex, 0U);
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.aspectRatio, 2U);
        EXPECT_EQ(out.rotation, 1U);
        EXPECT_EQ(out.mirror, 0U);

        MsgVideoDisplay facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVideoDisplay(pkt, facadeOut));
        EXPECT_EQ(facadeOut.aspectRatio, 2U);

        const auto queryPkt = SightlineDisplayBuilder::buildGetVideoDisplay(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetVideoDisplay(0U));
    }

    /// @brief Verify multi-display PiP split screen serialization, parsing, and query.
    TEST(TestSightlineVideoPipeline, BuildAndParseMultiDisplay)
    {
        MsgMultiDisplay msg {};
        msg.displayIndex = 0U;
        msg.layout = 1U; // PiP
        msg.pipCameraIndex = 2U;
        msg.pipX = 1400U;
        msg.pipY = 50U;
        msg.pipWidth = 480U;
        msg.pipHeight = 270U;

        const auto pkt = SightlineDisplayBuilder::buildSetMultiDisplay(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::MultiDisplay);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetMultiDisplay(msg));

        MsgMultiDisplay out {};
        ASSERT_TRUE(SightlineDisplayParser::parseMultiDisplay(pkt, out));
        EXPECT_EQ(out.displayIndex, 0U);
        EXPECT_EQ(out.layout, 1U);
        EXPECT_EQ(out.pipCameraIndex, 2U);
        EXPECT_EQ(out.pipX, 1400U);
        EXPECT_EQ(out.pipY, 50U);
        EXPECT_EQ(out.pipWidth, 480U);
        EXPECT_EQ(out.pipHeight, 270U);

        MsgMultiDisplay facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMultiDisplay(pkt, facadeOut));
        EXPECT_EQ(facadeOut.pipWidth, 480U);

        const auto queryPkt = SightlineDisplayBuilder::buildGetMultiDisplay(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetMultiDisplay(0U));
    }

    TEST(SightlineEnhancementTest, FullEnhancementEncodingAndDecoding)
    {
        MsgSetVideoEnhancementFull msg {};
        msg.cameraIndex = 1U;
        msg.mode = ContrastMode::Lap16;
        msg.sharpening = 12U;
        msg.alphaBlend = 210U;
        msg.enhanceParam = 15U;
        msg.denoiseRate = 180U;
        msg.flags = static_cast<std::uint8_t>(EnhancementFlags::AerialMotionMask) |
                    static_cast<std::uint8_t>(EnhancementFlags::FeatureBasedHist);
        msg.histAveRate = 64U;
        msg.histMaxPctBin = 20U;
        msg.roiRow = 100U;
        msg.roiCol = 150U;
        msg.roiHigh = 480U;
        msg.roiWide = 640U;
        msg.deconvSigma = 0U;
        msg.gaussianBlur = 4U;
        msg.lapMinDiff = 5U;
        msg.colorEnhance = 150U;
        msg.brightness = 135U;
        msg.contrast = 140U;
        msg.scintillation = ScintillationPreset::InfraRed;
        msg.sharpenRadius = 3U;
        msg.customKernel = { -1, -1, -1, -1, 9, -1, -1, -1, -1 };
        msg.normalizeKernel = true;

        const auto pkt = SightlineEnhancementBuilder::buildSetVideoEnhanceFull(msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetVideoEnhancementParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetVideoEnhanceFull(msg));

        // Test parser
        MsgSetVideoEnhancementFull parsed {};
        ASSERT_TRUE(SightlineEnhancementParser::parseVideoEnhanceFull(pkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_EQ(parsed.mode, ContrastMode::Lap16);
        EXPECT_EQ(parsed.sharpening, 12U);
        EXPECT_EQ(parsed.alphaBlend, 210U);
        EXPECT_EQ(parsed.enhanceParam, 15U);
        EXPECT_EQ(parsed.denoiseRate, 180U);
        EXPECT_EQ(parsed.flags, msg.flags);
        EXPECT_EQ(parsed.histAveRate, 64U);
        EXPECT_EQ(parsed.histMaxPctBin, 20U);
        EXPECT_EQ(parsed.roiRow, 100U);
        EXPECT_EQ(parsed.roiCol, 150U);
        EXPECT_EQ(parsed.roiHigh, 480U);
        EXPECT_EQ(parsed.roiWide, 640U);
        EXPECT_EQ(parsed.gaussianBlur, 4U);
        EXPECT_EQ(parsed.lapMinDiff, 5U);
        EXPECT_EQ(parsed.colorEnhance, 150U);
        EXPECT_EQ(parsed.brightness, 135U);
        EXPECT_EQ(parsed.contrast, 140U);
        EXPECT_EQ(parsed.scintillation, ScintillationPreset::InfraRed);
        EXPECT_EQ(parsed.sharpenRadius, 3U);
        ASSERT_EQ(parsed.customKernel.size(), 9U);
        EXPECT_EQ(parsed.customKernel[4], 9);
        EXPECT_TRUE(parsed.normalizeKernel);

        // Test facade parser
        MsgSetVideoEnhancementFull facadeParsed {};
        ASSERT_TRUE(SightlineProtocolParser::parseVideoEnhanceFull(pkt, facadeParsed));
        EXPECT_EQ(facadeParsed.mode, ContrastMode::Lap16);

        // Test legacy parser interoperability
        MsgSetVideoEnhancement legacyParsed {};
        ASSERT_TRUE(SightlineEnhancementParser::parseVideoEnhance(pkt, legacyParsed));
        EXPECT_EQ(legacyParsed.cameraIndex, 1U);
        EXPECT_EQ(legacyParsed.sharpening, 12U);
        EXPECT_EQ(legacyParsed.contrast, 140U);
        EXPECT_EQ(legacyParsed.brightness, 135U);
    }

    TEST(SightlineEnhancementTest, FalseColorEncoding)
    {
        const auto pkt = SightlineEnhancementBuilder::buildSetFalseColor(0U, FalseColorPalette::Iron256);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetDisplayParameters);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetFalseColor(0U, FalseColorPalette::Iron256));

        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_GE(payload.size(), 17U);
        EXPECT_EQ(payload[5], static_cast<std::uint8_t>(FalseColorPalette::Iron256)); // falseColorZTT
        EXPECT_EQ(payload[6], 64U); // zoom = 1X (64)
        EXPECT_EQ(payload[11], 0U); // cameraIndex
    }

    TEST(SightlineEnhancementTest, CustomKernelSizes)
    {
        MsgSetVideoEnhancementFull msg {};
        msg.cameraIndex = 0U;
        // 5x5 kernel (25 elements)
        msg.customKernel.assign(25U, 1);
        msg.normalizeKernel = false;

        const auto pkt = SightlineEnhancementBuilder::buildSetVideoEnhanceFull(msg);
        MsgSetVideoEnhancementFull parsed {};
        ASSERT_TRUE(SightlineEnhancementParser::parseVideoEnhanceFull(pkt, parsed));
        ASSERT_EQ(parsed.customKernel.size(), 25U);
        EXPECT_FALSE(parsed.normalizeKernel);
        for (std::size_t i = 0; i < 25U; ++i) {
            EXPECT_EQ(parsed.customKernel[i], 1);
        }
    }

    // ==========================================================================
    // Phase 1 Recording & Validation Tests
    // ==========================================================================

    TEST(RecordingValidatorTest, FilenameValidation)
    {
        // Valid filename without trailing numerals
        EXPECT_EQ(
            RecordingValidator::checkFilename("mission_rec", 0U),
            RecordingStatusCode::Success);

        // Valid filename with underscore suffix
        EXPECT_EQ(
            RecordingValidator::checkFilename("flight_A_", 0U),
            RecordingStatusCode::Success);

        // Trailing numeral without overwrite flag must fail
        EXPECT_EQ(
            RecordingValidator::checkFilename("flight_01", 0U),
            RecordingStatusCode::ErrNumericFilename);

        // Trailing numeral with AllowNumericOverwrite flag must pass
        const auto allowFlag { static_cast<std::uint8_t>(RecordingFlags::AllowNumericOverwrite) };
        EXPECT_EQ(
            RecordingValidator::checkFilename("flight_01", allowFlag),
            RecordingStatusCode::Success);

        // Empty filename must fail
        EXPECT_EQ(
            RecordingValidator::checkFilename("", 0U),
            RecordingStatusCode::ErrInvalidCharacters);

        // Filename exceeding max length (64 chars) must fail
        const std::string longName(65U, 'a');
        EXPECT_EQ(
            RecordingValidator::checkFilename(longName, 0U),
            RecordingStatusCode::ErrInvalidCharacters);

        // Forbidden characters
        EXPECT_EQ(
            RecordingValidator::checkFilename("path/to/file", 0U),
            RecordingStatusCode::ErrInvalidCharacters);
        EXPECT_EQ(
            RecordingValidator::checkFilename("file*name", 0U),
            RecordingStatusCode::ErrInvalidCharacters);
        EXPECT_EQ(
            RecordingValidator::checkFilename("file?name", 0U),
            RecordingStatusCode::ErrInvalidCharacters);
    }

    TEST(RecordingValidatorTest, CameraValidation)
    {
        // Valid camera indexes
        EXPECT_EQ(RecordingValidator::checkCamera(0U), RecordingStatusCode::Success);
        EXPECT_EQ(RecordingValidator::checkCamera(1U), RecordingStatusCode::Success);
        EXPECT_EQ(RecordingValidator::checkCamera(2U), RecordingStatusCode::Success);

        // Invalid camera index
        EXPECT_EQ(RecordingValidator::checkCamera(3U), RecordingStatusCode::ErrChannelUnsupported);

        // 0xFF (Snap All Cams) disallowed when false
        EXPECT_EQ(RecordingValidator::checkCamera(0xFFU, false), RecordingStatusCode::ErrChannelUnsupported);

        // 0xFF allowed when true
        EXPECT_EQ(RecordingValidator::checkCamera(0xFFU, true), RecordingStatusCode::Success);
    }

    TEST(RecordingValidatorTest, SnapshotValidation)
    {
        MsgDoSnapShotV2 msg {};
        msg.sequenceId = 1U;
        msg.cameraIndex = 0U;
        msg.format = SnapshotFormat::Jpeg;
        msg.domain = SnapshotDomain::Capture;
        msg.qualityLevel = 85U;
        msg.burstCount = 1U;
        msg.customFilename = "snap_ok";

        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::Success);

        // Invalid quality
        msg.qualityLevel = 0U;
        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::ErrMalformedPayload);
        msg.qualityLevel = 101U;
        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::ErrMalformedPayload);
        msg.qualityLevel = 85U;

        // Invalid burst count
        msg.burstCount = 0U;
        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::ErrMalformedPayload);
        msg.burstCount = 121U;
        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::ErrMalformedPayload);
        msg.burstCount = 10U;

        // Invalid filename
        msg.customFilename = "snap/invalid";
        EXPECT_EQ(RecordingValidator::checkSnapshot(msg), RecordingStatusCode::ErrInvalidCharacters);
    }

    TEST(RecordingValidatorTest, StorageValidation)
    {
        std::uint32_t freeMB { 0U };

        // FTP push does not require local mount
        EXPECT_EQ(
            RecordingValidator::checkStorage(StorageDestination::FtpPush, 0ULL, false, freeMB),
            RecordingStatusCode::Success);

        // Unmounted MicroSD
        EXPECT_EQ(
            RecordingValidator::checkStorage(StorageDestination::MicroSD, 100000000ULL, false, freeMB),
            RecordingStatusCode::ErrMediaUnavailable);

        // Insufficient storage (< 50 MB)
        EXPECT_EQ(
            RecordingValidator::checkStorage(StorageDestination::MicroSD, 1000000ULL, true, freeMB),
            RecordingStatusCode::ErrInsufficientStorage);

        // Valid storage (100 MB available)
        EXPECT_EQ(
            RecordingValidator::checkStorage(StorageDestination::MicroSD, 104857600ULL, true, freeMB),
            RecordingStatusCode::Success);
        EXPECT_EQ(freeMB, 100U);
    }

    TEST(TestSightlineVideoPipeline, CommandAckRoundTrip)
    {
        MsgCommandAck inAck {};
        inAck.sequenceId = 0x1A2BU;
        inAck.originalMsgId = static_cast<std::uint8_t>(MessageId::SetFileRecordingParamsV2);
        inAck.statusCode = RecordingStatusCode::Success;
        inAck.freeStorageMB = 14200U;
        inAck.subsystemState = 0x03U;

        const auto pkt = SightlineRecordingBuilder::buildCmdAck(inAck);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::CommandAck);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildCmdAck(inAck));

        MsgCommandAck outAck {};
        ASSERT_TRUE(SightlineRecordingParser::parseCmdAck(pkt, outAck));
        EXPECT_EQ(outAck.sequenceId, 0x1A2BU);
        EXPECT_EQ(outAck.originalMsgId, static_cast<std::uint8_t>(MessageId::SetFileRecordingParamsV2));
        EXPECT_EQ(outAck.statusCode, RecordingStatusCode::Success);
        EXPECT_EQ(outAck.freeStorageMB, 14200U);
        EXPECT_EQ(outAck.subsystemState, 0x03U);

        // Facade equivalence
        MsgCommandAck facadeAck {};
        ASSERT_TRUE(SightlineProtocolParser::parseCmdAck(pkt, facadeAck));
        EXPECT_EQ(facadeAck.sequenceId, 0x1A2BU);
    }

    TEST(TestSightlineVideoPipeline, SetFileRecordingV2RoundTrip)
    {
        MsgSetFileRecordingParamsV2 inMsg {};
        inMsg.sequenceId = 42U;
        inMsg.cameraIndex = 1U;
        inMsg.action = RecordingAction::Start;
        inMsg.destination = StorageDestination::UsbDrive;
        inMsg.flags = 0x05U;
        inMsg.maxSplitSizeBytes = 1073741824U;
        inMsg.maxSplitFrames = 1800U;
        inMsg.baseFilename = "Flight_Record";

        const auto pkt = SightlineRecordingBuilder::buildSetFileRecordingV2(inMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetFileRecordingParamsV2);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetFileRecordingV2(inMsg));

        MsgSetFileRecordingParamsV2 outMsg {};
        ASSERT_TRUE(SightlineRecordingParser::parseSetFileRecordingV2(pkt, outMsg));
        EXPECT_EQ(outMsg.sequenceId, 42U);
        EXPECT_EQ(outMsg.cameraIndex, 1U);
        EXPECT_EQ(outMsg.action, RecordingAction::Start);
        EXPECT_EQ(outMsg.destination, StorageDestination::UsbDrive);
        EXPECT_EQ(outMsg.flags, 0x05U);
        EXPECT_EQ(outMsg.maxSplitSizeBytes, 1073741824U);
        EXPECT_EQ(outMsg.maxSplitFrames, 1800U);
        EXPECT_EQ(outMsg.baseFilename, "Flight_Record");

        // Facade equivalence
        MsgSetFileRecordingParamsV2 facadeMsg {};
        ASSERT_TRUE(SightlineProtocolParser::parseSetFileRecordingV2(pkt, facadeMsg));
        EXPECT_EQ(facadeMsg.baseFilename, "Flight_Record");
    }

    TEST(TestSightlineVideoPipeline, DoSnapShotV2RoundTrip)
    {
        MsgDoSnapShotV2 inSnap {};
        inSnap.sequenceId = 99U;
        inSnap.cameraIndex = 0xFFU; // All cameras
        inSnap.format = SnapshotFormat::Slraw;
        inSnap.domain = SnapshotDomain::Capture;
        inSnap.qualityLevel = 95U;
        inSnap.burstCount = 30U;
        inSnap.customFilename = "RawBurst";

        const auto pkt = SightlineRecordingBuilder::buildDoSnapShotV2(inSnap);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::DoSnapShotV2);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildDoSnapShotV2(inSnap));

        MsgDoSnapShotV2 outSnap {};
        ASSERT_TRUE(SightlineRecordingParser::parseDoSnapShotV2(pkt, outSnap));
        EXPECT_EQ(outSnap.sequenceId, 99U);
        EXPECT_EQ(outSnap.cameraIndex, 0xFFU);
        EXPECT_EQ(outSnap.format, SnapshotFormat::Slraw);
        EXPECT_EQ(outSnap.domain, SnapshotDomain::Capture);
        EXPECT_EQ(outSnap.qualityLevel, 95U);
        EXPECT_EQ(outSnap.burstCount, 30U);
        EXPECT_EQ(outSnap.customFilename, "RawBurst");

        // Facade equivalence
        MsgDoSnapShotV2 facadeSnap {};
        ASSERT_TRUE(SightlineProtocolParser::parseDoSnapShotV2(pkt, facadeSnap));
        EXPECT_EQ(facadeSnap.customFilename, "RawBurst");
    }

    TEST(TestSightlineVideoPipeline, RecordingRingBufferBasic)
    {
        RecordingRingBuffer ringBuffer(4U);
        EXPECT_EQ(ringBuffer.capacity(), 4U);
        EXPECT_EQ(ringBuffer.size(), 0U);
        EXPECT_TRUE(ringBuffer.empty());
        EXPECT_FALSE(ringBuffer.full());
        EXPECT_EQ(ringBuffer.droppedCount(), 0U);
        EXPECT_EQ(ringBuffer.utilizationPercent(), 0U);

        RecordingChunk c1 {};
        c1.timestampUs = 1000U;
        c1.cameraIndex = 0U;
        c1.data = { 0x01, 0x02 };

        RecordingChunk c2 {};
        c2.timestampUs = 2000U;

        RecordingChunk c3 {};
        c3.timestampUs = 3000U;

        RecordingChunk c4 {};
        c4.timestampUs = 4000U;

        EXPECT_TRUE(ringBuffer.push(std::move(c1)));
        EXPECT_TRUE(ringBuffer.push(std::move(c2)));
        EXPECT_TRUE(ringBuffer.push(std::move(c3)));
        EXPECT_EQ(ringBuffer.size(), 3U);
        EXPECT_FALSE(ringBuffer.empty());
        EXPECT_FALSE(ringBuffer.full());

        EXPECT_TRUE(ringBuffer.push(std::move(c4)));
        EXPECT_TRUE(ringBuffer.full());
        EXPECT_EQ(ringBuffer.size(), 4U);

        // Buffer full - next push must drop
        RecordingChunk c5 {};
        c5.timestampUs = 5000U;
        EXPECT_FALSE(ringBuffer.push(std::move(c5)));
        EXPECT_EQ(ringBuffer.droppedCount(), 1U);
        EXPECT_EQ(ringBuffer.size(), 4U);

        const auto poppedOpt = ringBuffer.pop();
        ASSERT_TRUE(poppedOpt.has_value());
        EXPECT_EQ(poppedOpt->timestampUs, 1000U);
        EXPECT_EQ(poppedOpt->data.size(), 2U);
        EXPECT_EQ(ringBuffer.size(), 3U);
        EXPECT_FALSE(ringBuffer.full());

        RecordingChunk c6 {};
        c6.timestampUs = 6000U;
        EXPECT_TRUE(ringBuffer.push(std::move(c6)));
        EXPECT_TRUE(ringBuffer.full());

        std::vector<RecordingChunk> drained {};
        while (auto opt = ringBuffer.pop()) {
            drained.push_back(std::move(*opt));
        }
        EXPECT_EQ(drained.size(), 4U);
        EXPECT_EQ(drained[0].timestampUs, 2000U);
        EXPECT_EQ(drained[1].timestampUs, 3000U);
        EXPECT_EQ(drained[2].timestampUs, 4000U);
        EXPECT_EQ(drained[3].timestampUs, 6000U);
        EXPECT_TRUE(ringBuffer.empty());

        ringBuffer.clear();
        EXPECT_EQ(ringBuffer.droppedCount(), 0U);
    }

    TEST(TestSightlineVideoPipeline, StorageRetentionManagerPrune)
    {
        StorageRetentionManager manager {};

        std::vector<FileMetadataEntry> entries {
            { "video_001.ts", 100000000ULL, 1000U, false },
            { "video_002.ts", 200000000ULL, 2000U, false },
            { "video_003.ts", 300000000ULL, 3000U, false },
            { "video_004.ts", 400000000ULL, 4000U, false }
        };

        // Pin video_001 so it cannot be pruned
        manager.pinFile("video_001.ts");
        EXPECT_TRUE(manager.isPinned("video_001.ts"));

        // Request reclamation of 250 MB (250,000,000 bytes)
        // Expected: oldest unpinned files evicted first (video_002 is 200MB, so need video_003 as well)
        std::uint64_t reclaimedBytes { 0ULL };
        const auto pruneList = manager.pruneOldest(entries, 250000000ULL, reclaimedBytes);
        ASSERT_EQ(pruneList.size(), 2U);
        EXPECT_EQ(pruneList[0], "video_002.ts");
        EXPECT_EQ(pruneList[1], "video_003.ts");
        EXPECT_GE(reclaimedBytes, 250000000ULL);

        // Now unpin video_001
        manager.unpinFile("video_001.ts");
        EXPECT_FALSE(manager.isPinned("video_001.ts"));

        // Recalculate: video_001 is now eligible as oldest
        const auto newPruneList = manager.pruneOldest(entries, 250000000ULL, reclaimedBytes);
        ASSERT_EQ(newPruneList.size(), 2U);
        EXPECT_EQ(newPruneList[0], "video_001.ts");
        EXPECT_EQ(newPruneList[1], "video_002.ts");
        EXPECT_GE(reclaimedBytes, 250000000ULL);

        // Clear all pinned
        manager.clearPinned();
        EXPECT_FALSE(manager.isPinned("video_001.ts"));
    }

    TEST(TestSightlineVideoPipeline, RecordingEventRoundTrip)
    {
        MsgFileRecordingEvent inEvt {};
        inEvt.timestampUs = 987654321ULL;
        inEvt.cameraIndex = 1U;
        inEvt.eventType = RecordingEventType::Stopped;
        inEvt.statusCode = 0U;
        inEvt.freeStorageMB = 24000U;
        inEvt.queueFullPercent = 15U;
        inEvt.eventPayload = "Mission_01_0001.ts";

        const auto pkt = SightlineRecordingBuilder::buildRecordingEvent(inEvt);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::FileRecordingEvent);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildRecordingEvent(inEvt));

        MsgFileRecordingEvent outEvt {};
        ASSERT_TRUE(SightlineRecordingParser::parseRecordingEvent(pkt, outEvt));
        EXPECT_EQ(outEvt.timestampUs, 987654321ULL);
        EXPECT_EQ(outEvt.cameraIndex, 1U);
        EXPECT_EQ(outEvt.eventType, RecordingEventType::Stopped);
        EXPECT_EQ(outEvt.statusCode, 0U);
        EXPECT_EQ(outEvt.freeStorageMB, 24000U);
        EXPECT_EQ(outEvt.queueFullPercent, 15U);
        EXPECT_EQ(outEvt.eventPayload, "Mission_01_0001.ts");

        // Facade equivalence
        MsgFileRecordingEvent facadeEvt {};
        ASSERT_TRUE(SightlineProtocolParser::parseRecordingEvent(pkt, facadeEvt));
        EXPECT_EQ(facadeEvt.eventPayload, "Mission_01_0001.ts");
    }

    TEST(TestSightlineVideoPipeline, RecordingStatusV2RoundTrip)
    {
        MsgCurrentRecordingStatusV2 inStatus {};
        inStatus.sequenceId = 200U;
        inStatus.cameraIndex = 2U;
        inStatus.recordingState = 1U;
        inStatus.currentBitrateKbps = 15200U;
        inStatus.totalBytesWritten = 1073741824ULL;
        inStatus.freeStorageMB = 28400U;
        inStatus.estRemainingSecs = 1800U;
        inStatus.ringBufferPercent = 12U;
        inStatus.droppedFrames = 2U;
        inStatus.activeFileFrameCount = 3600U;
        inStatus.activeFilename = "ActiveRec.ts";

        const auto pkt = SightlineRecordingBuilder::buildRecordingStatusV2(inStatus);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::CurrentRecordingStatusV2);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildRecordingStatusV2(inStatus));

        MsgCurrentRecordingStatusV2 outStatus {};
        ASSERT_TRUE(SightlineRecordingParser::parseRecordingStatusV2(pkt, outStatus));
        EXPECT_EQ(outStatus.sequenceId, 200U);
        EXPECT_EQ(outStatus.cameraIndex, 2U);
        EXPECT_EQ(outStatus.recordingState, 1U);
        EXPECT_EQ(outStatus.currentBitrateKbps, 15200U);
        EXPECT_EQ(outStatus.totalBytesWritten, 1073741824ULL);
        EXPECT_EQ(outStatus.freeStorageMB, 28400U);
        EXPECT_EQ(outStatus.estRemainingSecs, 1800U);
        EXPECT_EQ(outStatus.ringBufferPercent, 12U);
        EXPECT_EQ(outStatus.droppedFrames, 2U);
        EXPECT_EQ(outStatus.activeFileFrameCount, 3600U);
        EXPECT_EQ(outStatus.activeFilename, "ActiveRec.ts");

        // Facade equivalence
        MsgCurrentRecordingStatusV2 facadeStatus {};
        ASSERT_TRUE(SightlineProtocolParser::parseRecordingStatusV2(pkt, facadeStatus));
        EXPECT_EQ(facadeStatus.activeFilename, "ActiveRec.ts");
    }

} // namespace
} // namespace Sightline

