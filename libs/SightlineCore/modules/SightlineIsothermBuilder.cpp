/// @file SightlineIsothermBuilder.cpp
/// @brief Implementation of Isotherm false color lookup table generator and AGC binder.

#include "SightlineIsothermBuilder.h"
#include "SightlineFraming.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Sightline {

// ============================================================================
// IsothermColorLut Implementation
// ============================================================================

std::vector<std::uint8_t> IsothermColorLut::toYuvBytes() const
{
    std::vector<std::uint8_t> bytes {};
    bytes.reserve(768U);
    for (const auto& entry : yuv) {
        bytes.push_back(entry.y);
        bytes.push_back(entry.u);
        bytes.push_back(entry.v);
    }
    return bytes;
}

std::vector<std::uint8_t> IsothermColorLut::toRgbBytes() const
{
    std::vector<std::uint8_t> bytes {};
    bytes.reserve(768U);
    for (const auto& entry : rgb) {
        bytes.push_back(entry.r);
        bytes.push_back(entry.g);
        bytes.push_back(entry.b);
    }
    return bytes;
}

MsgUserPalette IsothermColorLut::toUserPalette(std::uint8_t paletteIndex) const
{
    MsgUserPalette msg {};
    msg.paletteIndex = paletteIndex;
    msg.lutData = toYuvBytes();
    return msg;
}

bool IsothermColorLut::saveToFile(const std::string& path, bool asYuv) const
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    const auto bytes = asYuv ? toYuvBytes() : toRgbBytes();
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

bool IsothermColorLut::loadFromFile(
    const std::string& path, IsothermColorLut& out, bool asYuv)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::vector<std::uint8_t> buffer(768U, 0U);
    file.read(reinterpret_cast<char*>(buffer.data()), 768);
    if (file.gcount() != 768) {
        return false;
    }

    for (std::size_t i = 0U; i < 256U; ++i) {
        const std::size_t idx { i * 3U };
        if (asYuv) {
            out.yuv[i] = YuvColor { buffer[idx], buffer[idx + 1U], buffer[idx + 2U] };
            out.rgb[i] = SightlineIsothermBuilder::toRgb(out.yuv[i]);
        } else {
            out.rgb[i] = RgbColor { buffer[idx], buffer[idx + 1U], buffer[idx + 2U] };
            out.yuv[i] = SightlineIsothermBuilder::toYuv(out.rgb[i]);
        }
    }
    return true;
}

// ============================================================================
// SightlineIsothermBuilder Implementation
// ============================================================================

SightlineIsothermBuilder::SightlineIsothermBuilder() noexcept = default;

