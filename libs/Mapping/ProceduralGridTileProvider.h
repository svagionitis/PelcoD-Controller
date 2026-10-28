#pragma once

/// @file ProceduralGridTileProvider.h
/// @brief Fallback tile generator that procedurally creates tactical coordinate grid tiles.

#include <optional>
#include <string>

#include "GeoTypes.h"
#include "ITileProvider.h"

namespace Mapping {

/// @class ProceduralGridTileProvider
/// @brief Generates lightweight tactical grid tiles for offline or unmapped areas.
/// @details Produces a deterministic 256x256 binary PPM (P6) image with a dark tactical background,
///          64-pixel grid lines, and a cyan center reticle.
class ProceduralGridTileProvider : public ITileProvider {
public:
    /// @brief Default constructor.
    /// @details Stateless generator requiring no external resources.
    ProceduralGridTileProvider() = default;

    /// @brief Generates a synthetic tactical grid tile for the specified coordinate.
    /// @details Delegates directly to generateGridTile(coord).
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData containing raw uncompressed PPM tile bytes.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Always returns true as procedural tiles can be generated for any coordinate.
    /// @details Procedural generation requires no external files or network connectivity.
    /// @param[in] coord Discrete tile coordinate (unused).
    /// @return Always true.
    [[nodiscard]] bool hasTile(const TileCoord& coord) const noexcept override
    {
        (void)coord;
        return true;
    }

    /// @brief Returns provider name.
    /// @details Identifies this tier as the procedural fallback generator.
    /// @return Provider identifier string.
    [[nodiscard]] std::string providerName() const override
    {
        return "ProceduralGridTileProvider";
    }

    /// @brief Helper to directly generate a procedural grid tile without instantiating the class.
    /// @details Synthesizes a 256x256 RGB portable pixmap (image/x-portable-pixmap) buffer in memory.
    /// @param[in] coord Discrete tile coordinate.
    /// @return Generated TileData payload.
    [[nodiscard]] static TileData generateGridTile(const TileCoord& coord);
};

} // namespace Mapping
