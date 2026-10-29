#pragma once

/// @file NmeaValueObjects.h
/// @brief Strongly-typed C++17 domain value objects for maritime coordinates, headings, velocities, and MMSI.
/// @details Eliminates Primitive Obsession across the NMEA subsystem per MISRA C++:2023 and SEI CERT C++.

#include <cstdint>

namespace Nmea {

/// @enum AisBeaconType
/// @brief Classification of AIS transponder based on ITU-R M.585-7 MMSI numbering formats.
enum class AisBeaconType : std::uint8_t {
    None = 0U,
    StandardVessel = 1U,
    AisSart = 2U, ///< 970 prefix (Search and Rescue Transponder)
    AisMob = 3U, ///< 972 prefix (Man Overboard Device)
    EpirbAis = 4U, ///< 974 prefix (Float-Free EPIRB with AIS)
    SarAircraft = 5U ///< 111 prefix (Search and Rescue Aircraft)
};

/// @class Latitude
/// @brief Strongly-typed geodetic latitude value object clamped to [-90.0, +90.0] degrees.
class Latitude {
public:
    /// @brief Default constructor initializing to 0.0 degrees (Equator).
    constexpr Latitude() noexcept = default;

    /// @brief Explicit constructor enforcing geodetic bounds [-90.0, +90.0].
    /// @param[in] deg Decimal degrees (North positive, South negative).
    explicit constexpr Latitude(double deg) noexcept
        : m_degrees((deg != deg) ? 0.0 : (deg > 90.0 ? 90.0 : (deg < -90.0 ? -90.0 : deg)))
    {
    }

    /// @brief Returns latitude in decimal degrees.
    /// @return Decimal degrees [-90.0 .. +90.0].
    [[nodiscard]] constexpr double degrees() const noexcept
    {
        return m_degrees;
    }

    /// @brief Named accessor alias for value extraction.
    /// @return Decimal degrees [-90.0 .. +90.0].
    [[nodiscard]] constexpr double value() const noexcept
    {
        return m_degrees;
    }

    /// @brief Verifies that latitude is within allowable geodetic bounds.
    /// @return True if between -90.0 and +90.0.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_degrees >= -90.0 && m_degrees <= 90.0;
    }

