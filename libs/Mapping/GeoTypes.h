#pragma once

/// @file GeoTypes.h
/// @brief Core geometric, geodetic, and tiling data structures for the Mapping engine.

#include "KlvTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Mapping {

/// @struct ScreenPoint
/// @brief 2D coordinate on a display surface / viewport in pixels.
struct ScreenPoint {
    double x { 0.0 }; ///< Horizontal pixel coordinate from top-left origin
    double y { 0.0 }; ///< Vertical pixel coordinate from top-left origin

    [[nodiscard]] constexpr bool operator==(const ScreenPoint& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] constexpr bool operator!=(const ScreenPoint& other) const noexcept {
        return !(*this == other);
    }
};

/// @struct ScreenRect
/// @brief 2D rectangle in viewport pixels.
struct ScreenRect {
    double x { 0.0 };      ///< Top-left X coordinate
    double y { 0.0 };      ///< Top-left Y coordinate
    double width { 0.0 };  ///< Rectangle width
    double height { 0.0 }; ///< Rectangle height

    [[nodiscard]] constexpr double left() const noexcept { return x; }
    [[nodiscard]] constexpr double top() const noexcept { return y; }
    [[nodiscard]] constexpr double right() const noexcept { return x + width; }
    [[nodiscard]] constexpr double bottom() const noexcept { return y + height; }

    [[nodiscard]] constexpr bool contains(const ScreenPoint& p) const noexcept {
        return p.x >= x && p.x <= (x + width) && p.y >= y && p.y <= (y + height);
    }
};

/// @struct TileCoord
/// @brief Discrete Slippy Map / XYZ tile coordinate.
struct TileCoord {
    int x { 0 };    ///< Column index [0, 2^zoom - 1]
    int y { 0 };    ///< Row index [0, 2^zoom - 1]
    int zoom { 0 }; ///< Zoom level [0, 22]

    [[nodiscard]] constexpr bool operator==(const TileCoord& other) const noexcept {
        return x == other.x && y == other.y && zoom == other.zoom;
    }

    [[nodiscard]] constexpr bool operator!=(const TileCoord& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const TileCoord& other) const noexcept {
        if (zoom != other.zoom) return zoom < other.zoom;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

/// @struct BoundingBox
/// @brief Geographic WGS-84 bounding box defined by cardinal coordinates.
struct BoundingBox {
    double north { 0.0 }; ///< Northernmost latitude [-90.0, 90.0] degrees
    double south { 0.0 }; ///< Southernmost latitude [-90.0, 90.0] degrees
    double east { 0.0 };  ///< Easternmost longitude [-180.0, 180.0] degrees
    double west { 0.0 };  ///< Westernmost longitude [-180.0, 180.0] degrees

    [[nodiscard]] bool contains(const Klv::GeoPoint2D& pt) const noexcept {
        const bool latOk = (pt.latitudeDeg >= south && pt.latitudeDeg <= north);
        const bool lonOk = (west <= east)
            ? (pt.longitudeDeg >= west && pt.longitudeDeg <= east)
            : (pt.longitudeDeg >= west || pt.longitudeDeg <= east); // Handles anti-meridian wrap
        return latOk && lonOk;
    }

    [[nodiscard]] bool intersects(const BoundingBox& other) const noexcept {
        return !(other.south > north || other.north < south || other.west > east || other.east < west);
    }
};

/// @struct TileData
/// @brief Raw encoded or decoded binary tile payload with metadata.
struct TileData {
    std::vector<std::uint8_t> bytes {};
    std::string mimeType { "image/png" };
    bool valid { false };
};

} // namespace Mapping

// Hash specialization for TileCoord to enable std::unordered_map
namespace std {
template <>
struct hash<Mapping::TileCoord> {
    std::size_t operator()(const Mapping::TileCoord& c) const noexcept {
        // Boost-inspired hash_combine
        std::size_t seed = 0;
        seed ^= std::hash<int>{}(c.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(c.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(c.zoom) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std
