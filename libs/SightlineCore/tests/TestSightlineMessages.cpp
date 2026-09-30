/// @file TestSightlineMessages.cpp
/// @brief Unit tests for Sightline SLA message builders and parsers.

#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify start tracking command serialization.
    TEST(TestSightlineMessages, BuildStartTracking)
    {
        MsgStartTracking msg {};
        msg.cameraIndex = 1U;
        msg.centerCol = 640U;
        msg.centerRow = 480U;
        msg.width = 80U;
        msg.height = 60U;
        msg.flags = 0x01U;

        const auto pkt = SightlineProtocolBuilder::buildStartTracking(msg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(pkt), MessageId::StartTracking);

        const auto payload = SightlineProtocolParser::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 10U);
        EXPECT_EQ(payload[0], 1U);
    }

    /// @brief Verify stop tracking command serialization.
    TEST(TestSightlineMessages, BuildStopTracking)
    {
        MsgStopTracking msg {};
        msg.cameraIndex = 2U;
        msg.trackId = 5U;

        const auto pkt = SightlineProtocolBuilder::buildStopTracking(msg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(pkt), MessageId::StopTracking);

        const auto payload = SightlineProtocolParser::extractPayload(pkt);
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
        const auto modPkt = SightlineProtocolBuilder::buildModifyTrackIndex(modMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(modPkt), MessageId::ModifyTrackIndex);
        const auto modPayload = SightlineProtocolParser::extractPayload(modPkt);
        ASSERT_EQ(modPayload.size(), 7U);
        EXPECT_EQ(modPayload[0], 5U);
        EXPECT_EQ(modPayload[1], 0U);
        EXPECT_EQ(modPayload[2], 2U);
    }

    /// @brief Verify system and network command serialization.
    TEST(TestSightlineMessages, BuildSystemCommands)
    {
        MsgResetAllParameters resetMsg {};
        resetMsg.resetType = 1U;
        const auto resetPkt = SightlineProtocolBuilder::buildResetAllParameters(resetMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(resetPkt), MessageId::ResetAllParameters);
        EXPECT_EQ(SightlineProtocolParser::extractPayload(resetPkt)[0], 1U);

        MsgSaveParameters saveMsg {};
        saveMsg.commitType = 2U;
        const auto savePkt = SightlineProtocolBuilder::buildSaveParameters(saveMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(savePkt), MessageId::SaveParameters);
        EXPECT_TRUE(SightlineProtocolParser::extractPayload(savePkt).empty());

        const auto verPkt = SightlineProtocolBuilder::buildGetVersionNumber();
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(verPkt), MessageId::GetVersionNumber);
        EXPECT_TRUE(SightlineProtocolParser::extractPayload(verPkt).empty());

        MsgSetNetworkParameters netMsg {};
        netMsg.ipAddress = 0xC0A80164U; // 192.168.1.100
        netMsg.subnetMask = 0xFFFFFF00U;
        netMsg.gateway = 0xC0A80101U;
        netMsg.dhcpEnable = 0U;
        netMsg.commandPort = 14001U;
        netMsg.replyPort = 14002U;
        const auto netPkt = SightlineProtocolBuilder::buildSetNetworkParameters(netMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(netPkt), MessageId::SetNetworkParameters);
        EXPECT_EQ(SightlineProtocolParser::extractPayload(netPkt).size(), 17U);

        MsgSetPortConfiguration portMsg {};
        portMsg.portIndex = 0U;
        portMsg.baudRate = 115200U;
        portMsg.mode = 1U;
        const auto portPkt = SightlineProtocolBuilder::buildSetPortConfiguration(portMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(portPkt), MessageId::SetPortConfiguration);

        MsgCommandPassThrough passMsg {};
        passMsg.destPort = 2U;
        passMsg.data = { 0xAAU, 0xBBU, 0xCCU };
        const auto passPkt = SightlineProtocolBuilder::buildCommandPassThrough(passMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(passPkt), MessageId::CommandPassThrough);
        EXPECT_EQ(SightlineProtocolParser::extractPayload(passPkt).size(), 4U);
    }

    /// @brief Verify tracking, motion and detection builder serialization.
    TEST(TestSightlineMessages, BuildTrackingMotion)
    {
        MsgNudgeTrackingCoordinate nudgeMsg {};
        nudgeMsg.cameraIndex = 0U;
        nudgeMsg.deltaCol = -2;
        nudgeMsg.deltaRow = 3;
        const auto nudgePkt = SightlineProtocolBuilder::buildNudgeTracking(nudgeMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(nudgePkt), MessageId::NudgeTrackingCoordinate);

        MsgCoordinateReportingMode repMsg {};
        repMsg.cameraIndex = 1U;
        repMsg.framePeriod = 2U;
        repMsg.reportingFlags = 0x07U;
        const auto repPkt = SightlineProtocolBuilder::buildSetReportingMode(repMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(repPkt), MessageId::CoordinateReportingMode);

        MsgSetTrackingParameters trkParams {};
        trkParams.cameraIndex = 0U;
        trkParams.mode = 1U;
        trkParams.acquisitionSearchCol = 200U;
        trkParams.acquisitionSearchRow = 150U;
        const auto trkPkt = SightlineProtocolBuilder::buildSetTrackingParameters(trkParams);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(trkPkt), MessageId::SetTrackingParameters);

        MsgDesignateSelectedTrackPrimary desMsg {};
        desMsg.cameraIndex = 0U;
        desMsg.trackId = 3U;
        const auto desPkt = SightlineProtocolBuilder::buildDesignatePrimary(desMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(desPkt), MessageId::DesignateSelectedTrackPrimary);

        MsgShiftSelectedTrack shiftMsg {};
        shiftMsg.cameraIndex = 0U;
        shiftMsg.trackId = 2U;
        shiftMsg.shiftCol = 5;
        shiftMsg.shiftRow = -4;
        const auto shiftPkt = SightlineProtocolBuilder::buildShiftSelectedTrack(shiftMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(shiftPkt), MessageId::ShiftSelectedTrack);

        MsgStopSelectedTrack stopMsg {};
        stopMsg.cameraIndex = 1U;
        stopMsg.trackId = 4U;
        const auto stopPkt = SightlineProtocolBuilder::buildStopSelectedTrack(stopMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(stopPkt), MessageId::StopSelectedTrack);

        MsgSetDetectionParameters detMsg {};
        detMsg.cameraIndex = 0U;
        detMsg.mode = 2U;
        detMsg.threshold = 30U;
        detMsg.minTargetSize = 8U;
        detMsg.maxTargetSize = 150U;
        const auto detPkt = SightlineProtocolBuilder::buildSetDetectionParams(detMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(detPkt), MessageId::SetDetectionParameters);

        MsgCustomAIDetect aiMsg {};
        aiMsg.cameraIndex = 0U;
        aiMsg.modelId = 1U;
        aiMsg.confidenceThreshold = 75U;
        aiMsg.nmsThreshold = 50U;
        const auto aiPkt = SightlineProtocolBuilder::buildCustomAIDetect(aiMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(aiPkt), MessageId::CustomAIDetect);
    }

    /// @brief Verify stabilization and multi-sensor blending serialization.
    TEST(TestSightlineMessages, BuildStabilizationRegistration)
    {
        MsgSetStabilizationParameters stabMsg {};
        stabMsg.cameraIndex = 0U;
        stabMsg.mode = 1U;
        stabMsg.autoBias = 1U;
        stabMsg.maxShift = 48U;
        const auto stabPkt = SightlineProtocolBuilder::buildSetStabilization(stabMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(stabPkt), MessageId::SetStabilizationParameters);

        MsgResetStabilizationParameters resetStab {};
        resetStab.resetType = 0U;
        resetStab.cameraIndex = 1U;
        const auto resetPkt = SightlineProtocolBuilder::buildResetStabilization(resetStab);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(resetPkt), MessageId::ResetStabilizationParameters);
        const auto resetPayload = SightlineProtocolParser::extractPayload(resetPkt);
        ASSERT_EQ(resetPayload.size(), 2U);
        EXPECT_EQ(resetPayload[0], 0U);
        EXPECT_EQ(resetPayload[1], 1U);

        MsgSetStabilizationBias biasMsg {};
        biasMsg.cameraIndex = 0U;
        biasMsg.biasCol = 10;
        biasMsg.biasRow = -15;
        biasMsg.biasRotation = 5;
        const auto biasPkt = SightlineProtocolBuilder::buildSetStabilizationBias(biasMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(biasPkt), MessageId::StabilizationBias);

        MsgSetRegistrationParameters regMsg {};
        regMsg.cameraIndex = 0U;
        regMsg.searchRange = 16U;
        regMsg.pyramidLevels = 4U;
        const auto regPkt = SightlineProtocolBuilder::buildSetRegistration(regMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(regPkt), MessageId::SetRegistrationParameters);

        MsgSetBlendParameters blendMsg {};
        blendMsg.primaryCamera = 0U;
        blendMsg.secondaryCamera = 1U;
        blendMsg.blendMode = 1U;
        blendMsg.alphaPercent = 75U;
        const auto blendPkt = SightlineProtocolBuilder::buildSetBlendParameters(blendMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(blendPkt), MessageId::SetBlendParameters);

        MsgNoise3D noiseMsg {};
        noiseMsg.cameraIndex = 0U;
        noiseMsg.enable = 1U;
        noiseMsg.temporalStrength = 60U;
        noiseMsg.spatialStrength = 40U;
        const auto noisePkt = SightlineProtocolBuilder::buildSetNoise3D(noiseMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(noisePkt), MessageId::Noise3D);
    }

    /// @brief Verify video pipeline and H.264 streaming commands serialization.
    TEST(TestSightlineMessages, BuildVideoPipeline)
    {
        MsgSetVideoParameters vidMsg {};
        vidMsg.cameraIndex = 0U;
        vidMsg.inputFormat = 1U;
        vidMsg.width = 1920U;
        vidMsg.height = 1080U;
        vidMsg.frameRate = 60U;
        const auto vidPkt = SightlineProtocolBuilder::buildSetVideoParameters(vidMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(vidPkt), MessageId::SetVideoParameters);

        MsgSetVideoMode modeMsg {};
        modeMsg.cameraIndex = 0U;
        modeMsg.freeze = 0U;
        modeMsg.digitalZoom = 150U;
        modeMsg.mirror = 1U;
        modeMsg.flip = 0U;
        const auto modePkt = SightlineProtocolBuilder::buildSetVideoMode(modeMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(modePkt), MessageId::SetVideoMode);

        MsgSetVideoEnhancement enhMsg {};
        enhMsg.cameraIndex = 0U;
        enhMsg.contrast = 65U;
        enhMsg.brightness = 55U;
        enhMsg.sharpening = 10U;
        enhMsg.claheEnable = 1U;
        const auto enhPkt = SightlineProtocolBuilder::buildSetVideoEnhance(enhMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(enhPkt), MessageId::SetVideoEnhancementParameters);

        MsgSetDisplayParameters dispMsg {};
        dispMsg.displayIndex = 0U;
        dispMsg.cameraIndex = 0U;
        dispMsg.xOffset = 100U;
        dispMsg.yOffset = 50U;
        dispMsg.displayWidth = 1280U;
        dispMsg.displayHeight = 720U;
        const auto dispPkt = SightlineProtocolBuilder::buildSetDisplayParams(dispMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(dispPkt), MessageId::SetDisplayParameters);

        MsgSetEthernetVideoParameters ethMsg {};
        ethMsg.streamIndex = 0U;
        ethMsg.destIpAddress = 0xE0000001U; // 224.0.0.1
        ethMsg.destPort = 15004U;
        ethMsg.protocol = 0U;
        ethMsg.ttl = 128U;
        const auto ethPkt = SightlineProtocolBuilder::buildSetEthernetVideo(ethMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(ethPkt), MessageId::SetEthernetVideoParameters);

        MsgSetH264Parameters h264Msg {};
        h264Msg.streamIndex = 0U;
        h264Msg.targetBitrateBps = 8000000U;
        h264Msg.gopLength = 60U;
        h264Msg.qualityLevel = 2U;
        h264Msg.rateControl = 1U;
        const auto h264Pkt = SightlineProtocolBuilder::buildSetH264Parameters(h264Msg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(h264Pkt), MessageId::SetH264Parameters);

        MsgSetSDRecordingParameters sdMsg {};
        sdMsg.recordingState = 1U;
        sdMsg.cameraIndex = 0U;
        sdMsg.filenamePrefix = "MISSION_01";
        const auto sdPkt = SightlineProtocolBuilder::buildSetSDRecording(sdMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sdPkt), MessageId::SetSDRecordingParameters);

        MsgStreamingControl streamMsg {};
        streamMsg.streamIndex = 0U;
        streamMsg.action = 1U;
        const auto streamPkt = SightlineProtocolBuilder::buildStreamingControl(streamMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(streamPkt), MessageId::StreamingControl);
    }

    /// @brief Verify metadata and KLV commands serialization.
    TEST(TestSightlineMessages, BuildMetadataKLV)
    {
        MsgSetMetadataValues metaMsg {};
        metaMsg.platformLatitudeDeg = 37.7749;
        metaMsg.platformLongitudeDeg = -122.4194;
        metaMsg.platformAltitudeMeters = 1500.0;
        metaMsg.platformHeadingDeg = 180.5;
        metaMsg.platformPitchDeg = -5.2;
        metaMsg.platformRollDeg = 1.1;
        metaMsg.sensorHorizontalFovDeg = 30.0;
        metaMsg.sensorVerticalFovDeg = 20.0;
        const auto metaPkt = SightlineProtocolBuilder::buildSetMetadataValues(metaMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(metaPkt), MessageId::SetMetadataValues);

        MsgMetadataStaticValues statMsg {};
        statMsg.missionId = "TASK_ALPHA";
        statMsg.platformTailNumber = "N12345";
        statMsg.securityClassification = "UNCLASSIFIED";
        const auto statPkt = SightlineProtocolBuilder::buildMetadataStaticValues(statMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(statPkt), MessageId::MetadataStaticValues);

        MsgSetMetadataRate rateMsg {};
        rateMsg.metadataType = 0U;
        rateMsg.ratePeriod = 2U;
        const auto ratePkt = SightlineProtocolBuilder::buildSetMetadataRate(rateMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(ratePkt), MessageId::SetMetadataRate);

        MsgSetTelemetryDestination destMsg {};
        destMsg.clientIndex = 0U;
        destMsg.clientIpAddress = 0xC0A801C8U; // 192.168.1.200
        destMsg.clientPort = 14002U;
        destMsg.flags = 0x01U;
        const auto destPkt = SightlineProtocolBuilder::buildSetTelemetryDest(destMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(destPkt), MessageId::SetTelemetryDestination);

        MsgCursorOnTarget cotMsg {};
        cotMsg.enable = 1U;
        cotMsg.broadcastPort = 1870U;
        cotMsg.uid = "VEHICLE_1";
        cotMsg.cotType = "a-f-G-E-V-C";
        const auto cotPkt = SightlineProtocolBuilder::buildCursorOnTarget(cotMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(cotPkt), MessageId::CursorOnTarget);
    }

    /// @brief Verify lens and hardware control commands serialization.
    TEST(TestSightlineMessages, BuildLensHardware)
    {
        MsgLensCommand lensCmd {};
        lensCmd.cameraIndex = 0U;
        lensCmd.commandType = 3U; // Zoom In
        lensCmd.rateOrPosition = 500;
        const auto lensPkt = SightlineProtocolBuilder::buildLensCommand(lensCmd);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(lensPkt), MessageId::LensCommand);

        MsgFocusParameters focusMsg {};
        focusMsg.cameraIndex = 0U;
        focusMsg.focusMode = 1U;
        focusMsg.roiX = 400U;
        focusMsg.roiY = 300U;
        focusMsg.roiWidth = 200U;
        focusMsg.roiHeight = 150U;
        const auto focusPkt = SightlineProtocolBuilder::buildFocusParameters(focusMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(focusPkt), MessageId::FocusParameters);

        MsgSetLensParameters lensParams {};
        lensParams.cameraIndex = 0U;
        lensParams.minFocalLengthMm = 4.5;
        lensParams.maxFocalLengthMm = 135.0;
        lensParams.horizontalFovWideDeg = 63.0;
        lensParams.horizontalFovTeleDeg = 2.1;
        const auto paramsPkt = SightlineProtocolBuilder::buildSetLensParameters(lensParams);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(paramsPkt), MessageId::SetLensParameters);

        MsgGPIO gpioMsg {};
        gpioMsg.pinMask = 0x0FU;
        gpioMsg.pinValues = 0x05U;
        gpioMsg.directionMask = 0x0FU;
        const auto gpioPkt = SightlineProtocolBuilder::buildGPIO(gpioMsg);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(gpioPkt), MessageId::GPIO);
    }

    /// @brief Verify overlay and reticle commands serialization.
    TEST(TestSightlineMessages, BuildOverlays)
    {
        MsgSetOverlayMode overlayMode {};
        overlayMode.displayIndex = 0U;
        overlayMode.reticleMode = 2U;
        overlayMode.trackingBoxMode = 1U;
        overlayMode.telemetryTextMode = 1U;
        const auto modePkt = SightlineProtocolBuilder::buildSetOverlayMode(overlayMode);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(modePkt), MessageId::SetOverlayMode);

        MsgDrawObject drawObj {};
        drawObj.displayIndex = 0U;
        drawObj.objectId = 1U;
        drawObj.shapeType = 1U; // Rectangle
        drawObj.x = 100U;
        drawObj.y = 120U;
        drawObj.width = 60U;
        drawObj.height = 40U;
        drawObj.colorRgba = 0xFF0000FFU; // Red
        drawObj.text = "TARGET";
        const auto objPkt = SightlineProtocolBuilder::buildDrawObject(drawObj);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(objPkt), MessageId::DrawObject);

        MsgDrawOverlay drawBatch {};
        drawBatch.displayIndex = 0U;
        drawBatch.clearDisplay = 1U;
        drawBatch.objects.push_back(drawObj);
        const auto batchPkt = SightlineProtocolBuilder::buildDrawOverlay(drawBatch);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(batchPkt), MessageId::DrawOverlay);
    }

    /// @brief Verify version number packet parsing.
    TEST(TestSightlineMessages, ParseVersionNumber)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(3U); // byte 0: swMajor
        payload.push_back(11U); // byte 1: swMinor
        payload.push_back(1U); // byte 2: hwVersion
        payload.push_back(125U); // byte 3: degreesF (125 F)

        // bytes 4..6: hwID (u24 LE)
        payload.push_back(0x56U);
        payload.push_back(0x34U);
        payload.push_back(0x12U);

        // bytes 7..10: appBits (u32 LE: 0x12345678)
        payload.push_back(0x78U);
        payload.push_back(0x56U);
        payload.push_back(0x34U);
        payload.push_back(0x12U);

        // byte 11: boardType (18 = 4000-OEM)
        payload.push_back(18U);

        // byte 12: swRelease (6)
        payload.push_back(6U);

        // bytes 13..14: otherVersion (u16 LE: 0x0400 -> boardRevision = 4)
        payload.push_back(0x00U);
        payload.push_back(0x04U);

        // bytes 15..18: srcRevision (u32 LE)
        payload.push_back(100U);
        payload.push_back(0U);
        payload.push_back(0U);
        payload.push_back(0U);

        // bytes 19..22: buildDate (u32 LE)
        payload.push_back(0x01U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        // bytes 23..26: buildTime (u32 LE)
        payload.push_back(0x02U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        // bytes 27..28: swBuild (u16 LE: 42)
        payload.push_back(42U);
        payload.push_back(0U);

        // bytes 29..30: v4AppBits (u16 LE: 0)
        payload.push_back(0U);
        payload.push_back(0U);

        // bytes 31..32: degreesC (s16 LE: 52 C)
        payload.push_back(52U);
        payload.push_back(0U);

        // bytes 33..36: adapters (u32 LE: 1)
        payload.push_back(1U);
        payload.push_back(0U);
        payload.push_back(0U);
        payload.push_back(0U);

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::VersionNumber, payload);

        MsgVersionNumber out {};
        ASSERT_TRUE(SightlineProtocolParser::parseVersionNumber(pkt, out));
        EXPECT_EQ(out.softwareMajor, 3U);
        EXPECT_EQ(out.softwareMinor, 11U);
        EXPECT_EQ(out.softwareRelease, 6U);
        EXPECT_EQ(out.softwarePatch, 6U);
        EXPECT_EQ(out.hardwareType, 18U);
        EXPECT_EQ(out.boardType, 18U);
        EXPECT_EQ(out.hardwareVersion, 1U);
        EXPECT_EQ(out.degreesF, 125U);
        EXPECT_EQ(out.degreesC, 52);
        EXPECT_EQ(out.appBits, 0x12345678U);
        EXPECT_EQ(out.boardRevision, 4U);
        EXPECT_EQ(out.softwareBuild, 42U);
        EXPECT_EQ(out.versionString, "3.11.6 (build 42)");
    }

    /// @brief Verify tracking positions telemetry parsing (0x51).
    TEST(TestSightlineMessages, ParseTrackingPositions)
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
        payload.push_back(0U); // byte 9: velCol8: 0
        payload.push_back(0U); // byte 10: velRow8: 0
        payload.push_back(95U); // byte 11: confidence: 95
        payload.push_back(0x01U); // byte 12: flags: primary
        payload.push_back(0U); // bytes 13..14: nearVal: 0
        payload.push_back(0U);

        // Optional trailer (12 bytes: 8-byte timestamp + 4-byte frameId)
        const std::uint64_t ts = 1000000ULL;
        for (std::size_t i = 0; i < 8; ++i) {
            payload.push_back(static_cast<std::uint8_t>((ts >> (i * 8)) & 0xFFU));
        }

        const std::uint32_t fn = 42U;
        for (std::size_t i = 0; i < 4; ++i) {
            payload.push_back(static_cast<std::uint8_t>((fn >> (i * 8)) & 0xFFU));
        }

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::TrackingPositions, payload);

        MsgTrackingPositions out {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingPositions(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.timestampUs, 1000000ULL);
        EXPECT_EQ(out.frameNumber, 42U);
        ASSERT_EQ(out.tracks.size(), 1U);
        EXPECT_EQ(out.tracks[0].trackId, 10U);
        EXPECT_DOUBLE_EQ(out.tracks[0].centerCol, 320.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].centerRow, 240.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].width, 64.0);
        EXPECT_DOUBLE_EQ(out.tracks[0].height, 48.0);
        EXPECT_EQ(out.tracks[0].confidence, 95U);
        EXPECT_TRUE(out.tracks[0].isPrimary);
    }

    /// @brief Verify single tracking position telemetry parsing (0x43).
    TEST(TestSightlineMessages, ParseTrackingPosition)
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

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::TrackingPosition, payload);

        MsgTrackingPosition out {};
        ASSERT_TRUE(SightlineProtocolParser::parseTrackingPosition(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_DOUBLE_EQ(out.col, 320.0);
        EXPECT_DOUBLE_EQ(out.row, 240.0);
        EXPECT_DOUBLE_EQ(out.translationCol, 256.0);
        EXPECT_DOUBLE_EQ(out.translationRow, 512.0);
        EXPECT_DOUBLE_EQ(out.rotationDeg, 10.0);
        EXPECT_DOUBLE_EQ(out.scale, 1.0);
        EXPECT_EQ(out.confidence, 99U);
    }

    /// @brief Verify extended tracking positions with AI class telemetry parsing (0xA0).
    TEST(TestSightlineMessages, ParsePositionsExtended)
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
        payload.push_back(0U); // byte 9: velCol8: 0
        payload.push_back(0U); // byte 10: velRow8: 0
        payload.push_back(90U); // byte 11: confidence
        payload.push_back(0x01U); // byte 12: primary
        payload.push_back(0U); // bytes 13..14: nearVal
        payload.push_back(0U);
        payload.push_back(3U); // byte 15: classifierLabel (e.g. Vehicle)
        payload.push_back(95U); // byte 16: classifierConf
        payload.push_back(1U); // byte 17: userTrackId
        payload.push_back(0U); // byte 18: trackColFrac8
        payload.push_back(0U); // byte 19: trackRowFrac8
        payload.push_back(0U); // byte 20: flags1

        // Optional trailer (12 bytes)
        const std::uint64_t ts = 2000000ULL;
        for (std::size_t i = 0; i < 8; ++i) {
            payload.push_back(static_cast<std::uint8_t>((ts >> (i * 8)) & 0xFFU));
        }

        const std::uint32_t fn = 100U;
        for (std::size_t i = 0; i < 4; ++i) {
            payload.push_back(static_cast<std::uint8_t>((fn >> (i * 8)) & 0xFFU));
        }

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::TrackingPositionsExtended, payload);

        MsgTrackingPositionsExtended out {};
        ASSERT_TRUE(SightlineProtocolParser::parsePositionsExtended(pkt, out));
        EXPECT_EQ(out.timestampUs, 2000000ULL);
        EXPECT_EQ(out.frameNumber, 100U);
        ASSERT_EQ(out.tracks.size(), 1U);
        ASSERT_EQ(out.classIds.size(), 1U);
        EXPECT_EQ(out.tracks[0].trackId, 1U);
        EXPECT_EQ(out.classIds[0], 3U);
    }

    /// @brief Verify current stabilization parameters deserialization (0x41).
    TEST(TestSightlineMessages, ParseStabilizationParams)
    {
        std::vector<std::uint8_t> payload {
            1U, // mode: On
            30U, // rate
            64U, // translationLimit
            10U, // angleLimit
            2U, // cameraIndex
            48U // maxStabOff
        };
        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::CurrentStabilizationParameters, payload);

        MsgSetStabilizationParameters out {};
        ASSERT_TRUE(SightlineProtocolParser::parseStabilizationParams(pkt, out));
        EXPECT_EQ(out.mode, 1U);
        EXPECT_EQ(out.rate, 30U);
        EXPECT_EQ(out.translationLimit, 64U);
        EXPECT_EQ(out.angleLimit, 10U);
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.maxStabOff, 48U);
        EXPECT_EQ(out.maxShift, 48U);
    }

    /// @brief Verify system status message parsing (0x87).
    TEST(TestSightlineMessages, ParseSystemStatus)
    {
        std::vector<std::uint8_t> payload {};
        // bytes 0..7: errorFlags (u64 LE = 0)
        for (std::size_t i = 0; i < 8; ++i) {
            payload.push_back(0U);
        }
        // bytes 8..9: temperatureF (s16 LE = 125 F)
        payload.push_back(125U);
        payload.push_back(0U);
        // bytes 10..13: load0, load1, load2, load3
        payload.push_back(45U); // Core 0: 45%
        payload.push_back(30U); // Core 1: 30%
        payload.push_back(20U); // Core 2: 20%
        payload.push_back(10U); // Core 3: 10%
        // bytes 14..15: temperatureC (s16 LE = 52 C)
        payload.push_back(52U);
        payload.push_back(0U);
        // bytes 16..19: missedFrames
        payload.push_back(0U);
        payload.push_back(1U);
        payload.push_back(0U);
        payload.push_back(0U);

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::SystemStatusMessage, payload);

        MsgSystemStatusMessage out {};
        ASSERT_TRUE(SightlineProtocolParser::parseSystemStatus(pkt, out));
        EXPECT_EQ(out.cpuLoadPercent, 45U);
        EXPECT_EQ(out.load0, 45U);
        EXPECT_EQ(out.load1, 30U);
        EXPECT_EQ(out.load2, 20U);
        EXPECT_EQ(out.load3, 10U);
        EXPECT_EQ(out.temperatureF, 125);
        EXPECT_EQ(out.coreTempC, 52);
        EXPECT_EQ(out.missedFrames1, 1U);
        EXPECT_EQ(out.errorFlags, 0ULL);
    }

    /// @brief Verify system status mode builder (0x80).
    TEST(TestSightlineMessages, BuildSystemStatusMode)
    {
        MsgSystemStatusMode mode {};
        mode.systemStatusBits = 0x0001U;
        mode.systemDebugBits = 0x0400U; // Timing measurement bit 10

        const auto pkt = SightlineProtocolBuilder::buildSystemStatusMode(mode);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(pkt), MessageId::SystemStatusMode);
        const auto payload = SightlineProtocolParser::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 6U);
        EXPECT_EQ(payload[0], 0x01U);
        EXPECT_EQ(payload[1], 0x00U);
        EXPECT_EQ(payload[2], 0x00U);
        EXPECT_EQ(payload[3], 0x04U);
        EXPECT_EQ(payload[4], 0x00U);
        EXPECT_EQ(payload[5], 0x00U);
    }

    /// @brief Verify user warning message parsing (0x86).
    TEST(TestSightlineMessages, ParseUserWarning)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0x05U); // Warning code low
        payload.push_back(0x00U); // Warning code high
        const std::string msgText = "Camera 0 Track Dropped";
        payload.insert(payload.end(), msgText.begin(), msgText.end());

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::UserWarningMessage, payload);

        MsgUserWarningMessage out {};
        ASSERT_TRUE(SightlineProtocolParser::parseUserWarning(pkt, out));
        EXPECT_EQ(out.warningCode, 5U);
        EXPECT_EQ(out.message, "Camera 0 Track Dropped");
    }

    /// @brief Verify current configuration parsing (0x8E).
    TEST(TestSightlineMessages, ParseCurrentConfiguration)
    {
        std::vector<std::uint8_t> payload {
            2U, // maxCameras (byte 0)
            0U, // maxVirtCameras (byte 1)
            1U, // maxStreams (byte 2)
            1U, // maxProcessed (byte 3)
            0x03U, 0x00U, // cameraConfiguredBits (bytes 4..5): cameras 0, 1
            0x01U, 0x00U, // cameraConnectedBits (bytes 6..7): camera 0
            0x01U, 0x00U, 0x00U, 0x00U, // displayPresentBits (bytes 8..11)
            0x01U, 0x00U, 0x00U, 0x00U // captureStateBits (bytes 12..15)
        };
        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::CurrentConfiguration, payload);

        MsgCurrentConfiguration out {};
        ASSERT_TRUE(SightlineProtocolParser::parseCurrentConfiguration(pkt, out));
        EXPECT_EQ(out.maxCameras, 2U);
        EXPECT_EQ(out.maxVirtCameras, 0U);
        EXPECT_EQ(out.maxStreams, 1U);
        EXPECT_EQ(out.maxProcessed, 1U);
        EXPECT_EQ(out.cameraConfiguredBits, 0x0003U);
        EXPECT_EQ(out.cameraConnectedBits, 0x0001U);
        EXPECT_EQ(out.displayPresentBits, 1U);
        EXPECT_EQ(out.captureStateBits, 1U);
        EXPECT_EQ(out.numVideoInputs, 2U);
        EXPECT_EQ(out.numVideoOutputs, 1U);
        EXPECT_EQ(out.numDisplays, 1U);
    }

} // namespace
} // namespace Sightline
