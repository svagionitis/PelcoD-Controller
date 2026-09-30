/// @file TestSightlineHardwareOverlay.cpp
/// @brief Unit tests for Sightline optics, lens control, GPIO, and on-screen overlay builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineFocusBuilder.h"
#include "modules/SightlineFocusParser.h"
#include "modules/SightlineOverlayBuilder.h"
#include "modules/SightlineOverlayParser.h"
#include "modules/SightlineSerialBuilder.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify lens and hardware control commands serialization.
    TEST(TestSightlineHardwareOverlay, BuildLensHardware)
    {
        MsgLensCommand lensCmd {};
        lensCmd.cameraIndex = 0U;
        lensCmd.commandType = 3U; // Zoom In
        lensCmd.rateOrPosition = 500;
        const auto lensPkt = SightlineFocusBuilder::buildLensCommand(lensCmd);
        EXPECT_EQ(SightlineFraming::identifyMessage(lensPkt), MessageId::LensCommand);

        // Verify facade equivalence
        const auto facadeLensPkt = SightlineProtocolBuilder::buildLensCommand(lensCmd);
        EXPECT_EQ(lensPkt, facadeLensPkt);

        MsgFocusParameters focusMsg {};
        focusMsg.cameraIndex = 0U;
        focusMsg.focusMode = 1U;
        focusMsg.roiX = 400U;
        focusMsg.roiY = 300U;
        focusMsg.roiWidth = 200U;
        focusMsg.roiHeight = 150U;
        const auto focusPkt = SightlineFocusBuilder::buildFocusParameters(focusMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(focusPkt), MessageId::FocusParameters);

        MsgSetLensParameters lensParams {};
        lensParams.cameraIndex = 0U;
        lensParams.minFocalLengthMm = 4.5;
        lensParams.maxFocalLengthMm = 135.0;
        lensParams.horizontalFovWideDeg = 63.0;
        lensParams.horizontalFovTeleDeg = 2.1;
        const auto paramsPkt = SightlineFocusBuilder::buildSetLensParameters(lensParams);
        EXPECT_EQ(SightlineFraming::identifyMessage(paramsPkt), MessageId::SetLensParameters);

        MsgGPIO gpioMsg {};
        gpioMsg.pinMask = 0x0FU;
        gpioMsg.pinValues = 0x05U;
        gpioMsg.directionMask = 0x0FU;
        const auto gpioPkt = SightlineSerialBuilder::buildGPIO(gpioMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(gpioPkt), MessageId::GPIO);
    }

    /// @brief Verify overlay and reticle commands serialization.
    TEST(TestSightlineHardwareOverlay, BuildOverlays)
    {
        MsgSetOverlayMode overlayMode {};
        overlayMode.displayIndex = 0U;
        overlayMode.reticleMode = 2U;
        overlayMode.trackingBoxMode = 1U;
        overlayMode.telemetryTextMode = 1U;
        const auto modePkt = SightlineOverlayBuilder::buildSetOverlayMode(overlayMode);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetOverlayMode);

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
        const auto objPkt = SightlineOverlayBuilder::buildDrawObject(drawObj);
        EXPECT_EQ(SightlineFraming::identifyMessage(objPkt), MessageId::DrawObject);

        MsgDrawOverlay drawBatch {};
        drawBatch.displayIndex = 0U;
        drawBatch.clearDisplay = 1U;
        drawBatch.objects.push_back(drawObj);
        const auto batchPkt = SightlineOverlayBuilder::buildDrawOverlay(drawBatch);
        EXPECT_EQ(SightlineFraming::identifyMessage(batchPkt), MessageId::DrawOverlay);

        // Verify facade equivalence
        const auto facadeBatchPkt = SightlineProtocolBuilder::buildDrawOverlay(drawBatch);
        EXPECT_EQ(batchPkt, facadeBatchPkt);
    }

    /// @brief Verify auto-focus statistics parsing (0x55).
    TEST(TestSightlineHardwareOverlay, ParseFocusStats)
    {
        std::vector<std::uint8_t> payload {};
        payload.push_back(0U); // cameraIndex
        payload.push_back(1U); // focusMode
        SightlineFraming::appendU16Le(payload, 400U); // roiX
        SightlineFraming::appendU16Le(payload, 300U); // roiY
        SightlineFraming::appendU16Le(payload, 200U); // roiWidth
        SightlineFraming::appendU16Le(payload, 150U); // roiHeight

        const auto pkt = SightlineFraming::buildPacket(MessageId::FocusStats, payload);

        MsgFocusParameters out {};
        ASSERT_TRUE(SightlineFocusParser::parseFocusStats(pkt, out));
        EXPECT_EQ(out.cameraIndex, 0U);
        EXPECT_EQ(out.focusMode, 1U);
        EXPECT_EQ(out.roiX, 400U);
        EXPECT_EQ(out.roiY, 300U);
        EXPECT_EQ(out.roiWidth, 200U);
        EXPECT_EQ(out.roiHeight, 150U);

        // Verify facade equivalence
        MsgFocusParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseFocusStats(pkt, facadeOut));
        EXPECT_EQ(facadeOut.roiWidth, 200U);
    }

    /// @brief Verify lens parameters parsing and round-trip query.
    TEST(TestSightlineHardwareOverlay, LensParametersRoundTrip)
    {
        const auto getPkt = SightlineFocusBuilder::buildGetLensParameters(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getPkt), MessageId::GetParameters);

        MsgSetLensParameters setMsg {};
        setMsg.cameraIndex = 1U;
        setMsg.minFocalLengthMm = 5.0;
        setMsg.maxFocalLengthMm = 150.0;
        setMsg.horizontalFovWideDeg = 60.0;
        setMsg.horizontalFovTeleDeg = 2.0;

        const auto setPkt = SightlineFocusBuilder::buildSetLensParameters(setMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(setPkt), MessageId::SetLensParameters);

        MsgSetLensParameters parsed {};
        ASSERT_TRUE(SightlineFocusParser::parseLensParameters(setPkt, parsed));
        EXPECT_EQ(parsed.cameraIndex, 1U);
        EXPECT_DOUBLE_EQ(parsed.minFocalLengthMm, 5.0);
        EXPECT_DOUBLE_EQ(parsed.maxFocalLengthMm, 150.0);
        EXPECT_DOUBLE_EQ(parsed.horizontalFovWideDeg, 60.0);
        EXPECT_DOUBLE_EQ(parsed.horizontalFovTeleDeg, 2.0);

        // Facade equivalence
        MsgSetLensParameters facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseLensParameters(setPkt, facadeOut));
        EXPECT_DOUBLE_EQ(facadeOut.maxFocalLengthMm, 150.0);

        const auto getGpioPkt = SightlineSerialBuilder::buildGetGPIO();
        EXPECT_EQ(SightlineFraming::identifyMessage(getGpioPkt), MessageId::GetParameters);
        EXPECT_EQ(getGpioPkt, SightlineProtocolBuilder::buildGetGPIO());
    }

} // namespace
} // namespace Sightline
