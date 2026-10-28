/// @file ProceduralGridTileProvider.cpp
/// @brief Implementation of procedural tactical coordinate grid tile generator.

#include "ProceduralGridTileProvider.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "GeoTypes.h"

namespace Mapping {

{
    (void)coord;
    constexpr int kTileDim { 256 };
    const std::string header { "P6\n" + std::to_string(kTileDim) + " " + std::to_string(kTileDim) + "\n255\n" };

    TileData tile { };
    tile.mimeType = "image/x-portable-pixmap";
    tile.bytes.reserve(header.size() + static_cast<std::size_t>(kTileDim * kTileDim * 3));

    // Append PPM header
    for (const char ch : header) {
        tile.bytes.push_back(static_cast<std::uint8_t>(ch));
    }

    // Color definitions (RGB)
    constexpr std::uint8_t kBgR { 16U };
    constexpr std::uint8_t kBgG { 20U };
    constexpr std::uint8_t kBgB { 28U }; // #10141c Dark Tactical
    constexpr std::uint8_t kGridR { 36U };
    constexpr std::uint8_t kGridG { 48U };
    constexpr std::uint8_t kGridB { 68U }; // #243044 Grid lines
    constexpr std::uint8_t kCrossR { 0U };
    constexpr std::uint8_t kCrossG { 229U };
    constexpr std::uint8_t kCrossB { 255U }; // #00e5ff Tactical Cyan center cross

    for (int y { 0 }; y < kTileDim; ++y) {
        for (int x { 0 }; x < kTileDim; ++x) {
            // Center reticle (+- 8 pixels around 128, 128)
            const bool isCrossCenter { (x == 128 && y >= 120 && y <= 136) || (y == 128 && x >= 120 && x <= 136) };

            // Outer border and 64-pixel interval grid lines
            const bool isGridLine { x == 0 || y == 0 || x == (kTileDim - 1) || y == (kTileDim - 1) || (x % 64 == 0)
                || (y % 64 == 0) };

            if (isCrossCenter) {
                tile.bytes.push_back(kCrossR);
                tile.bytes.push_back(kCrossG);
                tile.bytes.push_back(kCrossB);
            } else if (isGridLine) {
                tile.bytes.push_back(kGridR);
                tile.bytes.push_back(kGridG);
                tile.bytes.push_back(kGridB);
            } else {
                tile.bytes.push_back(kBgR);
                tile.bytes.push_back(kBgG);
                tile.bytes.push_back(kBgB);
            }
        }
    }

    tile.valid = true;
    return tile;
}

std::optional<TileData> ProceduralGridTileProvider::getTile(const TileCoord& coord)
{
    return generateGridTile(coord);
}

} // namespace Mapping
