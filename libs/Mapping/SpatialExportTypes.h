#pragma once

/// @file SpatialExportTypes.h
/// @brief Geometric types, styles, and configurations for KML and GeoJSON spatial export.

#include <cstdint>
#include <cstdio>
#include <ctime>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace Mapping {

/// @struct SpatialPoint3D
/// @brief 3D geodetic point in WGS-84 (latitude, longitude, altitude).
/// @details Used as the fundamental 3D vertex coordinate across KML and GeoJSON export layers.
struct SpatialPoint3D {
    double latitudeDeg { 0.0 }; ///< Latitude [-90.0, +90.0] degrees
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0] degrees
    double altitudeM { 0.0 }; ///< Altitude above MSL or HAE in meters
};

/// @struct SpatialTrackPoint
/// @brief Time-stamped platform state for flight trajectory export.
/// @details Captures 3D position, Euler attitude angles, ground speed, and mission identifiers at a specific instant.
struct SpatialTrackPoint {
    std::uint64_t timestampUs { 0U }; ///< Epoch microseconds (Tag 2)
    SpatialPoint3D position { }; ///< Platform 3D position
    double headingDeg { 0.0 }; ///< Platform heading [0, 360) deg (Tag 5)
    double pitchDeg { 0.0 }; ///< Platform pitch [-90, +90] deg (Tag 6)
    double rollDeg { 0.0 }; ///< Platform roll [-180, +180] deg (Tag 7)
    double speedMps { 0.0 }; ///< Platform ground speed in meters/second
    std::string tailNumber { }; ///< Platform tail number / callsign (Tag 4)
    std::string missionId { }; ///< Mission identifier (Tag 3)
};

/// @struct FrustumMesh3D
/// @brief 3D sensor frustum geometry including apex, footprint base, and side faces.
/// @details Represents the volumetric viewing pyramid from the sensor apex to the 4-corner ground footprint.
struct FrustumMesh3D {
    std::uint64_t timestampUs { 0U }; ///< Epoch microseconds (Tag 2)
    SpatialPoint3D apex { }; ///< Camera / platform 3D position
    SpatialPoint3D targetCenter { }; ///< Ground frame center / boresight
    std::array<SpatialPoint3D, 4> base { }; ///< C1 (TL), C2 (TR), C3 (BR), C4 (BL)
    double hfovDeg { 0.0 }; ///< Horizontal field of view in degrees
    double vfovDeg { 0.0 }; ///< Vertical field of view in degrees
    double slantRangeM { 0.0 }; ///< Slant range in meters
    bool hasTargetCenter { false }; ///< True if target center point is valid
    bool valid { false }; ///< True if full geometry is complete
};

/// @struct SpatialVmtiTarget
/// @brief Time-stamped 3D geodetic target observation decoded from MISB ST 0903 VMTI.
/// @details Stores the resolved 3D target location, circular/linear error bounds, confidence, and motion state.
struct SpatialVmtiTarget {
    std::uint64_t timestampUs { 0U }; ///< Epoch microseconds (Tag 2)
    std::uint32_t targetId { 0U }; ///< VTargetPack unique target identifier
    SpatialPoint3D position { }; ///< Resolved target WGS-84 position (lat, lon, HAE)
    double ce90M { 0.0 }; ///< Horizontal circular error 90% in meters
    double le90M { 0.0 }; ///< Vertical linear error 90% in meters
    std::uint8_t confidence { 0U }; ///< Target confidence [0, 100]%
    std::uint8_t status { 0U }; ///< Detection status (1=Moving, 2=Stopped)
};

/// @brief Formats an epoch microsecond timestamp as an ISO-8601 UTC string (YYYY-MM-DDTHH:MM:SS.sssZ).
/// @details Returns an empty string when timestampUs == 0U. Uses thread-safe gmtime_s / gmtime_r.
/// @param[in] timestampUs Microseconds since UNIX epoch.
/// @return Formatted ISO-8601 UTC string, or empty string if timestampUs is zero.
[[nodiscard]] inline std::string formatIsoTime(std::uint64_t timestampUs)
{
    if (timestampUs == 0U) {
        return "";
    }
    const std::time_t sec { static_cast<std::time_t>(timestampUs / 1000000ULL) };
    const auto ms { static_cast<unsigned int>((timestampUs % 1000000ULL) / 1000ULL) };
    std::tm tmBuf { };
#if defined(_WIN32)
    gmtime_s(&tmBuf, &sec);
#else
    gmtime_r(&sec, &tmBuf);
#endif
    char buf[64] { };
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ", tmBuf.tm_year + 1900, tmBuf.tm_mon + 1,
        tmBuf.tm_mday, tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, ms);
    return std::string(buf);
}

