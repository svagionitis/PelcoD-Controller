#pragma once

/// @file GeoTypes.h
/// @brief Core geometric, geodetic, and tiling data structures for the Mapping engine.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string>
#include <vector>

#include "KlvTypes.h"

namespace Mapping {

/// @struct ScreenPoint
/// @brief 2D coordinate on a display surface / viewport in pixels.
/// @details Uses a top-left origin where +X points right and +Y points down.
struct ScreenPoint {
    double x { 0.0 }; ///< Horizontal pixel coordinate from top-left origin
    double y { 0.0 }; ///< Vertical pixel coordinate from top-left origin

    /// @brief Equality comparison operator.
    /// @details Compares horizontal and vertical pixel coordinates for exact equality.
    /// @param[in] other Right-hand ScreenPoint operand.
    /// @return True if both coordinates are equal.
    [[nodiscard]] constexpr bool operator==(const ScreenPoint& other) const noexcept
    {
        return x == other.x && y == other.y;
    }

    /// @brief Inequality comparison operator.
    /// @details Compares horizontal and vertical pixel coordinates for inequality.
    /// @param[in] other Right-hand ScreenPoint operand.
    /// @return True if coordinates differ.
    [[nodiscard]] constexpr bool operator!=(const ScreenPoint& other) const noexcept
    {
        return !(*this == other);
    }
};

/// @struct ScreenRect
/// @brief 2D rectangle in viewport pixels.
/// @details Represents an axis-aligned bounding box in screen space defined by its top-left origin and dimensions.
struct ScreenRect {
    double x { 0.0 }; ///< Top-left X coordinate
    double y { 0.0 }; ///< Top-left Y coordinate
    double width { 0.0 }; ///< Rectangle width
    double height { 0.0 }; ///< Rectangle height

    /// @brief Returns the left X boundary of the rectangle.
    /// @details Equivalent to the top-left X coordinate.
    /// @return Left X coordinate in pixels.
    [[nodiscard]] constexpr double left() const noexcept
    {
        return x;
    }

    /// @brief Returns the top Y boundary of the rectangle.
    /// @details Equivalent to the top-left Y coordinate.
    /// @return Top Y coordinate in pixels.
    [[nodiscard]] constexpr double top() const noexcept
    {
        return y;
    }

    /// @brief Returns the right X boundary of the rectangle.
    /// @details Computed as x + width.
    /// @return Right X coordinate in pixels.
    [[nodiscard]] constexpr double right() const noexcept
    {
        return x + width;
    }

    /// @brief Returns the bottom Y boundary of the rectangle.
    /// @details Computed as y + height.
    /// @return Bottom Y coordinate in pixels.
    [[nodiscard]] constexpr double bottom() const noexcept
    {
        return y + height;
    }

    /// @brief Tests whether a 2D pixel coordinate lies within or on the rectangle bounds.
    /// @details Evaluates closed interval inclusion along both X and Y axes.
    /// @param[in] p Screen point to test.
    /// @return True if point is inside or on the boundary.
    [[nodiscard]] constexpr bool contains(const ScreenPoint& p) const noexcept
    {
        return p.x >= x && p.x <= (x + width) && p.y >= y && p.y <= (y + height);
    }
};

/// @struct TileCoord
/// @brief Discrete Slippy Map / XYZ tile coordinate.
/// @details Identifies a single raster tile in a Web Mercator pyramid by column (x), row (y), and zoom level.
struct TileCoord {
    int x { 0 }; ///< Column index [0, 2^zoom - 1]
    int y { 0 }; ///< Row index [0, 2^zoom - 1]
    int zoom { 0 }; ///< Zoom level [0, 22]

    /// @brief Equality comparison operator.
    /// @details Compares x, y, and zoom indices for exact equality.
    /// @param[in] other Right-hand TileCoord operand.
    /// @return True if x, y, and zoom match.
    [[nodiscard]] constexpr bool operator==(const TileCoord& other) const noexcept
    {
        return x == other.x && y == other.y && zoom == other.zoom;
    }