    [[nodiscard]] constexpr bool operator==(const Latitude& other) const noexcept
    {
        return m_degrees == other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator!=(const Latitude& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const Latitude& other) const noexcept
    {
        return m_degrees < other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator<=(const Latitude& other) const noexcept
    {
        return m_degrees <= other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator>(const Latitude& other) const noexcept
    {
        return m_degrees > other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator>=(const Latitude& other) const noexcept
    {
        return m_degrees >= other.m_degrees;
    }

private:
    double m_degrees { 0.0 };
};

/// @class Longitude
/// @brief Strongly-typed geodetic longitude value object normalized to [-180.0, +180.0] degrees.
class Longitude {
public:
    /// @brief Default constructor initializing to 0.0 degrees (Prime Meridian).
    constexpr Longitude() noexcept = default;

    /// @brief Explicit constructor normalizing coordinates circularly to [-180.0, +180.0].
    /// @param[in] deg Decimal degrees (East positive, West negative).
    explicit constexpr Longitude(double deg) noexcept
        : m_degrees(normalize(deg))
    {
    }

    /// @brief Returns longitude in decimal degrees.
    /// @return Decimal degrees [-180.0 .. +180.0].
    [[nodiscard]] constexpr double degrees() const noexcept
    {
        return m_degrees;
    }

    /// @brief Named accessor alias for value extraction.
    /// @return Decimal degrees [-180.0 .. +180.0].
    [[nodiscard]] constexpr double value() const noexcept
    {
        return m_degrees;
    }

    /// @brief Verifies that longitude is within allowable geodetic bounds.
    /// @return True if between -180.0 and +180.0.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_degrees >= -180.0 && m_degrees <= 180.0;
    }

    [[nodiscard]] constexpr bool operator==(const Longitude& other) const noexcept
    {
        return m_degrees == other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator!=(const Longitude& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const Longitude& other) const noexcept
    {
        return m_degrees < other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator<=(const Longitude& other) const noexcept
    {
        return m_degrees <= other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator>(const Longitude& other) const noexcept
    {
        return m_degrees > other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator>=(const Longitude& other) const noexcept
    {
        return m_degrees >= other.m_degrees;
    }

private:
    [[nodiscard]] static constexpr double normalize(double deg) noexcept
    {
        if (deg != deg) {
            return 0.0;
        }
        if (deg > 360.0 || deg < -360.0) {
            const auto turns = static_cast<std::int64_t>(deg / 360.0);
            deg -= static_cast<double>(turns) * 360.0;
        }
        while (deg > 180.0) {
            deg -= 360.0;
        }
        while (deg < -180.0) {
            deg += 360.0;
        }
        return deg;
    }

    double m_degrees { 0.0 };
};

/// @class HeadingDegrees
/// @brief Strongly-typed vessel heading value object circularly normalized to [0.0, 360.0) degrees.
class HeadingDegrees {
public:
    /// @brief Default constructor initializing to 0.0 degrees (North).
    constexpr HeadingDegrees() noexcept = default;

    /// @brief Explicit constructor wrapping heading circularly into [0.0, 360.0).
    /// @param[in] deg Azimuth / heading angle in degrees.
    explicit constexpr HeadingDegrees(double deg) noexcept
        : m_degrees(normalize(deg))
    {
    }

    /// @brief Returns heading in degrees [0.0 .. 360.0).
    /// @return Heading in degrees.
    [[nodiscard]] constexpr double degrees() const noexcept
    {
        return m_degrees;
    }

    /// @brief Named accessor alias for value extraction.
    /// @return Heading in degrees.
    [[nodiscard]] constexpr double value() const noexcept
    {
        return m_degrees;
    }

    /// @brief Verifies that heading is within the interval [0.0, 360.0).
    /// @return True if valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_degrees >= 0.0 && m_degrees < 360.0;
    }

    [[nodiscard]] constexpr bool operator==(const HeadingDegrees& other) const noexcept
    {
        return m_degrees == other.m_degrees;
    }

    [[nodiscard]] constexpr bool operator!=(const HeadingDegrees& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const HeadingDegrees& other) const noexcept
    {
        return m_degrees < other.m_degrees;
    }

private:
    [[nodiscard]] static constexpr double normalize(double deg) noexcept
    {
        if (deg != deg) {
            return 0.0;
        }
        if (deg >= 360.0 || deg < -360.0) {
            const auto turns = static_cast<std::int64_t>(deg / 360.0);
            deg -= static_cast<double>(turns) * 360.0;
        }
        while (deg >= 360.0) {
            deg -= 360.0;
        }
        while (deg < 0.0) {
            deg += 360.0;
        }
        return deg;
    }

    double m_degrees { 0.0 };
};

/// @class SpeedKnots
/// @brief Strongly-typed velocity value object clamped to non-negative values (>= 0.0 knots).
class SpeedKnots {
public:
    /// @brief Default constructor initializing to 0.0 knots (Stationary).
    constexpr SpeedKnots() noexcept = default;

    /// @brief Explicit constructor clamping negative velocities to 0.0.
    /// @param[in] knots Speed in nautical miles per hour.
    explicit constexpr SpeedKnots(double knots) noexcept
        : m_knots((knots != knots || knots < 0.0) ? 0.0 : knots)
    {
    }

    /// @brief Returns speed magnitude in knots.
    /// @return Speed in knots (>= 0.0).
    [[nodiscard]] constexpr double knots() const noexcept
    {
        return m_knots;
    }

    /// @brief Named accessor alias for value extraction.
    /// @return Speed in knots.
    [[nodiscard]] constexpr double value() const noexcept
    {
        return m_knots;
    }

    [[nodiscard]] constexpr bool operator==(const SpeedKnots& other) const noexcept
    {
        return m_knots == other.m_knots;
    }

    [[nodiscard]] constexpr bool operator!=(const SpeedKnots& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const SpeedKnots& other) const noexcept
    {
        return m_knots < other.m_knots;
    }

    [[nodiscard]] constexpr bool operator<=(const SpeedKnots& other) const noexcept
    {
        return m_knots <= other.m_knots;
    }

    [[nodiscard]] constexpr bool operator>(const SpeedKnots& other) const noexcept
    {
        return m_knots > other.m_knots;
    }

    [[nodiscard]] constexpr bool operator>=(const SpeedKnots& other) const noexcept
    {
        return m_knots >= other.m_knots;
    }

private:
    double m_knots { 0.0 };
};

/// @class Mmsi
/// @brief Strongly-typed Maritime Mobile Service Identity value object conforming to ITU-R M.585-7.
class Mmsi {
public:
    /// @brief Default constructor initializing to 0 (Unassigned).
    constexpr Mmsi() noexcept = default;

    /// @brief Explicit constructor validating 9-digit MMSI range [000000000, 999999999].
    /// @param[in] val 9-digit MMSI integer.
    explicit constexpr Mmsi(std::uint32_t val) noexcept
        : m_value(val > 999999999U ? 0U : val)
    {
    }

    /// @brief Assignment operator from raw 32-bit unsigned integer enforcing MMSI bounds.
    /// @param[in] val 9-digit MMSI integer.
    /// @return Reference to self.
    constexpr Mmsi& operator=(std::uint32_t val) noexcept
    {
        m_value = (val > 999999999U ? 0U : val);
        return *this;
    }

    /// @brief Returns raw 32-bit unsigned MMSI value.
    /// @return 9-digit MMSI number.
    [[nodiscard]] constexpr std::uint32_t value() const noexcept
    {
        return m_value;
    }

    /// @brief Named accessor alias for raw value extraction.
    /// @return 9-digit MMSI number.
    [[nodiscard]] constexpr std::uint32_t raw() const noexcept
    {
        return m_value;
    }

    /// @brief Checks whether the MMSI is assigned and within valid 9-digit bounds.
    /// @return True if between 1 and 999999999.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_value > 0U && m_value <= 999999999U;
    }

    /// @brief Classifies the MMSI into an emergency beacon or standard vessel type.
    /// @return Classified AisBeaconType.
    [[nodiscard]] constexpr AisBeaconType beaconType() const noexcept
    {
        if (m_value == 0U) {
            return AisBeaconType::None;
        }
        const std::uint32_t prefix = m_value / 1000000U;
        if (prefix == 970U) {
            return AisBeaconType::AisSart;
        }
        if (prefix == 972U) {
            return AisBeaconType::AisMob;
        }
        if (prefix == 974U) {
            return AisBeaconType::EpirbAis;
        }
        if (prefix == 111U) {
            return AisBeaconType::SarAircraft;
        }
        return AisBeaconType::StandardVessel;
    }

    /// @brief Checks if the MMSI belongs to an active life-saving emergency distress beacon.
    /// @return True if SART, MOB, EPIRB, or SAR aircraft.
    [[nodiscard]] constexpr bool isEmergencyBeacon() const noexcept
    {
        const auto type = beaconType();
        return type == AisBeaconType::AisSart || type == AisBeaconType::AisMob || type == AisBeaconType::EpirbAis
            || type == AisBeaconType::SarAircraft;
    }

    [[nodiscard]] constexpr bool operator==(const Mmsi& other) const noexcept
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] constexpr bool operator!=(const Mmsi& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const Mmsi& other) const noexcept
    {
        return m_value < other.m_value;
    }

    [[nodiscard]] constexpr bool operator==(std::uint32_t rawVal) const noexcept
    {
        return m_value == rawVal;
    }

    [[nodiscard]] constexpr bool operator!=(std::uint32_t rawVal) const noexcept
    {
        return m_value != rawVal;
    }

private:
    std::uint32_t m_value { 0U };
};

} // namespace Nmea