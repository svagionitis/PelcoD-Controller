#pragma once

/// @file ITileProvider.h
/// @brief Abstract interface for asynchronous or synchronous tile suppliers.

#include <optional>
#include <string>

#include "GeoTypes.h"

namespace Mapping {

/// @class ITileProvider
/// @brief Interface defining standard operations for querying, retrieving, and caching map tiles.
/// @details Implementations may read from disk, memory caches, procedural generators, or network sources.
class ITileProvider {
public:
    /// @brief Virtual destructor for polymorphic cleanup.
    /// @details Ensures derived tile provider instances are destroyed safely through base pointers.
    virtual ~ITileProvider() = default;

    /// @brief Retrieves the binary tile payload for a specific coordinate.
    /// @details Queries the underlying storage or generator tier for the requested (x, y, zoom) tile.
    /// @param[in] coord Discrete tile coordinate (X, Y, Zoom).
    /// @return TileData payload if available, or std::nullopt.
    [[nodiscard]] virtual std::optional<TileData> getTile(const TileCoord& coord) = 0;

    /// @brief Checks if a tile is immediately available without downloading.
    /// @details Queries local cache or filesystem presence without initiating external network I/O.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if available locally, false otherwise.
    [[nodiscard]] virtual bool hasTile(const TileCoord& coord) const = 0;

    /// @brief Returns the identifier name of the tile provider implementation.
    /// @details Used for diagnostics and provider tier identification.
    /// @return Provider name string.
    [[nodiscard]] virtual std::string providerName() const = 0;
};

} // namespace Mapping
