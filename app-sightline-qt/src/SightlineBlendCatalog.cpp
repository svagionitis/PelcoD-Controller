#include "SightlineBlendCatalog.h"

#include <SightlineCore/modules/SightlineBlending.h>

#include <QCoreApplication>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using Sightline::BlendMode;

/// @struct ModeTraits
/// @brief Which tuning controls a blend mode honours.
struct ModeTraits {
    bool hue { false }; ///< Uses the hue field.
    bool hueFlag { false }; ///< Honours BlendFlags::UseHueForColor.
    bool thermal { false }; ///< Uses the thermal window (hot start / cold end).
    bool histEq { false }; ///< Honours BlendFlags::HistogramEq.
    bool palette { false }; ///< Colours IR with the user palette.
    bool dualEo { false }; ///< Blends two EO sensors.
};

/// @brief Display order of every selectable mode (reserved value 5 intentionally absent).
/// @note When a BlendMode enumerator is added, -Wswitch in modeTraits()/modeLabel() flags it; add it here too.
constexpr std::array<BlendMode, 12U> kModeOrder {
    BlendMode::NoChange,
    BlendMode::FrameBlendWarpEo,
    BlendMode::ThermalBlendWarpEo,
    BlendMode::NightBlendWarpEo,
    BlendMode::ColorBlendWarpEo,
    BlendMode::FrameBlendFixedEo,
    BlendMode::ThermalBlendFixedEo,
    BlendMode::NightBlendFixedEo,
    BlendMode::ColorBlendFixedEo,
    BlendMode::ColorIrBlendFixedEo,
    BlendMode::ColorIrBlendWarpEo,
    BlendMode::ColorEoBlendEo,
};

/// @brief Mode used when QML supplies an unknown value (matches the MsgSetBlendParameters default).
constexpr BlendMode kFallbackMode { BlendMode::FrameBlendWarpEo };

/// @brief Valid 0x2F preset alignment indices: 0xB9 slots 0..4, then 0x95 slots 10..14.
constexpr std::array<int, 10U> kPresetOrder { 0, 1, 2, 3, 4, 10, 11, 12, 13, 14 };

constexpr int kFourPointBase { 10 }; ///< First preset index that refers to a 0x95 slot.
constexpr int kFourPointLast { 14 }; ///< Last preset index that refers to a 0x95 slot.
constexpr int kAlignSlotLast { 4 }; ///< Last preset index that refers to a 0xB9 slot.
constexpr int kByteMax { 255 }; ///< Upper bound of a protocol byte.
constexpr int kPercent { 100 }; ///< Percent scale.
constexpr double kRotHalfSpan { 5.0 }; ///< Rotation half-range in degrees.
constexpr double kRotSteps { 254.0 }; ///< Number of steps between raw 1 and raw 255.

/// @brief Converts a BlendMode enumerator into its wire integer.
/// @param[in] mode Blend mode enumerator.
/// @return Integer wire value.
[[nodiscard]] constexpr int toWire(BlendMode mode) noexcept
{
    return static_cast<int>(static_cast<std::uint8_t>(mode));
}

static_assert(toWire(kModeOrder.back()) == 12, "kModeOrder must end with the highest defined BlendMode");

/// @brief Safely maps a wire integer onto a defined BlendMode without casting unchecked values.
/// @param[in] wire Wire value from QML.
/// @param[out] out Matching enumerator (unchanged on failure).
/// @return True if @p wire is a defined, non-reserved mode.
[[nodiscard]] bool findMode(int wire, BlendMode& out) noexcept
{
    for (const BlendMode mode : kModeOrder) {
        if (toWire(mode) == wire) {
            out = mode;
            return true;
        }
    }
    return false;
}

/// @brief Returns the control capabilities of a blend mode.
/// @param[in] mode Blend mode enumerator.
/// @return Capability flags.
[[nodiscard]] ModeTraits modeTraits(BlendMode mode) noexcept
{
    ModeTraits traits {};
    switch (mode) {
    case BlendMode::NoChange:
        // "No Change" keeps every tuning control available.
        traits.hue = true;
        traits.hueFlag = true;
        traits.thermal = true;
        traits.histEq = true;
        break;
    case BlendMode::FrameBlendWarpEo:
    case BlendMode::FrameBlendFixedEo:
        break;
    case BlendMode::ThermalBlendWarpEo:
    case BlendMode::ThermalBlendFixedEo:
        traits.thermal = true;
        traits.histEq = true;
        break;
    case BlendMode::NightBlendWarpEo:
    case BlendMode::NightBlendFixedEo:
        traits.hue = true;
        break;
    case BlendMode::ColorBlendWarpEo:
    case BlendMode::ColorBlendFixedEo:
        traits.hue = true;
        traits.hueFlag = true;
        break;
    case BlendMode::ColorIrBlendFixedEo:
    case BlendMode::ColorIrBlendWarpEo:
        traits.hue = true;
        traits.hueFlag = true;
        traits.thermal = true;
        traits.palette = true;
        break;
    case BlendMode::ColorEoBlendEo:
        traits.dualEo = true;
        break;
    }
    return traits;
}

