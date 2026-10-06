/// @file TestSightlineBlendBridge.cpp
/// @brief Unit tests for the SightlineQmlBridge dual-sensor blending API (0x2F / 0x4D / 0xB9 / 0x74 / 0x75).
/// @details Exercises the pure packing/validation helpers and the telemetry-to-QML signal mapping without
///          requiring a live UDP connection to a Sightline board.

#include "SightlineQmlBridge.h"

#include <SightlineCore/modules/SightlineBlendingBuilder.h>
#include <SightlineCore/modules/SightlineBlendingParser.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QSignalSpy>
#include <QVariantMap>
#include <gtest/gtest.h>

#include <cstdint>

namespace {

/// @class SightlineBlendBridgeTest
/// @brief Fixture guaranteeing a QCoreApplication instance for QObject signal delivery.
class SightlineBlendBridgeTest : public ::testing::Test {
protected:
    /// @brief Ensures a QCoreApplication exists for the lifetime of the test binary.
    static void SetUpTestSuite()
    {
        if (QCoreApplication::instance() == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestSightlineBlendBridge" };
            static char* argv[] { appName, nullptr };
            static QCoreApplication app { argc, argv };
            static_cast<void>(app);
        }
    }
};

// ---------------------------------------------------------------------------------------------
// B2: blend mode validation
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, BlendModeValidationRejectsReserved)
{
    EXPECT_FALSE(SightlineQmlBridge::isValidBlendMode(-1));
    EXPECT_FALSE(SightlineQmlBridge::isValidBlendMode(5));
    EXPECT_FALSE(SightlineQmlBridge::isValidBlendMode(13));
    for (const int m : { 0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12 }) {
        EXPECT_TRUE(SightlineQmlBridge::isValidBlendMode(m)) << "mode " << m;
    }
}

TEST_F(SightlineBlendBridgeTest, PresetIndexValidation)
{
    for (const int i : { 0, 1, 2, 3, 4, 10, 11, 12, 13, 14 }) {
        EXPECT_TRUE(SightlineQmlBridge::isValidPresetIdx(i)) << "index " << i;
    }
    for (const int i : { -1, 5, 9, 15, 255 }) {
        EXPECT_FALSE(SightlineQmlBridge::isValidPresetIdx(i)) << "index " << i;
    }
}

TEST_F(SightlineBlendBridgeTest, PackBlendConfigRejectsReservedMode)
{
    Sightline::MsgSetBlendParameters out {};
    const QVariantMap cfg { { QStringLiteral("mode"), 5 } };
    EXPECT_FALSE(SightlineQmlBridge::packBlendConfig(cfg, out));
}

TEST_F(SightlineBlendBridgeTest, PackBlendConfigRejectsBadPresetIndex)
{
    Sightline::MsgSetBlendParameters out {};
    const QVariantMap cfg { { QStringLiteral("mode"), 1 }, { QStringLiteral("usePresetAlign"), true },
        { QStringLiteral("presetAlignIndex"), 7 } };
    EXPECT_FALSE(SightlineQmlBridge::packBlendConfig(cfg, out));
}

// ---------------------------------------------------------------------------------------------
// B3 / B4: safe defaults ("no change" warp, full thermal window)
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, PackBlendConfigSafeDefaults)
{
    Sightline::MsgSetBlendParameters out {};
    const QVariantMap cfg { { QStringLiteral("mode"), 1 } };
    ASSERT_TRUE(SightlineQmlBridge::packBlendConfig(cfg, out));

    EXPECT_EQ(out.mode, Sightline::BlendMode::FrameBlendWarpEo);
    EXPECT_EQ(out.coldEnd, 255U);
    EXPECT_EQ(out.hotStart, 0U);
    EXPECT_EQ(out.rotation, 0U);
    EXPECT_EQ(out.zoom, 0U);
    EXPECT_EQ(out.hzoom, 0U);
    EXPECT_EQ(out.vertical, 0);
    EXPECT_EQ(out.horizontal, 0);
    EXPECT_EQ(out.absOffZoom, 0U);
    EXPECT_EQ(out.reset, 0U);
    EXPECT_EQ(out.usePresetAlign, 0U);
    EXPECT_EQ(out.presetAlignIndex, 0U);
    EXPECT_EQ(out.warpIndex, 0U);
    EXPECT_EQ(out.fixedIndex, 1U);
    EXPECT_EQ(out.amt, 128U);
}

