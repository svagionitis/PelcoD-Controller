/// @file TestSonyViscaCommands.cpp
/// @brief Unit tests for SonyViscaBuilder and SonyViscaParser encoding and decoding.

#include "SonyViscaBuilder.h"
#include "SonyViscaParser.h"
#include <gtest/gtest.h>

using namespace Visca;
using namespace Visca::Sony;

/// @brief Tests Sony FCB lens zoom and focus command construction.
TEST(TestSonyViscaCommands, LensCommands)
{
    // Zoom Stop: 81 01 04 07 00 FF
    EXPECT_EQ(SonyViscaBuilder::zoomStop(1).bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x07, 0x00, 0xFF }));

    // Zoom Direct (0x2000): 81 01 04 47 02 00 00 00 FF
    EXPECT_EQ(SonyViscaBuilder::zoomDirect(1, 0x2000).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x47, 0x02, 0x00, 0x00, 0x00, 0xFF }));

    // Zoom Tele Variable (speed 5): 81 01 04 07 25 FF
    EXPECT_EQ(SonyViscaBuilder::zoomTeleVariable(1, 5).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x07, 0x25, 0xFF }));

    // Focus Auto: 81 01 04 38 02 FF
    EXPECT_EQ(
        SonyViscaBuilder::focusAuto(1, true).bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x38, 0x02, 0xFF }));

    // Focus Direct (0x5432): 81 01 04 48 05 04 03 02 FF
    EXPECT_EQ(SonyViscaBuilder::focusDirect(1, 0x5432).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x48, 0x05, 0x04, 0x03, 0x02, 0xFF }));
}

/// @brief Tests Sony FCB exposure and white balance command construction.
TEST(TestSonyViscaCommands, ExposureAndWhiteBalance)
{
    EXPECT_EQ(SonyViscaBuilder::exposureMode(1, SonyExposureMode::FullAuto).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x39, 0x00, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::shutterDirect(1, 0x15).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x4A, 0x00, 0x00, 0x01, 0x05, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::wbMode(1, SonyWhiteBalanceMode::ATW).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x35, 0x04, 0xFF }));
}

/// @brief Tests Sony FCB image enhancements, registers, and Block Inquiry generation.
TEST(TestSonyViscaCommands, EnhancementsAndBlockInquiries)
{
    EXPECT_EQ(SonyViscaBuilder::stabilizer(1, SonyStabilizerMode::SuperPlus).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x34, 0x05, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::defog(1, SonyDefogMode::High).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x37, 0x03, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::writeRegister(1, 0x57, 0x01).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x24, 0x57, 0x00, 0x01, 0xFF }));

    EXPECT_EQ(
        SonyViscaBuilder::blockInquiry(1, 0).bytes(), (std::vector<uint8_t> { 0x81, 0x09, 0x7E, 0x7E, 0x00, 0xFF }));
}

/// @brief Tests decoding of Block Inquiry 00 (Lens Control).
TEST(TestSonyViscaCommands, Block00Decode)
{
    SonyFCBStatus status {};
    const ViscaFrame block00 { 0x90, 0x50, 0x01, 0x02, 0x04, 0x06, 0x08, 0x03, 0x05, 0x07, 0x09, 0x01, 0x00, 0x00, 0x00,
        0xFF };

    EXPECT_TRUE(SonyViscaParser::parseBlock00(block00, status));
    EXPECT_TRUE(status.focusAuto);
    EXPECT_EQ(status.zoomPosition, 0x2468);
    EXPECT_EQ(status.focusPosition, 0x3579);
    EXPECT_EQ(status.focusNearLimit, 0x1000);
}

/// @brief Tests decoding of Block Inquiry 01 (Camera Control).
TEST(TestSonyViscaCommands, Block01Decode)
{
    SonyFCBStatus status {};
    const ViscaFrame block01 { 0x90, 0x50, 0x00,
        0x02, // WB Outdoor
        0x05, // Aperture
        0x03, // Exposure Manual
        0x12, // Shutter
        0x0E, // Iris
        0x08, // Gain
        0x07, // Exp comp pos
        0x00, 0x01, 0x05, // RGain: 0x15
        0x02, 0x0A, // BGain: 0x2A
        0xFF };

    EXPECT_TRUE(SonyViscaParser::parseBlock01(block01, status));
    EXPECT_EQ(status.exposureMode, SonyExposureMode::Manual);
    EXPECT_EQ(status.wbMode, SonyWhiteBalanceMode::Outdoor);
    EXPECT_EQ(status.shutterPosition, 0x12);
    EXPECT_EQ(status.irisPosition, 0x0E);
    EXPECT_EQ(status.gainPosition, 0x08);
}

