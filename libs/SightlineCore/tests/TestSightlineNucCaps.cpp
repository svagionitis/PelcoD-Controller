/// @file TestSightlineNucCaps.cpp
/// @brief Tests for firmware-gated NUC/DPR capabilities, validation, and tail truncation.
/// @details Version gates come from the "New in x.y" notes in
///          docs/protocols/Sightline/EAN-NUC-and-DPR.pdf. Range rules come from the
///          SLANucParameters_t / SLAReadWriteNuc_t / SLADeadPixel_t tables in
///          docs/protocols/Sightline/IDD-SLA-Protocol_3_11_6.pdf.

#include "SightlineFraming.h"
#include "modules/SightlineNucBuilder.h"
#include "modules/SightlineNucCaps.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {
namespace {

    constexpr FwVersion kFw300 { 3U, 0U };
    constexpr FwVersion kFw303 { 3U, 3U };
    constexpr FwVersion kFw304 { 3U, 4U };
    constexpr FwVersion kFw309 { 3U, 9U };
    constexpr FwVersion kFw310 { 3U, 10U };
    constexpr FwVersion kFw311 { 3U, 11U };
    constexpr FwVersion kFwUnknown {};

    /// @brief Returns the payload size of a framed packet.
    /// @param[in] packet Framed packet.
    /// @return Payload byte count.
    std::size_t payloadSize(const std::vector<std::uint8_t>& packet)
    {
        return SightlineFraming::extractPayload(packet).size();
    }

    /// @brief Returns a valid "Add Frames" message.
    /// @return Message accepted by checkNucParams on any firmware.
    MsgNucParameters addFrames()
    {
        MsgNucParameters msg {};
        msg.nucRunMode = NucRunMode::AddFrames;
        msg.numFrames = 30U;
        return msg;
    }

    /// @brief Returns a valid "Calculate Dead Pixels" message (EAN section 3.5 example limits).
    /// @return Message accepted by checkNucParams on any firmware.
    MsgNucParameters calcDead()
    {
        MsgNucParameters msg {};
        msg.nucRunMode = NucRunMode::CalcDead;
        msg.minDeadGain = 76U;
        msg.maxDeadGain = 124U;
        msg.minDeadOff = -520;
        msg.maxDeadOff = 520;
        msg.maxStdDevDead = 1000U;
        msg.maxNumDead = 5000;
        return msg;
    }

    // =========================================================================
    // Firmware version and capability gates
    // =========================================================================

    /// @brief 0x40 swMajor / swMinor map onto FwVersion.
    TEST(TestSightlineNucCaps, FwFromVersionReply)
    {
        MsgVersionNumber ver {};
        ver.softwareMajor = 3U;
        ver.softwareMinor = 11U;
        const FwVersion fw { SightlineNucCaps::fwFromVersion(ver) };
        EXPECT_EQ(fw.major, 3U);
        EXPECT_EQ(fw.minor, 11U);
    }

    /// @brief atLeast compares major first, then minor.
    TEST(TestSightlineNucCaps, AtLeastOrdering)
    {
        EXPECT_TRUE(SightlineNucCaps::atLeast(kFw311, 3U, 9U));
        EXPECT_TRUE(SightlineNucCaps::atLeast(kFw309, 3U, 9U));
        EXPECT_FALSE(SightlineNucCaps::atLeast(kFw304, 3U, 9U));
        EXPECT_TRUE(SightlineNucCaps::atLeast(FwVersion { 4U, 0U }, 3U, 11U));
        EXPECT_FALSE(SightlineNucCaps::atLeast(kFwUnknown, 3U, 0U));
    }

