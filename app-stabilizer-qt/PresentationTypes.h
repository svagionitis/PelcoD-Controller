#pragma once

/// @file PresentationTypes.h
/// @brief Domain enums and value types for video presentation and overlays.

#include <cstdint>

#include <QObject>

namespace StabilizerApp {

/// @brief Presentation layout comparison mode.
enum class PresentationMode : std::uint8_t {
    SplitWipe = 0U, ///< Interactive split wipe divider (raw vs. stabilized)
    SideBySide, ///< Synchronized dual-viewport view
    SingleToggle, ///< Fullscreen single view with raw/stabilized toggle
    DifferenceHeatmap ///< Pixel difference heatmap with Turbo colormap LUT
};

/// @brief Orientation of the split wipe divider.
enum class SplitOrientation : std::uint8_t {
    Vertical = 0U, ///< Vertical divider (left = raw, right = stabilized)
    Horizontal ///< Horizontal divider (top = raw, bottom = stabilized)
};

/// @brief Strongly-typed zoom factor clamped within [1.0 .. 8.0].
class ZoomFactor {
public:
    constexpr ZoomFactor() noexcept = default;

    /// @brief Construct zoom factor clamped within [1.0 .. 8.0].
    /// @param val Raw magnification factor.
    explicit constexpr ZoomFactor(float val) noexcept
        : value_ { (val < 1.0F) ? 1.0F : ((val > 8.0F) ? 8.0F : val) }
    {
    }

    /// @brief Underlying float magnification value.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

    /// @brief Check if zoom is active (> 1.0).
    [[nodiscard]] constexpr bool isZoomed() const noexcept
    {
        return value_ > 1.001F;
    }

private:
    float value_ { 1.0F };
};

} // namespace StabilizerApp
