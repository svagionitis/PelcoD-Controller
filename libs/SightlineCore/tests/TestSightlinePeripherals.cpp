/// @file TestSightlinePeripherals.cpp
/// @brief Unit tests for Sightline serial/GPIO, autonomous landing aid, and thermal NUC builders and parsers.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineLandingBuilder.h"
#include "modules/SightlineLandingParser.h"
#include "modules/SightlineNetworkParser.h"
#include "modules/SightlineNucBuilder.h"
#include "modules/SightlineNucParser.h"
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

    /// @brief Verify thermal NUC calibration and dead pixel table serialization & deserialization.
    TEST(TestSightlinePeripherals, BuildAndParseNuc)
    {
        // 1. NUC Parameters (0x35)
        MsgNucParameters nucMsg {};
        nucMsg.cameraIndex = 1U;
        nucMsg.nucAction = 2U; // 2-point NUC
        nucMsg.shutterMode = 0U; // Manual

        const auto nucPkt = SightlineNucBuilder::buildNucParameters(nucMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(nucPkt), MessageId::NucParameters);

        MsgNucParameters nucOut {};
        ASSERT_TRUE(SightlineNucParser::parseNucParameters(nucPkt, nucOut));
        EXPECT_EQ(nucOut.cameraIndex, 1U);
        EXPECT_EQ(nucOut.nucAction, 2U);
        EXPECT_EQ(nucOut.shutterMode, 0U);

        // 2. Dead Pixel (0xA8)
        MsgDeadPixel dpMsg {};
        dpMsg.cameraIndex = 1U;
        dpMsg.mode = 1U; // Replacement enable
        dpMsg.deadPixelCount = 24U;

        const auto dpPkt = SightlineNucBuilder::buildDeadPixel(dpMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(dpPkt), MessageId::DeadPixel);

        MsgDeadPixel dpOut {};
        ASSERT_TRUE(SightlineNucParser::parseDeadPixel(dpPkt, dpOut));
        EXPECT_EQ(dpOut.cameraIndex, 1U);
        EXPECT_EQ(dpOut.mode, 1U);
        EXPECT_EQ(dpOut.deadPixelCount, 24U);

        // 3. Read/Write NUC Flash (0x36)
        MsgReadWriteNuc rwNucMsg {};
        rwNucMsg.cameraIndex = 2U;
        rwNucMsg.action = 1U; // Write to flash
        rwNucMsg.tableIndex = 3U;

        const auto rwNucPkt = SightlineNucBuilder::buildReadWriteNuc(rwNucMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(rwNucPkt), MessageId::ReadWriteNuc);

        MsgReadWriteNuc rwNucOut {};
        ASSERT_TRUE(SightlineNucParser::parseReadWriteNuc(rwNucPkt, rwNucOut));
        EXPECT_EQ(rwNucOut.cameraIndex, 2U);
        EXPECT_EQ(rwNucOut.action, 1U);
        EXPECT_EQ(rwNucOut.tableIndex, 3U);

        // 4. User Thermal Palette (0x72)
        MsgUserPalette palMsg {};
        palMsg.paletteIndex = 1U;
        palMsg.lutData = { 0x10U, 0x20U, 0x30U, 0x40U, 0x50U };

        const auto palPkt = SightlineNucBuilder::buildSetUserPalette(palMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(palPkt), MessageId::SetUserPalette);

        MsgUserPalette palOut {};
        ASSERT_TRUE(SightlineNucParser::parseUserPalette(palPkt, palOut));
        EXPECT_EQ(palOut.paletteIndex, 1U);
        EXPECT_EQ(palOut.lutData, palMsg.lutData);

        // 5. Dead Pixel Statistics (0xA1)
        std::vector<std::uint8_t> dpStatsPayload {};
        dpStatsPayload.push_back(0U); // cameraIndex 0
        SightlineFraming::appendU16Le(dpStatsPayload, 42U); // deadPixelCount
        SightlineFraming::appendU16Le(dpStatsPayload, 3U); // badColumns
        SightlineFraming::appendU16Le(dpStatsPayload, 1U); // badRows

        const auto dpStatsPkt = SightlineFraming::buildPacket(MessageId::DeadPixelStats, dpStatsPayload);
        MsgDeadPixelStats dpStatsOut {};
        ASSERT_TRUE(SightlineNucParser::parseDeadPixelStats(dpStatsPkt, dpStatsOut));
        EXPECT_EQ(dpStatsOut.cameraIndex, 0U);
        EXPECT_EQ(dpStatsOut.deadPixelCount, 42U);
        EXPECT_EQ(dpStatsOut.badColumns, 3U);
        EXPECT_EQ(dpStatsOut.badRows, 1U);

        // 6. Camera Calibration (0xC0)
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

        const auto calibPkt = SightlineNucBuilder::buildCameraCalibration(calibMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(calibPkt), MessageId::CameraCalibration);

        MsgCameraCalibration calibOut {};
        ASSERT_TRUE(SightlineNucParser::parseCameraCalibration(calibPkt, calibOut));
        EXPECT_EQ(calibOut.cameraIndex, 0U);
        EXPECT_FLOAT_EQ(calibOut.focalLengthX, 1000.5F);
        EXPECT_FLOAT_EQ(calibOut.focalLengthY, 1000.8F);
        EXPECT_FLOAT_EQ(calibOut.principalPointX, 640.0F);
        EXPECT_FLOAT_EQ(calibOut.principalPointY, 512.0F);
        EXPECT_FLOAT_EQ(calibOut.radialDistortionK1, -0.15F);
        EXPECT_FLOAT_EQ(calibOut.radialDistortionK2, 0.05F);
        EXPECT_FLOAT_EQ(calibOut.tangentialP1, 0.001F);
        EXPECT_FLOAT_EQ(calibOut.tangentialP2, -0.002F);

        // 7. Camera Parameter File (0xC2)
        MsgCameraParameterFile paramFileMsg {};
        paramFileMsg.cameraIndex = 1U;
        paramFileMsg.action = 0U; // Load
        paramFileMsg.filename = "boson640_calib.bin";

        const auto filePkt = SightlineNucBuilder::buildCameraParameterFile(paramFileMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(filePkt), MessageId::CameraParameterFile);

        MsgCameraParameterFile fileOut {};
        ASSERT_TRUE(SightlineNucParser::parseCameraParameterFile(filePkt, fileOut));
        EXPECT_EQ(fileOut.cameraIndex, 1U);
        EXPECT_EQ(fileOut.action, 0U);
        EXPECT_EQ(fileOut.filename, "boson640_calib.bin");
    }

    /// @brief Verify network list reply deserialization and dead pixel query builder.
    TEST(TestSightlinePeripherals, NetworkListAndDeadPixelQuery)
    {
        const auto getDpPkt = SightlineNucBuilder::buildGetDeadPixel(1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(getDpPkt), MessageId::GetParameters);
        EXPECT_EQ(getDpPkt, SightlineProtocolBuilder::buildGetDeadPixel(1U));

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

} // namespace
} // namespace Sightline