/// @brief Tests Sony FCB aperture and sharpness commands.
TEST(TestSonyViscaCommands, ApertureCommands)
{
    EXPECT_EQ(SonyViscaBuilder::apertureReset(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x02, 0x00, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::apertureUp(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x02, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::apertureDown(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x02, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::apertureDirect(1, 0x0A).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x42, 0x00, 0x00, 0x00, 0x0A, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::apertureInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x42, 0xFF }));
}

/// @brief Tests Sony FCB backlight, slow shutter, and high sensitivity controls.
TEST(TestSonyViscaCommands, BacklightAndSensitivity)
{
    EXPECT_EQ(SonyViscaBuilder::backlight(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x33, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::backlight(1, false).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x33, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::backlightInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x33, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::autoSlowShutter(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x5A, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::autoSlowShutterInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x5A, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::highSensitivity(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x5E, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::highSensitivityInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x5E, 0xFF }));
}

/// @brief Tests Sony FCB 2D and 3D noise reduction commands.
TEST(TestSonyViscaCommands, NoiseReductionCommands)
{
    EXPECT_EQ(SonyViscaBuilder::noiseReduction2D(1, 3).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x53, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::noiseReduction2DInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x53, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::noiseReduction3D(1, 4).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x54, 0x04, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::noiseReduction3DInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x54, 0xFF }));
}

/// @brief Tests Sony FCB Wide-D, freeze, picture flip, and LR reverse commands.
TEST(TestSonyViscaCommands, EffectsAndFlip)
{
    EXPECT_EQ(SonyViscaBuilder::wideD(1, SonyWideDMode::WideD).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3D, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::wideD(1, SonyWideDMode::Off).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3D, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::wideD(1, SonyWideDMode::VisibilityEnhancer).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3D, 0x06, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::wideDInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x3D, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::freeze(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x62, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::freeze(1, false).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x62, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::freezeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x62, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::pictureFlip(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x66, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::pictureFlipInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x66, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::lrReverse(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x61, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::lrReverseInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x61, 0xFF }));
}

/// @brief Tests Sony FCB color gain, color hue, chroma suppress, and gamma controls.
TEST(TestSonyViscaCommands, ColorAndGamma)
{
    EXPECT_EQ(SonyViscaBuilder::colorGain(1, 8).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x49, 0x00, 0x00, 0x00, 0x08, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::colorGainInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x49, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::colorHue(1, 6).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x4F, 0x00, 0x00, 0x00, 0x06, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::colorHueInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x4F, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::chromaSuppress(1, 2).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x5F, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::chromaSuppressInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x5F, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::gamma(1, 1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x5B, 0x01, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::gammaInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x5B, 0xFF }));
}

/// @brief Tests Sony FCB preset memory commands (Set, Recall, Reset, Inq).
TEST(TestSonyViscaCommands, PresetMemoryCommands)
{
    EXPECT_EQ(SonyViscaBuilder::memorySet(1, 3).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3F, 0x01, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::memoryRecall(1, 5).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3F, 0x02, 0x05, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::memoryReset(1, 2).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x3F, 0x00, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::memoryInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x3F, 0xFF }));
}

/// @brief Tests Sony FCB Spot AE, Spot Focus, and Spot AWB coordinate controls.
TEST(TestSonyViscaCommands, SpotControls)
{
    EXPECT_EQ(SonyViscaBuilder::spotAe(1, true).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x59, 0x02, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::spotAe(1, false).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x59, 0x03, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::spotAeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x59, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::spotAePosition(1, 7, 9).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x29, 0x00, 0x07, 0x00, 0x09, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::spotAePositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x29, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::spotFocusPosition(1, 4, 12).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x2A, 0x00, 0x04, 0x00, 0x0C, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::spotFocusPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x2A, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::spotAwbPosition(1, 8, 8).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x2B, 0x00, 0x08, 0x00, 0x08, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::spotAwbPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x2B, 0xFF }));
}

