/// @file TestSightlineStabilization.cpp
/// @brief Unit tests for Sightline video stabilization, registration, and blending builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineBlendingBuilder.h"
#include "modules/SightlineEnhancementBuilder.h"
#include "modules/SightlineStabilizationBuilder.h"
#include "modules/SightlineStabilizationParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify stabilization and multi-sensor blending serialization.
    TEST(TestSightlineStabilization, BuildStabilizationRegistration)
    {
        MsgSetStabilizationParameters stabMsg {};
        stabMsg.mode = 1U;
        stabMsg.rate = 30U;
        stabMsg.translationLimit = 64U;
        stabMsg.angleLimit = 10U;
        stabMsg.cameraIndex = 0U;
        stabMsg.maxStabOff = 48U;
        stabMsg.edgeY = 0x10U;
        stabMsg.edgeU = 0x80U;
        stabMsg.edgeV = 0x80U;
        const auto stabPkt = SightlineStabilizationBuilder::buildSetStabilization(stabMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(stabPkt), MessageId::SetStabilizationParameters);
        const auto stabPayload = SightlineFraming::extractPayload(stabPkt);
        ASSERT_EQ(stabPayload.size(), 9U);
        EXPECT_EQ(stabPayload[0], 1U);
        EXPECT_EQ(stabPayload[1], 30U);
        EXPECT_EQ(stabPayload[2], 64U);
        EXPECT_EQ(stabPayload[3], 10U);
        EXPECT_EQ(stabPayload[4], 0U);
        EXPECT_EQ(stabPayload[5], 48U);
        EXPECT_EQ(stabPayload[6], 0x10U);
        EXPECT_EQ(stabPayload[7], 0x80U);
        EXPECT_EQ(stabPayload[8], 0x80U);

        // Verify facade equivalence
        const auto facadeStabPkt = SightlineProtocolBuilder::buildSetStabilization(stabMsg);
        EXPECT_EQ(stabPkt, facadeStabPkt);

        MsgResetStabilizationParameters resetStab {};
        resetStab.resetType = 0U;
        resetStab.cameraIndex = 1U;
        const auto resetPkt = SightlineStabilizationBuilder::buildResetStabilization(resetStab);
        EXPECT_EQ(SightlineFraming::identifyMessage(resetPkt), MessageId::ResetStabilizationParameters);
        const auto resetPayload = SightlineFraming::extractPayload(resetPkt);
        ASSERT_EQ(resetPayload.size(), 2U);
        EXPECT_EQ(resetPayload[0], 0U);
        EXPECT_EQ(resetPayload[1], 1U);

        MsgSetStabilizationBias biasMsg {};
        biasMsg.cameraIndex = 0U;
        biasMsg.biasCol = 10;
        biasMsg.biasRow = -15;
        biasMsg.autoBias = 1U;
        biasMsg.updateRate = 50U;
        const auto biasPkt = SightlineStabilizationBuilder::buildSetStabilizationBias(biasMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(biasPkt), MessageId::StabilizationBias);
        const auto biasPayload = SightlineFraming::extractPayload(biasPkt);
        ASSERT_EQ(biasPayload.size(), 7U);
        EXPECT_EQ(biasPayload[0], 0U);
        EXPECT_EQ(SightlineFraming::readS16Le(biasPayload.data() + 1U), 10);
        EXPECT_EQ(SightlineFraming::readS16Le(biasPayload.data() + 3U), -15);
        EXPECT_EQ(biasPayload[5], 1U);
        EXPECT_EQ(biasPayload[6], 50U);

        MsgSetRegistrationParameters regMsg {};
        regMsg.cameraIndex = 0U;
        regMsg.maxTranslation = 60U;
        regMsg.maxRotation = 5U;
        regMsg.zoomRange = 0U;
        regMsg.left = 100U;
        regMsg.right = 100U;
        regMsg.top = 50U;
        regMsg.bottom = 0U;
        regMsg.updateRate = 100U;
        regMsg.flags = 0U;
        const auto regPkt = SightlineStabilizationBuilder::buildSetRegistration(regMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(regPkt), MessageId::RegistrationParameters);
        const auto regPayload = SightlineFraming::extractPayload(regPkt);
        ASSERT_EQ(regPayload.size(), 15U);
        EXPECT_EQ(regPayload[0], 0U);
        EXPECT_EQ(SightlineFraming::readU16Le(regPayload.data() + 1U), 60U);
        EXPECT_EQ(regPayload[3], 5U);
        EXPECT_EQ(regPayload[4], 0U);
        EXPECT_EQ(SightlineFraming::readU16Le(regPayload.data() + 5U), 100U);
        EXPECT_EQ(SightlineFraming::readU16Le(regPayload.data() + 7U), 100U);
        EXPECT_EQ(SightlineFraming::readU16Le(regPayload.data() + 9U), 50U);
        EXPECT_EQ(SightlineFraming::readU16Le(regPayload.data() + 11U), 0U);
        EXPECT_EQ(regPayload[13], 100U);
        EXPECT_EQ(regPayload[14], 0U);

        MsgSetBlendParameters blendMsg {};
        blendMsg.absOffZoom = 1U;
        blendMsg.vertical = 5;
        blendMsg.horizontal = -10;
        blendMsg.rotation = 0U;
        blendMsg.zoom = 128U;
        blendMsg.mode = BlendMode::FrameBlendWarpEo;
        blendMsg.amt = 75U;
        blendMsg.warpIndex = 0U;
        blendMsg.fixedIndex = 1U;
        const auto blendPkt = SightlineStabilizationBuilder::buildSetBlendParameters(blendMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(blendPkt), MessageId::SetBlendParameters);
        const auto blendPayload = SightlineFraming::extractPayload(blendPkt);
        ASSERT_EQ(blendPayload.size(), 18U);
        EXPECT_EQ(blendPayload[0], 1U);
        EXPECT_EQ(static_cast<std::int8_t>(blendPayload[1]), 5);
        EXPECT_EQ(static_cast<std::int8_t>(blendPayload[2]), -10);
        EXPECT_EQ(blendPayload[5], 1U); // mode
        EXPECT_EQ(blendPayload[6], 75U); // amt
        EXPECT_EQ(blendPayload[11], 0U); // warpIndex
        EXPECT_EQ(blendPayload[12], 1U); // fixedIndex
    }

    /// @brief Verify current stabilization parameters deserialization (0x41).
    TEST(TestSightlineStabilization, ParseStabilizationParams)
    {
        std::vector<std::uint8_t> payload {
            1U, // mode: On
            30U, // rate
            64U, // translationLimit
            10U, // angleLimit
            2U, // cameraIndex
            48U, // maxStabOff
            0x10U, // edgeY
            0x80U, // edgeU
            0x80U // edgeV
        };
        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentStabilizationParameters, payload);

        MsgSetStabilizationParameters out {};
        ASSERT_TRUE(SightlineStabilizationParser::parseStabilizationParams(pkt, out));
        EXPECT_EQ(out.mode, 1U);
        EXPECT_EQ(out.rate, 30U);
        EXPECT_EQ(out.translationLimit, 64U);
        EXPECT_EQ(out.angleLimit, 10U);
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.maxStabOff, 48U);
        EXPECT_EQ(out.edgeY, 0x10U);
        EXPECT_EQ(out.edgeU, 0x80U);
        EXPECT_EQ(out.edgeV, 0x80U);

        // Verify facade equivalence
        MsgSetStabilizationParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseStabilizationParams(pkt, facadeOut));
        EXPECT_EQ(facadeOut.maxStabOff, 48U);
    }

    /// @brief Verify registration parameters deserialization (0x9E).
    TEST(TestSightlineStabilization, ParseRegistrationParams)
    {
        MsgSetRegistrationParameters in {};
        in.cameraIndex = 1U;
        in.maxTranslation = 80U;
        in.maxRotation = 3U;
        in.zoomRange = 1U;
        in.left = 120U;
        in.right = 110U;
        in.top = 40U;
        in.bottom = 20U;
        in.updateRate = 10U;
        in.flags = 2U;

        const auto pkt = SightlineStabilizationBuilder::buildSetRegistration(in);
        MsgSetRegistrationParameters out {};
        ASSERT_TRUE(SightlineStabilizationParser::parseRegistration(pkt, out));
        EXPECT_EQ(out.cameraIndex, 1U);
        EXPECT_EQ(out.maxTranslation, 80U);
        EXPECT_EQ(out.maxRotation, 3U);
        EXPECT_EQ(out.zoomRange, 1U);
        EXPECT_EQ(out.left, 120U);
        EXPECT_EQ(out.right, 110U);
        EXPECT_EQ(out.top, 40U);
        EXPECT_EQ(out.bottom, 20U);
        EXPECT_EQ(out.updateRate, 10U);
        EXPECT_EQ(out.flags, 2U);

        // Facade test
        MsgSetRegistrationParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseRegistration(pkt, facadeOut));
        EXPECT_EQ(facadeOut.left, 120U);
    }

    /// @brief Verify stabilization bias deserialization (0x9F).
    TEST(TestSightlineStabilization, ParseStabilizationBias)
    {
        MsgSetStabilizationBias in {};
        in.cameraIndex = 2U;
        in.biasCol = -42;
        in.biasRow = 88;
        in.autoBias = 1U;
        in.updateRate = 60U;

        const auto pkt = SightlineStabilizationBuilder::buildSetStabilizationBias(in);
        MsgSetStabilizationBias out {};
        ASSERT_TRUE(SightlineStabilizationParser::parseStabilizationBias(pkt, out));
        EXPECT_EQ(out.cameraIndex, 2U);
        EXPECT_EQ(out.biasCol, -42);
        EXPECT_EQ(out.biasRow, 88);
        EXPECT_EQ(out.autoBias, 1U);
        EXPECT_EQ(out.updateRate, 60U);

        // Facade test
        MsgSetStabilizationBias facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseStabilizationBias(pkt, facadeOut));
        EXPECT_EQ(facadeOut.biasRow, 88);
    }

    /// @brief Verify Gimbal Feed-Forward Manual Stabilization Bias Calculation (EAN Section 3.1 & 3.2).
    TEST(TestSightlineStabilization, GimbalBiasCalculation)
    {
        // Example from EAN-Stabilization Section 3.2:
        // panLeftDegPerSec = 2.0, hRes = 1920, hFov = 8.0, fps = 30.0
        // Expected biasCol = 2 * 1920 / (8 * 30) = +16 pixels/frame
        const auto bias = SightlineStabilizationBuilder::calcGimbalBias(2.0, // panLeft
            0.0, // tiltUp
            1920U, // hRes
            1080U, // vRes
            8.0, // hFov
            4.5, // vFov
            30.0, // fps
            0U // cameraIndex
        );

        EXPECT_EQ(bias.biasCol, 16);
        EXPECT_EQ(bias.biasRow, 0);
        EXPECT_EQ(bias.autoBias, 1U);

        // Pan right (negative panLeft) and tilt down (negative tiltUp)
        const auto biasOpposite
            = SightlineStabilizationBuilder::calcGimbalBias(-2.0, -1.0, 1920U, 1080U, 8.0, 4.5, 30.0, 0U);
        EXPECT_EQ(biasOpposite.biasCol, -16);
        // tiltUp = -1.0 -> -1 * 1080 / (4.5 * 30) = -1080 / 135 = -8
        EXPECT_EQ(biasOpposite.biasRow, -8);
    }

    /// @brief Verify EAN Operational Profiles Presets.
    TEST(TestSightlineStabilization, OperationalPresets)
    {
        // 1. Airborne Gimbal (EAN 2.2.1)
        const auto airReg
            = SightlineStabilizationBuilder::makeRegistrationPreset(StabilizationPreset::AirborneGimbal, 0U);
        EXPECT_EQ(airReg.maxTranslation, 0U);
        EXPECT_EQ(airReg.maxRotation, 5U);
        EXPECT_EQ(airReg.updateRate, 100U); // Low drift unchecked

        const auto airStab
            = SightlineStabilizationBuilder::makeStabilizationPreset(StabilizationPreset::AirborneGimbal, 0U);
        EXPECT_EQ(airStab.rate, 50U);
        EXPECT_EQ(airStab.maxStabOff, 0U);

        // 2. Fixed Mount PTZ (EAN 2.2.2)
        const auto ptzReg
            = SightlineStabilizationBuilder::makeRegistrationPreset(StabilizationPreset::FixedMountPtz, 0U);
        EXPECT_EQ(ptzReg.maxTranslation, 50U);
        EXPECT_EQ(ptzReg.maxRotation, 0U);
        EXPECT_EQ(ptzReg.updateRate, 10U); // Low drift checked

        const auto ptzStab
            = SightlineStabilizationBuilder::makeStabilizationPreset(StabilizationPreset::FixedMountPtz, 0U);
        EXPECT_EQ(ptzStab.rate, 20U);
        EXPECT_EQ(ptzStab.maxStabOff, 32U);

        // 3. Moving Vehicle (EAN 2.2.3)
        const auto vehReg
            = SightlineStabilizationBuilder::makeRegistrationPreset(StabilizationPreset::MovingVehicle, 0U);
        EXPECT_EQ(vehReg.maxTranslation, 0U);
        EXPECT_EQ(vehReg.maxRotation, 0U);
        EXPECT_EQ(vehReg.left, 100U);
        EXPECT_EQ(vehReg.right, 100U);
        EXPECT_EQ(vehReg.top, 50U);

        const auto vehStab
            = SightlineStabilizationBuilder::makeStabilizationPreset(StabilizationPreset::MovingVehicle, 0U);
        EXPECT_EQ(vehStab.rate, 50U);
        EXPECT_EQ(vehStab.maxStabOff, 50U);
    }

    /// @brief Verify stabilization bias query builder.
    TEST(TestSightlineStabilization, StabilizationBiasQuery)
    {
        const auto queryPkt = SightlineStabilizationBuilder::buildGetStabilizationBias(2U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetStabilizationBias(2U));

        const auto regQueryPkt = SightlineStabilizationBuilder::buildGetRegistration(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(regQueryPkt), MessageId::GetParameters);
        EXPECT_EQ(regQueryPkt, SightlineProtocolBuilder::buildGetRegistration(1U));
    }

} // namespace
} // namespace Sightline
