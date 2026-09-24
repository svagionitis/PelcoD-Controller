#pragma once

/// @file ProceduralGridTileProvider.h
/// @brief Fallback tile generator that procedurally creates tactical coordinate grid tiles.

#include "ITileProvider.h"

namespace Mapping {

/// @class ProceduralGridTileProvider
/// @brief Generates lightweight tactical grid tiles for offline or unmapped areas.
class ProceduralGridTileProvider : public ITileProvider {
public:
    ProceduralGridTileProvider() = default;

    /// @brief Generates a synthetic tactical grid tile for the specified coordinate.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData containing raw uncompressed RGBA or PPM tile bytes.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Always returns true as procedural tiles can be generated for any coordinate.
    [[nodiscard]] bool hasTile(const TileCoord& /*coord*/) const noexcept override {
        return true;
    }

    /// @brief Returns provider name.
    [[nodiscard]] std::string providerName() const override {
        return "ProceduralGridTileProvider";
    }

    /// @brief Helper to directly generate a procedural grid tile without instantiating the class.
    [[nodiscard]] static TileData generateGridTile(const TileCoord& coord);
};

} // namespace Mapping
