/// @file FrameView.h
/// @brief Non-owning, zero-heap view over a 2D grayscale image buffer.
#pragma once

#include <cstddef>
#include <cstdint>

#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Non-owning, zero-heap view over a 2D image buffer.
class FrameView {
public:
    constexpr FrameView() noexcept = default;

    /// @brief Construct a frame view over existing memory.
    /// @param data Pointer to the first byte of image memory.
    /// @param dims Image dimensions and pitch.
    constexpr FrameView(const std::uint8_t* data, FrameDimensions dims) noexcept
        : data_ { data }
        , dims_ { dims }
    {
    }

    /// @brief Underlying raw const buffer pointer.
    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept
    {
        return data_;
    }

    /// @brief Frame dimensions descriptor.
    [[nodiscard]] constexpr const FrameDimensions& dimensions() const noexcept
    {
        return dims_;
    }

    /// @brief Frame width in pixels.
    [[nodiscard]] constexpr std::uint32_t width() const noexcept
    {
        return dims_.width();
    }

    /// @brief Frame height in pixels.
    [[nodiscard]] constexpr std::uint32_t height() const noexcept
    {
        return dims_.height();
    }

    /// @brief Stride in bytes between rows.
    [[nodiscard]] constexpr std::uint32_t stride() const noexcept
    {
        return dims_.stride();
    }

    /// @brief Check whether view is empty or uninitialized.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return (data_ == nullptr) || !dims_.isValid();
    }

    /// @brief Pointer to the start of a specified row.
    /// @param y Row index [0 .. height-1].
    [[nodiscard]] constexpr const std::uint8_t* rowPtr(std::uint32_t y) const noexcept
    {
        return data_ + (static_cast<std::size_t>(y) * dims_.stride());
    }

    /// @brief Clamped pixel access with zero undefined behavior.
    /// @param x Horizontal coordinate (may be outside frame).
    /// @param y Vertical coordinate (may be outside frame).
    [[nodiscard]] constexpr std::uint8_t pixelClamped(std::int32_t x, std::int32_t y) const noexcept
    {
        if (isEmpty()) {
            return 0U;
        }
        const std::uint32_t cx = (x < 0)
            ? 0U
            : ((static_cast<std::uint32_t>(x) >= dims_.width()) ? dims_.width() - 1U : static_cast<std::uint32_t>(x));
        const std::uint32_t cy = (y < 0)
            ? 0U
            : ((static_cast<std::uint32_t>(y) >= dims_.height()) ? dims_.height() - 1U : static_cast<std::uint32_t>(y));
        return *(rowPtr(cy) + cx);
    }

    /// @brief Check memory pointer alignment.
    /// @param alignment Required alignment boundary in bytes (e.g. 16, 32, 64).
    [[nodiscard]] bool isAligned(std::size_t alignment) const noexcept
    {
        if (alignment == 0U) {
            return false;
        }
        return (reinterpret_cast<std::uintptr_t>(data_) % alignment) == 0U;
    }

    /// @brief Zero-copy sub-ROI windowing.
    /// @param x Sub-window left offset.
    /// @param y Sub-window top offset.
    /// @param w Sub-window width.
    /// @param h Sub-window height.
    [[nodiscard]] FrameView subView(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) const noexcept
    {
        if (isEmpty() || (w == 0U) || (h == 0U)) {
            return FrameView { };
        }
        if ((x + w > dims_.width()) || (y + h > dims_.height())) {
            return FrameView { };
        }
        const std::uint8_t* subData = rowPtr(y) + x;
        return FrameView { subData, FrameDimensions { w, h, dims_.stride() } };
    }

private:
    const std::uint8_t* data_ { nullptr };
    FrameDimensions dims_ { };
};

} // namespace Stabilizer