// ---------------------------------------------------------------------------------------------
// B1: every SLASetBlendParameters_t field reachable and correctly serialised
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, PackBlendConfigFullWireRoundTrip)
{
    const QVariantMap cfg {
        { QStringLiteral("warpIndex"), 2 },
        { QStringLiteral("fixedIndex"), 3 },
        { QStringLiteral("mode"), 11 },
        { QStringLiteral("amt"), 200 },
        { QStringLiteral("hue"), 90 },
        { QStringLiteral("flags"), 0x02 },
        { QStringLiteral("hotStart"), 40 },
        { QStringLiteral("coldEnd"), 220 },
        { QStringLiteral("absolute"), true },
        { QStringLiteral("zoomMultiplier"), 3 },
        { QStringLiteral("vertical"), -12 },
        { QStringLiteral("horizontal"), 7 },
        { QStringLiteral("rotation"), 140 },
        { QStringLiteral("zoom"), 130 },
        { QStringLiteral("hzoom"), 125 },
        { QStringLiteral("reset"), true },
        { QStringLiteral("usePresetAlign"), true },
        { QStringLiteral("presetAlignIndex"), 12 },
    };

    Sightline::MsgSetBlendParameters packed {};
    ASSERT_TRUE(SightlineQmlBridge::packBlendConfig(cfg, packed));

    // absOffZoom: bit0 = absolute, bits1..3 = zoom multiplier (3 << 1 = 0x06)
    EXPECT_EQ(packed.absOffZoom, 0x07U);

    const auto wire { Sightline::SightlineBlendingBuilder::buildSetBlendParameters(packed) };
    Sightline::MsgSetBlendParameters parsed {};
    ASSERT_TRUE(Sightline::SightlineBlendingParser::parseBlendParameters(
        Sightline::ByteView { wire.data(), wire.size() }, parsed));

    EXPECT_EQ(parsed.warpIndex, 2U);
    EXPECT_EQ(parsed.fixedIndex, 3U);
    EXPECT_EQ(parsed.mode, Sightline::BlendMode::ColorIrBlendWarpEo);
    EXPECT_EQ(parsed.amt, 200U);
    EXPECT_EQ(parsed.hue, 90U);
    EXPECT_EQ(parsed.flags, 0x02U);
    EXPECT_EQ(parsed.hotStart, 40U);
    EXPECT_EQ(parsed.coldEnd, 220U);
    EXPECT_EQ(parsed.absOffZoom, 0x07U);
    EXPECT_EQ(parsed.vertical, -12);
    EXPECT_EQ(parsed.horizontal, 7);
    EXPECT_EQ(parsed.rotation, 140U);
    EXPECT_EQ(parsed.zoom, 130U);
    EXPECT_EQ(parsed.hzoom, 125U);
    EXPECT_EQ(parsed.reset, 1U);
    EXPECT_EQ(parsed.usePresetAlign, 1U);
    EXPECT_EQ(parsed.presetAlignIndex, 12U);
}

// ---------------------------------------------------------------------------------------------
// B5: narrowing conversions are clamped/masked, never silently truncated
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, PackBlendConfigClampsOutOfRange)
{
    const QVariantMap cfg {
        { QStringLiteral("mode"), 2 },
        { QStringLiteral("warpIndex"), 9 },
        { QStringLiteral("fixedIndex"), -4 },
        { QStringLiteral("amt"), 999 },
        { QStringLiteral("hue"), -10 },
        { QStringLiteral("flags"), 0xFF },
        { QStringLiteral("hotStart"), 400 },
        { QStringLiteral("coldEnd"), -1 },
        { QStringLiteral("zoomMultiplier"), 12 },
        { QStringLiteral("vertical"), -500 },
        { QStringLiteral("horizontal"), 500 },
        { QStringLiteral("rotation"), 300 },
        { QStringLiteral("zoom"), -3 },
        { QStringLiteral("hzoom"), 1000 },
    };

    Sightline::MsgSetBlendParameters out {};
    ASSERT_TRUE(SightlineQmlBridge::packBlendConfig(cfg, out));

    EXPECT_EQ(out.warpIndex, 3U);
    EXPECT_EQ(out.fixedIndex, 0U);
    EXPECT_EQ(out.amt, 255U);
    EXPECT_EQ(out.hue, 0U);
    EXPECT_EQ(out.flags, 0x03U); // only bits 0..1 are defined
    EXPECT_EQ(out.hotStart, 255U);
    EXPECT_EQ(out.coldEnd, 0U);
    EXPECT_EQ(out.absOffZoom, 0x0EU); // multiplier clamped to 7 -> (7 << 1)
    EXPECT_EQ(out.vertical, -128);
    EXPECT_EQ(out.horizontal, 127);
    EXPECT_EQ(out.rotation, 255U);
    EXPECT_EQ(out.zoom, 0U);
    EXPECT_EQ(out.hzoom, 255U);
}

