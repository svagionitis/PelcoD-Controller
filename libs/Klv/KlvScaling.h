#pragma once

/// @file KlvScaling.h
/// @brief Centralized numerical scaling and bounds validation helpers.

#include <cmath>
#include <cstdint>

#include <algorithm>
namespace Klv {

/// @brief Utility namespace providing deterministic, range-bounded, and NaN-safe scaling helpers.
namespace KlvScaling {

    /// @brief Wraps an arbitrary degree angle into [0.0, 360.0).
    /// @param[in] deg Input angle in degrees.
    /// @return Wrapped angle in [0.0, 360.0) or 0.0 if not finite.
    [[nodiscard]] inline double wrapAngle360(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0.0;
        }
        double wrapped = std::fmod(deg, 360.0);
        if (wrapped < 0.0) {
            wrapped += 360.0;
        }
        if (wrapped >= 360.0) {
            wrapped = 0.0;
        }
        return wrapped;
    }

    /// @brief Normalizes longitude into [-180.0, 180.0].
    /// @param[in] lonDeg Input longitude in degrees.
    /// @return Normalized longitude or 0.0 if not finite.
    [[nodiscard]] inline double wrapLon180(double lonDeg) noexcept
    {
        if (!std::isfinite(lonDeg)) {
            return 0.0;
        }
        double wrapped = std::fmod(lonDeg + 180.0, 360.0);
        if (wrapped < 0.0) {
            wrapped += 360.0;
        }
        return wrapped - 180.0;
    }

