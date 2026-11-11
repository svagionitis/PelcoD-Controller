/// @file RgbWarpUtility.cpp
/// @brief High-throughput bilinear image warping for packed RGB24 video frames.
#include "RgbWarpUtility.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#include "StabilizerTypes.h"
#include "WarpEngine.h"

namespace StabilizerApp {

namespace {

    /// @brief Clamps integer coordinate to [0 .. maxVal - 1].
    [[nodiscard]] inline constexpr std::int32_t clampCoord(std::int32_t val, std::int32_t maxVal) noexcept
    {
        return (val < 0) ? 0 : ((val >= maxVal) ? (maxVal - 1) : val);
    }

    /// @brief Reflects integer coordinate symmetrically across border.
    [[nodiscard]] inline std::int32_t reflectCoord(std::int32_t val, std::int32_t maxVal) noexcept
    {
        if (maxVal <= 1) {
            return 0;
        }
        std::int32_t v = val;
        while ((v < 0) || (v >= maxVal)) {
            if (v < 0) {
                v = -v;
            } else if (v >= maxVal) {
                v = (2 * (maxVal - 1)) - v;
            }
        }
        return v;
    }

    /// @brief Reads single RGB pixel from source with boundary handling.
    inline void sampleRgbPixel(const std::uint8_t* src, std::int32_t x, std::int32_t y, std::int32_t width,
        std::int32_t height, std::uint32_t stride, Stabilizer::BorderMode mode, std::uint8_t borderVal, std::uint8_t& r,
        std::uint8_t& g, std::uint8_t& b) noexcept
    {
        if ((x < 0) || (x >= width) || (y < 0) || (y >= height)) {
            switch (mode) {
            case Stabilizer::BorderMode::Constant:
                r = borderVal;
                g = borderVal;
                b = borderVal;
                return;
            case Stabilizer::BorderMode::Replicate:
                x = clampCoord(x, width);
                y = clampCoord(y, height);
                break;
            case Stabilizer::BorderMode::Reflect:
                x = reflectCoord(x, width);
                y = reflectCoord(y, height);
                break;
            }
        }

        const std::size_t offset = (static_cast<std::size_t>(y) * stride) + (static_cast<std::size_t>(x) * 3U);
        r = src[offset];
        g = src[offset + 1U];
        b = src[offset + 2U];
    }

} // namespace

void RgbWarpUtility::extractLuma(
    const std::uint8_t* rgbSrc, std::uint8_t* lumaDst, std::uint32_t width, std::uint32_t height) noexcept
{
    if ((rgbSrc == nullptr) || (lumaDst == nullptr) || (width == 0U) || (height == 0U)) {
        return;
    }

    const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0U; i < totalPixels; ++i) {
        const std::size_t rgbIdx = i * 3U;
        const auto r = static_cast<std::uint32_t>(rgbSrc[rgbIdx]);
        const auto g = static_cast<std::uint32_t>(rgbSrc[rgbIdx + 1U]);
        const auto b = static_cast<std::uint32_t>(rgbSrc[rgbIdx + 2U]);

        // Integer Rec.601 approximation: Y = (77*R + 150*G + 29*B + 128) >> 8
        const std::uint32_t y = ((77U * r) + (150U * g) + (29U * b) + 128U) >> 8U;
        lumaDst[i] = static_cast<std::uint8_t>((y > 255U) ? 255U : y);
    }
}

bool RgbWarpUtility::warpRgb24(const std::uint8_t* src, std::uint8_t* dst, std::uint32_t width, std::uint32_t height,
    std::uint32_t stride, const Stabilizer::AffineTransform2D& transform, const Stabilizer::WarpConfig& config) noexcept
{
    if ((src == nullptr) || (dst == nullptr) || (width == 0U) || (height == 0U) || (stride < width * 3U)) {
        return false;
    }

    Stabilizer::AffineTransform2D forward = transform;
    if (config.enableAutoCrop) {
        forward = Stabilizer::WarpEngine::composeZoom(
            forward, Stabilizer::FrameDimensions { width, height, stride }, config.cropRatio);
    }

    Stabilizer::AffineTransform2D invWarp { Stabilizer::AffineTransform2D::identity() };
    if (!forward.invert(invWarp)) {
        invWarp = Stabilizer::AffineTransform2D::identity();
    }

    const auto iWidth = static_cast<std::int32_t>(width);
    const auto iHeight = static_cast<std::int32_t>(height);
    const Stabilizer::BorderMode bMode = config.borderMode;
    const std::uint8_t bVal = config.borderValue;

    for (std::uint32_t y = 0U; y < height; ++y) {
        const auto fy = static_cast<float>(y);
        const std::size_t rowOffset = static_cast<std::size_t>(y) * stride;

        for (std::uint32_t x = 0U; x < width; ++x) {
            const auto fx = static_cast<float>(x);

            const float u = (invWarp.a11 * fx) + (invWarp.a12 * fy) + invWarp.tx;
            const float v = (invWarp.a21 * fx) + (invWarp.a22 * fy) + invWarp.ty;

            const auto u0 = static_cast<std::int32_t>(std::floor(u));
            const auto v0 = static_cast<std::int32_t>(std::floor(v));
            const float s = u - static_cast<float>(u0);
            const float t = v - static_cast<float>(v0);

            const auto s256 = static_cast<std::uint32_t>(std::clamp(s * 256.0F, 0.0F, 256.0F));
            const auto t256 = static_cast<std::uint32_t>(std::clamp(t * 256.0F, 0.0F, 256.0F));

            const std::uint32_t w00 = (256U - s256) * (256U - t256);
            const std::uint32_t w10 = s256 * (256U - t256);
            const std::uint32_t w01 = (256U - s256) * t256;
            const std::uint32_t w11 = s256 * t256;

            std::uint8_t r00 = 0U, g00 = 0U, b00 = 0U;
            std::uint8_t r10 = 0U, g10 = 0U, b10 = 0U;
            std::uint8_t r01 = 0U, g01 = 0U, b01 = 0U;
            std::uint8_t r11 = 0U, g11 = 0U, b11 = 0U;

            sampleRgbPixel(src, u0, v0, iWidth, iHeight, stride, bMode, bVal, r00, g00, b00);
            sampleRgbPixel(src, u0 + 1, v0, iWidth, iHeight, stride, bMode, bVal, r10, g10, b10);
            sampleRgbPixel(src, u0, v0 + 1, iWidth, iHeight, stride, bMode, bVal, r01, g01, b01);
            sampleRgbPixel(src, u0 + 1, v0 + 1, iWidth, iHeight, stride, bMode, bVal, r11, g11, b11);

            const std::uint32_t rInterp = ((w00 * r00) + (w10 * r10) + (w01 * r01) + (w11 * r11) + 32768U) >> 16U;
            const std::uint32_t gInterp = ((w00 * g00) + (w10 * g10) + (w01 * g01) + (w11 * g11) + 32768U) >> 16U;
            const std::uint32_t bInterp = ((w00 * b00) + (w10 * b10) + (w01 * b01) + (w11 * b11) + 32768U) >> 16U;

            const std::size_t pxOffset = rowOffset + (static_cast<std::size_t>(x) * 3U);
            dst[pxOffset] = static_cast<std::uint8_t>((rInterp > 255U) ? 255U : rInterp);
            dst[pxOffset + 1U] = static_cast<std::uint8_t>((gInterp > 255U) ? 255U : gInterp);
            dst[pxOffset + 2U] = static_cast<std::uint8_t>((bInterp > 255U) ? 255U : bInterp);
        }
    }

    return true;
}

} // namespace StabilizerApp
