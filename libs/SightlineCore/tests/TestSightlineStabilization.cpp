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
        biasMsg.biasRotation = 5;
        const auto biasPkt = SightlineStabilizationBuilder::buildSetStabilizationBias(biasMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(biasPkt), MessageId::StabilizationBias);

        MsgSetRegistrationParameters regMsg {};
        regMsg.cameraIndex = 0U;
        regMsg.searchRange = 16U;
        regMsg.pyramidLevels = 4U;
        const auto regPkt = SightlineStabilizationBuilder::buildSetRegistration(regMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(regPkt), MessageId::SetRegistrationParameters);

        MsgSetBlendParameters blendMsg {};
        blendMsg.absOffZoom = 1U;
        blendMsg.vertical = 5;
        blendMsg.horizontal = -10;
        blendMsg.rotation = 0U;
        blendMsg.zoom = 128U;
        blendMsg.mode = 1U;
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

        MsgNoise3D noiseMsg {};
        noiseMsg.cameraIndex = 0U;
        noiseMsg.enable = 1U;
        noiseMsg.temporalStrength = 60U;
        noiseMsg.spatialStrength = 40U;
        const auto noisePkt = SightlineStabilizationBuilder::buildSetNoise3D(noiseMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(noisePkt), MessageId::Noise3D);
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

    /// @brief Verify stabilization bias query builder.
    TEST(TestSightlineStabilization, StabilizationBiasQuery)
    {
        const auto queryPkt = SightlineStabilizationBuilder::buildGetStabilizationBias(2U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetStabilizationBias(2U));
    }

} // namespace
} // namespace Sightline