    /// @brief Inequality comparison operator.
    /// @details Compares x, y, and zoom indices for inequality.
    /// @param[in] other Right-hand TileCoord operand.
    /// @return True if any component differs.
    [[nodiscard]] constexpr bool operator!=(const TileCoord& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Strict weak ordering comparison operator.
    /// @details Orders lexicographically by zoom level first, then column x, then row y.
    /// @param[in] other Right-hand TileCoord operand.
    /// @return True if this tile coordinate precedes other.
    [[nodiscard]] constexpr bool operator<(const TileCoord& other) const noexcept
    {
        if (zoom != other.zoom) {
            return zoom < other.zoom;
        }
        if (x != other.x) {
            return x < other.x;
        }
        return y < other.y;
    }
};

/// @struct BoundingBox
/// @brief Geographic WGS-84 bounding box defined by cardinal coordinates.
/// @details Supports standard bounding boxes (west <= east) as well as boxes spanning the +-180 deg anti-meridian (west
/// > east).
struct BoundingBox {
    double north { 0.0 }; ///< Northernmost latitude [-90.0, 90.0] degrees
    double south { 0.0 }; ///< Southernmost latitude [-90.0, 90.0] degrees
    double east { 0.0 }; ///< Easternmost longitude [-180.0, 180.0] degrees
    double west { 0.0 }; ///< Westernmost longitude [-180.0, 180.0] degrees

    /// @brief Tests whether a geodetic point lies inside the bounding box, including across the anti-meridian.
    /// @details When west > east, the box wraps across the +-180 deg International Date Line.
    /// @param[in] pt WGS-84 latitude/longitude point.
    /// @return True if pt lies within the bounding box.
    [[nodiscard]] constexpr bool contains(const Klv::GeoPoint2D& pt) const noexcept
    {
        const bool latOk { pt.latitudeDeg >= south && pt.latitudeDeg <= north };
        const bool lonOk { (west <= east) ? (pt.longitudeDeg >= west && pt.longitudeDeg <= east)
                                          : (pt.longitudeDeg >= west || pt.longitudeDeg <= east) };
        return latOk && lonOk;
    }

    /// @brief Tests whether two geographic bounding boxes overlap, including across the +-180 deg anti-meridian.
    /// @details Decomposes anti-meridian-crossing bounding boxes (where west > east) into two closed longitude
    ///          intervals [west, 180.0] and [-180.0, east] prior to testing interval overlap.
    /// @param[in] other Bounding box to test against.
    /// @return True if the two bounding boxes share any common area or boundary.
    [[nodiscard]] constexpr bool intersects(const BoundingBox& other) const noexcept
    {
        if (other.south > north || other.north < south) {
            return false;
        }
        const bool thisWraps { west > east };
        const bool otherWraps { other.west > other.east };
        if (!thisWraps && !otherWraps) {
            return intervalsOverlap(west, east, other.west, other.east);
        }
        if (thisWraps && !otherWraps) {
            return intervalsOverlap(west, 180.0, other.west, other.east)
                || intervalsOverlap(-180.0, east, other.west, other.east);
        }
        if (!thisWraps && otherWraps) {
            return intervalsOverlap(west, east, other.west, 180.0) || intervalsOverlap(west, east, -180.0, other.east);
        }
        return true;
    }

private:
    [[nodiscard]] static constexpr bool intervalsOverlap(double aMin, double aMax, double bMin, double bMax) noexcept
    {
        return aMin <= bMax && bMin <= aMax;
    }
};

/// @struct TileData
/// @brief Raw encoded or decoded binary tile payload with metadata.
/// @details Encapsulates the raw image bytes (PNG, JPEG, or PPM), MIME type string, and validity status.
struct TileData {
    std::vector<std::uint8_t> bytes { }; ///< Raw encoded tile image payload bytes
    std::string mimeType { "image/png" }; ///< IANA MIME type identifier for the image format
    bool valid { false }; ///< True if the tile payload was successfully loaded or generated
};

} // namespace Mapping

// Hash specialization for TileCoord to enable std::unordered_map
namespace std {
/// @struct hash<Mapping::TileCoord>
/// @brief Standard hash specialization for Mapping::TileCoord.
/// @details Combines x, y, and zoom hashes using a Boost-inspired bit-mixing step for unordered containers.
template <> struct hash<Mapping::TileCoord> {
    /// @brief Computes a combined hash code for a TileCoord.
    /// @details Mixes the integer hashes of x, y, and zoom into a single std::size_t seed.
    /// @param[in] c Tile coordinate to hash.
    /// @return Computed hash value.
    [[nodiscard]] std::size_t operator()(const Mapping::TileCoord& c) const noexcept
    {
        // Boost-inspired hash_combine
        std::size_t seed { 0U };
        seed ^= std::hash<int> { }(c.x) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= std::hash<int> { }(c.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= std::hash<int> { }(c.zoom) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};
} // namespace std
