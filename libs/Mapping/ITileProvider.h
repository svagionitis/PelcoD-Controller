#pragma once

/// @file ITileProvider.h
/// @brief Abstract interface for asynchronous or synchronous tile suppliers.

#include "GeoTypes.h"
#include <optional>
#include <string>

namespace Mapping {

/// @class ITileProvider
/// @brief Interface defining standard operations for querying, retrieving, and caching map tiles.
class ITileProvider {
public:
    virtual ~ITileProvider() = default;

    /// @brief Retrieves the binary tile payload for a specific coordinate.
    /// @param[in] coord Discrete tile coordinate (X, Y, Zoom).
    /// @return TileData payload if available, or std::nullopt.
    [[nodiscard]] virtual std::optional<TileData> getTile(const TileCoord& coord) = 0;

    /// @brief Checks if a tile is immediately available without downloading.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if available locally, false otherwise.
    [[nodiscard]] virtual bool hasTile(const TileCoord& coord) const noexcept = 0;

    /// @brief Returns the identifier name of the tile provider implementation.
    [[nodiscard]] virtual std::string providerName() const = 0;
};

} // namespace Mapping
