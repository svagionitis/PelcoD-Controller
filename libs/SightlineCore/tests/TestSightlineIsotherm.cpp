/// @file TestSightlineIsotherm.cpp
/// @brief Unit tests for Isotherm Color LUT Builder, BT.601 conversions, and AGC limits.

#include "modules/SightlineIsothermBuilder.h"
#include "modules/SightlineNucParser.h"
#include "SightlineDevice.h"
#include "SightlineFraming.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace Sightline;

class TestSightlineIsotherm : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestSightlineIsotherm, ColorConversions)
{
    // Test pure primary colors through BT.601 transformation
    const RgbColor black { 0U, 0U, 0U };
    const YuvColor yuvBlack { SightlineIsothermBuilder::toYuv(black) };
    EXPECT_EQ(yuvBlack.y, 0U);
    EXPECT_EQ(yuvBlack.u, 128U);
    EXPECT_EQ(yuvBlack.v, 128U);

    const RgbColor backBlack { SightlineIsothermBuilder::toRgb(yuvBlack) };
    EXPECT_EQ(backBlack.r, 0U);
    EXPECT_EQ(backBlack.g, 0U);
    EXPECT_EQ(backBlack.b, 0U);

    const RgbColor white { 255U, 255U, 255U };
    const YuvColor yuvWhite { SightlineIsothermBuilder::toYuv(white) };
    EXPECT_EQ(yuvWhite.y, 255U);
    EXPECT_EQ(yuvWhite.u, 128U);
    EXPECT_EQ(yuvWhite.v, 128U);

    const RgbColor backWhite { SightlineIsothermBuilder::toRgb(yuvWhite) };
    EXPECT_EQ(backWhite.r, 255U);
    EXPECT_EQ(backWhite.g, 255U);
    EXPECT_EQ(backWhite.b, 255U);

    // Green: Y = 0.587 * 255 = 150, U = 128 - 74 = 54, V = 128 - 131 = 0
    const RgbColor green { 0U, 255U, 0U };
    const YuvColor yuvGreen { SightlineIsothermBuilder::toYuv(green) };
    EXPECT_NEAR(static_cast<int>(yuvGreen.y), 150, 1);
    EXPECT_NEAR(static_cast<int>(yuvGreen.u), 54, 1);
    EXPECT_NEAR(static_cast<int>(yuvGreen.v), 0, 1);

    const RgbColor backGreen { SightlineIsothermBuilder::toRgb(yuvGreen) };
    EXPECT_NEAR(static_cast<int>(backGreen.r), 0, 5);
    EXPECT_NEAR(static_cast<int>(backGreen.g), 255, 5);
    EXPECT_NEAR(static_cast<int>(backGreen.b), 0, 5);
}

TEST_F(TestSightlineIsotherm, BasePaletteGeneration)
{
    SightlineIsothermBuilder builder {};

    // 1. Grayscale White Hot
    builder.setBasePalette(BasePaletteType::GrayscaleWhiteHot);
    const auto whiteHot = builder.buildLut();
    EXPECT_EQ(whiteHot.rgb[0U], (RgbColor { 0U, 0U, 0U }));
    EXPECT_EQ(whiteHot.rgb[255U], (RgbColor { 255U, 255U, 255U }));
    EXPECT_EQ(whiteHot.rgb[128U], (RgbColor { 128U, 128U, 128U }));

    // 2. Grayscale Black Hot
    builder.setBasePalette(BasePaletteType::GrayscaleBlackHot);
    const auto blackHot = builder.buildLut();
    EXPECT_EQ(blackHot.rgb[0U], (RgbColor { 255U, 255U, 255U }));
    EXPECT_EQ(blackHot.rgb[255U], (RgbColor { 0U, 0U, 0U }));
    EXPECT_EQ(blackHot.rgb[128U], (RgbColor { 127U, 127U, 127U }));

    // 3. Ironbow
    builder.setBasePalette(BasePaletteType::Ironbow);
    const auto ironbow = builder.buildLut();
    EXPECT_EQ(ironbow.rgb[0U], (RgbColor { 0U, 0U, 0U }));
    EXPECT_EQ(ironbow.rgb[255U], (RgbColor { 255U, 255U, 255U }));
    // Midpoint should have warm red/purple hue
    EXPECT_GT(ironbow.rgb[128U].r, 150U);

    // 4. Rainbow
    builder.setBasePalette(BasePaletteType::Rainbow);
    const auto rainbow = builder.buildLut();
    EXPECT_GT(rainbow.rgb[0U].b, 150U); // Blue at min
    EXPECT_GT(rainbow.rgb[255U].r, 200U); // Red at max
}

