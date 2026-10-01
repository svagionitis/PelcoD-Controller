/// @file TestSightlineSystem.cpp
/// @brief Unit tests for Sightline system configuration, diagnostics, and network builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineGeneralBuilder.h"
#include "modules/SightlineGeneralParser.h"
#include "modules/SightlineNetworkBuilder.h"
#include "modules/SightlineSerialBuilder.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify system and network command serialization.
    TEST(TestSightlineSystem, BuildSystemCommands)
    {
        MsgResetAllParameters resetMsg {};
        resetMsg.resetType = 1U;
        const auto resetPkt = SightlineGeneralBuilder::buildResetAllParameters(resetMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(resetPkt), MessageId::ResetAllParameters);
        EXPECT_EQ(SightlineFraming::extractPayload(resetPkt)[0], 1U);

        // Verify facade equivalence
        const auto facadeResetPkt = SightlineProtocolBuilder::buildResetAllParameters(resetMsg);
        EXPECT_EQ(resetPkt, facadeResetPkt);

        MsgSaveParameters saveMsg {};
        saveMsg.commitType = 2U;
        const auto savePkt = SightlineGeneralBuilder::buildSaveParameters(saveMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(savePkt), MessageId::SaveParameters);
        EXPECT_TRUE(SightlineFraming::extractPayload(savePkt).empty());

        const auto verPkt = SightlineGeneralBuilder::buildGetVersionNumber();
        EXPECT_EQ(SightlineFraming::identifyMessage(verPkt), MessageId::GetVersionNumber);
        EXPECT_TRUE(SightlineFraming::extractPayload(verPkt).empty());

        MsgSetNetworkParameters netMsg {};
        netMsg.ipAddress = 0xC0A80164U; // 192.168.1.100
        netMsg.subnetMask = 0xFFFFFF00U;
        netMsg.gateway = 0xC0A80101U;
        netMsg.dhcpEnable = 0U;
        netMsg.commandPort = 14001U;
        netMsg.replyPort = 14002U;
        const auto netPkt = SightlineNetworkBuilder::buildSetNetworkParameters(netMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(netPkt), MessageId::SetNetworkParameters);
        EXPECT_EQ(SightlineFraming::extractPayload(netPkt).size(), 17U);

        MsgSetPortConfiguration portMsg {};
        portMsg.portIndex = 0U;
        portMsg.baudRate = 115200U;
        portMsg.mode = 1U;
        const auto portPkt = SightlineSerialBuilder::buildSetPortConfiguration(portMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(portPkt), MessageId::SetPortConfiguration);

        MsgCommandPassThrough passMsg {};
        passMsg.destPort = 2U;
        passMsg.data = { 0xAAU, 0xBBU, 0xCCU };
        const auto passPkt = SightlineSerialBuilder::buildCommandPassThrough(passMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(passPkt), MessageId::CommandPassThrough);
        EXPECT_EQ(SightlineFraming::extractPayload(passPkt).size(), 4U);
    }

    /// @brief Verify version number packet parsing.
    TEST(TestSightlineSystem, ParseVersionNumber)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(3U); // byte 0: swMajor
        payload.push_back(11U); // byte 1: swMinor
        payload.push_back(1U); // byte 2: hwVersion
        payload.push_back(125U); // byte 3: tempF

        // bytes 4..6: hardwareId (24-bit LE)
        payload.push_back(0x78U);
        payload.push_back(0x56U);
        payload.push_back(0x34U);

        // bytes 7..10: appBits (32-bit LE)
        payload.push_back(0x78U);
        payload.push_back(0x56U);
        payload.push_back(0x34U);
        payload.push_back(0x12U);

        payload.push_back(18U); // byte 11: boardType (18 = SLA-3000)
        payload.push_back(6U); // byte 12: softwareRelease
        payload.push_back(0x04U); // bytes 13..14: otherVersion (boardRevision=4)
        payload.push_back(0x04U);

        // bytes 15..18: srcRevision
        payload.push_back(0x01U);
        payload.push_back(0x02U);
        payload.push_back(0x03U);
        payload.push_back(0x04U);

        // bytes 19..22: buildDate
        payload.push_back(0x10U);
        payload.push_back(0x20U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        // bytes 23..26: buildTime
        payload.push_back(0x00U);
        payload.push_back(0x12U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        // bytes 27..28: softwareBuild (42)
        payload.push_back(42U);
        payload.push_back(0U);

        // bytes 29..30: v4AppBits
        payload.push_back(0U);
        payload.push_back(0U);

        // bytes 31..32: degreesC (52 C)
        payload.push_back(52U);
        payload.push_back(0U);

        // bytes 33..36: adapters
        payload.push_back(0x01U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        const auto pkt = SightlineFraming::buildPacket(MessageId::VersionNumber, payload);

        MsgVersionNumber out {};
        ASSERT_TRUE(SightlineGeneralParser::parseVersionNumber(pkt, out));
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

        // Verify facade equivalence
        MsgVersionNumber facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseVersionNumber(pkt, facadeOut));
        EXPECT_EQ(facadeOut.softwareMajor, 3U);
    }

    /// @brief Verify system status message parsing (0x87).
    TEST(TestSightlineSystem, ParseSystemStatus)
    {
        std::vector<std::uint8_t> payload {};
        // bytes 0..7: errorFlags (u64 LE = 0)
        for (std::size_t i { 0U }; i < 8U; ++i) {
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

        const auto pkt = SightlineFraming::buildPacket(MessageId::SystemStatusMessage, payload);

        MsgSystemStatusMessage out {};
        ASSERT_TRUE(SightlineGeneralParser::parseSystemStatus(pkt, out));
        EXPECT_EQ(out.cpuLoadPercent, 45U);
        EXPECT_EQ(out.load0, 45U);
        EXPECT_EQ(out.load1, 30U);
        EXPECT_EQ(out.load2, 20U);
        EXPECT_EQ(out.load3, 10U);
        EXPECT_EQ(out.temperatureF, 125);
        EXPECT_EQ(out.coreTempC, 52);
        EXPECT_EQ(out.missedFrames1, 1U);
        EXPECT_EQ(out.errorFlags, 0ULL);

        // Verify facade equivalence
        MsgSystemStatusMessage facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseSystemStatus(pkt, facadeOut));
        EXPECT_EQ(facadeOut.cpuLoadPercent, 45U);
    }

    /// @brief Verify system status mode builder (0x80).
    TEST(TestSightlineSystem, BuildSystemStatusMode)
    {
        MsgSystemStatusMode mode {};
        mode.systemStatusBits = 0x0001U;
        mode.systemDebugBits = 0x0400U; // Timing measurement bit 10

        const auto pkt = SightlineGeneralBuilder::buildSystemStatusMode(mode);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SystemStatusMode);
        const auto payload = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(payload.size(), 6U);
        EXPECT_EQ(payload[0], 0x01U);
        EXPECT_EQ(payload[1], 0x00U);
        EXPECT_EQ(payload[2], 0x00U);
        EXPECT_EQ(payload[3], 0x04U);
        EXPECT_EQ(payload[4], 0x00U);
        EXPECT_EQ(payload[5], 0x00U);
    }

    /// @brief Verify user warning message parsing (0x86).
    TEST(TestSightlineSystem, ParseUserWarning)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0x05U); // Warning code low
        payload.push_back(0x00U); // Warning code high
        const std::string msgText = "Camera 0 Track Dropped";
        payload.insert(payload.end(), msgText.begin(), msgText.end());

        const auto pkt = SightlineFraming::buildPacket(MessageId::UserWarningMessage, payload);

        MsgUserWarningMessage out {};
        ASSERT_TRUE(SightlineGeneralParser::parseUserWarning(pkt, out));
        EXPECT_EQ(out.warningCode, 5U);
        EXPECT_EQ(out.message, "Camera 0 Track Dropped");
    }

    /// @brief Verify current configuration parsing (0x8E).
    TEST(TestSightlineSystem, ParseCurrentConfiguration)
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
        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentConfiguration, payload);

        MsgCurrentConfiguration out {};
        ASSERT_TRUE(SightlineGeneralParser::parseCurrentConfiguration(pkt, out));
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

    /// @brief Verify system status mode query and deserializer, plus config query.
    TEST(TestSightlineSystem, SystemStatusModeAndConfigRoundTrip)
    {
        const auto getModePkt = SightlineGeneralBuilder::buildGetSystemStatusMode();
        EXPECT_EQ(SightlineFraming::identifyMessage(getModePkt), MessageId::GetParameters);
        EXPECT_EQ(getModePkt, SightlineProtocolBuilder::buildGetSystemStatusMode());

        const auto getConfigPkt = SightlineGeneralBuilder::buildGetCurrentConfig();
        EXPECT_EQ(SightlineFraming::identifyMessage(getConfigPkt), MessageId::GetParameters);
        EXPECT_EQ(getConfigPkt, SightlineProtocolBuilder::buildGetCurrentConfig());

        MsgSystemStatusMode modeIn {};
        modeIn.systemStatusBits = 0x0001U;
        modeIn.systemDebugBits = 0x0100U;

        const auto setPkt = SightlineGeneralBuilder::buildSystemStatusMode(modeIn);
        MsgSystemStatusMode modeOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseSystemStatusMode(setPkt, modeOut));
        EXPECT_EQ(modeOut.systemStatusBits, 0x0001U);
        EXPECT_EQ(modeOut.systemDebugBits, 0x0100U);

        // Facade equivalence
        MsgSystemStatusMode facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseSystemStatusMode(setPkt, facadeOut));
        EXPECT_EQ(facadeOut.systemStatusBits, 0x0001U);
    }

    /// @brief Verify Phase 4 Low-Level Bus & Telemetry Tags (0x92, 0x93, 0x96, 0x97, 0x98, 0x88, 0x89, 0x8A).
    TEST(TestSightlineSystem, BuildAndParsePhase4SystemValues)
    {
        // 1. System Value (0x92 / 0x93)
        MsgSystemValue valIn {};
        valIn.systemValueId = 0x04U;
        valIn.value = 0x12345678U;

        const auto valPkt = SightlineGeneralBuilder::buildSetSystemValue(valIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(valPkt), MessageId::SetSystemValue);
        EXPECT_EQ(valPkt, SightlineProtocolBuilder::buildSetSystemValue(valIn));

        const auto getValPkt = SightlineGeneralBuilder::buildGetSystemValue(0x04U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getValPkt), MessageId::GetParameters);
        EXPECT_EQ(getValPkt, SightlineProtocolBuilder::buildGetSystemValue(0x04U));

        MsgSystemValue valOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseSystemValue(valPkt, valOut));
        EXPECT_EQ(valOut.systemValueId, 0x04U);
        EXPECT_EQ(valOut.value, 0x12345678U);

        // Check CurrentSystemValue (0x93) parsing
        const auto curValPkt
            = SightlineFraming::buildPacket(MessageId::CurrentSystemValue, SightlineFraming::extractPayload(valPkt));
        MsgSystemValue curValOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseSystemValue(curValPkt, curValOut));
        EXPECT_EQ(curValOut.systemValueId, 0x04U);
        EXPECT_EQ(curValOut.value, 0x12345678U);

        // 2. Tag Data (0x96)
        MsgTagData tagIn {};
        tagIn.tagId = 0x0102U;
        tagIn.data = { 0xAAU, 0xBBU, 0xCCU, 0xDDU };

        const auto tagPkt = SightlineGeneralBuilder::buildTagData(tagIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(tagPkt), MessageId::TagData);
        EXPECT_EQ(tagPkt, SightlineProtocolBuilder::buildTagData(tagIn));

        MsgTagData tagOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseTagData(tagPkt, tagOut));
        EXPECT_EQ(tagOut.tagId, 0x0102U);
        ASSERT_EQ(tagOut.data.size(), 4U);
        EXPECT_EQ(tagOut.data[0], 0xAAU);
        EXPECT_EQ(tagOut.data[3], 0xDDU);

        MsgTagData facadeTagOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTagData(tagPkt, facadeTagOut));
        EXPECT_EQ(facadeTagOut.tagId, 0x0102U);

        // 3. Tag Data Rate (0x97)
        MsgTagDataRate rateIn {};
        rateIn.tagId = 0x0102U;
        rateIn.rate = 5U;

        const auto ratePkt = SightlineGeneralBuilder::buildSetTagDataRate(rateIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(ratePkt), MessageId::TagDataRate);
        EXPECT_EQ(ratePkt, SightlineProtocolBuilder::buildSetTagDataRate(rateIn));

        const auto getRatePkt = SightlineGeneralBuilder::buildGetTagDataRate(0x0102U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getRatePkt), MessageId::GetParameters);
        EXPECT_EQ(getRatePkt, SightlineProtocolBuilder::buildGetTagDataRate(0x0102U));

        MsgTagDataRate rateOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseTagDataRate(ratePkt, rateOut));
        EXPECT_EQ(rateOut.tagId, 0x0102U);
        EXPECT_EQ(rateOut.rate, 5U);

        MsgTagDataRate facadeRateOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTagDataRate(ratePkt, facadeRateOut));
        EXPECT_EQ(facadeRateOut.rate, 5U);

        // 4. Tag Source Selector (0x98)
        MsgTagSourceSelector srcIn {};
        srcIn.tagId = 0x0204U;
        srcIn.source = 2U;

        const auto srcPkt = SightlineGeneralBuilder::buildSetTagSourceSelector(srcIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(srcPkt), MessageId::TagSourceSelector);
        EXPECT_EQ(srcPkt, SightlineProtocolBuilder::buildSetTagSourceSelector(srcIn));

        const auto getSrcPkt = SightlineGeneralBuilder::buildGetTagSourceSelector(0x0204U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getSrcPkt), MessageId::GetParameters);
        EXPECT_EQ(getSrcPkt, SightlineProtocolBuilder::buildGetTagSourceSelector(0x0204U));

        MsgTagSourceSelector srcOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseTagSourceSelector(srcPkt, srcOut));
        EXPECT_EQ(srcOut.tagId, 0x0204U);
        EXPECT_EQ(srcOut.source, 2U);

        MsgTagSourceSelector facadeSrcOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseTagSourceSelector(srcPkt, facadeSrcOut));
        EXPECT_EQ(facadeSrcOut.source, 2U);

        // 5. Detailed Timing (0x88)
        MsgDetailedTiming timingIn {};
        timingIn.frameNumber = 123456U;
        timingIn.captureLatencyUs = 8300U;
        timingIn.processLatencyUs = 12500U;
        timingIn.transmitLatencyUs = 4200U;

        const auto timingPkt = SightlineGeneralBuilder::buildDetailedTiming(timingIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(timingPkt), MessageId::DetailedTimingMessage);
        EXPECT_EQ(timingPkt, SightlineProtocolBuilder::buildDetailedTiming(timingIn));

        MsgDetailedTiming timingOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseDetailedTiming(timingPkt, timingOut));
        EXPECT_EQ(timingOut.frameNumber, 123456U);
        EXPECT_EQ(timingOut.captureLatencyUs, 8300U);
        EXPECT_EQ(timingOut.processLatencyUs, 12500U);
        EXPECT_EQ(timingOut.transmitLatencyUs, 4200U);

        MsgDetailedTiming facadeTimingOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDetailedTiming(timingPkt, facadeTimingOut));
        EXPECT_EQ(facadeTimingOut.frameNumber, 123456U);

        // 6. Appended Metadata (0x89)
        MsgAppendedMetadata metaIn {};
        metaIn.cameraIndex = 1U;
        metaIn.enable = 1U;

        const auto metaPkt = SightlineGeneralBuilder::buildSetAppendedMetadata(metaIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(metaPkt), MessageId::AppendedMetadata);
        EXPECT_EQ(metaPkt, SightlineProtocolBuilder::buildSetAppendedMetadata(metaIn));

        const auto getMetaPkt = SightlineGeneralBuilder::buildGetAppendedMetadata(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getMetaPkt), MessageId::GetParameters);
        EXPECT_EQ(getMetaPkt, SightlineProtocolBuilder::buildGetAppendedMetadata(1U));

        MsgAppendedMetadata metaOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseAppendedMetadata(metaPkt, metaOut));
        EXPECT_EQ(metaOut.cameraIndex, 1U);
        EXPECT_EQ(metaOut.enable, 1U);

        MsgAppendedMetadata facadeMetaOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseAppendedMetadata(metaPkt, facadeMetaOut));
        EXPECT_EQ(facadeMetaOut.enable, 1U);

        // 7. Frame Index (0x8A)
        MsgFrameIndex frameIn {};
        frameIn.cameraIndex = 2U;
        frameIn.frameIndex = 987654U;
        frameIn.timestampUs = 1696160000000000ULL;

        const auto framePkt = SightlineGeneralBuilder::buildFrameIndex(frameIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(framePkt), MessageId::FrameIndex);
        EXPECT_EQ(framePkt, SightlineProtocolBuilder::buildFrameIndex(frameIn));

        MsgFrameIndex frameOut {};
        ASSERT_TRUE(SightlineGeneralParser::parseFrameIndex(framePkt, frameOut));
        EXPECT_EQ(frameOut.cameraIndex, 2U);
        EXPECT_EQ(frameOut.frameIndex, 987654U);
        EXPECT_EQ(frameOut.timestampUs, 1696160000000000ULL);

        MsgFrameIndex facadeFrameOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseFrameIndex(framePkt, facadeFrameOut));
        EXPECT_EQ(facadeFrameOut.timestampUs, 1696160000000000ULL);
    }

} // namespace
} // namespace Sightline
