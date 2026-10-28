#pragma once

/// @file DiskTileCache.h
/// @brief Persistent filesystem tile storage and offline directory reader.

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ITileProvider.h"

namespace Mapping {

/// @class DiskTileCache
/// @brief Manages reading and writing tiles to a hierarchical filesystem structure:
///        {root}/{zoom}/{x}/{y}.png
class DiskTileCache : public ITileProvider {
public:
    /// @brief Constructs a DiskTileCache targeting a primary directory.
    /// @param[in] primaryCacheDir Path to read and write tiles.
    explicit DiskTileCache(std::filesystem::path primaryCacheDir);

    /// @brief Adds a read-only secondary directory to search when tiles are missed in the primary.
    /// @param[in] offlineDir Secondary path (e.g. read-only offline map repository).
    void addSearchDirectory(std::filesystem::path offlineDir);

    /// @brief Retrieves tile data from disk if it exists in primary or search paths.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData if found, or std::nullopt.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Checks whether the tile exists on disk.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if file exists on disk, false otherwise.
    [[nodiscard]] bool hasTile(const TileCoord& coord) const override;

    /// @brief Writes a tile to the primary cache directory.
    /// @param[in] coord Discrete tile coordinate.
    /// @param[in] data Binary tile payload.
    /// @return True if successfully written, false on I/O error.
    [[nodiscard]] bool storeTile(const TileCoord& coord, const TileData& data);

    /// @brief Returns provider identification name.
    /// @return Provider identifier string.
    [[nodiscard]] std::string providerName() const override
    {
        return "DiskTileCache";
    }

    /// @brief Returns the primary cache directory.
    /// @return Const reference to primary cache directory path.
    [[nodiscard]] const std::filesystem::path& primaryDirectory() const noexcept
    {
        return m_primaryDir;
    }

private:
    [[nodiscard]] std::optional<std::filesystem::path> findTileFile(const TileCoord& coord) const;

    std::filesystem::path m_primaryDir { };
    std::vector<std::filesystem::path> m_searchDirs { };
    mutable std::mutex m_ioMutex { };
};

} // namespace Mapping