TEST_F(TestSightlineIsotherm, EanInfraredTemperatureExample)
{
    // Replicates exact scenario from EAN-Infrared-Temperature.pdf Section 8.1:
    // Scene AGC range: 80°F to 400°F
    // Human body (80°F - 100°F) -> mapped to index 0..16 -> Green
    // Electrical wires (150°F - 200°F) -> mapped to index 56..96 -> Yellow
    // Fire / hotspot (> 400°F) -> index 255 -> Red
    SightlineIsothermBuilder builder {};
    builder.setBasePalette(BasePaletteType::GrayscaleWhiteHot)
           .setAgcRange(80.0F, 400.0F, TemperatureScale::Fahrenheit);

    // Verify index mapping calculations from EAN
    EXPECT_EQ(builder.mapTempToIndex(80.0F, TemperatureScale::Fahrenheit), 0U);
    EXPECT_EQ(builder.mapTempToIndex(100.0F, TemperatureScale::Fahrenheit), 16U);
    EXPECT_EQ(builder.mapTempToIndex(150.0F, TemperatureScale::Fahrenheit), 56U);
    EXPECT_EQ(builder.mapTempToIndex(200.0F, TemperatureScale::Fahrenheit), 96U);
    EXPECT_EQ(builder.mapTempToIndex(400.0F, TemperatureScale::Fahrenheit), 255U);

    // Add isotherm bands using physical temperature calls
    builder.addBandTemp(80.0F, 100.0F, TemperatureScale::Fahrenheit, RgbColor::green())
           .addBandTemp(150.0F, 200.0F, TemperatureScale::Fahrenheit, RgbColor::yellow())
           .addBandIndex(255U, 255U, RgbColor::red());

    const auto lut = builder.buildLut();

    // Verify human body interval is solid green
    for (std::uint8_t i = 0U; i <= 16U; ++i) {
        EXPECT_EQ(lut.rgb[i], RgbColor::green()) << "Index " << static_cast<int>(i) << " should be green";
    }

    // Verify interval between 17 and 55 is undisturbed white-hot grayscale
    EXPECT_EQ(lut.rgb[30U], (RgbColor { 30U, 30U, 30U }));

    // Verify electrical wires interval is solid yellow
    for (std::uint8_t i = 56U; i <= 96U; ++i) {
        EXPECT_EQ(lut.rgb[i], RgbColor::yellow()) << "Index " << static_cast<int>(i) << " should be yellow";
    }

    // Verify hotspot at index 255 is red
    EXPECT_EQ(lut.rgb[255U], RgbColor::red());

    // Verify AGC configuration aligns with FLIR Tau 2 TLinear high-res sensor (0.04 K/count)
    // 80°F = 299.82 K -> ~7500 counts (FLIR spec 300K = 7500 counts)
    // 400°F = 477.59 K -> ~11940 counts (FLIR spec 478K = 11950 counts)
    const auto agc = builder.buildAgcConfig(RadiometricSensor::FlirTau2HighRes, 0U);
    EXPECT_EQ(agc.mode, AutoGainMode::Manual);
    EXPECT_EQ(agc.cameraIndex, 0U);
    EXPECT_NEAR(agc.agHoldmin, 7500U, 5U);
    EXPECT_NEAR(agc.agHoldmax, 11950U, 15U);
    EXPECT_EQ(agc.midpoint, 128U);
}