/// @brief Returns the capabilities for a wire value, or none if it is undefined.
/// @param[in] wire Wire value from QML.
/// @return Capability flags (all false for undefined values).
[[nodiscard]] ModeTraits traitsFor(int wire) noexcept
{
    BlendMode mode { kFallbackMode };
    return findMode(wire, mode) ? modeTraits(mode) : ModeTraits {};
}

/// @brief Translates a catalogue string.
/// @param[in] text Source text.
/// @return Translated text.
[[nodiscard]] QString catalogTr(const char* text)
{
    return QCoreApplication::translate("SightlineBlendCatalog", text);
}

/// @brief Returns the display name of a blend mode.
/// @param[in] mode Blend mode enumerator.
/// @return "Algorithm · Routing" display name.
[[nodiscard]] QString modeLabel(BlendMode mode)
{
    const QString sep { QStringLiteral(" \u00B7 ") };
    const QString warpEo { catalogTr("Warped EO + Fixed IR") };
    const QString fixedEo { catalogTr("Fixed EO + Warped IR") };
    QString label {};
    switch (mode) {
    case BlendMode::NoChange:
        label = catalogTr("No Change (keep active mode)");
        break;
    case BlendMode::FrameBlendWarpEo:
        label = catalogTr("Frame Blend") + sep + warpEo;
        break;
    case BlendMode::ThermalBlendWarpEo:
        label = catalogTr("Thermal False Color") + sep + warpEo;
        break;
    case BlendMode::NightBlendWarpEo:
        label = catalogTr("Night Blend") + sep + warpEo;
        break;
    case BlendMode::ColorBlendWarpEo:
        label = catalogTr("Color Blend") + sep + warpEo;
        break;
    case BlendMode::FrameBlendFixedEo:
        label = catalogTr("Frame Blend") + sep + fixedEo;
        break;
    case BlendMode::ThermalBlendFixedEo:
        label = catalogTr("Thermal False Color") + sep + fixedEo;
        break;
    case BlendMode::NightBlendFixedEo:
        label = catalogTr("Night Blend") + sep + fixedEo;
        break;
    case BlendMode::ColorBlendFixedEo:
        label = catalogTr("Color Blend") + sep + fixedEo;
        break;
    case BlendMode::ColorIrBlendFixedEo:
        label = catalogTr("Color IR Blend") + sep + fixedEo;
        break;
    case BlendMode::ColorIrBlendWarpEo:
        label = catalogTr("Color IR Blend") + sep + warpEo;
        break;
    case BlendMode::ColorEoBlendEo:
        label = catalogTr("Dual Color Blend") + sep + catalogTr("EO + EO");
        break;
    }
    return label;
}

} // namespace

SightlineBlendCatalog::SightlineBlendCatalog(QObject* parent)
    : QObject { parent }
{
}

bool SightlineBlendCatalog::isValidMode(int mode) noexcept
{
    BlendMode unused { kFallbackMode };
    return findMode(mode, unused);
}

bool SightlineBlendCatalog::isValidPreset(int index) noexcept
{
    return std::find(kPresetOrder.cbegin(), kPresetOrder.cend(), index) != kPresetOrder.cend();
}

QVariantList SightlineBlendCatalog::modes() const
{
    QVariantList list {};
    list.reserve(static_cast<qsizetype>(kModeOrder.size()));
    for (const BlendMode mode : kModeOrder) {
        QVariantMap entry {};
        entry.insert(QStringLiteral("value"), toWire(mode));
        entry.insert(QStringLiteral("name"), modeLabel(mode));
        list.append(entry);
    }
    return list;
}