    /// @brief Scales latitude in [-90.0, +90.0] to signed 32-bit integer.
    /// @param[in] deg Latitude in degrees.
    /// @return Scaled int32 in [-2147483647, +2147483647].
    [[nodiscard]] inline std::int32_t scaleLatitude(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0;
        }
        constexpr double kMaxVal = 2147483647.0;
        const double clamped = std::clamp(deg, -90.0, 90.0);
        return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 90.0)));
    }

    /// @brief Scales longitude in [-180.0, +180.0] to signed 32-bit integer.
    /// @param[in] deg Longitude in degrees.
    /// @return Scaled int32 in [-2147483647, +2147483647].
    [[nodiscard]] inline std::int32_t scaleLongitude(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0;
        }
        constexpr double kMaxVal = 2147483647.0;
        const double clamped = std::clamp(deg, -180.0, 180.0);
        return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 180.0)));
    }

    /// @brief Scales corner offset in [-0.075, +0.075] to signed 16-bit integer.
    /// @param[in] offsetDeg Offset in degrees.
    /// @return Scaled int16 in [-32767, +32767].
    [[nodiscard]] inline std::int16_t scaleCornerOffset(double offsetDeg) noexcept
    {
        if (!std::isfinite(offsetDeg)) {
            return 0;
        }
        const double clamped = std::clamp(offsetDeg, -0.075, 0.075);
        return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 0.075)));
    }

    /// @brief Scales altitude in [-900.0, +19000.0] meters to unsigned 16-bit integer.
    /// @param[in] altM Altitude in meters.
    /// @return Scaled uint16 in [0, 65535].
    [[nodiscard]] inline std::uint16_t scaleAltitude(double altM) noexcept
    {
        if (!std::isfinite(altM)) {
            return 0U;
        }
        const double clamped = std::clamp(altM, -900.0, 19000.0);
        return static_cast<std::uint16_t>(std::round((clamped + 900.0) * (65535.0 / 19900.0)));
    }

    /// @brief Scales heading in [0.0, 360.0) to unsigned 16-bit integer.
    /// @param[in] deg Heading in degrees.
    /// @return Scaled uint16 in [0, 65535].
    [[nodiscard]] inline std::uint16_t scaleHeading(double deg) noexcept
    {
        const double wrapped = wrapAngle360(deg);
        return static_cast<std::uint16_t>(std::round(wrapped * (65535.0 / 360.0)));
    }

    /// @brief Scales pitch in [-20.0, +20.0] to signed 16-bit integer.
    /// @param[in] deg Pitch in degrees.
    /// @return Scaled int16 in [-32767, +32767].
    [[nodiscard]] inline std::int16_t scalePitch(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0;
        }
        const double clamped = std::clamp(deg, -20.0, 20.0);
        return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 20.0)));
    }

    /// @brief Scales roll in [-50.0, +50.0] to signed 16-bit integer.
    /// @param[in] deg Roll in degrees.
    /// @return Scaled int16 in [-32767, +32767].
    [[nodiscard]] inline std::int16_t scaleRoll(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0;
        }
        const double clamped = std::clamp(deg, -50.0, 50.0);
        return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 50.0)));
    }

    /// @brief Scales FOV in [0.0, 180.0] to unsigned 16-bit integer.
    /// @param[in] deg Horizontal or vertical FOV in degrees.
    /// @return Scaled uint16 in [0, 65535].
    [[nodiscard]] inline std::uint16_t scaleFov(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0U;
        }
        const double clamped = std::clamp(deg, 0.0, 180.0);
        return static_cast<std::uint16_t>(std::round(clamped * (65535.0 / 180.0)));
    }

    /// @brief Scales relative azimuth in [0.0, 360.0) to unsigned 32-bit integer.
    /// @param[in] deg Relative azimuth in degrees.
    /// @return Scaled uint32 in [0, 4294967295].
    [[nodiscard]] inline std::uint32_t scaleRelAzimuth(double deg) noexcept
    {
        const double wrapped = wrapAngle360(deg);
        return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
    }

    /// @brief Scales relative elevation in [-180.0, +180.0] to signed 32-bit integer.
    /// @param[in] deg Relative elevation in degrees.
    /// @return Scaled int32 in [-2147483647, +2147483647].
    [[nodiscard]] inline std::int32_t scaleRelElevation(double deg) noexcept
    {
        if (!std::isfinite(deg)) {
            return 0;
        }
        const double clamped = std::clamp(deg, -180.0, 180.0);
        return static_cast<std::int32_t>(std::round(clamped * (2147483647.0 / 180.0)));
    }

    /// @brief Scales relative roll in [0.0, 360.0) to unsigned 32-bit integer.
    /// @param[in] deg Relative roll in degrees.
    /// @return Scaled uint32 in [0, 4294967295].
    [[nodiscard]] inline std::uint32_t scaleRelRoll(double deg) noexcept
    {
        const double wrapped = wrapAngle360(deg);
        return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
    }

    /// @brief Scales slant range in [0.0, 5000000.0] meters to unsigned 32-bit integer.
    /// @param[in] rangeM Slant range in meters.
    /// @return Scaled uint32 in [0, 4294967295].
    [[nodiscard]] inline std::uint32_t scaleSlantRange(double rangeM) noexcept
    {
        if (!std::isfinite(rangeM)) {
            return 0U;
        }
        const double clamped = std::clamp(rangeM, 0.0, 5000000.0);
        return static_cast<std::uint32_t>(std::round(clamped * (4294967295.0 / 5000000.0)));
    }

    /// @brief Scales target width in [0.0, 10000.0] meters to unsigned 16-bit integer.
    /// @param[in] widthM Target width in meters.
    /// @return Scaled uint16 in [0, 65535].
    [[nodiscard]] inline std::uint16_t scaleTargetWidth(double widthM) noexcept
    {
        if (!std::isfinite(widthM)) {
            return 0U;
        }
        const double clamped = std::clamp(widthM, 0.0, 10000.0);
        return static_cast<std::uint16_t>(std::round(clamped * (65535.0 / 10000.0)));
    }

    /// @brief Clamps and casts confidence error metrics (CE90 / LE90) to uint16.
    /// @param[in] errM Confidence error in meters.
    /// @return Scaled uint16 clamped to [0, 65535] or 0 if not finite.
    [[nodiscard]] inline std::uint16_t scaleErrorCe90(double errM) noexcept
    {
        if (!std::isfinite(errM) || errM <= 0.0) {
            return 0U;
        }
        const double clamped = std::clamp(errM, 0.0, 65535.0);
        return static_cast<std::uint16_t>(std::round(clamped));
    }

} // namespace KlvScaling
} // namespace Klv