    /// @brief The maximum 0x35 tail follows the EAN feature versions; unknown FW is conservative.
    TEST(TestSightlineNucCaps, MaxTailPerVersion)
    {
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFwUnknown), NucTail::Base);
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFw300), NucTail::Base);
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFw303), NucTail::Dpr);
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFw309), NucTail::Destripe);
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFw310), NucTail::Named);
        EXPECT_EQ(SightlineNucCaps::maxNucTail(kFw311), NucTail::Named);
    }

    /// @brief Run modes 11/12 need 3.3; run mode 13 needs 3.4; 0..10 are always available.
    TEST(TestSightlineNucCaps, RunModeGates)
    {
        EXPECT_TRUE(SightlineNucCaps::supportsRun(kFwUnknown, NucRunMode::CalcReplace));
        EXPECT_FALSE(SightlineNucCaps::supportsRun(kFw300, NucRunMode::AutoDead));
        EXPECT_TRUE(SightlineNucCaps::supportsRun(kFw303, NucRunMode::AutoDead));
        EXPECT_TRUE(SightlineNucCaps::supportsRun(kFw303, NucRunMode::ShutterFlatten));
        EXPECT_FALSE(SightlineNucCaps::supportsRun(kFw303, NucRunMode::Noise3DStats));
        EXPECT_TRUE(SightlineNucCaps::supportsRun(kFw304, NucRunMode::Noise3DStats));
    }

    /// @brief 0x36 op 5 needs 3.9; ops 6/7 need 3.11; 0xA8 needs 3.3.
    TEST(TestSightlineNucCaps, FileOpAndDeadPixelGates)
    {
        EXPECT_TRUE(SightlineNucCaps::supportsFileOp(kFwUnknown, NucFileOp::SaveDead));
        EXPECT_FALSE(SightlineNucCaps::supportsFileOp(kFw304, NucFileOp::LoadInterpolated));
        EXPECT_TRUE(SightlineNucCaps::supportsFileOp(kFw309, NucFileOp::LoadInterpolated));
        EXPECT_FALSE(SightlineNucCaps::supportsFileOp(kFw310, NucFileOp::SaveShutterFlatten));
        EXPECT_TRUE(SightlineNucCaps::supportsFileOp(kFw311, NucFileOp::LoadShutterFlatten));
        EXPECT_FALSE(SightlineNucCaps::supportsDeadPixel(kFw300));
        EXPECT_TRUE(SightlineNucCaps::supportsDeadPixel(kFw303));
    }

    /// @brief Every NucError has a non-empty description.
    TEST(TestSightlineNucCaps, ErrorTextNonEmpty)
    {
        EXPECT_STREQ(SightlineNucCaps::errorText(NucError::Ok), "OK");
        EXPECT_GT(std::string { SightlineNucCaps::errorText(NucError::Unsupported) }.size(), 0U);
        EXPECT_GT(std::string { SightlineNucCaps::errorText(NucError::DprFieldsSet) }.size(), 0U);
    }

    // =========================================================================
    // 0x35 validation
    // =========================================================================

    /// @brief Well-formed run commands are accepted.
    TEST(TestSightlineNucCaps, CheckNucAcceptsValid)
    {
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(addFrames(), kFw311), NucError::Ok);
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(calcDead(), kFw300), NucError::Ok);
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(MsgNucParameters {}, kFwUnknown), NucError::Ok);
    }

    /// @brief IDD: numReplace is 1..8.
    TEST(TestSightlineNucCaps, CheckNucNumReplace)
    {
        MsgNucParameters msg { addFrames() };
        msg.numReplace = 0U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NumReplace);
        msg.numReplace = 9U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NumReplace);
        msg.numReplace = 8U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::Ok);
    }

    /// @brief IDD: deadFilterThresh is 0..255.
    TEST(TestSightlineNucCaps, CheckNucFilterThresh)
    {
        MsgNucParameters msg { addFrames() };
        msg.deadFilterThresh = -1;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::FilterThresh);
        msg.deadFilterThresh = 256;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::FilterThresh);
        msg.deadFilterThresh = 255;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::Ok);
    }

    /// @brief IDD: DPR limits (bytes 8..31) must be 0 unless nucRunMode = 9.
    TEST(TestSightlineNucCaps, CheckNucDprFieldsOnlyForCalcDead)
    {
        MsgNucParameters msg { addFrames() };
        msg.maxNumDead = 1;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::DprFieldsSet);
        msg = addFrames();
        msg.minDeadOff = -1;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::DprFieldsSet);
    }

    /// @brief IDD: gains 0..999 %, offsets -999999..999999, maxStdDevDead <= 65535.
    TEST(TestSightlineNucCaps, CheckNucDprRanges)
    {
        MsgNucParameters msg { calcDead() };
        msg.maxDeadGain = 1000U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::GainRange);

        msg = calcDead();
        msg.maxDeadOff = 1000000;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::OffsetRange);
        msg.maxDeadOff = 999999;
        msg.minDeadOff = -1000000;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::OffsetRange);

        msg = calcDead();
        msg.maxStdDevDead = 65536U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::StdDevRange);
    }

    /// @brief IDD: numFrames must be 0 for the 2-point calculation.
    TEST(TestSightlineNucCaps, CheckNucCalc2PointFrames)
    {
        MsgNucParameters msg {};
        msg.nucRunMode = NucRunMode::Calc2Point;
        msg.numFrames = 1U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NumFramesSet);
        msg.numFrames = 0U;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::Ok);
    }

    /// @brief Run modes not available on the firmware are rejected.
    TEST(TestSightlineNucCaps, CheckNucUnsupportedRun)
    {
        MsgNucParameters msg {};
        msg.nucRunMode = NucRunMode::Noise3DStats;
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw303), NucError::Unsupported);
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw304), NucError::Ok);
    }

    /// @brief EAN 4.4: nucName < 64 chars, no ".nuc", no special characters; needs FW 3.10.
    TEST(TestSightlineNucCaps, CheckNucName)
    {
        MsgNucParameters msg { addFrames() };
        msg.nucName = "zoom_1-wide";
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw310), NucError::Ok);
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw309), NucError::Unsupported);

        msg.nucName = std::string(64U, 'a');
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NameTooLong);
        msg.nucName = std::string(63U, 'a');
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::Ok);

        msg.nucName = "zoom1.nuc";
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NameInvalid);
        msg.nucName = "zoom 1";
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NameInvalid);
        msg.nucName = "../x";
        EXPECT_EQ(SightlineNucBuilder::checkNucParams(msg, kFw311), NucError::NameInvalid);
    }

    // =========================================================================
    // 0x35 tail truncation
    // =========================================================================

    /// @brief Each tail ends on its IDD field boundary (EAN A4 uses the 28-byte Base form).
    TEST(TestSightlineNucCaps, TruncatedLengths)
    {
        const MsgNucParameters msg { addFrames() };
        EXPECT_EQ(payloadSize(SightlineNucBuilder::buildNucParameters(msg, NucTail::Base)), 28U);
        EXPECT_EQ(payloadSize(SightlineNucBuilder::buildNucParameters(msg, NucTail::Dpr)), 33U);
        EXPECT_EQ(payloadSize(SightlineNucBuilder::buildNucParameters(msg, NucTail::Destripe)), 35U);
        EXPECT_EQ(payloadSize(SightlineNucBuilder::buildNucParameters(msg, NucTail::Named)), 36U);
    }

    /// @brief Truncated output is a prefix of the full encoding.
    TEST(TestSightlineNucCaps, TruncatedIsPrefix)
    {
        MsgNucParameters msg { calcDead() };
        msg.deadReplace = DeadReplace::Median;
        msg.destripeAmount = 9U;
        const auto fullPkt { SightlineNucBuilder::buildNucParameters(msg) };
        const ByteView full { SightlineFraming::extractPayload(fullPkt) };
        const auto dprPkt { SightlineNucBuilder::buildNucParameters(msg, NucTail::Dpr) };
        const ByteView dpr { SightlineFraming::extractPayload(dprPkt) };
        ASSERT_EQ(dpr.size(), 33U);
        for (std::size_t i { 0U }; i < dpr.size(); ++i) {
            EXPECT_EQ(dpr[i], full[i]) << "byte " << i;
        }
    }

    /// @brief A name cannot be silently dropped by a shorter tail.
    TEST(TestSightlineNucCaps, TruncatedRejectsDroppedName)
    {
        MsgNucParameters msg { addFrames() };
        msg.nucName = "zoom1";
        EXPECT_TRUE(SightlineNucBuilder::buildNucParameters(msg, NucTail::Destripe).empty());
        EXPECT_EQ(payloadSize(SightlineNucBuilder::buildNucParameters(msg, NucTail::Named)), 41U);
    }

    // =========================================================================
    // 0x36 and 0xA8 validation
    // =========================================================================

    /// @brief File names follow the same rules; blank only for ClearNuc / ClearDead.
    TEST(TestSightlineNucCaps, CheckReadWriteNames)
    {
        MsgReadWriteNuc msg {};
        msg.fileOp = NucFileOp::SaveNuc;
        msg.fileName = "zoom1";
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw300), NucError::Ok);

        msg.fileName = "";
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw300), NucError::FileNameBlank);

        msg.fileName = "zoom1.nuc";
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw300), NucError::NameInvalid);

        MsgReadWriteNuc clr {};
        clr.defaultOp = NucDefaultOp::ClearDead;
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(clr, kFw300), NucError::Ok);
    }

    /// @brief Secondary name / ratio are only meaningful for interpolate / shutter-flatten load.
    TEST(TestSightlineNucCaps, CheckReadWriteSecondary)
    {
        MsgReadWriteNuc msg {};
        msg.fileOp = NucFileOp::LoadInterpolated;
        msg.fileName = "zoom1";
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw311), NucError::FileNameBlank);
        msg.secondaryFileName = "zoom2";
        msg.interpolationRatio = 128U;
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw311), NucError::Ok);
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(msg, kFw304), NucError::Unsupported);

        MsgReadWriteNuc save {};
        save.fileOp = NucFileOp::SaveNuc;
        save.fileName = "zoom1";
        save.interpolationRatio = 1U;
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(save, kFw311), NucError::SecondaryUnused);
        save.interpolationRatio = 0U;
        save.secondaryFileName = "x";
        EXPECT_EQ(SightlineNucBuilder::checkReadWriteNuc(save, kFw311), NucError::SecondaryUnused);
    }

    /// @brief 0xA8 needs FW 3.3; c is 0/1 for add/remove; d is reserved (0).
    TEST(TestSightlineNucCaps, CheckDeadPixel)
    {
        MsgDeadPixel msg {};
        msg.mode = DeadPixelMode::Add;
        msg.a = 1U;
        msg.b = 256U;
        EXPECT_EQ(SightlineNucBuilder::checkDeadPixel(msg, kFw303), NucError::Ok);
        EXPECT_EQ(SightlineNucBuilder::checkDeadPixel(msg, kFw300), NucError::Unsupported);

        msg.c = 2U;
        EXPECT_EQ(SightlineNucBuilder::checkDeadPixel(msg, kFw303), NucError::ReservedSet);
        msg.c = 0U;
        msg.d = 1U;
        EXPECT_EQ(SightlineNucBuilder::checkDeadPixel(msg, kFw303), NucError::ReservedSet);
    }

} // namespace
} // namespace Sightline