/// @brief Tests Sony FCB individual parameter inquiry frame generation.
TEST(TestSonyViscaCommands, IndividualInquiries)
{
    EXPECT_EQ(SonyViscaBuilder::zoomPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x47, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::dzoomModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x06, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::focusPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x48, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::focusModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x38, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::focusNearLimitInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x28, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::exposureModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x39, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::shutterPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x4A, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::irisPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x4B, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::gainPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x4C, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::exposureCompModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x3E, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::exposureCompPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x4E, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::wbModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x35, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::rGainInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x43, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::bGainInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x44, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::stabilizerModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x34, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::defogModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x37, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::icrModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x01, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::autoIcrModeInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x51, 0xFF }));
    EXPECT_EQ(SonyViscaBuilder::cameraIdInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x04, 0x22, 0xFF }));
}

/// @brief Tests decoding of individual inquiry responses (words, bytes, booleans, modes, spots).
TEST(TestSonyViscaCommands, InquiryParsers)
{
    // Word inquiry (16-bit)
    uint16_t wordVal { 0 };
    const ViscaFrame validWord { 0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseWordInquiry(validWord, wordVal));
    EXPECT_EQ(wordVal, 0x1234);

    const ViscaFrame invalidWordLen { 0x90, 0x50, 0x01, 0x02, 0xFF };
    EXPECT_FALSE(SonyViscaParser::parseWordInquiry(invalidWordLen, wordVal));

    const ViscaFrame invalidWordHdr { 0x90, 0x40, 0x01, 0x02, 0x03, 0x04, 0xFF };
    EXPECT_FALSE(SonyViscaParser::parseWordInquiry(invalidWordHdr, wordVal));

    // Byte inquiry (8-bit)
    uint8_t byteVal { 0 };
    const ViscaFrame validByte7 { 0x90, 0x50, 0x00, 0x00, 0x01, 0x0A, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseByteInquiry(validByte7, byteVal));
    EXPECT_EQ(byteVal, 0x1A);

    const ViscaFrame validByte5 { 0x90, 0x50, 0x02, 0x0B, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseByteInquiry(validByte5, byteVal));
    EXPECT_EQ(byteVal, 0x2B);

    const ViscaFrame invalidByte { 0x90, 0x50, 0x01, 0xFF };
    EXPECT_FALSE(SonyViscaParser::parseByteInquiry(invalidByte, byteVal));

    // Bool inquiry (0x02 = On/True, 0x03 = Off/False)
    bool boolVal { false };
    const ViscaFrame boolOn { 0x90, 0x50, 0x02, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseBoolInquiry(boolOn, boolVal));
    EXPECT_TRUE(boolVal);

    const ViscaFrame boolOff { 0x90, 0x50, 0x03, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseBoolInquiry(boolOff, boolVal));
    EXPECT_FALSE(boolVal);

    const ViscaFrame boolInvalid { 0x90, 0x50, 0x00, 0xFF };
    EXPECT_FALSE(SonyViscaParser::parseBoolInquiry(boolInvalid, boolVal));

    // Mode inquiry (0x0p)
    uint8_t modeVal { 0 };
    const ViscaFrame modeFrame { 0x90, 0x50, 0x04, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseModeInquiry(modeFrame, modeVal));
    EXPECT_EQ(modeVal, 0x04);

    // Spot position inquiry (X, Y)
    uint8_t spotX { 0 };
    uint8_t spotY { 0 };
    const ViscaFrame spotFrame { 0x90, 0x50, 0x00, 0x07, 0x00, 0x0C, 0xFF };
    EXPECT_TRUE(SonyViscaParser::parseSpotPosition(spotFrame, spotX, spotY));
    EXPECT_EQ(spotX, 7);
    EXPECT_EQ(spotY, 12);

    const ViscaFrame invalidSpot { 0x90, 0x50, 0x00, 0x07, 0xFF };
    EXPECT_FALSE(SonyViscaParser::parseSpotPosition(invalidSpot, spotX, spotY));
}