// ---------------------------------------------------------------------------------------------
// B5 / U5: 0xB9 rotation normalisation and zoom range clamping
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, PackBlendAlignNormalisesRotation)
{
    // -10 deg * 128 = -1280 must wrap to 350 deg * 128 = 44800 (not uint16 wrap 64256)
    const auto neg { SightlineQmlBridge::packBlendAlign(1, 0, 0, -1280, 4096, 4096) };
    EXPECT_EQ(neg.rotate, 44800U);

    // 390 deg * 128 = 49920 must wrap to 30 deg * 128 = 3840
    const auto over { SightlineQmlBridge::packBlendAlign(1, 0, 0, 49920, 4096, 4096) };
    EXPECT_EQ(over.rotate, 3840U);

    const auto ok { SightlineQmlBridge::packBlendAlign(1, 0, 0, 11520, 4096, 4096) };
    EXPECT_EQ(ok.rotate, 11520U);
}

TEST_F(SightlineBlendBridgeTest, PackBlendAlignClampsZoomAndOffsets)
{
    const auto a { SightlineQmlBridge::packBlendAlign(9, 40000, -40000, 0, 0, 70000) };
    EXPECT_EQ(a.index, 4U);
    EXPECT_EQ(a.vertical, 32767);
    EXPECT_EQ(a.horizontal, -32768);
    EXPECT_EQ(a.zoom, 41U); // 0.01x * 4096
    EXPECT_EQ(a.hzoom, 65495U); // 15.99x * 4096
}

// ---------------------------------------------------------------------------------------------
// B6: multiple-alignment telemetry must carry nAlignments to QML
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, MultipleAlignmentSignalCarriesCount)
{
    SightlineQmlBridge bridge {};
    QSignalSpy spy { &bridge, &SightlineQmlBridge::multipleAlignmentReceived };

    Sightline::MsgSetMultipleAlignment m {};
    m.nAlignments = 3U;
    m.alignment[2].vertical = 17U;

    const bool invoked { QMetaObject::invokeMethod(
        &bridge, "handleMultipleAlignment", Qt::DirectConnection, Q_ARG(Sightline::MsgSetMultipleAlignment, m)) };
    ASSERT_TRUE(invoked);
    ASSERT_EQ(spy.count(), 1);

    // Copy-initialise: brace-init would select QList's initializer_list ctor and nest the list.
    const QList<QVariant> args = spy.takeFirst();
    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args.at(0).toInt(), 3);
    const QVariantList entries = args.at(1).toList();
    ASSERT_EQ(entries.size(), 5);
    EXPECT_EQ(entries.at(2).toMap().value(QStringLiteral("vertical")).toInt(), 17);
}

// ---------------------------------------------------------------------------------------------
// U10 / U11: telemetry maps expose warp fields required by the UI
// ---------------------------------------------------------------------------------------------
TEST_F(SightlineBlendBridgeTest, BlendParamsSignalExposesWarpFields)
{
    SightlineQmlBridge bridge {};
    QSignalSpy spy { &bridge, &SightlineQmlBridge::blendParametersReceived };

    Sightline::MsgSetBlendParameters p {};
    p.absOffZoom = 0x05U; // absolute + multiplier 2
    p.reset = 1U;

    const bool invoked { QMetaObject::invokeMethod(
        &bridge, "handleBlendParams", Qt::DirectConnection, Q_ARG(Sightline::MsgSetBlendParameters, p)) };
    ASSERT_TRUE(invoked);
    ASSERT_EQ(spy.count(), 1);

    const QVariantMap map { spy.takeFirst().at(0).toMap() };
    EXPECT_EQ(map.value(QStringLiteral("absOffZoom")).toInt(), 5);
    EXPECT_TRUE(map.value(QStringLiteral("absolute")).toBool());
    EXPECT_EQ(map.value(QStringLiteral("zoomMultiplier")).toInt(), 2);
    EXPECT_EQ(map.value(QStringLiteral("reset")).toInt(), 1);
}

TEST_F(SightlineBlendBridgeTest, CurrentBlendSignalExposesZoomMode)
{
    SightlineQmlBridge bridge {};
    QSignalSpy spy { &bridge, &SightlineQmlBridge::currentBlendParamsReceived };

    Sightline::MsgCurrentBlendParameters p {};
    p.absOffZoom = 0x02U; // bit 1: fine zoom mode
    p.usePresetAlign = 1U;
    p.presetAlignIndex = 11U;

    const bool invoked { QMetaObject::invokeMethod(
        &bridge, "handleCurrentBlendParams", Qt::DirectConnection, Q_ARG(Sightline::MsgCurrentBlendParameters, p)) };
    ASSERT_TRUE(invoked);
    ASSERT_EQ(spy.count(), 1);

    const QVariantMap map { spy.takeFirst().at(0).toMap() };
    EXPECT_EQ(map.value(QStringLiteral("absOffZoom")).toInt(), 2);
    EXPECT_EQ(map.value(QStringLiteral("zoomMultiplier")).toInt(), 1);
    EXPECT_EQ(map.value(QStringLiteral("usePresetAlign")).toInt(), 1);
    EXPECT_EQ(map.value(QStringLiteral("presetAlignIndex")).toInt(), 11);
}

} // namespace
