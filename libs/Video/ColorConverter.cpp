#include "ColorConverter.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>

namespace Video {

namespace {

    /// @brief Clamps an integer to the unsigned 8-bit range [0, 255].
    [[nodiscard]] constexpr std::uint8_t clampToUint8(std::int32_t val) noexcept
    {
        return static_cast<std::uint8_t>(std::clamp(val, 0, 255));
    }

} // namespace

bool ColorConverter::yuv420ToRgb(const std::uint8_t* yPlane, const std::uint8_t* uPlane, const std::uint8_t* vPlane,
    int width, int height, int yStride, int uStride, int vStride, std::uint8_t* dst, PixelFormat format)
{
    if ((yPlane == nullptr) || (uPlane == nullptr) || (vPlane == nullptr) || (dst == nullptr)) {
        return false;
    }
    if ((width <= 0) || (height <= 0) || (yStride < width) || (uStride < (width / 2)) || (vStride < (width / 2))) {
        return false;
    }

    const bool isBgr { format == PixelFormat::BGR24 };

    for (int y { 0 }; y < height; ++y) {
        const std::size_t yRowOffset { static_cast<std::size_t>(y) * static_cast<std::size_t>(yStride) };
        const std::size_t uvRow { static_cast<std::size_t>(y) / 2U };
        const std::size_t uRowOffset { uvRow * static_cast<std::size_t>(uStride) };
        const std::size_t vRowOffset { uvRow * static_cast<std::size_t>(vStride) };
        const std::size_t dstRowOffset { static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3U };

        for (int x { 0 }; x < width; ++x) {
            const std::size_t uvCol { static_cast<std::size_t>(x) / 2U };
            const std::int32_t yVal { static_cast<std::int32_t>(yPlane[yRowOffset + static_cast<std::size_t>(x)]) };
            const std::int32_t uVal { static_cast<std::int32_t>(uPlane[uRowOffset + uvCol]) - 128 };
            const std::int32_t vVal { static_cast<std::int32_t>(vPlane[vRowOffset + uvCol]) - 128 };

            // Full-range standard coefficients scaled by 1024 for fixed-point integer math:
            // R = Y + 1.402 * V
            // G = Y - 0.344136 * U - 0.714136 * V
            // B = Y + 1.772 * U
            const std::int32_t rVal { clampToUint8(yVal + ((1436 * vVal + 512) >> 10)) };
            const std::int32_t gVal { clampToUint8(yVal - ((352 * uVal + 731 * vVal + 512) >> 10)) };
            const std::int32_t bVal { clampToUint8(yVal + ((1815 * uVal + 512) >> 10)) };

            const std::size_t pixelOffset { dstRowOffset + (static_cast<std::size_t>(x) * 3U) };
            if (isBgr) {
                dst[pixelOffset] = static_cast<std::uint8_t>(bVal);
                dst[pixelOffset + 1U] = static_cast<std::uint8_t>(gVal);
                dst[pixelOffset + 2U] = static_cast<std::uint8_t>(rVal);
            } else {
                dst[pixelOffset] = static_cast<std::uint8_t>(rVal);
                dst[pixelOffset + 1U] = static_cast<std::uint8_t>(gVal);
                dst[pixelOffset + 2U] = static_cast<std::uint8_t>(bVal);
            }
        }
    }

    return true;
}

} // namespace Video