SightlineIsothermBuilder& SightlineIsothermBuilder::setBasePalette(BasePaletteType type) noexcept
{
    m_baseType = type;
    m_hasCustomBase = false;
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::setCustomBase(
    const std::array<RgbColor, 256U>& base) noexcept
{
    m_customBase = base;
    m_hasCustomBase = true;
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::setAgcRange(
    float minTemp, float maxTemp, TemperatureScale scale) noexcept
{
    m_minTemp = minTemp;
    m_maxTemp = maxTemp;
    m_scale = scale;
    m_hasAgcRange = true;
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::addBand(const IsothermBand& band)
{
    m_bands.push_back(band);
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::addBandIndex(
    std::uint8_t minIdx, std::uint8_t maxIdx, RgbColor color,
    IsothermBlendMode mode, float alpha)
{
    IsothermBand b {};
    b.minIndex = minIdx;
    b.maxIndex = maxIdx;
    b.startColor = color;
    b.endColor = color;
    b.blendMode = mode;
    b.alpha = std::clamp(alpha, 0.0F, 1.0F);
    m_bands.push_back(b);
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::addGradientIndex(
    std::uint8_t minIdx, std::uint8_t maxIdx, RgbColor start, RgbColor end,
    IsothermBlendMode mode, float alpha)
{
    IsothermBand b {};
    b.minIndex = minIdx;
    b.maxIndex = maxIdx;
    b.startColor = start;
    b.endColor = end;
    b.blendMode = mode;
    b.alpha = std::clamp(alpha, 0.0F, 1.0F);
    m_bands.push_back(b);
    return *this;
}

SightlineIsothermBuilder& SightlineIsothermBuilder::addBandTemp(
    float minTemp, float maxTemp, TemperatureScale scale, RgbColor color,
    IsothermBlendMode mode, float alpha)
{
    const auto minIdx = mapTempToIndex(minTemp, scale);
    const auto maxIdx = mapTempToIndex(maxTemp, scale);
    return addBandIndex(minIdx, maxIdx, color, mode, alpha);
}

SightlineIsothermBuilder& SightlineIsothermBuilder::addGradientTemp(
    float minTemp, float maxTemp, TemperatureScale scale, RgbColor start, RgbColor end,
    IsothermBlendMode mode, float alpha)
{
    const auto minIdx = mapTempToIndex(minTemp, scale);
    const auto maxIdx = mapTempToIndex(maxTemp, scale);
    return addGradientIndex(minIdx, maxIdx, start, end, mode, alpha);
}

SightlineIsothermBuilder& SightlineIsothermBuilder::clearBands() noexcept
{
    m_bands.clear();
    return *this;
}

std::uint8_t SightlineIsothermBuilder::mapTempToIndex(
    float temp, TemperatureScale scale) const noexcept
{
    if (!m_hasAgcRange) {
        return 0U;
    }

    const float minK { SightlineRadiometry::toKelvin(m_minTemp, m_scale) };
    const float maxK { SightlineRadiometry::toKelvin(m_maxTemp, m_scale) };
    const float curK { SightlineRadiometry::toKelvin(temp, scale) };

    const float range { maxK - minK };
    if (std::abs(range) < 1e-4F) {
        return 0U;
    }

    const float frac { (curK - minK) / range };
    if (frac <= 0.0F) {
        return 0U;
    }
    if (frac >= 1.0F) {
        return 255U;
    }

    const int idx { static_cast<int>(std::round(frac * 255.0F)) };
    return static_cast<std::uint8_t>(std::clamp(idx, 0, 255));
}

IsothermColorLut SightlineIsothermBuilder::buildLut() const
{
    std::array<RgbColor, 256U> rgbTable {
        m_hasCustomBase ? m_customBase : makeBasePalette(m_baseType)
    };

    for (const auto& band : m_bands) {
        const std::uint8_t startIdx { std::min(band.minIndex, band.maxIndex) };
        const std::uint8_t endIdx { std::max(band.minIndex, band.maxIndex) };
        const std::uint32_t span { static_cast<std::uint32_t>(endIdx - startIdx) };

        for (std::uint32_t i = startIdx; i <= endIdx; ++i) {
            const float t { (span == 0U) ? 0.0F : static_cast<float>(i - startIdx) / static_cast<float>(span) };
            const RgbColor bandColor { interpolate(band.startColor, band.endColor, t) };

            if (band.blendMode == IsothermBlendMode::Replace || band.alpha >= 0.999F) {
                rgbTable[i] = bandColor;
            } else if (band.alpha <= 0.001F) {
                // Opacity negligible, keep underlying color
            } else {
                rgbTable[i] = interpolate(rgbTable[i], bandColor, band.alpha);
            }
        }
    }

    IsothermColorLut lut {};
    lut.rgb = rgbTable;
    for (std::size_t i = 0U; i < 256U; ++i) {
        lut.yuv[i] = toYuv(lut.rgb[i]);
    }
    return lut;
}

MsgUserPalette SightlineIsothermBuilder::buildUserPalette(std::uint8_t paletteIdx) const
{
    const auto lut = buildLut();
    MsgUserPalette msg {};
    msg.paletteIndex = paletteIdx;
    msg.lutData = lut.toYuvBytes();
    return msg;
}

std::vector<std::uint8_t> SightlineIsothermBuilder::buildPacket(std::uint8_t paletteIdx) const
{
    const auto lut = buildLut();
    const auto yuvBytes = lut.toYuvBytes();
    if (paletteIdx == 0U) {
        return SightlineFraming::buildPacket(MessageId::SetUserPalette, yuvBytes);
    }
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + yuvBytes.size());
    payload.push_back(paletteIdx);
    payload.insert(payload.end(), yuvBytes.begin(), yuvBytes.end());
    return SightlineFraming::buildPacket(MessageId::SetUserPalette, payload);
}

MsgDigitalCameraParameters SightlineIsothermBuilder::buildAgcConfig(
    RadiometricSensor sensor, std::uint8_t cameraIndex,
    float scaleA, float offsetB) const noexcept
{
    MsgDigitalCameraParameters p {};
    p.cameraIndex = cameraIndex;
    p.mode = AutoGainMode::Manual;

    if (m_hasAgcRange) {
        const float minK { SightlineRadiometry::toKelvin(m_minTemp, m_scale) };
        const float maxK { SightlineRadiometry::toKelvin(m_maxTemp, m_scale) };
        const auto agc = SightlineRadiometry::calcIsothermAgc(minK, maxK, sensor, scaleA, offsetB);
        p.agHoldmin = agc.agHoldmin;
        p.agHoldmax = agc.agHoldmax;
        p.midpoint = 128U;
    }
    return p;
}

YuvColor SightlineIsothermBuilder::toYuv(RgbColor rgb) noexcept
{
    const float r { static_cast<float>(rgb.r) };
    const float g { static_cast<float>(rgb.g) };
    const float b { static_cast<float>(rgb.b) };

    const float y { (0.299F * r) + (0.587F * g) + (0.114F * b) };
    const float u { (-0.14713F * r) - (0.28886F * g) + (0.436F * b) + 128.0F };
    const float v { (0.615F * r) - (0.51499F * g) - (0.10001F * b) + 128.0F };

    YuvColor out {};
    out.y = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(y)), 0, 255));
    out.u = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(u)), 0, 255));
    out.v = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(v)), 0, 255));
    return out;
}