QStringList SightlineBlendCatalog::modeNames() const
{
    QStringList names {};
    names.reserve(static_cast<qsizetype>(kModeOrder.size()));
    for (const BlendMode mode : kModeOrder) {
        names.append(modeLabel(mode));
    }
    return names;
}

QVariantList SightlineBlendCatalog::presetValues() const
{
    QVariantList list {};
    list.reserve(static_cast<qsizetype>(kPresetOrder.size()));
    for (const int index : kPresetOrder) {
        list.append(index);
    }
    return list;
}

QStringList SightlineBlendCatalog::presetNames() const
{
    QStringList names {};
    names.reserve(static_cast<qsizetype>(kPresetOrder.size()));
    for (const int index : kPresetOrder) {
        names.append(presetLabel(index));
    }
    return names;
}

int SightlineBlendCatalog::indexOfMode(int mode) const noexcept
{
    int fallback { 0 };
    for (std::size_t i { 0U }; i < kModeOrder.size(); ++i) {
        const int wire { toWire(kModeOrder[i]) };
        if (wire == mode) {
            return static_cast<int>(i);
        }
        if (kModeOrder[i] == kFallbackMode) {
            fallback = static_cast<int>(i);
        }
    }
    return fallback;
}

int SightlineBlendCatalog::modeAt(int index) const noexcept
{
    if ((index < 0) || (static_cast<std::size_t>(index) >= kModeOrder.size())) {
        return toWire(kFallbackMode);
    }
    return toWire(kModeOrder[static_cast<std::size_t>(index)]);
}

QString SightlineBlendCatalog::modeName(int mode) const
{
    BlendMode found { kFallbackMode };
    if (!findMode(mode, found)) {
        return catalogTr("Unknown (%1)").arg(mode);
    }
    return modeLabel(found);
}

bool SightlineBlendCatalog::usesHue(int mode) const noexcept
{
    return traitsFor(mode).hue;
}

bool SightlineBlendCatalog::usesHueFlag(int mode) const noexcept
{
    return traitsFor(mode).hueFlag;
}

bool SightlineBlendCatalog::usesThermal(int mode) const noexcept
{
    return traitsFor(mode).thermal;
}

bool SightlineBlendCatalog::usesHistEq(int mode) const noexcept
{
    return traitsFor(mode).histEq;
}

bool SightlineBlendCatalog::usesPalette(int mode) const noexcept
{
    return traitsFor(mode).palette;
}

QString SightlineBlendCatalog::mixLabel(int mode, int amt) const
{
    const int clamped { std::clamp(amt, 0, kByteMax) };
    // Integer round-to-nearest of clamped * 100 / 255 (no ties are possible for odd 255).
    const int pct { ((clamped * kPercent) + (kByteMax / 2)) / kByteMax };
    const int rest { kPercent - pct };
    if (traitsFor(mode).dualEo) {
        return QStringLiteral("%1% Warp \u00B7 %2% Fixed").arg(pct).arg(rest);
    }
    return QStringLiteral("%1% EO \u00B7 %2% IR").arg(pct).arg(rest);
}

double SightlineBlendCatalog::rotationDeg(int raw) const noexcept
{
    const int clamped { std::clamp(raw, 0, kByteMax) };
    if (clamped == 0) {
        return 0.0;
    }
    return -kRotHalfSpan + ((static_cast<double>(clamped - 1) * (2.0 * kRotHalfSpan)) / kRotSteps);
}

QString SightlineBlendCatalog::presetLabel(int index) const
{
    if ((index >= kFourPointBase) && (index <= kFourPointLast)) {
        return catalogTr("4-Point Slot %1 (0x95)").arg(index - kFourPointBase);
    }
    if ((index >= 0) && (index <= kAlignSlotLast)) {
        return catalogTr("Align Slot %1 (0xB9)").arg(index);
    }
    return catalogTr("Invalid Slot (%1)").arg(index);
}

QString SightlineBlendCatalog::flagsLabel(int flags) const
{
    const auto bits { static_cast<unsigned>(std::clamp(flags, 0, kByteMax)) };
    QStringList parts {};
    if ((bits & static_cast<unsigned>(Sightline::BlendFlags::HistogramEq)) != 0U) {
        parts.append(QStringLiteral("HistEq"));
    }
    if ((bits & static_cast<unsigned>(Sightline::BlendFlags::UseHueForColor)) != 0U) {
        parts.append(QStringLiteral("UseHue"));
    }
    return parts.isEmpty() ? catalogTr("None") : parts.join(QStringLiteral(" + "));
}
