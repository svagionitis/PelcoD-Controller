/// @file TestSightlinePeripherals.cpp
/// @brief Unit tests for Sightline serial/GPIO, autonomous landing aid, and thermal NUC builders and parsers.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineCompressionBuilder.h"
#include "modules/SightlineCompressionParser.h"
#include "modules/SightlineLandingBuilder.h"
#include "modules/SightlineLandingParser.h"
#include "modules/SightlineNetworkParser.h"
#include "modules/SightlineCalibrationBuilder.h"
#include "modules/SightlineCalibrationParser.h"
#include "modules/SightlinePaletteBuilder.h"
#include "modules/SightlinePaletteParser.h"
#include "modules/SightlineSerialBuilder.h"
#include "modules/SightlineSerialParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify serial port, passthrough, and GPIO serialization & deserialization.
    TEST(TestSightlinePeripherals, BuildAndParseSerial)
    {
        // 1. Port Configuration (0x3E / 0x53)
        MsgSetPortConfiguration portMsg {};
        portMsg.portIndex = 1U;
        portMsg.baudRate = 115200U;
        portMsg.mode = 2U;

        const auto portPkt = SightlineSerialBuilder::buildSetPortConfiguration(portMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(portPkt), MessageId::SetPortConfiguration);

        MsgSetPortConfiguration portOut {};
        ASSERT_TRUE(SightlineSerialParser::parsePortConfiguration(portPkt, portOut));
        EXPECT_EQ(portOut.portIndex, 1U);
        EXPECT_EQ(portOut.baudRate, 115200U);
        EXPECT_EQ(portOut.mode, 2U);

        // 2. Command Pass-Through (0x3D)
        MsgCommandPassThrough passMsg {};
        passMsg.destPort = 3U;
        passMsg.data = { 0xDEU, 0xADU, 0xBEU, 0xEFU };

        const auto passPkt = SightlineSerialBuilder::buildCommandPassThrough(passMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(passPkt), MessageId::CommandPassThrough);

        MsgCommandPassThrough passOut {};
        ASSERT_TRUE(SightlineSerialParser::parseCommandPassThrough(passPkt, passOut));
        EXPECT_EQ(passOut.destPort, 3U);
        ASSERT_EQ(passOut.data.size(), 4U);
        EXPECT_EQ(passOut.data[0], 0xDEU);
        EXPECT_EQ(passOut.data[3], 0xEFU);

        // 3. GPIO (0xB6)
        MsgGPIO gpioMsg {};
        gpioMsg.pinMask = 0x0FU;
        gpioMsg.pinValues = 0x05U;
        gpioMsg.directionMask = 0x0FU;

        const auto gpioPkt = SightlineSerialBuilder::buildGPIO(gpioMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(gpioPkt), MessageId::GPIO);

        MsgGPIO gpioOut {};
        ASSERT_TRUE(SightlineSerialParser::parseGPIO(gpioPkt, gpioOut));
        EXPECT_EQ(gpioOut.pinMask, 0x0FU);
        EXPECT_EQ(gpioOut.pinValues, 0x05U);
        EXPECT_EQ(gpioOut.directionMask, 0x0FU);
    }

    /// @brief Verify visual landing aid and relative target position serialization & deserialization.
    TEST(TestSightlinePeripherals, BuildAndParseLanding)
    {
        // 1. Landing Aid (0x81)
        MsgLandingAid aidMsg {};
        aidMsg.cameraIndex = 0U;
        aidMsg.mode = 1U;
        aidMsg.patternType = 1U;

        const auto aidPkt = SightlineLandingBuilder::buildLandingAid(aidMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(aidPkt), MessageId::LandingAid);

        MsgLandingAid aidOut {};
        ASSERT_TRUE(SightlineLandingParser::parseLandingAid(aidPkt, aidOut));
        EXPECT_EQ(aidOut.cameraIndex, 0U);
        EXPECT_EQ(aidOut.mode, 1U);
        EXPECT_EQ(aidOut.patternType, 1U);

        // 2. Landing Position (0x83)
        MsgLandingPosition posMsg {};
        posMsg.cameraIndex = 0U;
        posMsg.relativeX = 12.5;
        posMsg.relativeY = -4.2;
        posMsg.relativeZ = 35.0;
        posMsg.yawDeg = 45.0;
        posMsg.pitchDeg = -2.5;
        posMsg.rollDeg = 0.5;
        posMsg.confidence = 98U;

        const auto posPkt = SightlineLandingBuilder::buildLandingPosition(posMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(posPkt), MessageId::LandingPosition);

        MsgLandingPosition posOut {};
        ASSERT_TRUE(SightlineLandingParser::parseLandingPosition(posPkt, posOut));
        EXPECT_EQ(posOut.cameraIndex, 0U);
        EXPECT_DOUBLE_EQ(posOut.relativeX, 12.5);
        EXPECT_DOUBLE_EQ(posOut.relativeY, -4.2);
        EXPECT_DOUBLE_EQ(posOut.relativeZ, 35.0);
        EXPECT_DOUBLE_EQ(posOut.yawDeg, 45.0);
        EXPECT_DOUBLE_EQ(posOut.pitchDeg, -2.5);
        EXPECT_DOUBLE_EQ(posOut.rollDeg, 0.5);
        EXPECT_EQ(posOut.confidence, 98U);
    }

    /// @brief Verify thermal palette, camera calibration and parameter file serialization & deserialization.
    /// @details NUC/DPR messages (0x35, 0x36, 0xA8, 0xA1, 0xAF) are covered against IDD golden
    ///          vectors in TestSightlineNuc.cpp.
    TEST(TestSightlinePeripherals, BuildAndParseThermalAux)
    {
        // 1. User Thermal Palette (0x72)
        MsgUserPalette palMsg {};
        palMsg.paletteIndex = 1U;
        palMsg.lutData = { 0x10U, 0x20U, 0x30U, 0x40U, 0x50U };

        const auto palPkt = SightlinePaletteBuilder::buildSetUserPalette(palMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(palPkt), MessageId::SetUserPalette);

        MsgUserPalette palOut {};
        ASSERT_TRUE(SightlinePaletteParser::parseUserPalette(palPkt, palOut));
        EXPECT_EQ(palOut.paletteIndex, 1U);
        EXPECT_EQ(palOut.lutData, palMsg.lutData);

        // 2. Camera Calibration (0xC0)
        MsgCameraCalibration calibMsg {};
        calibMsg.cameraIndex = 0U;
        calibMsg.focalLengthX = 1000.5F;
        calibMsg.focalLengthY = 1000.8F;
        calibMsg.principalPointX = 640.0F;
        calibMsg.principalPointY = 512.0F;
        calibMsg.radialDistortionK1 = -0.15F;
        calibMsg.radialDistortionK2 = 0.05F;
        calibMsg.tangentialP1 = 0.001F;
        calibMsg.tangentialP2 = -0.002F;

        const auto calibPkt = SightlineCalibrationBuilder::buildCameraCalibration(calibMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(calibPkt), MessageId::CameraCalibration);

        MsgCameraCalibration calibOut {};
        ASSERT_TRUE(SightlineCalibrationParser::parseCameraCalibration(calibPkt, calibOut));
        EXPECT_EQ(calibOut.cameraIndex, 0U);
        EXPECT_FLOAT_EQ(calibOut.focalLengthX, 1000.5F);
        EXPECT_FLOAT_EQ(calibOut.focalLengthY, 1000.8F);
        EXPECT_FLOAT_EQ(calibOut.principalPointX, 640.0F);
        EXPECT_FLOAT_EQ(calibOut.principalPointY, 512.0F);
        EXPECT_FLOAT_EQ(calibOut.radialDistortionK1, -0.15F);
        EXPECT_FLOAT_EQ(calibOut.radialDistortionK2, 0.05F);
        EXPECT_FLOAT_EQ(calibOut.tangentialP1, 0.001F);
        EXPECT_FLOAT_EQ(calibOut.tangentialP2, -0.002F);

        // 3. Camera Parameter File (0xC2)
        MsgCameraParameterFile paramFileMsg {};
        paramFileMsg.cameraIndex = 1U;
        paramFileMsg.action = 0U; // Load
        paramFileMsg.filename = "boson640_calib.bin";

        const auto filePkt = SightlineCalibrationBuilder::buildCameraParameterFile(paramFileMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(filePkt), MessageId::CameraParameterFile);

        MsgCameraParameterFile fileOut {};
        ASSERT_TRUE(SightlineCalibrationParser::parseCameraParameterFile(filePkt, fileOut));
        EXPECT_EQ(fileOut.cameraIndex, 1U);
        EXPECT_EQ(fileOut.action, 0U);
        EXPECT_EQ(fileOut.filename, "boson640_calib.bin");
    }

    /// @brief Verify network list reply deserialization.
    /// @details 0xA8 is not a valid GetParameters target per IDD v3.11; the former dead pixel
    ///          query assertions were removed with buildGetDeadPixel.
    TEST(TestSightlinePeripherals, NetworkList)
    {
        // Network list parsing (0x67)
        std::vector<std::uint8_t> payload {};
        payload.push_back(2U); // 2 interfaces
        // "eth0\0"
        payload.push_back('e');
        payload.push_back('t');
        payload.push_back('h');
        payload.push_back('0');
        payload.push_back('\0');
        // "wlan0\0"
        payload.push_back('w');
        payload.push_back('l');
        payload.push_back('a');
        payload.push_back('n');
        payload.push_back('0');
        payload.push_back('\0');

        const auto netListPkt = SightlineFraming::buildPacket(MessageId::CurrentNetworkList, payload);
        MsgCurrentNetworkList netList {};
        ASSERT_TRUE(SightlineNetworkParser::parseNetworkList(netListPkt, netList));
        EXPECT_EQ(netList.numInterfaces, 2U);
        ASSERT_EQ(netList.interfaceNames.size(), 2U);
        EXPECT_EQ(netList.interfaceNames[0], "eth0");
        EXPECT_EQ(netList.interfaceNames[1], "wlan0");

        // Facade equivalence
        MsgCurrentNetworkList facadeNetList {};
        ASSERT_TRUE(SightlineProtocolParser::parseNetworkList(netListPkt, facadeNetList));
        EXPECT_EQ(facadeNetList.numInterfaces, 2U);
    }

    /// @brief Verify I2C master transaction (0x94) builder and parser.
    TEST(TestSightlinePeripherals, BuildAndParseI2CCommand)
    {
        MsgI2CCommand i2cIn {};
        i2cIn.busIndex = 1U;
        i2cIn.deviceAddress = 0x48U;
        i2cIn.subAddress = 0x02U;
        i2cIn.writeLength = 3U;
        i2cIn.readLength = 2U;
        i2cIn.data = { 0x11U, 0x22U, 0x33U };

        const auto pkt = SightlineSerialBuilder::buildI2CCommand(i2cIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::I2CCommand);

        // Facade builder equivalence
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildI2CCommand(i2cIn));

        MsgI2CCommand i2cOut {};
        ASSERT_TRUE(SightlineSerialParser::parseI2CCommand(pkt, i2cOut));
        EXPECT_EQ(i2cOut.busIndex, 1U);
        EXPECT_EQ(i2cOut.deviceAddress, 0x48U);
        EXPECT_EQ(i2cOut.subAddress, 0x02U);
        EXPECT_EQ(i2cOut.writeLength, 3U);
        EXPECT_EQ(i2cOut.readLength, 2U);
        ASSERT_EQ(i2cOut.data.size(), 3U);
        EXPECT_EQ(i2cOut.data[0], 0x11U);
        EXPECT_EQ(i2cOut.data[1], 0x22U);
        EXPECT_EQ(i2cOut.data[2], 0x33U);

        // Facade parser equivalence
        MsgI2CCommand facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseI2CCommand(pkt, facadeOut));
        EXPECT_EQ(facadeOut.busIndex, 1U);
        EXPECT_EQ(facadeOut.deviceAddress, 0x48U);
        EXPECT_EQ(facadeOut.data.size(), 3U);
    }

    /// @brief Verify Phase 5 hardware decoder parameters, landing facade, and BTS downlink forwarder.
    TEST(TestSightlinePeripherals, BuildAndParsePhase5Peripherals)
    {
        // 1. Decoder Parameters (0x99)
        MsgDecoderParameters decIn {};
        decIn.decoderIndex = 1U;
        decIn.enable = 1U;
        decIn.codec = 1U; // H.265
        decIn.networkPort = 15008U;
        decIn.bufferDepthMs = 150U;
        decIn.multicastIp = 0xE0000101U; // 224.0.1.1

        const auto decPkt = SightlineCompressionBuilder::buildSetDecoderParameters(decIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(decPkt), MessageId::DecoderParameters);
        EXPECT_EQ(decPkt, SightlineProtocolBuilder::buildSetDecoderParameters(decIn));

        const auto getDecPkt = SightlineCompressionBuilder::buildGetDecoderParameters(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getDecPkt), MessageId::GetParameters);
        EXPECT_EQ(getDecPkt, SightlineProtocolBuilder::buildGetDecoderParameters(1U));

        MsgDecoderParameters decOut {};
        ASSERT_TRUE(SightlineCompressionParser::parseDecoderParameters(decPkt, decOut));
        EXPECT_EQ(decOut.decoderIndex, 1U);
        EXPECT_EQ(decOut.enable, 1U);
        EXPECT_EQ(decOut.codec, 1U);
        EXPECT_EQ(decOut.networkPort, 15008U);
        EXPECT_EQ(decOut.bufferDepthMs, 150U);
        EXPECT_EQ(decOut.multicastIp, 0xE0000101U);

        MsgDecoderParameters facadeDecOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseDecoderParameters(decPkt, facadeDecOut));
        EXPECT_EQ(facadeDecOut.networkPort, 15008U);

        // 2. SendToBTS (0xBE)
        MsgSendToBTS btsIn {};
        btsIn.btsPort = 2U;
        btsIn.data = { 0xDEU, 0xADU, 0xBEU, 0xEFU, 0x01U, 0x02U };

        const auto btsPkt = SightlineSerialBuilder::buildSendToBTS(btsIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(btsPkt), MessageId::SendToBTS);
        EXPECT_EQ(btsPkt, SightlineProtocolBuilder::buildSendToBTS(btsIn));

        MsgSendToBTS btsOut {};
        ASSERT_TRUE(SightlineSerialParser::parseSendToBTS(btsPkt, btsOut));
        EXPECT_EQ(btsOut.btsPort, 2U);
        ASSERT_EQ(btsOut.data.size(), 6U);
        EXPECT_EQ(btsOut.data[0], 0xDEU);
        EXPECT_EQ(btsOut.data[3], 0xEFU);

        MsgSendToBTS facadeBtsOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseSendToBTS(btsPkt, facadeBtsOut));
        EXPECT_EQ(facadeBtsOut.btsPort, 2U);
        EXPECT_EQ(facadeBtsOut.data.size(), 6U);

        // 3. Landing Aid Facades (0x81 / 0x83)
        MsgLandingAid aidIn {};
        aidIn.cameraIndex = 2U;
        aidIn.mode = 1U;
        aidIn.patternType = 0U;

        const auto aidPkt = SightlineProtocolBuilder::buildLandingAid(aidIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(aidPkt), MessageId::LandingAid);

        MsgLandingAid aidOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseLandingAid(aidPkt, aidOut));
        EXPECT_EQ(aidOut.cameraIndex, 2U);

        MsgLandingPosition posIn {};
        posIn.cameraIndex = 1U;
        posIn.relativeX = 5.0;
        posIn.relativeY = -2.0;
        posIn.relativeZ = 20.0;
        posIn.yawDeg = 15.0;
        posIn.pitchDeg = -1.0;
        posIn.rollDeg = 0.0;
        posIn.confidence = 90U;

        const auto posPkt = SightlineProtocolBuilder::buildLandingPosition(posIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(posPkt), MessageId::LandingPosition);

        MsgLandingPosition posOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseLandingPosition(posPkt, posOut));
        EXPECT_EQ(posOut.cameraIndex, 1U);
        EXPECT_DOUBLE_EQ(posOut.relativeX, 5.0);
        EXPECT_EQ(posOut.confidence, 90U);
    }

} // namespace
} // namespace Sightline