/// @struct ColorRgba
/// @brief 32-bit RGBA color representation with KML and CSS converters.
/// @details Provides conversion helpers for both OGC KML little-endian hex (aabbggrr) and CSS hex (#rrggbb).
struct ColorRgba {
    std::uint8_t r { 255U }; ///< Red channel [0, 255]
    std::uint8_t g { 255U }; ///< Green channel [0, 255]
    std::uint8_t b { 255U }; ///< Blue channel [0, 255]
    std::uint8_t a { 255U }; ///< Alpha channel [0, 255] (255 = fully opaque)

    /// @brief Converts RGBA to KML aabbggrr hex string format.
    /// @details Serializes alpha, blue, green, and red channels in lowercase two-digit hexadecimal order.
    /// @return 8-character hex string (e.g., "ff0000ff" for opaque red).
    [[nodiscard]] std::string toKmlColor() const
    {
        std::ostringstream oss { };
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(a) << std::setw(2)
            << static_cast<int>(b) << std::setw(2) << static_cast<int>(g) << std::setw(2) << static_cast<int>(r);
        return oss.str();
    }

    /// @brief Converts RGBA to CSS hex string format (#rrggbb).
    /// @details Serializes red, green, and blue channels prefixed with '#'.
    /// @return CSS hex string (e.g. "#ff0000").
    [[nodiscard]] std::string toHexColor() const
    {
        std::ostringstream oss { };
        oss << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(r) << std::setw(2)
            << static_cast<int>(g) << std::setw(2) << static_cast<int>(b);
        return oss.str();
    }
};

/// @enum DecimationMode
/// @brief Sampling strategy for trajectory and frustum time-series filtering.
/// @details Selects whether telemetry frames are downsampled by time interval, ground distance, or heading change.
enum class DecimationMode {
    None, ///< Retain every telemetry frame without downsampling
    UniformTime, ///< Retain frames spaced by minimum time interval (seconds)
    DistanceThreshold, ///< Retain frames when platform moves beyond distance delta (meters)
    HeadingThreshold ///< Retain frames when platform heading changes beyond angular delta (degrees)
};

/// @struct DecimationConfig
/// @brief Configuration controlling downsampling of dense telemetry streams.
/// @details Holds threshold parameters for time, distance, and angular decimation policies.
struct DecimationConfig {
    DecimationMode mode { DecimationMode::UniformTime }; ///< Decimation mode
    double intervalSec { 0.2 }; ///< Used when mode == UniformTime (e.g., 5 Hz)
    double distanceDeltaM { 2.0 }; ///< Used when mode == DistanceThreshold (meters)
    double angleDeltaDeg { 1.0 }; ///< Used when mode == HeadingThreshold (degrees)
};

/// @struct KmlConfig
/// @brief Configuration options for Google Earth KML 2.2 export.
/// @details Controls layer visibility, TimeSpan generation, and RGBA styling for KML documents.
struct KmlConfig {
    std::string documentName { "STANAG 4609 Mission Export" }; ///< Document title
    std::string documentDescription { "Exported telemetry geometry." }; ///< Description
    bool enableGxTrack { true }; ///< Export Google Earth <gx:Track>
    bool enableVolumetricPyramid { true }; ///< Export 3D frustum pyramid mesh
    bool enableGroundFootprint { true }; ///< Export 2D ground footprint polygon
    bool enableBoresightRay { true }; ///< Export optical line-of-sight vector
    bool enableVmtiTargets { true }; ///< Export MISB ST 0903 VMTI moving target points
    bool enableTimeSpan { true }; ///< Add <TimeSpan> for 4D animation slider
    ColorRgba trackColor { 0U, 255U, 0U, 255U }; ///< Flight track line (green)
    ColorRgba frustumColor { 0U, 255U, 255U, 80U }; ///< 3D Frustum walls (translucent cyan)
    ColorRgba footprintColor { 255U, 255U, 0U, 120U }; ///< Ground footprint fill (translucent yellow)
    ColorRgba boresightColor { 255U, 0U, 0U, 200U }; ///< Optical LOS vector (red)
    ColorRgba vmtiColor { 255U, 140U, 0U, 255U }; ///< VMTI target marker (orange)
};

/// @struct GeoJsonConfig
/// @brief Configuration options for RFC 7946 GeoJSON export.
/// @details Controls feature layer inclusion flags and floating-point coordinate decimal precision.
struct GeoJsonConfig {
    bool includeFlightTrack { true }; ///< Export flight path LineString
    bool includeFootprints { true }; ///< Export ground footprint Polygons
    bool includeVolumetricFrustums { true }; ///< Export 3D Frustum MultiPolygons
    bool includeTargetPoints { true }; ///< Export frame center Target Points
    bool includeVmtiTargets { true }; ///< Export MISB ST 0903 VMTI target Points
    int coordinatePrecision { 7 }; ///< Decimal precision for coordinates
};

} // namespace Mapping
