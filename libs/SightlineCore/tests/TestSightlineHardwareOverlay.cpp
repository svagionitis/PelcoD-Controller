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
        overlayMode.cameraIndex = 0U;
        overlayMode.primaryReticle = 0x11U; // Cross reticle, white
        overlayMode.graphics = OverlayGraphicsFlags::TrackIndex | OverlayGraphicsFlags::UserOverlayObjects;
        const auto modePkt = SightlineOverlayBuilder::buildSetOverlayMode(overlayMode);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetOverlayMode);

        MsgDrawOverlay drawObj {};
        drawObj.cameraIndex = 0U;
        drawObj.objectId = 1U;
        drawObj.action = OverlayActionFlags::Create;
        drawObj.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
        drawObj.type = OverlayObjectType::Rectangle;
        drawObj.a = 100U;
        drawObj.b = 120U;
        drawObj.c = 60U;
        drawObj.d = 40U;
        drawObj.backgroundColor = 0x0EU;
        drawObj.text = "TARGET";
        const auto objPkt = SightlineOverlayBuilder::buildDrawOverlay(drawObj);
        EXPECT_EQ(SightlineFraming::identifyMessage(objPkt), MessageId::DrawOverlay);

        // Verify facade equivalence
        const auto facadeObjPkt = SightlineProtocolBuilder::buildDrawOverlay(drawObj);
        EXPECT_EQ(objPkt, facadeObjPkt);
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
        modeIn.cameraIndex = 1U;
        modeIn.primaryReticle = 0x11U; // Cross reticle, white
        modeIn.secondaryReticle = 0x21U; // Circle reticle, white
        modeIn.graphics = OverlayGraphicsFlags::TrackIndex | OverlayGraphicsFlags::LogoWatermark
            | OverlayGraphicsFlags::UserOverlayObjects;
        modeIn.lineThickness = 2U;
        modeIn.modernMode = 1U;

        const auto modePkt = SightlineOverlayBuilder::buildSetOverlayMode(modeIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetOverlayMode);
        EXPECT_EQ(modePkt, SightlineProtocolBuilder::buildSetOverlayMode(modeIn));

        MsgSetOverlayMode modeOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayMode(modePkt, modeOut));
        EXPECT_EQ(modeOut.cameraIndex, 1U);
        EXPECT_EQ(modeOut.primaryReticle, 0x11U);
        EXPECT_EQ(modeOut.secondaryReticle, 0x21U);
        EXPECT_EQ(modeOut.graphics, modeIn.graphics);
        EXPECT_EQ(modeOut.lineThickness, 2U);
        EXPECT_EQ(modeOut.modernMode, 1U);

        // CurrentOverlayMode (0x42) response
        const auto curModePkt
            = SightlineFraming::buildPacket(MessageId::CurrentOverlayMode, SightlineFraming::extractPayload(modePkt));
        MsgSetOverlayMode curModeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseOverlayMode(curModePkt, curModeOut));
        EXPECT_EQ(curModeOut.primaryReticle, 0x11U);

        // 2. Draw Object (0x3B)
        MsgDrawObject objIn {};
        objIn.objectId = 5U;
        objIn.action = OverlayActionFlags::Create;
        objIn.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
        objIn.shapeType = 1U; // Rectangle
        objIn.a = 320U;
        objIn.b = 240U;
        objIn.c = 120U;
        objIn.d = 30U;
        objIn.color = 0x0EU;
        objIn.text = "UAV-1";

        const auto objPkt = SightlineOverlayBuilder::buildDrawObject(objIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(objPkt), MessageId::DrawObject);
        EXPECT_EQ(objPkt, SightlineProtocolBuilder::buildDrawObject(objIn));

        MsgDrawObject objOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawObject(objPkt, objOut));
        EXPECT_EQ(objOut.objectId, 5U);
        EXPECT_EQ(objOut.shapeType, 1U);
        EXPECT_EQ(objOut.a, 320U);
        EXPECT_EQ(objOut.b, 240U);
        EXPECT_EQ(objOut.color, 0x0EU);
        EXPECT_EQ(objOut.text, "UAV-1");

        MsgDrawObject facadeObjOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDrawObject(objPkt, facadeObjOut));
        EXPECT_EQ(facadeObjOut.text, "UAV-1");

        // 3. Draw Overlay (0x9C)
        MsgDrawOverlay drawIn {};
        drawIn.cameraIndex = 2U;
        drawIn.objectId = 7U;
        drawIn.action = OverlayActionFlags::Create;
        drawIn.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
        drawIn.type = OverlayObjectType::TextExtended;
        drawIn.a = 150U;
        drawIn.b = 200U;
        drawIn.c = 0x2020U; // 100% scale
        drawIn.d = 0U; // Courier font
        drawIn.backgroundColor = 0x0EU;
        drawIn.text = "ALPHA";
        drawIn.e = 2U;
        drawIn.hasE = true;

        const auto drawPkt = SightlineOverlayBuilder::buildDrawOverlay(drawIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(drawPkt), MessageId::DrawOverlay);
        EXPECT_EQ(drawPkt, SightlineProtocolBuilder::buildDrawOverlay(drawIn));

        MsgDrawOverlay drawOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(drawPkt, drawOut));
        EXPECT_EQ(drawOut.cameraIndex, 2U);
        EXPECT_EQ(drawOut.objectId, 7U);
        EXPECT_EQ(drawOut.type, OverlayObjectType::TextExtended);
        EXPECT_EQ(drawOut.text, "ALPHA");
        EXPECT_TRUE(drawOut.hasE);
        EXPECT_EQ(drawOut.e, 2U);

        MsgDrawOverlay facadeDrawOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDrawOverlay(drawPkt, facadeDrawOut));
        EXPECT_EQ(facadeDrawOut.text, "ALPHA");

        // 4. Logo Parameters (0x9B)
        MsgLogoParameters logoIn {};
        logoIn.cameraIndex = 1U;
        logoIn.logoOpacity = 200U;
        logoIn.offsetX = 50U;
        logoIn.offsetY = 80U;

        const auto logoPkt = SightlineOverlayBuilder::buildSetLogoParameters(logoIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(logoPkt), MessageId::LogoParameters);
        EXPECT_EQ(logoPkt, SightlineProtocolBuilder::buildSetLogoParameters(logoIn));

        const auto getLogoPkt = SightlineOverlayBuilder::buildGetLogoParameters(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getLogoPkt), MessageId::GetParameters);
        EXPECT_EQ(getLogoPkt, SightlineProtocolBuilder::buildGetLogoParameters(1U));

        MsgLogoParameters logoOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseLogoParameters(logoPkt, logoOut));
        EXPECT_EQ(logoOut.cameraIndex, 1U);
        EXPECT_EQ(logoOut.logoOpacity, 200U);
        EXPECT_EQ(logoOut.offsetX, 50U);
        EXPECT_EQ(logoOut.offsetY, 80U);

        MsgLogoParameters facadeLogoOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseLogoParameters(logoPkt, facadeLogoOut));
        EXPECT_EQ(facadeLogoOut.logoOpacity, 200U);

        // 5. Ancillary Text Metadata (0xAC)
        MsgAncillaryTextMetadata textIn {};
        textIn.creationTime = 123456789ULL;
        textIn.source = "human";
        textIn.originator = "UAV-OP1";
        textIn.messageBody = "LAT: 37.7749 N LON: 122.4194 W";
        textIn.displayId = 0x0002U;

        const auto textPkt = SightlineOverlayBuilder::buildAncillaryTextMetadata(textIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(textPkt), MessageId::AncillaryTextMetadata);
        EXPECT_EQ(textPkt, SightlineProtocolBuilder::buildAncillaryTextMetadata(textIn));

        MsgAncillaryTextMetadata textOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseAncillaryTextMetadata(textPkt, textOut));
        EXPECT_EQ(textOut.creationTime, 123456789ULL);
        EXPECT_EQ(textOut.source, "human");
        EXPECT_EQ(textOut.originator, "UAV-OP1");
        EXPECT_EQ(textOut.messageBody, "LAT: 37.7749 N LON: 122.4194 W");
        EXPECT_EQ(textOut.displayId, 0x0002U);

        MsgAncillaryTextMetadata facadeTextOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseAncillaryTextMetadata(textPkt, facadeTextOut));
        EXPECT_EQ(facadeTextOut.messageBody, textIn.messageBody);

        // 6. User Font (0xAE)
        MsgUserFont fontIn {};
        fontIn.userFontIndex = 2U;
        fontIn.fontFileName = "NotoSansKR-Regular.ttf";

        const auto fontPkt = SightlineOverlayBuilder::buildUserFont(fontIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(fontPkt), MessageId::UserFont);
        EXPECT_EQ(fontPkt, SightlineProtocolBuilder::buildUserFont(fontIn));

        MsgUserFont fontOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseUserFont(fontPkt, fontOut));
        EXPECT_EQ(fontOut.userFontIndex, 2U);
        EXPECT_EQ(fontOut.fontFileName, "NotoSansKR-Regular.ttf");

        MsgUserFont facadeFontOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseUserFont(fontPkt, facadeFontOut));
        EXPECT_EQ(facadeFontOut.fontFileName, "NotoSansKR-Regular.ttf");

        // 7. Active Objects Bitmask (0x68) & Parameters (0x6B) Query
        const auto getIdsPkt = SightlineOverlayBuilder::buildGetOverlayObjectsIds(0U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getIdsPkt), MessageId::GetParameters);

        MsgCurrentOverlayObjectsIds idsOut {};
        idsOut.idMask[0U] = (1ULL << 1U) | (1ULL << 5U); // Objects 1 and 5 active
        EXPECT_TRUE(idsOut.isObjectActive(1U));
        EXPECT_TRUE(idsOut.isObjectActive(5U));
        EXPECT_FALSE(idsOut.isObjectActive(2U));

        std::vector<std::uint8_t> idsPayload {};
        SightlineFraming::appendU64Le(idsPayload, idsOut.idMask[0U]);
        SightlineFraming::appendU64Le(idsPayload, idsOut.idMask[1U]);
        SightlineFraming::appendU64Le(idsPayload, idsOut.idMask[2U]);
        SightlineFraming::appendU64Le(idsPayload, idsOut.idMask[3U]);
        // Re-construct framed 0x68 packet
        const auto idsPkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectsIds, idsPayload);
        MsgCurrentOverlayObjectsIds parsedIds {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayObjectsIds(idsPkt, parsedIds));
        EXPECT_TRUE(parsedIds.isObjectActive(1U));
        EXPECT_TRUE(parsedIds.isObjectActive(5U));
        EXPECT_FALSE(parsedIds.isObjectActive(2U));
    }

    /// @brief Verify exact byte vectors from EAN-Overlay Graphics document (Section 9.1 and Section 10).
    TEST(TestSightlineHardwareOverlay, EANComplianceVectors)
    {
        // EAN Section 9.1: Drawing a 25x25 pixel white cross in the middle of video from cam0:
        // DrawOverlay 51,AC,13,9C,00,01,01,04,09,00,00,00,00,19,00,00,00,0E,00,01,00,97
        const std::vector<std::uint8_t> expectedCrossPkt { 0x51U, 0xACU, 0x13U, 0x9CU, 0x00U, 0x01U, 0x01U, 0x04U,
            0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U, 0x0EU, 0x00U, 0x01U, 0x00U, 0x97U };

        MsgDrawOverlay crossIn {};
        crossIn.cameraIndex = 0U;
        crossIn.objectId = 1U;
        crossIn.action = OverlayActionFlags::Create;
        crossIn.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic; // 0x04
        crossIn.type = OverlayObjectType::Cross; // 0x09
        crossIn.a = 0U;
        crossIn.b = 0U;
        crossIn.c = 25U; // 25 px width
        crossIn.d = 0U;
        crossIn.backgroundColor = 0x0EU; // White fg (0), transparent bg (14)
        crossIn.text = "";
        crossIn.e = 1U; // 1 px thickness
        crossIn.hasE = true;

        const auto builtCrossPkt = SightlineOverlayBuilder::buildDrawOverlay(crossIn);
        EXPECT_EQ(builtCrossPkt, expectedCrossPkt);

        MsgDrawOverlay parsedCross {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(expectedCrossPkt, parsedCross));
        EXPECT_EQ(parsedCross.cameraIndex, 0U);
        EXPECT_EQ(parsedCross.objectId, 1U);
        EXPECT_EQ(parsedCross.type, OverlayObjectType::Cross);
        EXPECT_EQ(parsedCross.c, 25U);
        EXPECT_TRUE(parsedCross.hasE);
        EXPECT_EQ(parsedCross.e, 1U);

        // EAN Section 10: Cooler countdown black opaque filled rectangle:
        // DrawOverlay 51,AC,13,9C,00,01,01,84,05,01,00,01,00,28,00,28,00,11,00,01,00,0F
        const std::vector<std::uint8_t> expectedBoxPkt { 0x51U, 0xACU, 0x13U, 0x9CU, 0x00U, 0x01U, 0x01U, 0x84U, 0x05U,
            0x01U, 0x00U, 0x01U, 0x00U, 0x28U, 0x00U, 0x28U, 0x00U, 0x11U, 0x00U, 0x01U, 0x00U, 0x0FU };

        MsgDrawOverlay boxIn {};
        boxIn.cameraIndex = 0U;
        boxIn.objectId = 1U;
        boxIn.action = OverlayActionFlags::Create;
        boxIn.propertyFlags = OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::CoordDisplayStatic; // 0x84
        boxIn.type = OverlayObjectType::FilledRectangle; // 0x05
        boxIn.a = 1U;
        boxIn.b = 1U;
        boxIn.c = 40U; // 0x0028
        boxIn.d = 40U; // 0x0028
        boxIn.backgroundColor = 0x11U; // Black fg (1), black bg (1)
        boxIn.text = "";
        boxIn.e = 1U;
        boxIn.hasE = true;

        const auto builtBoxPkt = SightlineOverlayBuilder::buildDrawOverlay(boxIn);
        EXPECT_EQ(builtBoxPkt, expectedBoxPkt);

        // EAN Section 10: Delete black overlay box #1:
        // DrawOverlay 51,AC,13,9C,00,01,00,84,05,01,00,01,00,80,02,E0,01,11,00,01,00,16
        const std::vector<std::uint8_t> expectedDeletePkt { 0x51U, 0xACU, 0x13U, 0x9CU, 0x00U, 0x01U, 0x00U, 0x84U,
            0x05U, 0x01U, 0x00U, 0x01U, 0x00U, 0x80U, 0x02U, 0xE0U, 0x01U, 0x11U, 0x00U, 0x01U, 0x00U, 0x16U };

        MsgDrawOverlay deleteIn {};
        deleteIn.cameraIndex = 0U;
        deleteIn.objectId = 1U;
        deleteIn.action = OverlayActionFlags::Destroy; // 0x00
        deleteIn.propertyFlags
            = OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::CoordDisplayStatic; // 0x84
        deleteIn.type = OverlayObjectType::FilledRectangle;
        deleteIn.a = 1U;
        deleteIn.b = 1U;
        deleteIn.c = 640U; // 0x0280
        deleteIn.d = 480U; // 0x01E0
        deleteIn.backgroundColor = 0x11U;
        deleteIn.text = "";
        deleteIn.e = 1U;
        deleteIn.hasE = true;

        const auto builtDeletePkt = SightlineOverlayBuilder::buildDrawOverlay(deleteIn);
        EXPECT_EQ(builtDeletePkt, expectedDeletePkt);

        // EAN Section 10: Countdown text:
        // DrawOverlay
        // 51,AC,26,9C,00,01,01,84,06,01,00,01,00,20,20,00,00,10,13,49,6D,61,67,65,72,20,63,6F,6F,6C,69,6E,67,20,64,6F,77,6E,00,00,9B
        const std::vector<std::uint8_t> expectedTextPkt { 0x51U, 0xACU, 0x26U, 0x9CU, 0x00U, 0x01U, 0x01U, 0x84U, 0x06U,
            0x01U, 0x00U, 0x01U, 0x00U, 0x20U, 0x20U, 0x00U, 0x00U, 0x10U, 0x13U, 0x49U, 0x6DU, 0x61U, 0x67U, 0x65U,
            0x72U, 0x20U, 0x63U, 0x6FU, 0x6FU, 0x6CU, 0x69U, 0x6EU, 0x67U, 0x20U, 0x64U, 0x6FU, 0x77U, 0x6EU, 0x00U,
            0x00U, 0x9BU };

        MsgDrawOverlay textIn {};
        textIn.cameraIndex = 0U;
        textIn.objectId = 1U;
        textIn.action = OverlayActionFlags::Create;
        textIn.propertyFlags = OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::CoordDisplayStatic; // 0x84
        textIn.type = OverlayObjectType::TextExtended; // 0x06
        textIn.a = 1U;
        textIn.b = 1U;
        textIn.c = 0x2020U; // H & V scaling 32/32 = 100%
        textIn.d = 0U; // Courier font
        textIn.backgroundColor = 0x10U;
        textIn.text = "Imager cooling down";
        textIn.e = 0U;
        textIn.hasE = true;

        const auto builtTextPkt = SightlineOverlayBuilder::buildDrawOverlay(textIn);
        EXPECT_EQ(builtTextPkt, expectedTextPkt);
    }

    /// @brief Verify factory helpers and EAN compliance.
    TEST(TestSightlineHardwareOverlay, FactoryHelpers)
    {
        // 1. makeCrossOverlay matching EAN Section 9.1
        const auto crossObj
            = SightlineOverlayBuilder::makeCrossOverlay(0U, 1U, 0, 0, 25U, OverlayPaletteColor::White, 1U, false);
        const auto crossPkt = SightlineOverlayBuilder::buildDrawOverlay(crossObj);

        const std::vector<std::uint8_t> expectedCrossPkt { 0x51U, 0xACU, 0x13U, 0x9CU, 0x00U, 0x01U, 0x01U, 0x04U,
            0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U, 0x0EU, 0x00U, 0x01U, 0x00U, 0x97U };
        EXPECT_EQ(crossPkt, expectedCrossPkt);

        // Facade equivalence
        const auto facadeCrossObj
            = SightlineProtocolBuilder::makeCrossOverlay(0U, 1U, 0, 0, 25U, OverlayPaletteColor::White, 1U, false);
        EXPECT_EQ(SightlineProtocolBuilder::buildDrawOverlay(facadeCrossObj), expectedCrossPkt);

        // 2. makeBlackoutOverlay matching EAN Section 10 box (640x480)
        const auto blackoutObj = SightlineOverlayBuilder::makeBlackoutOverlay(0U, 1U, 640U, 480U);
        EXPECT_EQ(blackoutObj.cameraIndex, 0U);
        EXPECT_EQ(blackoutObj.objectId, 1U);
        EXPECT_EQ(blackoutObj.action, OverlayActionFlags::Create);
        EXPECT_EQ(blackoutObj.type, OverlayObjectType::FilledRectangle);
        EXPECT_EQ(blackoutObj.c, 640U);
        EXPECT_EQ(blackoutObj.d, 480U);
        EXPECT_EQ(blackoutObj.backgroundColor, 0x11U); // Black fg, black bg

        // 3. makeDestroyOverlay
        const auto destroyObj = SightlineOverlayBuilder::makeDestroyOverlay(0U, 5U);
        EXPECT_EQ(destroyObj.objectId, 5U);
        EXPECT_EQ(destroyObj.action, OverlayActionFlags::Destroy);
        EXPECT_EQ(destroyObj.type, OverlayObjectType::Circle);

        const auto destroyAllObj = SightlineOverlayBuilder::makeDestroyOverlay(0U, 0U);
        EXPECT_EQ(destroyAllObj.objectId, 0U);
        EXPECT_EQ(destroyAllObj.action, OverlayActionFlags::Destroy);

        // 4. makeRectangleOverlay with alpha transparency
        const auto rectObj = SightlineOverlayBuilder::makeRectangleOverlay(
            0U, 2U, 100, 150, 200U, 100U, true, OverlayPaletteColor::Red, OverlayPaletteColor::DarkGray, 16U, 2U, true);
        EXPECT_EQ(rectObj.type, OverlayObjectType::FilledRectangle);
        EXPECT_EQ(rectObj.a, 100U);
        EXPECT_EQ(rectObj.b, 150U);
        EXPECT_EQ(rectObj.c, 200U);
        EXPECT_EQ(rectObj.d, 100U);
        EXPECT_EQ(rectObj.action, static_cast<std::uint8_t>(OverlayActionFlags::Create | (16U << 3U)));

        // 5. makeTextOverlay
        const auto textObj
            = SightlineOverlayBuilder::makeTextOverlay(0U, 3U, 50, 60, "TARGET ACQUIRED", OverlayFontId::ArialBold,
                OverlayPaletteColor::Green, OverlayPaletteColor::TransparentBgOrTurquoiseFg, 32U, 32U, true);
        EXPECT_EQ(textObj.type, OverlayObjectType::TextExtended);
        EXPECT_EQ(textObj.text, "TARGET ACQUIRED");
        EXPECT_EQ(textObj.d, static_cast<std::uint16_t>(OverlayFontId::ArialBold));
    }

    /// @brief Verify KLV field overlay encoding and tag/format packing.
    TEST(TestSightlineHardwareOverlay, KlvOverlayPacking)
    {
        // Tag 0 (UtcTime), Format 0 (TimeYmdHms)
        const auto utcObj = SightlineOverlayBuilder::makeKlvFieldOverlay(
            0U, 4U, 10, 20, KlvFieldTag::UtcTime, KlvFormatType::TimeYmdHms, "UTC: %s", OverlayFontId::Courier);
        EXPECT_EQ(utcObj.type, OverlayObjectType::KlvField);
        EXPECT_EQ(utcObj.d & 0xFFU, static_cast<std::uint16_t>(KlvFieldTag::UtcTime));
        EXPECT_EQ((utcObj.d >> 8U) & 0xFFU, static_cast<std::uint16_t>(KlvFormatType::TimeYmdHms));
        EXPECT_EQ(utcObj.text, "UTC: %s");

        const auto utcPkt = SightlineOverlayBuilder::buildDrawOverlay(utcObj);
        MsgDrawOverlay parsedUtc {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(utcPkt, parsedUtc));
        EXPECT_EQ(parsedUtc.type, OverlayObjectType::KlvField);
        EXPECT_EQ(parsedUtc.text, "UTC: %s");
        EXPECT_EQ(parsedUtc.d & 0xFFU, static_cast<std::uint16_t>(KlvFieldTag::UtcTime));

        // Tag 19 (SlantRange), Format 0 (DistanceMeters)
        const auto slantObj = SightlineOverlayBuilder::makeKlvFieldOverlay(
            0U, 5U, 10, 50, KlvFieldTag::SlantRange, KlvFormatType::DistanceMeters, "Slant: %f m");
        EXPECT_EQ(slantObj.d & 0xFFU, 19U);
        EXPECT_EQ((slantObj.d >> 8U) & 0xFFU, 0U);

        const auto slantPkt = SightlineOverlayBuilder::buildDrawOverlay(slantObj);
        MsgDrawOverlay parsedSlant {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(slantPkt, parsedSlant));
        EXPECT_EQ(parsedSlant.text, "Slant: %f m");
    }

    /// @brief Verify batch draw overlay serializer.
    TEST(TestSightlineHardwareOverlay, BatchOverlay)
    {
        std::vector<MsgDrawOverlay> batch {};
        batch.push_back(SightlineOverlayBuilder::makeCrossOverlay(0U, 1U, 320, 240, 20U));
        batch.push_back(SightlineOverlayBuilder::makeTextOverlay(0U, 2U, 100, 100, "TRACK 1"));
        batch.push_back(SightlineOverlayBuilder::makeDestroyOverlay(0U, 3U));

        const auto batchBytes = SightlineOverlayBuilder::buildDrawOverlayBatch(batch);
        EXPECT_FALSE(batchBytes.empty());

        // Also test facade
        const auto facadeBytes = SightlineProtocolBuilder::buildDrawOverlayBatch(batch);
        EXPECT_EQ(batchBytes, facadeBytes);

        // Verify sequential framing: each packet starts with SLA preamble 0x51, 0xAC
        std::size_t offset { 0U };
        std::size_t packetCount { 0U };
        while (offset + 4U <= batchBytes.size()) {
            EXPECT_EQ(batchBytes[offset], 0x51U);
            EXPECT_EQ(batchBytes[offset + 1U], 0xACU);
            const auto headerLen = SightlineFraming::getHeaderLength(
                ByteView { batchBytes.data() + offset, batchBytes.size() - offset });
            ASSERT_GT(headerLen, 0U);
            const auto len = batchBytes[offset + 2U];
            const auto totalPktLen = static_cast<std::size_t>(headerLen + len);
            ASSERT_LE(offset + totalPktLen, batchBytes.size());

            ByteView pktView { batchBytes.data() + offset, totalPktLen };
            EXPECT_EQ(SightlineFraming::identifyMessage(pktView), MessageId::DrawOverlay);

            MsgDrawOverlay parsed {};
            ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(pktView, parsed));
            EXPECT_EQ(parsed.objectId, batch[packetCount].objectId);

            offset += totalPktLen;
            ++packetCount;
        }
        EXPECT_EQ(packetCount, 3U);
        EXPECT_EQ(offset, batchBytes.size());
    }

    /// @brief Verify active object IDs bitmask extraction.
    TEST(TestSightlineHardwareOverlay, OverlayIdsBitmask)
    {
        MsgCurrentOverlayObjectsIds msg {};
        EXPECT_TRUE(msg.getActiveObjectIds().empty());

        msg.setObjectActive(1U, true);
        msg.setObjectActive(5U, true);
        msg.setObjectActive(42U, true);
        msg.setObjectActive(199U, true);

        const auto activeIds = msg.getActiveObjectIds();
        ASSERT_EQ(activeIds.size(), 4U);
        EXPECT_EQ(activeIds[0U], 1U);
        EXPECT_EQ(activeIds[1U], 5U);
        EXPECT_EQ(activeIds[2U], 42U);
        EXPECT_EQ(activeIds[3U], 199U);

        // Test wire parsing of bitmask
        std::vector<std::uint8_t> payload(32U, 0U);
        // Bit 1 = 1 << 1 = 0x02
        payload[0U] = 0x02U;
        // Bit 64 = first bit of word 1 -> payload[8U] = 0x01
        payload[8U] = 0x01U;

        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectsIds, payload);
        MsgCurrentOverlayObjectsIds parsed {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayObjectsIds(pkt, parsed));
        EXPECT_TRUE(parsed.isObjectActive(1U));
        EXPECT_TRUE(parsed.isObjectActive(64U));
        EXPECT_FALSE(parsed.isObjectActive(2U));
    }

    /// @brief Verify truncated and boundary packet safety.
    TEST(TestSightlineHardwareOverlay, TruncatedSafety)
    {
        // Truncated DrawOverlay (< 15 bytes payload)
        std::vector<std::uint8_t> shortPayload(10U, 0U);
        const auto shortDrawPkt = SightlineFraming::buildPacket(MessageId::DrawOverlay, shortPayload);
        MsgDrawOverlay parsedDraw {};
        EXPECT_FALSE(SightlineOverlayParser::parseDrawOverlay(shortDrawPkt, parsedDraw));

        // Truncated OverlayMode:
        // Less than 4 bytes must fail
        std::vector<std::uint8_t> tooShortModePayload(3U, 0U);
        const auto tooShortModePkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayMode, tooShortModePayload);
        MsgSetOverlayMode parsedMode {};
        EXPECT_FALSE(SightlineOverlayParser::parseOverlayMode(tooShortModePkt, parsedMode));

        // 4 bytes minimal payload must succeed
        std::vector<std::uint8_t> minModePayload { 0x11U, 0x22U, 0x34U, 0x12U };
        const auto minModePkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayMode, minModePayload);
        EXPECT_TRUE(SightlineOverlayParser::parseOverlayMode(minModePkt, parsedMode));
        EXPECT_EQ(parsedMode.primaryReticle, 0x11U);
        EXPECT_EQ(parsedMode.secondaryReticle, 0x22U);
        EXPECT_EQ(parsedMode.graphics, 0x1234U);

        // 10 bytes payload must succeed and extract intermediate fields
        std::vector<std::uint8_t> midModePayload { 0x11U, 0x22U, 0x34U, 0x12U, 0x08U, 0x07U, 0x02U, 0x03U, 0x04U,
            0x05U };
        const auto midModePkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayMode, midModePayload);
        EXPECT_TRUE(SightlineOverlayParser::parseOverlayMode(midModePkt, parsedMode));
        EXPECT_EQ(parsedMode.mtiColor, 0x08U);
        EXPECT_EQ(parsedMode.cameraIndex, 0x02U);
        EXPECT_EQ(parsedMode.cursorReticle, 0x05U);

        // Empty CurrentOverlayObjectsIds must fail gracefully
        const auto emptyIdsPkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectsIds, {});
        MsgCurrentOverlayObjectsIds parsedIds {};
        EXPECT_FALSE(SightlineOverlayParser::parseOverlayObjectsIds(emptyIdsPkt, parsedIds));

        // Partial (e.g. 16 bytes) CurrentOverlayObjectsIds must succeed and populate first 128 bits
        std::vector<std::uint8_t> partialIdsPayload(16U, 0xFFU);
        const auto partialIdsPkt
            = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectsIds, partialIdsPayload);
        EXPECT_TRUE(SightlineOverlayParser::parseOverlayObjectsIds(partialIdsPkt, parsedIds));
        EXPECT_TRUE(parsedIds.isObjectActive(1U));
        EXPECT_TRUE(parsedIds.isObjectActive(127U));
        EXPECT_FALSE(parsedIds.isObjectActive(128U)); // not in payload, stays 0
    }

    /// @brief Verify overlay object parameters query and deserialization (Message ID 0x6B & Query 0x28).
    TEST(TestSightlineHardwareOverlay, OverlayObjectParams)
    {
        // 1. Serialization of query 0x28 for object 7
        const auto queryPkt = SightlineOverlayBuilder::buildGetOverlayObjectParams(7U);
        EXPECT_EQ(SightlineFraming::identifyMessage(queryPkt), MessageId::GetParameters);
        EXPECT_EQ(queryPkt, SightlineProtocolBuilder::buildGetOverlayObjectParams(7U));

        const auto queryPayload = SightlineFraming::extractPayload(queryPkt);
        ASSERT_EQ(queryPayload.size(), 2U);
        EXPECT_EQ(queryPayload[0U], static_cast<std::uint8_t>(MessageId::CurrentOverlayObjectParameters));
        EXPECT_EQ(queryPayload[1U], 7U);

        // 2. Deserialization of 0x6B parameter packet with text
        std::vector<std::uint8_t> paramPayload {};
        paramPayload.push_back(static_cast<std::uint8_t>(OverlayObjectType::TextExtended)); // type
        paramPayload.push_back(7U); // objectId
        paramPayload.push_back(OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::SaveToFlash); // flags
        paramPayload.push_back(1U); // staticObject
        SightlineFraming::appendU16Le(paramPayload, 320U); // a (X)
        SightlineFraming::appendU16Le(paramPayload, 240U); // b (Y)
        SightlineFraming::appendU16Le(paramPayload, 64U); // c (Scale)
        SightlineFraming::appendU16Le(paramPayload, 1U); // d (Font)
        paramPayload.push_back(0x0EU); // color (White on Transparent)
        const std::string bannerText = "AIR DEFENSE ALPHA";
        paramPayload.insert(paramPayload.end(), bannerText.begin(), bannerText.end());
        paramPayload.push_back(0U); // Null terminator

        const auto paramPkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectParameters, paramPayload);
        MsgCurrentOverlayObjectParameters parsedParams {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayObjectParams(paramPkt, parsedParams));
        EXPECT_EQ(parsedParams.type, static_cast<std::uint8_t>(OverlayObjectType::TextExtended));
        EXPECT_EQ(parsedParams.objectId, 7U);
        EXPECT_EQ(parsedParams.flags, OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::SaveToFlash);
        EXPECT_EQ(parsedParams.staticObject, 1U);
        EXPECT_EQ(parsedParams.a, 320U);
        EXPECT_EQ(parsedParams.b, 240U);
        EXPECT_EQ(parsedParams.c, 64U);
        EXPECT_EQ(parsedParams.d, 1U);
        EXPECT_EQ(parsedParams.color, 0x0EU);
        EXPECT_EQ(parsedParams.text, "AIR DEFENSE ALPHA");

        // Facade equivalence
        MsgCurrentOverlayObjectParameters facadeParams {};
        ASSERT_TRUE(SightlineProtocolParser::parseOverlayObjectParams(paramPkt, facadeParams));
        EXPECT_EQ(facadeParams.text, parsedParams.text);

        // 3. Truncation and wrong message ID rejection
        std::vector<std::uint8_t> shortPayload(12U, 0U);
        const auto shortPkt = SightlineFraming::buildPacket(MessageId::CurrentOverlayObjectParameters, shortPayload);
        EXPECT_FALSE(SightlineOverlayParser::parseOverlayObjectParams(shortPkt, parsedParams));

        const auto wrongIdPkt = SightlineFraming::buildPacket(MessageId::GetParameters, paramPayload);
        EXPECT_FALSE(SightlineOverlayParser::parseOverlayObjectParams(wrongIdPkt, parsedParams));
    }

    /// @brief Verify full 21-byte round-trip covering all fields of MsgSetOverlayMode.
    TEST(TestSightlineHardwareOverlay, OverlayModeRoundTrip)
    {
        MsgSetOverlayMode modeIn {};
        modeIn.primaryReticle = 0x11U; // Cross, White
        modeIn.secondaryReticle = 0x24U; // Circle, Dark Gray
        modeIn.graphics = 0x1234U; // Custom bitmask
        modeIn.mtiColor = 0x08U; // Light Green
        modeIn.mtiSelectableColor = 0x0DU; // Yellow
        modeIn.cameraIndex = 2U; // Camera 2
        modeIn.selectedReticle = 0x02U;
        modeIn.personReticle = 0x0BU; // Cyan
        modeIn.cursorReticle = 0x01U;
        modeIn.lineThickness = 3U; // 3px
        modeIn.fontScale = 48U; // 150%
        modeIn.fontId = 1U; // Arial
        modeIn.extraLabels = ClassifierLabelsFlags::ClassifierTrackLabels | ClassifierLabelsFlags::MtiLabelsOnly;
        modeIn.modernMode = 1U;
        modeIn.mtiReticle = static_cast<std::uint8_t>(MtiReticleType::InvertedTriangleWithClass);
        modeIn.mtiLabelAdv = static_cast<std::uint8_t>(MtiLabelPosition::TopRight);
        modeIn.mtiMultiColor = 1U;
        modeIn.detCounterColor = 0x0EU;
        modeIn.mtiDisplayLabelLimit = 16U;
        modeIn.detCounterPos = 2U;
        modeIn.mtiLabelDeconflict = 1U;

        const auto modePkt = SightlineOverlayBuilder::buildSetOverlayMode(modeIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(modePkt), MessageId::SetOverlayMode);
        EXPECT_EQ(modePkt, SightlineProtocolBuilder::buildSetOverlayMode(modeIn));

        // Exact payload length check (22 bytes payload + 5 bytes framing = 27 bytes)
        EXPECT_EQ(modePkt.size(), 27U);

        MsgSetOverlayMode modeOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseOverlayMode(modePkt, modeOut));
        EXPECT_EQ(modeOut.primaryReticle, modeIn.primaryReticle);
        EXPECT_EQ(modeOut.secondaryReticle, modeIn.secondaryReticle);
        EXPECT_EQ(modeOut.graphics, modeIn.graphics);
        EXPECT_EQ(modeOut.mtiColor, modeIn.mtiColor);
        EXPECT_EQ(modeOut.mtiSelectableColor, modeIn.mtiSelectableColor);
        EXPECT_EQ(modeOut.cameraIndex, modeIn.cameraIndex);
        EXPECT_EQ(modeOut.selectedReticle, modeIn.selectedReticle);
        EXPECT_EQ(modeOut.personReticle, modeIn.personReticle);
        EXPECT_EQ(modeOut.cursorReticle, modeIn.cursorReticle);
        EXPECT_EQ(modeOut.lineThickness, modeIn.lineThickness);
        EXPECT_EQ(modeOut.fontScale, modeIn.fontScale);
        EXPECT_EQ(modeOut.fontId, modeIn.fontId);
        EXPECT_EQ(modeOut.extraLabels, modeIn.extraLabels);
        EXPECT_EQ(modeOut.modernMode, modeIn.modernMode);
        EXPECT_EQ(modeOut.mtiReticle, modeIn.mtiReticle);
        EXPECT_EQ(modeOut.mtiLabelAdv, modeIn.mtiLabelAdv);
        EXPECT_EQ(modeOut.mtiMultiColor, modeIn.mtiMultiColor);
        EXPECT_EQ(modeOut.detCounterColor, modeIn.detCounterColor);
        EXPECT_EQ(modeOut.mtiDisplayLabelLimit, modeIn.mtiDisplayLabelLimit);
        EXPECT_EQ(modeOut.detCounterPos, modeIn.detCounterPos);
        EXPECT_EQ(modeOut.mtiLabelDeconflict, modeIn.mtiLabelDeconflict);

        // Facade equivalence
        MsgSetOverlayMode facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseOverlayMode(modePkt, facadeOut));
        EXPECT_EQ(facadeOut.fontScale, modeIn.fontScale);
        EXPECT_EQ(facadeOut.graphics, modeIn.graphics);
    }

    /// @brief Verify primitive geometry types, 5-bit alpha blending, and negative coordinates.
    TEST(TestSightlineHardwareOverlay, DrawOverlayPrimitives)
    {
        // 1. Circle Primitive (Type 0)
        MsgDrawOverlay circle {};
        circle.cameraIndex = 0U;
        circle.objectId = 10U;
        circle.action = OverlayActionFlags::Create;
        circle.type = OverlayObjectType::Circle;
        circle.a = 200U; // X
        circle.b = 150U; // Y
        circle.c = 50U; // Radius
        const auto circlePkt = SightlineOverlayBuilder::buildDrawOverlay(circle);
        MsgDrawOverlay circleOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(circlePkt, circleOut));
        EXPECT_EQ(circleOut.type, OverlayObjectType::Circle);
        EXPECT_EQ(circleOut.a, 200U);
        EXPECT_EQ(circleOut.c, 50U);

        // 2. Line Primitive (Type 2)
        MsgDrawOverlay line {};
        line.cameraIndex = 1U;
        line.objectId = 11U;
        line.action = OverlayActionFlags::Create;
        line.type = OverlayObjectType::Line;
        line.a = 50U; // X1
        line.b = 50U; // Y1
        line.c = 300U; // X2
        line.d = 200U; // Y2
        line.e = 3U; // Thickness
        line.hasE = true;
        const auto linePkt = SightlineOverlayBuilder::buildDrawOverlay(line);
        MsgDrawOverlay lineOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(linePkt, lineOut));
        EXPECT_EQ(lineOut.type, OverlayObjectType::Line);
        EXPECT_EQ(lineOut.c, 300U);
        EXPECT_EQ(lineOut.d, 200U);
        EXPECT_TRUE(lineOut.hasE);
        EXPECT_EQ(lineOut.e, 3U);

        // 3. 5-bit Alpha Blending in Action Byte (Bits 3..7)
        MsgDrawOverlay alphaRect {};
        alphaRect.cameraIndex = 0U;
        alphaRect.objectId = 12U;
        // Alpha = 25 (out of 31) -> 25 << 3 | 0x01
        alphaRect.action
            = static_cast<std::uint8_t>(OverlayActionFlags::Create | (25U << OverlayActionFlags::AlphaShift));
        alphaRect.type = OverlayObjectType::FilledRectangle;
        alphaRect.a = 100U;
        alphaRect.b = 80U;
        alphaRect.c = 200U;
        alphaRect.d = 120U;
        const auto alphaPkt = SightlineOverlayBuilder::buildDrawOverlay(alphaRect);
        MsgDrawOverlay alphaOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(alphaPkt, alphaOut));
        EXPECT_EQ((alphaOut.action >> OverlayActionFlags::AlphaShift) & 0x1FU, 25U);
        EXPECT_EQ(alphaOut.action & 0x01U, OverlayActionFlags::Create);

        // 4. Signed Negative Coordinates preservation
        const std::int16_t negX = -320;
        const std::int16_t negY = -240;
        MsgDrawOverlay negCross = SightlineOverlayBuilder::makeCrossOverlay(
            0U, 13U, negX, negY, 20U, OverlayPaletteColor::LightBlue, 1U, false);
        const auto negPkt = SightlineOverlayBuilder::buildDrawOverlay(negCross);
        MsgDrawOverlay negOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(negPkt, negOut));
        EXPECT_EQ(static_cast<std::int16_t>(negOut.a), negX);
        EXPECT_EQ(static_cast<std::int16_t>(negOut.b), negY);

        // 5. Square and FilledSquare Primitives (Types 10 & 11)
        MsgDrawOverlay square {};
        square.type = OverlayObjectType::Square;
        square.objectId = 14U;
        square.action = OverlayActionFlags::Create;
        square.a = 500U;
        square.b = 400U;
        square.c = 60U; // Side length
        const auto squarePkt = SightlineOverlayBuilder::buildDrawOverlay(square);
        MsgDrawOverlay squareOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(squarePkt, squareOut));
        EXPECT_EQ(squareOut.type, OverlayObjectType::Square);
        EXPECT_EQ(squareOut.c, 60U);
    }

    /// @brief Verify optional extension parameters E & F and text layout combinations.
    TEST(TestSightlineHardwareOverlay, OptionalParamsAndText)
    {
        // 1. Standard 15-byte fixed layout (hasE = false, hasF = false)
        MsgDrawOverlay fixed15 {};
        fixed15.type = OverlayObjectType::Rectangle;
        fixed15.objectId = 20U;
        fixed15.action = OverlayActionFlags::Create;
        fixed15.hasE = false;
        fixed15.hasF = false;
        const auto pkt15 = SightlineOverlayBuilder::buildDrawOverlay(fixed15);
        EXPECT_EQ(SightlineFraming::extractPayload(pkt15).size(), 15U);

        // 2. 17-byte layout (hasE = true, hasF = false)
        MsgDrawOverlay ext17 = fixed15;
        ext17.hasE = true;
        ext17.e = 5U; // Line thickness 5px
        const auto pkt17 = SightlineOverlayBuilder::buildDrawOverlay(ext17);
        EXPECT_EQ(SightlineFraming::extractPayload(pkt17).size(), 17U);
        MsgDrawOverlay out17 {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(pkt17, out17));
        EXPECT_TRUE(out17.hasE);
        EXPECT_FALSE(out17.hasF);
        EXPECT_EQ(out17.e, 5U);

        // 3. 19-byte layout (hasE = true, hasF = true)
        MsgDrawOverlay ext19 = ext17;
        ext19.hasF = true;
        ext19.f = 45U; // 45 degrees angle
        const auto pkt19 = SightlineOverlayBuilder::buildDrawOverlay(ext19);
        EXPECT_EQ(SightlineFraming::extractPayload(pkt19).size(), 19U);
        MsgDrawOverlay out19 {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(pkt19, out19));
        EXPECT_TRUE(out19.hasE);
        EXPECT_TRUE(out19.hasF);
        EXPECT_EQ(out19.e, 5U);
        EXPECT_EQ(out19.f, 45U);

        // 4. Text with extension E (e.g. outline thickness)
        MsgDrawOverlay textWithE = SightlineOverlayBuilder::makeTextOverlay(0U, 21U, 10, 10, "FIRE MISSION");
        textWithE.hasE = true;
        textWithE.e = 2U;
        const auto textPkt = SightlineOverlayBuilder::buildDrawOverlay(textWithE);
        MsgDrawOverlay textOut {};
        ASSERT_TRUE(SightlineOverlayParser::parseDrawOverlay(textPkt, textOut));
        EXPECT_EQ(textOut.text, "FIRE MISSION");
        EXPECT_TRUE(textOut.hasE);
        EXPECT_EQ(textOut.e, 2U);
    }

    /// @brief Verify active object IDs 256-bit bitmask boundaries across all 4 words.
    TEST(TestSightlineHardwareOverlay, BitmaskBoundaries)
    {
        MsgCurrentOverlayObjectsIds msg {};

        // Boundary bit testing: 0, 1, 63, 64, 127, 128, 199, 200, 255
        const std::vector<std::uint8_t> boundaryIds { 0U, 1U, 63U, 64U, 127U, 128U, 199U, 200U, 255U };
        for (const auto id : boundaryIds) {
            EXPECT_FALSE(msg.isObjectActive(id));
            msg.setObjectActive(id, true);
            EXPECT_TRUE(msg.isObjectActive(id));
        }

        // Active user object IDs filter strictly [1..199]
        const auto activeUserIds = msg.getActiveObjectIds();
        EXPECT_EQ(activeUserIds.size(), 6U); // 1, 63, 64, 127, 128, 199
        EXPECT_EQ(activeUserIds.front(), 1U);
        EXPECT_EQ(activeUserIds.back(), 199U);

        // Test clearing bits
        for (const auto id : boundaryIds) {
            msg.setObjectActive(id, false);
            EXPECT_FALSE(msg.isObjectActive(id));
        }
        EXPECT_TRUE(msg.getActiveObjectIds().empty());
    }

    /// @brief Verify user TrueType font assignment and logo watermark parameters.
    TEST(TestSightlineHardwareOverlay, FontAndLogoWatermark)
    {
        // 1. User Font Slot 0 (short filename)
        MsgUserFont font0Msg {};
        font0Msg.userFontIndex = 0U;
        font0Msg.fontFileName = "hud.ttf";
        const auto font0 = SightlineOverlayBuilder::buildUserFont(font0Msg);
        EXPECT_EQ(SightlineFraming::identifyMessage(font0), MessageId::UserFont);
        MsgUserFont font0Out {};
        ASSERT_TRUE(SightlineOverlayParser::parseUserFont(font0, font0Out));
        EXPECT_EQ(font0Out.userFontIndex, 0U);
        EXPECT_EQ(font0Out.fontFileName, "hud.ttf");

        // 2. User Font Slot 15 (max slot, path filename)
        MsgUserFont font15Msg {};
        font15Msg.userFontIndex = 15U;
        font15Msg.fontFileName = "fonts/tactical_symbols_v2.otf";
        const auto font15 = SightlineOverlayBuilder::buildUserFont(font15Msg);
        MsgUserFont font15Out {};
        ASSERT_TRUE(SightlineOverlayParser::parseUserFont(font15, font15Out));
        EXPECT_EQ(font15Out.userFontIndex, 15U);
        EXPECT_EQ(font15Out.fontFileName, "fonts/tactical_symbols_v2.otf");

        // 3. Logo Watermark Opacity and Offsets
        MsgLogoParameters logo {};
        logo.cameraIndex = 3U;
        logo.logoOpacity = 0U; // Fully transparent
        logo.offsetX = 0U;
        logo.offsetY = 0U;
        const auto logoPkt0 = SightlineOverlayBuilder::buildSetLogoParameters(logo);
        MsgLogoParameters logoOut0 {};
        ASSERT_TRUE(SightlineOverlayParser::parseLogoParameters(logoPkt0, logoOut0));
        EXPECT_EQ(logoOut0.cameraIndex, 3U);
        EXPECT_EQ(logoOut0.logoOpacity, 0U);

        logo.logoOpacity = 255U; // Fully opaque
        logo.offsetX = 1920U;
        logo.offsetY = 1080U;
        const auto logoPktMax = SightlineOverlayBuilder::buildSetLogoParameters(logo);
        MsgLogoParameters logoOutMax {};
        ASSERT_TRUE(SightlineOverlayParser::parseLogoParameters(logoPktMax, logoOutMax));
        EXPECT_EQ(logoOutMax.logoOpacity, 255U);
        EXPECT_EQ(logoOutMax.offsetX, 1920U);
        EXPECT_EQ(logoOutMax.offsetY, 1080U);
    }

    /// @brief Verify parser resilience against corrupted CRCs, truncated headers, and malformed payloads.
    TEST(TestSightlineHardwareOverlay, MalformedFuzzingSafety)
    {
        // 1. Corrupt CRC-8 checksum detection
        MsgDrawOverlay validDraw = SightlineOverlayBuilder::makeCrossOverlay(0U, 1U, 0, 0, 20U);
        auto corruptPkt = SightlineOverlayBuilder::buildDrawOverlay(validDraw);
        ASSERT_FALSE(corruptPkt.empty());
        const auto expectedCrc = corruptPkt.back();
        corruptPkt.back() ^= 0xFFU; // Invert CRC
        const auto headerLen = SightlineFraming::getHeaderLength(corruptPkt);
        const auto computedCrc
            = SightlineCrc8::compute(corruptPkt.data() + headerLen, corruptPkt.size() - headerLen - 1U);
        EXPECT_NE(computedCrc, corruptPkt.back());
        EXPECT_EQ(computedCrc, expectedCrc);

        // 2. Incomplete header (< 4 bytes)
        const std::vector<std::uint8_t> truncHeader { 0x51U, 0xACU, 0x10U };
        MsgDrawOverlay dummyDraw {};
        EXPECT_FALSE(SightlineOverlayParser::parseDrawOverlay(truncHeader, dummyDraw));
        MsgSetOverlayMode dummyMode {};
        EXPECT_FALSE(SightlineOverlayParser::parseOverlayMode(truncHeader, dummyMode));
        MsgLogoParameters dummyLogo {};
        EXPECT_FALSE(SightlineOverlayParser::parseLogoParameters(truncHeader, dummyLogo));

        // 3. Mismatched Message ID injection
        const auto validModePkt = SightlineOverlayBuilder::buildSetOverlayMode(MsgSetOverlayMode {});
        EXPECT_FALSE(SightlineOverlayParser::parseDrawOverlay(validModePkt, dummyDraw));
        EXPECT_FALSE(SightlineOverlayParser::parseLogoParameters(validModePkt, dummyLogo));
        MsgUserFont dummyFont {};
        EXPECT_FALSE(SightlineOverlayParser::parseUserFont(validModePkt, dummyFont));

        // 4. Undersized DrawOverlay payload (e.g. 4 bytes instead of 15+)
        const std::vector<std::uint8_t> underBytes { 0x00U, 0x01U, 0x01U, 0x04U };
        const auto underPkt = SightlineFraming::buildPacket(MessageId::DrawOverlay, underBytes);
        EXPECT_FALSE(SightlineOverlayParser::parseDrawOverlay(underPkt, dummyDraw));
    }

} // namespace
} // namespace Sightline
