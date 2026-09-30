/// @file TestSightlineVideoPipeline.cpp
/// @brief Unit tests for Sightline video capture, display, compression, and streaming builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineCaptureBuilder.h"
#include "modules/SightlineCaptureParser.h"
#include "modules/SightlineCompressionBuilder.h"
#include "modules/SightlineCompressionParser.h"
#include "modules/SightlineDisplayBuilder.h"
#include "modules/SightlineEnhancementBuilder.h"
#include "modules/SightlineNetworkBuilder.h"
#include "modules/SightlineRecordingBuilder.h"
#include "modules/SightlineRecordingParser.h"

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
        payload.insert(payload.end(), fn.begin(), fn.end());

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

} // namespace
} // namespace Sightline
