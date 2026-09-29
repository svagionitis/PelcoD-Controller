#pragma once

/// @file PfecTypes.h
/// @brief Strongly-typed data models and enums for FLIR Maritime proprietary NMEA 0183 ($PFEC) PTZ cameras.

#include <chrono>
#include <cstdint>

namespace Nmea {

/// @struct PfecGimbalPosition
/// @brief Azimuth and elevation angular orientation reported by FLIR camera head.
struct PfecGimbalPosition {
    double panDegrees { 0.0 }; ///< Azimuth angle in degrees [0.0 .. 360.0) or [-180.0 .. +180.0]
    double tiltDegrees { 0.0 }; ///< Elevation angle in degrees [-90.0 .. +90.0]
    std::chrono::steady_clock::time_point timestamp {};
    bool valid { false };
};

/// @enum FlirSensorType
/// @brief Optical sensor active on dual-sensor FLIR camera head.
enum class FlirSensorType : std::uint8_t {
    DaylightVisible = 0U, ///< Visible daylight EO optical sensor
    ThermalInfrared = 1U ///< Thermal LWIR / MWIR uncooled/cooled sensor
};

/// @enum FlirColorPalette
/// @brief Thermal infrared pseudo-color palettes supported by FLIR marine cameras.
enum class FlirColorPalette : std::uint8_t { WhiteHot = 0U, BlackHot = 1U, Rainbow = 2U, Ironbow = 3U, Sepia = 4U };

/// @enum FlirZoomLevel
/// @brief Digital zoom magnification levels supported by FLIR thermal/visible cameras.
enum class FlirZoomLevel : std::uint8_t {
    Zoom1x = 1U,
    Zoom2x = 2U,
    Zoom4x = 4U,
    Zoom8x = 8U
};

} // namespace Nmea
