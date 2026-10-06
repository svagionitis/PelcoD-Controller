/// @file TestSightlineBlendCatalog.cpp
/// @brief Unit tests for SightlineBlendCatalog, the C++ source of blend-mode metadata exposed to QML.
/// @details Verifies that the catalog stays in lock-step with Sightline::BlendMode (IDD 3.11 / EAN-Blending
///          Table 3), never exposes the reserved value 5, and formats operator-facing labels correctly.

#include "SightlineBlendCatalog.h"

#include <SightlineCore/modules/SightlineBlending.h>

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <gtest/gtest.h>

#include <cstdint>

namespace {

/// @brief Converts a BlendMode enumerator into its wire integer.
/// @param[in] mode Blend mode enumerator.
/// @return Integer wire value.
[[nodiscard]] int wire(Sightline::BlendMode mode) noexcept
{
    return static_cast<int>(static_cast<std::uint8_t>(mode));
}

TEST(SightlineBlendCatalogTest, ModeListMatchesCoreEnum)
{
    const SightlineBlendCatalog catalog {};
    const QVariantList modes = catalog.modes();
    ASSERT_EQ(modes.size(), 12);
    EXPECT_EQ(catalog.modeNames().size(), 12);

    const int expected[] { 0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12 };
    for (int i { 0 }; i < 12; ++i) {
        const QVariantMap entry = modes.at(i).toMap();
        EXPECT_EQ(entry.value(QStringLiteral("value")).toInt(), expected[i]);
        EXPECT_FALSE(entry.value(QStringLiteral("name")).toString().isEmpty());
        EXPECT_EQ(entry.value(QStringLiteral("name")).toString(), catalog.modeNames().at(i));
    }
    EXPECT_EQ(modes.at(11).toMap().value(QStringLiteral("value")).toInt(), wire(Sightline::BlendMode::ColorEoBlendEo));
}

TEST(SightlineBlendCatalogTest, ReservedModeIsRejected)
{
    EXPECT_FALSE(SightlineBlendCatalog::isValidMode(5));
    EXPECT_FALSE(SightlineBlendCatalog::isValidMode(-1));
    EXPECT_FALSE(SightlineBlendCatalog::isValidMode(13));
    EXPECT_TRUE(SightlineBlendCatalog::isValidMode(0));
    EXPECT_TRUE(SightlineBlendCatalog::isValidMode(12));

    const SightlineBlendCatalog catalog {};
    EXPECT_EQ(catalog.modeName(5), QStringLiteral("Unknown (5)"));
}

TEST(SightlineBlendCatalogTest, IndexAndValueRoundTrip)
{
    const SightlineBlendCatalog catalog {};
    for (int i { 0 }; i < 12; ++i) {
        EXPECT_EQ(catalog.indexOfMode(catalog.modeAt(i)), i);
    }
    EXPECT_EQ(catalog.indexOfMode(6), 5);
    // Out-of-range falls back to Frame Blend (Warped EO), the struct default.
    EXPECT_EQ(catalog.indexOfMode(5), 1);
    EXPECT_EQ(catalog.modeAt(-1), wire(Sightline::BlendMode::FrameBlendWarpEo));
    EXPECT_EQ(catalog.modeAt(99), wire(Sightline::BlendMode::FrameBlendWarpEo));
}

TEST(SightlineBlendCatalogTest, ModeCapabilities)
{
    const SightlineBlendCatalog catalog {};
    // Night/Color/Color-IR blends use hue.
    EXPECT_TRUE(catalog.usesHue(3));
    EXPECT_TRUE(catalog.usesHue(11));
    EXPECT_FALSE(catalog.usesHue(1));
    EXPECT_FALSE(catalog.usesHue(12));
    // Hue-for-colour flag only matters in Color / Color IR blends.
    EXPECT_TRUE(catalog.usesHueFlag(4));
    EXPECT_FALSE(catalog.usesHueFlag(3));
    // Thermal window: thermal false-colour and Color IR blends.
    EXPECT_TRUE(catalog.usesThermal(2));
    EXPECT_TRUE(catalog.usesThermal(10));
    EXPECT_FALSE(catalog.usesThermal(4));
    // Histogram equalisation: thermal blends only.
    EXPECT_TRUE(catalog.usesHistEq(7));
    EXPECT_FALSE(catalog.usesHistEq(10));
    // User palette: Color IR blends only.
    EXPECT_TRUE(catalog.usesPalette(10));
    EXPECT_TRUE(catalog.usesPalette(11));
    EXPECT_FALSE(catalog.usesPalette(0));
    // "No Change" keeps every tuning control available, but not the palette.
    EXPECT_TRUE(catalog.usesHue(0));
    EXPECT_TRUE(catalog.usesHueFlag(0));
    EXPECT_TRUE(catalog.usesThermal(0));
    EXPECT_TRUE(catalog.usesHistEq(0));
    // Invalid modes expose nothing.
    EXPECT_FALSE(catalog.usesHue(5));
    EXPECT_FALSE(catalog.usesThermal(42));
}

TEST(SightlineBlendCatalogTest, MixLabel)
{
    const SightlineBlendCatalog catalog {};
    EXPECT_EQ(catalog.mixLabel(1, 180), QString::fromUtf8("71% EO \xC2\xB7 29% IR"));
    EXPECT_EQ(catalog.mixLabel(1, 255), QString::fromUtf8("100% EO \xC2\xB7 0% IR"));
    EXPECT_EQ(catalog.mixLabel(12, 0), QString::fromUtf8("0% Warp \xC2\xB7 100% Fixed"));
    // Out-of-range amounts are clamped to 0..255.
    EXPECT_EQ(catalog.mixLabel(1, 999), QString::fromUtf8("100% EO \xC2\xB7 0% IR"));
}

TEST(SightlineBlendCatalogTest, RotationDegrees)
{
    const SightlineBlendCatalog catalog {};
    EXPECT_DOUBLE_EQ(catalog.rotationDeg(0), 0.0);
    EXPECT_DOUBLE_EQ(catalog.rotationDeg(1), -5.0);
    EXPECT_DOUBLE_EQ(catalog.rotationDeg(255), 5.0);
    EXPECT_NEAR(catalog.rotationDeg(128), 0.0, 0.02);
    EXPECT_DOUBLE_EQ(catalog.rotationDeg(-3), 0.0);
    EXPECT_DOUBLE_EQ(catalog.rotationDeg(400), 5.0);
}

TEST(SightlineBlendCatalogTest, PresetsAndFlags)
{
    const SightlineBlendCatalog catalog {};
    const QVariantList values = catalog.presetValues();
    ASSERT_EQ(values.size(), 10);
    EXPECT_EQ(values.at(0).toInt(), 0);
    EXPECT_EQ(values.at(4).toInt(), 4);
    EXPECT_EQ(values.at(5).toInt(), 10);
    EXPECT_EQ(values.at(9).toInt(), 14);
    EXPECT_EQ(catalog.presetNames().size(), 10);

    EXPECT_EQ(catalog.presetLabel(2), QStringLiteral("Align Slot 2 (0xB9)"));
    EXPECT_EQ(catalog.presetLabel(11), QStringLiteral("4-Point Slot 1 (0x95)"));
    EXPECT_EQ(catalog.presetNames().at(5), QStringLiteral("4-Point Slot 0 (0x95)"));

    EXPECT_TRUE(SightlineBlendCatalog::isValidPreset(14));
    EXPECT_FALSE(SightlineBlendCatalog::isValidPreset(5));
    EXPECT_FALSE(SightlineBlendCatalog::isValidPreset(15));

    EXPECT_EQ(catalog.flagsLabel(0), QStringLiteral("None"));
    EXPECT_EQ(catalog.flagsLabel(1), QStringLiteral("HistEq"));
    EXPECT_EQ(catalog.flagsLabel(2), QStringLiteral("UseHue"));
    EXPECT_EQ(catalog.flagsLabel(3), QStringLiteral("HistEq + UseHue"));
}

} // namespace