RgbColor SightlineIsothermBuilder::toRgb(YuvColor yuv) noexcept
{
    const float y { static_cast<float>(yuv.y) };
    const float u { static_cast<float>(yuv.u) - 128.0F };
    const float v { static_cast<float>(yuv.v) - 128.0F };

    const float r { y + (1.13983F * v) };
    const float g { y - (0.39465F * u) - (0.58060F * v) };
    const float b { y + (2.03211F * u) };

    RgbColor out {};
    out.r = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(r)), 0, 255));
    out.g = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(g)), 0, 255));
    out.b = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(b)), 0, 255));
    return out;
}

std::array<RgbColor, 256U> SightlineIsothermBuilder::makeBasePalette(
    BasePaletteType type) noexcept
{
    std::array<RgbColor, 256U> table {};

    switch (type) {
    case BasePaletteType::GrayscaleWhiteHot:
        for (std::size_t i = 0U; i < 256U; ++i) {
            const auto val = static_cast<std::uint8_t>(i);
            table[i] = RgbColor { val, val, val };
        }
        break;

    case BasePaletteType::GrayscaleBlackHot:
        for (std::size_t i = 0U; i < 256U; ++i) {
            const auto val = static_cast<std::uint8_t>(255U - i);
            table[i] = RgbColor { val, val, val };
        }
        break;

    case BasePaletteType::Ironbow: {
        // Multi-stop gradient: Black (0) -> Dark Violet (64) -> Crimson (128) -> Amber (192) -> White (255)
        const RgbColor s0 { 0U, 0U, 0U };
        const RgbColor s1 { 40U, 0U, 110U };
        const RgbColor s2 { 180U, 20U, 100U };
        const RgbColor s3 { 240U, 160U, 20U };
        const RgbColor s4 { 255U, 255U, 255U };

        for (std::size_t i = 0U; i < 64U; ++i) {
            table[i] = interpolate(s0, s1, static_cast<float>(i) / 64.0F);
        }
        for (std::size_t i = 64U; i < 128U; ++i) {
            table[i] = interpolate(s1, s2, static_cast<float>(i - 64U) / 64.0F);
        }
        for (std::size_t i = 128U; i < 192U; ++i) {
            table[i] = interpolate(s2, s3, static_cast<float>(i - 128U) / 64.0F);
        }
        for (std::size_t i = 192U; i < 256U; ++i) {
            table[i] = interpolate(s3, s4, static_cast<float>(i - 192U) / 63.0F);
        }
        break;
    }

    case BasePaletteType::Rainbow: {
        // Spectral gradient: Deep Blue (0) -> Cyan (64) -> Green (128) -> Yellow (192) -> Red (255)
        const RgbColor s0 { 0U, 0U, 180U };
        const RgbColor s1 { 0U, 200U, 240U };
        const RgbColor s2 { 0U, 220U, 40U };
        const RgbColor s3 { 255U, 230U, 0U };
        const RgbColor s4 { 255U, 0U, 0U };

        for (std::size_t i = 0U; i < 64U; ++i) {
            table[i] = interpolate(s0, s1, static_cast<float>(i) / 64.0F);
        }
        for (std::size_t i = 64U; i < 128U; ++i) {
            table[i] = interpolate(s1, s2, static_cast<float>(i - 64U) / 64.0F);
        }
        for (std::size_t i = 128U; i < 192U; ++i) {
            table[i] = interpolate(s2, s3, static_cast<float>(i - 128U) / 64.0F);
        }
        for (std::size_t i = 192U; i < 256U; ++i) {
            table[i] = interpolate(s3, s4, static_cast<float>(i - 192U) / 63.0F);
        }
        break;
    }

    case BasePaletteType::Sepia:
        for (std::size_t i = 0U; i < 256U; ++i) {
            const float fi { static_cast<float>(i) };
            const int r { static_cast<int>(std::round(fi * 1.15F)) };
            const int g { static_cast<int>(std::round(fi * 0.90F)) };
            const int b { static_cast<int>(std::round(fi * 0.60F)) };
            table[i] = RgbColor {
                static_cast<std::uint8_t>(std::clamp(r, 0, 255)),
                static_cast<std::uint8_t>(std::clamp(g, 0, 255)),
                static_cast<std::uint8_t>(std::clamp(b, 0, 255))
            };
        }
        break;
    }

    return table;
}

RgbColor SightlineIsothermBuilder::interpolate(
    RgbColor c1, RgbColor c2, float t) noexcept
{
    const float clampedT { std::clamp(t, 0.0F, 1.0F) };
    const float r { ((1.0F - clampedT) * static_cast<float>(c1.r)) + (clampedT * static_cast<float>(c2.r)) };
    const float g { ((1.0F - clampedT) * static_cast<float>(c1.g)) + (clampedT * static_cast<float>(c2.g)) };
    const float b { ((1.0F - clampedT) * static_cast<float>(c1.b)) + (clampedT * static_cast<float>(c2.b)) };

    return RgbColor {
        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(r)), 0, 255)),
        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(g)), 0, 255)),
        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(b)), 0, 255))
    };
}

} // namespace Sightline
