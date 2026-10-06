/// @file TestSightlineNuc.cpp
/// @brief Spec-conformance (golden vector) tests for Sightline NUC / DPR messages.
/// @details Every expected byte sequence in this file is derived directly from the
///          Sightline IDD v3.11 byte-offset tables (SLANucParameters_t 0x35,
///          SLAReadWriteNuc_t 0x36, SLADeadPixelStats_t 0xA1, SLADeadPixel_t 0xA8,
///          SLANoise3D_t 0xAF, SLAGetParameters_t 0x28) and from the worked examples in
///          docs/protocols/Sightline/EAN-NUC-and-DPR.pdf (Appendix A1 and A4).
///          IDD byte offsets include the 4-byte frame prefix; payload index = IDD offset - 4.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "modules/SightlineEnhancementBuilder.h"
#include "modules/SightlineEnhancementParser.h"
#include "modules/SightlineNucBuilder.h"
#include "modules/SightlineNucParser.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {
namespace {

    /// @brief Returns the payload bytes of a framed packet as an owning vector.
    /// @param[in] packet Framed packet.
    /// @return Payload bytes between Message ID and CRC.
    std::vector<std::uint8_t> payloadOf(const std::vector<std::uint8_t>& packet)
    {
        const ByteView view { SightlineFraming::extractPayload(packet) };
        return std::vector<std::uint8_t> { view.begin(), view.end() };
    }

    /// @brief Returns a NucParameters message with every tail field zeroed.
    /// @details Matches the all-zero padding used in the EAN Appendix A4 examples.
    /// @return Zeroed message.
    MsgNucParameters zeroNucParams()
    {
        MsgNucParameters msg {};
        msg.numReplace = 0U;
        msg.deadFilter = DeadFilter::None;
        msg.deadFilterThresh = 0;
        msg.destripeSections = 0U;
        return msg;
    }

    // =========================================================================
    // NucParameters (0x35)
    // =========================================================================

    /// @brief EAN A4 step 5: add 5 frames for a 1-point NUC on cam0.
    TEST(TestSightlineNuc, NucParamsEanA4AddFrames)
    {
        MsgNucParameters msg { zeroNucParams() };
        msg.cameraIndex = 0U;
        msg.nucShow = NucShow::NucAndDpr;
        msg.nucRunMode = NucRunMode::AddFrames;
        msg.numFrames = 5U;

        const auto pkt { SightlineNucBuilder::buildNucParameters(msg) };
        ASSERT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::NucParameters);

