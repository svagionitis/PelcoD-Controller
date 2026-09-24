#include "ProceduralGridTileProvider.h"

#include <string>

namespace Mapping {

TileData ProceduralGridTileProvider::generateGridTile(const TileCoord& /*coord*/) {
    constexpr int kTileDim = 256;
    const std::string header = "P6\n" + std::to_string(kTileDim) + " " + std::to_string(kTileDim) + "\n255\n";

    TileData tile;
    tile.mimeType = "image/x-portable-pixmap";
    tile.bytes.reserve(header.size() + static_cast<std::size_t>(kTileDim * kTileDim * 3));

    // Append PPM header
    tile.bytes.insert(tile.bytes.end(), header.begin(), header.end());

    // Color definitions (RGB)
    constexpr std::uint8_t kBgR = 16, kBgG = 20, kBgB = 28;       // #10141c Dark Tactical
    constexpr std::uint8_t kGridR = 36, kGridG = 48, kGridB = 68;   // #243044 Grid lines
    constexpr std::uint8_t kCrossR = 0, kCrossG = 229, kCrossB = 255; // #00e5ff Tactical Cyan center cross

    for (int y = 0; y < kTileDim; ++y) {
        for (int x = 0; x < kTileDim; ++x) {
            // Center reticle (+- 8 pixels around 128, 128)
            const bool isCrossCenter = ((x == 128 && y >= 120 && y <= 136) ||
                                        (y == 128 && x >= 120 && x <= 136));

            // Outer border and 64-pixel interval grid lines
            const bool isGridLine = (x == 0 || y == 0 || x == (kTileDim - 1) || y == (kTileDim - 1) ||
                                     (x % 64 == 0) || (y % 64 == 0));

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

std::optional<TileData> ProceduralGridTileProvider::getTile(const TileCoord& coord) {
    return generateGridTile(coord);
}

} // namespace Mapping