TEST_F(TestSightlineIsotherm, GradientAndAlphaBlend)
{
    SightlineIsothermBuilder builder {};
    builder.setBasePalette(BasePaletteType::GrayscaleWhiteHot);

    // Add gradient from Blue to Red across 100..200
    builder.addGradientIndex(100U, 200U, RgbColor::blue(), RgbColor::red());
    const auto gradLut = builder.buildLut();

    EXPECT_EQ(gradLut.rgb[100U], RgbColor::blue());
    EXPECT_EQ(gradLut.rgb[200U], RgbColor::red());
    // Midpoint 150 should be equal mix (purple)
    EXPECT_NEAR(static_cast<int>(gradLut.rgb[150U].r), 128, 2);
    EXPECT_NEAR(static_cast<int>(gradLut.rgb[150U].b), 128, 2);
    EXPECT_EQ(gradLut.rgb[150U].g, 0U);

    // Alpha blend: 50% red over white base at index 255
    builder.clearBands();
    builder.addBandIndex(255U, 255U, RgbColor::red(), IsothermBlendMode::Blend, 0.5F);
    const auto blendLut = builder.buildLut();

    // Base was (255, 255, 255), blended with red (255, 0, 0) at 50% -> (255, 128, 128)
    EXPECT_EQ(blendLut.rgb[255U].r, 255U);
    EXPECT_NEAR(static_cast<int>(blendLut.rgb[255U].g), 128, 2);
    EXPECT_NEAR(static_cast<int>(blendLut.rgb[255U].b), 128, 2);
}

TEST_F(TestSightlineIsotherm, SerializationAndPacket)
{
    SightlineIsothermBuilder builder {};
    builder.setBasePalette(BasePaletteType::GrayscaleWhiteHot);
    builder.addBandIndex(10U, 20U, RgbColor::red());

    const auto lut = builder.buildLut();
    const auto yuvBytes = lut.toYuvBytes();
    const auto rgbBytes = lut.toRgbBytes();

    EXPECT_EQ(yuvBytes.size(), 768U);
    EXPECT_EQ(rgbBytes.size(), 768U);

    // Packet generation
    const auto pkt = builder.buildPacket(0U);
    EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetUserPalette);

    MsgUserPalette parsed {};
    ASSERT_TRUE(SightlineNucParser::parseUserPalette(pkt, parsed));
    EXPECT_EQ(parsed.paletteIndex, 0U);
    EXPECT_EQ(parsed.lutData.size(), 768U);
    EXPECT_EQ(parsed.lutData, yuvBytes);
}

TEST_F(TestSightlineIsotherm, BinaryFilePersistence)
{
    SightlineIsothermBuilder builder {};
    builder.setBasePalette(BasePaletteType::Ironbow);
    builder.addBandIndex(50U, 80U, RgbColor::green());

    const auto originalLut = builder.buildLut();
    const std::string testFilePath { "test_isotherm_palette.bin" };

    // Save as YUV binary
    ASSERT_TRUE(originalLut.saveToFile(testFilePath, true));

    // Load back
    IsothermColorLut loadedLut {};
    ASSERT_TRUE(IsothermColorLut::loadFromFile(testFilePath, loadedLut, true));

    // Compare entries
    for (std::size_t i = 0U; i < 256U; ++i) {
        EXPECT_EQ(loadedLut.yuv[i], originalLut.yuv[i]);
        EXPECT_NEAR(static_cast<int>(loadedLut.rgb[i].r), static_cast<int>(originalLut.rgb[i].r), 5);
        EXPECT_NEAR(static_cast<int>(loadedLut.rgb[i].g), static_cast<int>(originalLut.rgb[i].g), 5);
        EXPECT_NEAR(static_cast<int>(loadedLut.rgb[i].b), static_cast<int>(originalLut.rgb[i].b), 5);
    }

    // Clean up temporary file
    std::remove(testFilePath.c_str());
}