        std::vector<std::uint8_t> expected { 0x00U, 0x01U, 0x01U, 0x05U };
        expected.resize(35U, 0x00U); // IDD bytes 8..38 zero
        expected.push_back(0x00U);   // nucName.len = 0
        EXPECT_EQ(payloadOf(pkt), expected);
    }

    /// @brief EAN A4 step 6: calculate the 1-point correction on cam2.
    TEST(TestSightlineNuc, NucParamsEanA4Calc1Point)
    {
        MsgNucParameters msg { zeroNucParams() };
        msg.cameraIndex = 2U;
        msg.nucShow = NucShow::NucAndDpr;
        msg.nucRunMode = NucRunMode::Calc1Point;

        const auto payload { payloadOf(SightlineNucBuilder::buildNucParameters(msg)) };
        ASSERT_EQ(payload.size(), 36U);
        EXPECT_EQ(payload[0U], 0x02U);
        EXPECT_EQ(payload[1U], 0x01U);
        EXPECT_EQ(payload[2U], 0x07U);
        EXPECT_EQ(payload[3U], 0x00U);
    }

    /// @brief Every field lands on its IDD byte offset with the IDD width/endianness.
    TEST(TestSightlineNuc, NucParamsAllFieldOffsets)
    {
        MsgNucParameters msg {};
        msg.cameraIndex = 1U;
        msg.nucShow = NucShow::NucAndDpr;
        msg.nucRunMode = NucRunMode::CalcDead;
        msg.numFrames = 0U;
        msg.minDeadGain = 76U;
        msg.maxDeadGain = 124U;
        msg.minDeadVal = 0x1234U;
        msg.maxDeadVal = 0xFEDCU;
        msg.minDeadOff = -520;
        msg.maxDeadOff = 520;
        msg.maxStdDevDead = 65535U;
        msg.maxNumDead = 1000;
        msg.deadReplace = DeadReplace::Median;
        msg.numReplace = 5U;
        msg.deadFilter = DeadFilter::NearFar;
        msg.deadFilterThresh = 64;
        msg.destripeAmount = 200U;
        msg.destripeSections = 4U;
        msg.nucName = "zoom1";

        const std::vector<std::uint8_t> expected {
            0x01U, 0x01U, 0x09U, 0x00U,             // cam, nucShow, nucRunMode, numFrames
            0x4CU, 0x00U, 0x7CU, 0x00U,             // minDeadGain, maxDeadGain
            0x34U, 0x12U, 0xDCU, 0xFEU,             // minDeadVal, maxDeadVal
            0xF8U, 0xFDU, 0xFFU, 0xFFU,             // minDeadOff = -520
            0x08U, 0x02U, 0x00U, 0x00U,             // maxDeadOff = 520
            0xFFU, 0xFFU, 0x00U, 0x00U,             // maxStdDevDead
            0xE8U, 0x03U, 0x00U, 0x00U,             // maxNumDead = 1000
            0x02U, 0x05U, 0x02U,                    // deadReplace, numReplace, deadFilter
            0x40U, 0x00U,                           // deadFilterThresh = 64
            0xC8U, 0x04U,                           // destripeAmount, destripeSections
            0x05U, 'z', 'o', 'o', 'm', '1'          // nucName (SVPLenString_t)
        };
        EXPECT_EQ(payloadOf(SightlineNucBuilder::buildNucParameters(msg)), expected);
    }

    /// @brief A default-constructed message must not alter the board's video state.
    /// @details Regression: the previous layout serialised defaults as nucShow=0
    ///          (uncorrected video) and nucRunMode=1 (Add Frames).
    TEST(TestSightlineNuc, NucParamsDefaultIsStatePreserving)
    {
        const auto payload { payloadOf(SightlineNucBuilder::buildNucParameters(MsgNucParameters {})) };
        ASSERT_GE(payload.size(), 35U);
        EXPECT_EQ(payload[1U], 0x01U); // NucShow::NucAndDpr
        EXPECT_EQ(payload[2U], 0x00U); // NucRunMode::None
        EXPECT_EQ(payload[29U], 5U);   // numReplace default
        EXPECT_EQ(payload[30U], 0xFFU); // DeadFilter::Ignore keeps current filter
        EXPECT_EQ(payload[31U], 64U);  // deadFilterThresh default
        EXPECT_EQ(payload[34U], 1U);   // destripeSections default
    }

    /// @brief nucName longer than a u8 length prefix allows is rejected.
    TEST(TestSightlineNuc, NucParamsRejectsOverlongName)
    {
        MsgNucParameters msg {};
        msg.nucName = std::string(256U, 'a');
        EXPECT_TRUE(SightlineNucBuilder::buildNucParameters(msg).empty());
    }

    /// @brief Full round trip including the length-prefixed name.
    TEST(TestSightlineNuc, NucParamsRoundTrip)
    {
        MsgNucParameters msg {};
        msg.cameraIndex = 3U;
        msg.nucShow = NucShow::DeadImage;
        msg.nucRunMode = NucRunMode::Noise3DStats;
        msg.numFrames = 30U;
        msg.minDeadOff = -999999;
        msg.maxDeadOff = 999999;
        msg.deadReplace = DeadReplace::Average;
        msg.deadFilter = DeadFilter::MinMax;
        msg.deadFilterThresh = -5;
        msg.nucName = "zoom10";

        MsgNucParameters out {};
        ASSERT_TRUE(SightlineNucParser::parseNucParameters(SightlineNucBuilder::buildNucParameters(msg), out));
        EXPECT_EQ(out.cameraIndex, 3U);
        EXPECT_EQ(out.nucShow, NucShow::DeadImage);
        EXPECT_EQ(out.nucRunMode, NucRunMode::Noise3DStats);
        EXPECT_EQ(out.numFrames, 30U);
        EXPECT_EQ(out.minDeadOff, -999999);
        EXPECT_EQ(out.maxDeadOff, 999999);
        EXPECT_EQ(out.deadReplace, DeadReplace::Average);
        EXPECT_EQ(out.deadFilter, DeadFilter::MinMax);
        EXPECT_EQ(out.deadFilterThresh, -5);
        EXPECT_EQ(out.nucName, "zoom10");
    }

    /// @brief Legacy firmware replies stop after maxNumDead (28 bytes, as in EAN A4).
    TEST(TestSightlineNuc, NucParamsParsesLegacyTruncated)
    {
        std::vector<std::uint8_t> payload { 0x01U, 0x02U, 0x00U, 0x00U };
        payload.resize(28U, 0x00U);
        payload[4U] = 76U; // minDeadGain lsb

        MsgNucParameters out {};
        ASSERT_TRUE(SightlineNucParser::parseNucParameters(
            SightlineFraming::buildPacket(MessageId::NucParameters, payload), out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.nucShow, NucShow::NucOnly);
        EXPECT_EQ(out.minDeadGain, 76U);
        EXPECT_EQ(out.numReplace, 5U);              // untouched default
        EXPECT_EQ(out.deadFilter, DeadFilter::Ignore); // untouched default
        EXPECT_TRUE(out.nucName.empty());
    }

    /// @brief Malformed replies are rejected.
    TEST(TestSightlineNuc, NucParamsRejectsMalformed)
    {
        MsgNucParameters out {};
        const std::vector<std::uint8_t> tooShort { 0x00U, 0x01U, 0x00U };
        EXPECT_FALSE(SightlineNucParser::parseNucParameters(
            SightlineFraming::buildPacket(MessageId::NucParameters, tooShort), out));

        const std::vector<std::uint8_t> badShow { 0x00U, 0x07U, 0x00U, 0x00U };
        EXPECT_FALSE(SightlineNucParser::parseNucParameters(
            SightlineFraming::buildPacket(MessageId::NucParameters, badShow), out));

        const std::vector<std::uint8_t> badRun { 0x00U, 0x01U, 0x0EU, 0x00U };
        EXPECT_FALSE(SightlineNucParser::parseNucParameters(
            SightlineFraming::buildPacket(MessageId::NucParameters, badRun), out));

        std::vector<std::uint8_t> nameOverrun { 0x00U, 0x01U, 0x00U, 0x00U };
        nameOverrun.resize(35U, 0x00U);
        nameOverrun.push_back(10U); // claims 10 chars, provides 2
        nameOverrun.push_back('a');
        nameOverrun.push_back('b');
        EXPECT_FALSE(SightlineNucParser::parseNucParameters(
            SightlineFraming::buildPacket(MessageId::NucParameters, nameOverrun), out));
    }

    // =========================================================================
    // ReadWriteNuc (0x36)
    // =========================================================================

    /// @brief Save NUC table "zoom1" (mode 0x01).
    TEST(TestSightlineNuc, ReadWriteNucSaveNuc)
    {
        MsgReadWriteNuc msg {};
        msg.fileOp = NucFileOp::SaveNuc;
        msg.fileName = "zoom1";

        const std::vector<std::uint8_t> expected {
            0x00U, 0x00U, 0x01U, 0x05U, 'z', 'o', 'o', 'm', '1', 0x00U, 0x00U
        };
        const auto pkt { SightlineNucBuilder::buildReadWriteNuc(msg) };
        ASSERT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::ReadWriteNuc);
        EXPECT_EQ(payloadOf(pkt), expected);
    }

    /// @brief Set dead table "zoom1" as startup default (high nibble 2 -> 0x20).
    TEST(TestSightlineNuc, ReadWriteNucSetDefaultDead)
    {
        MsgReadWriteNuc msg {};
        msg.cameraIndex = 1U;
        msg.defaultOp = NucDefaultOp::SetDead;
        msg.fileName = "zoom1";

        const auto payload { payloadOf(SightlineNucBuilder::buildReadWriteNuc(msg)) };
        ASSERT_GE(payload.size(), 3U);
        EXPECT_EQ(payload[0U], 0x01U);
        EXPECT_EQ(payload[1U], 0x00U);
        EXPECT_EQ(payload[2U], 0x20U);
    }

    /// @brief Clearing the default NUC table (0x30) is the only blank-name case allowed.
    TEST(TestSightlineNuc, ReadWriteNucClearDefaultNuc)
    {
        MsgReadWriteNuc msg {};
        msg.defaultOp = NucDefaultOp::ClearNuc;

        const std::vector<std::uint8_t> expected { 0x00U, 0x00U, 0x30U, 0x00U, 0x00U, 0x00U };
        EXPECT_EQ(payloadOf(SightlineNucBuilder::buildReadWriteNuc(msg)), expected);
    }

    /// @brief A blank filename for a file operation is rejected (IDD note on 0x36).
    TEST(TestSightlineNuc, ReadWriteNucRejectsBlankName)
    {
        MsgReadWriteNuc msg {};
        msg.fileOp = NucFileOp::SaveDead;
        EXPECT_TRUE(SightlineNucBuilder::buildReadWriteNuc(msg).empty());

        MsgReadWriteNuc none {};
        EXPECT_TRUE(SightlineNucBuilder::buildReadWriteNuc(none).empty());
    }

    /// @brief Interpolated load between two tables (EAN 4.5).
    TEST(TestSightlineNuc, ReadWriteNucInterpolate)
    {
        MsgReadWriteNuc msg {};
        msg.cameraIndex = 1U;
        msg.fileOp = NucFileOp::LoadInterpolated;
        msg.fileName = "zoom1";
        msg.secondaryFileName = "zoom4";
        msg.interpolationRatio = 128U;

        const std::vector<std::uint8_t> expected {
            0x01U, 0x00U, 0x05U,
            0x05U, 'z', 'o', 'o', 'm', '1',
            0x05U, 'z', 'o', 'o', 'm', '4',
            0x80U
        };
        const auto pkt { SightlineNucBuilder::buildReadWriteNuc(msg) };
        EXPECT_EQ(payloadOf(pkt), expected);

        MsgReadWriteNuc out {};
        ASSERT_TRUE(SightlineNucParser::parseReadWriteNuc(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.fileOp, NucFileOp::LoadInterpolated);
        EXPECT_EQ(out.defaultOp, NucDefaultOp::None);
        EXPECT_EQ(out.fileName, "zoom1");
        EXPECT_EQ(out.secondaryFileName, "zoom4");
        EXPECT_EQ(out.interpolationRatio, 128U);
    }

    /// @brief Reply without the optional secondary name / ratio still parses.
    TEST(TestSightlineNuc, ReadWriteNucParsesShortReply)
    {
        const std::vector<std::uint8_t> payload { 0x00U, 0x00U, 0x10U, 0x03U, 'a', 'b', 'c' };
        MsgReadWriteNuc out {};
        ASSERT_TRUE(SightlineNucParser::parseReadWriteNuc(
            SightlineFraming::buildPacket(MessageId::ReadWriteNuc, payload), out));
        EXPECT_EQ(out.fileOp, NucFileOp::None);
        EXPECT_EQ(out.defaultOp, NucDefaultOp::SetNuc);
        EXPECT_EQ(out.fileName, "abc");
        EXPECT_TRUE(out.secondaryFileName.empty());
        EXPECT_EQ(out.interpolationRatio, 0U);
    }

    /// @brief Reserved mode nibbles are rejected.
    TEST(TestSightlineNuc, ReadWriteNucRejectsReservedMode)
    {
        const std::vector<std::uint8_t> payload { 0x00U, 0x00U, 0x08U, 0x00U };
        MsgReadWriteNuc out {};
        EXPECT_FALSE(SightlineNucParser::parseReadWriteNuc(
            SightlineFraming::buildPacket(MessageId::ReadWriteNuc, payload), out));
    }

    /// @brief GetParameters for 0x36: payload0 = table query, payload1 = camera.
    TEST(TestSightlineNuc, GetReadWriteNucQuery)
    {
        const auto pkt { SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery::DefaultDead, 2U) };
        ASSERT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::GetParameters);
        const std::vector<std::uint8_t> expected { 0x36U, 0x04U, 0x02U };
        EXPECT_EQ(payloadOf(pkt), expected);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildGetReadWriteNuc(NucTableQuery::DefaultDead, 2U));
    }

    // =========================================================================
    // DeadPixel (0xA8)
    // =========================================================================

    /// @brief EAN A1: add the dead pixel at column 1, row 256 on cam0.
    TEST(TestSightlineNuc, DeadPixelEanA1Add)
    {
        const auto pkt { SightlineNucBuilder::buildAddDeadPixel(0U, 1U, 256U, false) };
        ASSERT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::DeadPixel);
        const std::vector<std::uint8_t> expected { 0x00U, 0x00U, 0x01U, 0x00U, 0x00U, 0x01U, 0x00U, 0x00U };
        EXPECT_EQ(payloadOf(pkt), expected);
    }

    /// @brief Batched add sets c = 1 (do not update map yet).
    TEST(TestSightlineNuc, DeadPixelBatchDefersUpdate)
    {
        const auto payload { payloadOf(SightlineNucBuilder::buildAddDeadPixel(1U, 10U, 20U, true)) };
        ASSERT_EQ(payload.size(), 8U);
        EXPECT_EQ(payload[6U], 0x01U);
    }

    /// @brief Remove uses mode 1.
    TEST(TestSightlineNuc, DeadPixelRemove)
    {
        const std::vector<std::uint8_t> expected { 0x00U, 0x01U, 0x0AU, 0x00U, 0x14U, 0x00U, 0x00U, 0x00U };
        EXPECT_EQ(payloadOf(SightlineNucBuilder::buildRemoveDeadPixel(0U, 10U, 20U, false)), expected);
    }

    /// @brief Dynamic detection uses mode 2 with kernel size and max difference.
    TEST(TestSightlineNuc, DeadPixelDynamicDetect)
    {
        const std::vector<std::uint8_t> expected { 0x02U, 0x02U, 0x0FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
        EXPECT_EQ(payloadOf(SightlineNucBuilder::buildDynamicDead(2U, 15U, 0U)), expected);
    }

    /// @brief A default-constructed message must never encode Remove.
    TEST(TestSightlineNuc, DeadPixelDefaultIsNotRemove)
    {
        const auto payload { payloadOf(SightlineNucBuilder::buildDeadPixel(MsgDeadPixel {})) };
        ASSERT_EQ(payload.size(), 8U);
        EXPECT_NE(payload[1U], 0x01U);
    }

    /// @brief Round trip through the parser.
    TEST(TestSightlineNuc, DeadPixelRoundTrip)
    {
        MsgDeadPixel out {};
        ASSERT_TRUE(SightlineNucParser::parseDeadPixel(
            SightlineNucBuilder::buildRemoveDeadPixel(3U, 640U, 512U, true), out));
        EXPECT_EQ(out.cameraIndex, 3U);
        EXPECT_EQ(out.mode, DeadPixelMode::Remove);
        EXPECT_EQ(out.a, 640U);
        EXPECT_EQ(out.b, 512U);
        EXPECT_EQ(out.c, 1U);

        const std::vector<std::uint8_t> badMode { 0x00U, 0x03U, 0U, 0U, 0U, 0U, 0U, 0U };
        EXPECT_FALSE(SightlineNucParser::parseDeadPixel(
            SightlineFraming::buildPacket(MessageId::DeadPixel, badMode), out));
    }

    // =========================================================================
    // DeadPixelStats (0xA1)
    // =========================================================================

    /// @brief Parse the 33-byte IDD layout (u8 + s32 + 7 x u32).
    TEST(TestSightlineNuc, DeadPixelStatsLayout)
    {
        std::vector<std::uint8_t> payload { 0x01U };
        SightlineFraming::appendS32Le(payload, 1234);
        SightlineFraming::appendU32Le(payload, 10U);
        SightlineFraming::appendU32Le(payload, 20U);
        SightlineFraming::appendU32Le(payload, 30U);
        SightlineFraming::appendU32Le(payload, 40U);
        SightlineFraming::appendU32Le(payload, 50U);
        SightlineFraming::appendU32Le(payload, 60U);
        SightlineFraming::appendU32Le(payload, 70000U);
        ASSERT_EQ(payload.size(), 33U);

        MsgDeadPixelStats out {};
        ASSERT_TRUE(SightlineNucParser::parseDeadPixelStats(
            SightlineFraming::buildPacket(MessageId::DeadPixelStats, payload), out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.nDead, 1234);
        EXPECT_EQ(out.nGainLo, 10U);
        EXPECT_EQ(out.nGainHi, 20U);
        EXPECT_EQ(out.nAvgLo, 30U);
        EXPECT_EQ(out.nAvgHi, 40U);
        EXPECT_EQ(out.nOffLo, 50U);
        EXPECT_EQ(out.nOffHi, 60U);
        EXPECT_EQ(out.nDevHi, 70000U);

        payload.pop_back();
        EXPECT_FALSE(SightlineNucParser::parseDeadPixelStats(
            SightlineFraming::buildPacket(MessageId::DeadPixelStats, payload), out));
    }

    // =========================================================================
    // Noise3D (0xAF) - read-only statistics reply
    // =========================================================================

    /// @brief Parse the 17-byte IDD layout (u8 + 8 x u16, values scaled by 256).
    TEST(TestSightlineNuc, Noise3DStatsLayout)
    {
        std::vector<std::uint8_t> payload { 0x02U };
        for (std::uint16_t i { 1U }; i <= 8U; ++i) {
            SightlineFraming::appendU16Le(payload, static_cast<std::uint16_t>(i * 256U));
        }
        ASSERT_EQ(payload.size(), 17U);

        MsgNoise3D out {};
        ASSERT_TRUE(SightlineEnhancementParser::parseNoise3D(
            SightlineFraming::buildPacket(MessageId::Noise3D, payload), out));
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.sigT8, 256U);
        EXPECT_EQ(out.sigV8, 512U);
        EXPECT_EQ(out.sigH8, 768U);
        EXPECT_EQ(out.sigVh8, 1024U);
        EXPECT_EQ(out.sigTv8, 1280U);
        EXPECT_EQ(out.sigTh8, 1536U);
        EXPECT_EQ(out.sigTvh8, 1792U);
        EXPECT_EQ(out.noiseTemporal8, 2048U);
        EXPECT_EQ(kNoise3DScale, 256U);

        payload.pop_back();
        EXPECT_FALSE(SightlineEnhancementParser::parseNoise3D(
            SightlineFraming::buildPacket(MessageId::Noise3D, payload), out));
    }

    /// @brief Noise stats are requested via GetParameters [0xAF, cam].
    TEST(TestSightlineNuc, Noise3DQuery)
    {
        const std::vector<std::uint8_t> expected { 0xAFU, 0x01U };
        EXPECT_EQ(payloadOf(SightlineEnhancementBuilder::buildGetNoise3D(1U)), expected);
    }

    // =========================================================================
    // Framing: SVPLenString_t
    // =========================================================================

    /// @brief Length-prefixed strings carry a u8 length and no terminator.
    TEST(TestSightlineNuc, AppendLenString)
    {
        std::vector<std::uint8_t> buf {};
        ASSERT_TRUE(SightlineFraming::appendLenString(buf, "ab"));
        const std::vector<std::uint8_t> expected { 0x02U, 'a', 'b' };
        EXPECT_EQ(buf, expected);

        std::vector<std::uint8_t> big {};
        EXPECT_FALSE(SightlineFraming::appendLenString(big, std::string(256U, 'x')));
        EXPECT_TRUE(big.empty());
    }

} // namespace
} // namespace Sightline
