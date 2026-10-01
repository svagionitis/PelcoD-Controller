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

    /// @brief Verify Phase 5 overlay, graphic primitives, logo, ancillary text, and user fonts.
    TEST(TestSightlineHardwareOverlay, BuildAndParsePhase5Overlays)
    {
        // 1. Overlay Mode (0x06 / 0x42)
        MsgSetOverlayMode modeIn {};
        modeIn.displayIndex = 1U;
        modeIn.reticleMode = 2U;
        modeIn.trackingBoxMode = 1U;
        modeIn.telemetryTextMode = 1U;

        const auto modePkt = SightlineOverlayBuilder::buildSetOverlayMode(modeIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetOverlayMode);
        EXPECT_EQ(modePkt, SightlineProtocolBuilder::buildSetOverlayMode(modeIn));

        MsgSetOverlayMode modeOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayMode(modePkt, modeOut));
        EXPECT_EQ(modeOut.displayIndex, 1U);
        EXPECT_EQ(modeOut.reticleMode, 2U);

        // CurrentOverlayMode (0x42) response
        const auto curModePkt
            = SightlineFraming::buildPacket(MessageId::CurrentOverlayMode, SightlineFraming::extractPayload(modePkt));
        MsgSetOverlayMode curModeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseOverlayMode(curModePkt, curModeOut));
        EXPECT_EQ(curModeOut.reticleMode, 2U);

        // 2. Draw Object (0x3B)
        MsgDrawObject objIn {};
        objIn.displayIndex = 0U;
        objIn.objectId = 5U;
        objIn.shapeType = 3U; // Text
        objIn.x = 320U;
        objIn.y = 240U;
        objIn.width = 120U;
        objIn.height = 30U;
        objIn.colorRgba = 0x00FF00FFU;
        objIn.text = "UAV-1";

        const auto objPkt = SightlineOverlayBuilder::buildDrawObject(objIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(objPkt), MessageId::DrawObject);
        EXPECT_EQ(objPkt, SightlineProtocolBuilder::buildDrawObject(objIn));

        MsgDrawObject objOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawObject(objPkt, objOut));
        EXPECT_EQ(objOut.displayIndex, 0U);
        EXPECT_EQ(objOut.objectId, 5U);
        EXPECT_EQ(objOut.shapeType, 3U);
        EXPECT_EQ(objOut.x, 320U);
        EXPECT_EQ(objOut.y, 240U);
        EXPECT_EQ(objOut.colorRgba, 0x00FF00FFU);
        EXPECT_EQ(objOut.text, "UAV-1");

        MsgDrawObject facadeObjOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDrawObject(objPkt, facadeObjOut));
        EXPECT_EQ(facadeObjOut.text, "UAV-1");

        // 3. Batch Draw Overlay (0x9C)
        MsgDrawOverlay batchIn {};
        batchIn.displayIndex = 2U;
        batchIn.clearDisplay = 1U;
        batchIn.objects.push_back(objIn);

        MsgDrawObject lineObj {};
        lineObj.displayIndex = 2U;
        lineObj.objectId = 6U;
        lineObj.shapeType = 0U; // Line
        lineObj.x = 10U;
        lineObj.y = 20U;
        lineObj.width = 100U;
        lineObj.height = 0U;
        lineObj.colorRgba = 0xFFFF00FFU;
        lineObj.text = "";
        batchIn.objects.push_back(lineObj);

        const auto batchPkt = SightlineOverlayBuilder::buildDrawOverlay(batchIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(batchPkt), MessageId::DrawOverlay);
        EXPECT_EQ(batchPkt, SightlineProtocolBuilder::buildDrawOverlay(batchIn));

        MsgDrawOverlay batchOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(batchPkt, batchOut));
        EXPECT_EQ(batchOut.displayIndex, 2U);
        EXPECT_EQ(batchOut.clearDisplay, 1U);
        ASSERT_EQ(batchOut.objects.size(), 2U);
        EXPECT_EQ(batchOut.objects[0].objectId, 5U);
        EXPECT_EQ(batchOut.objects[0].text, "UAV-1");
        EXPECT_EQ(batchOut.objects[1].objectId, 6U);
        EXPECT_EQ(batchOut.objects[1].shapeType, 0U);

        MsgDrawOverlay facadeBatchOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDrawOverlay(batchPkt, facadeBatchOut));
        EXPECT_EQ(facadeBatchOut.objects.size(), 2U);

        // 4. Logo Parameters (0x9B)
        MsgLogoParameters logoIn {};
        logoIn.displayIndex = 1U;
        logoIn.logoIndex = 3U;
        logoIn.enable = 1U;
        logoIn.x = 50U;
        logoIn.y = 80U;
        logoIn.opacity = 200U;
        logoIn.scale = 2U;

        const auto logoPkt = SightlineOverlayBuilder::buildSetLogoParameters(logoIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(logoPkt), MessageId::LogoParameters);
        EXPECT_EQ(logoPkt, SightlineProtocolBuilder::buildSetLogoParameters(logoIn));

        const auto getLogoPkt = SightlineOverlayBuilder::buildGetLogoParameters(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getLogoPkt), MessageId::GetParameters);
        EXPECT_EQ(getLogoPkt, SightlineProtocolBuilder::buildGetLogoParameters(1U));

        MsgLogoParameters logoOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseLogoParameters(logoPkt, logoOut));
        EXPECT_EQ(logoOut.displayIndex, 1U);
        EXPECT_EQ(logoOut.logoIndex, 3U);
        EXPECT_EQ(logoOut.enable, 1U);
        EXPECT_EQ(logoOut.x, 50U);
        EXPECT_EQ(logoOut.y, 80U);
        EXPECT_EQ(logoOut.opacity, 200U);
        EXPECT_EQ(logoOut.scale, 2U);

        MsgLogoParameters facadeLogoOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseLogoParameters(logoPkt, facadeLogoOut));
        EXPECT_EQ(facadeLogoOut.opacity, 200U);

        // 5. Ancillary Text Metadata (0xAC)
        MsgAncillaryTextMetadata textIn {};
        textIn.displayIndex = 0U;
        textIn.lineIndex = 4U;
        textIn.x = 150U;
        textIn.y = 400U;
        textIn.fontId = 1U;
        textIn.colorRgba = 0xFFFFFFFFU;
        textIn.text = "LAT: 37.7749 N LON: 122.4194 W";

        const auto textPkt = SightlineOverlayBuilder::buildAncillaryTextMetadata(textIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(textPkt), MessageId::AncillaryTextMetadata);
        EXPECT_EQ(textPkt, SightlineProtocolBuilder::buildAncillaryTextMetadata(textIn));

        MsgAncillaryTextMetadata textOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseAncillaryTextMetadata(textPkt, textOut));
        EXPECT_EQ(textOut.displayIndex, 0U);
        EXPECT_EQ(textOut.lineIndex, 4U);
        EXPECT_EQ(textOut.x, 150U);
        EXPECT_EQ(textOut.y, 400U);
        EXPECT_EQ(textOut.fontId, 1U);
        EXPECT_EQ(textOut.colorRgba, 0xFFFFFFFFU);
        EXPECT_EQ(textOut.text, "LAT: 37.7749 N LON: 122.4194 W");

        MsgAncillaryTextMetadata facadeTextOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseAncillaryTextMetadata(textPkt, facadeTextOut));
        EXPECT_EQ(facadeTextOut.text, "LAT: 37.7749 N LON: 122.4194 W");

        // 6. User Font (0xAE)
        MsgUserFont fontIn {};
        fontIn.fontId = 2U;
        fontIn.charWidth = 8U;
        fontIn.charHeight = 16U;
        fontIn.firstChar = 0x20U;
        fontIn.numChars = 4U;
        fontIn.glyphData = { 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U };

        const auto fontPkt = SightlineOverlayBuilder::buildUserFont(fontIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(fontPkt), MessageId::UserFont);
        EXPECT_EQ(fontPkt, SightlineProtocolBuilder::buildUserFont(fontIn));

        MsgUserFont fontOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseUserFont(fontPkt, fontOut));
        EXPECT_EQ(fontOut.fontId, 2U);
        EXPECT_EQ(fontOut.charWidth, 8U);
        EXPECT_EQ(fontOut.charHeight, 16U);
        EXPECT_EQ(fontOut.firstChar, 0x20U);
        EXPECT_EQ(fontOut.numChars, 4U);
        ASSERT_EQ(fontOut.glyphData.size(), 8U);
        EXPECT_EQ(fontOut.glyphData[0], 0x01U);
        EXPECT_EQ(fontOut.glyphData[7], 0x80U);

        MsgUserFont facadeFontOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseUserFont(fontPkt, facadeFontOut));
        EXPECT_EQ(facadeFontOut.fontId, 2U);
    }

} // namespace
} // namespace Sightline
