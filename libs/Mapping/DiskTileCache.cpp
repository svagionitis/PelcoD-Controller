/// @file DiskTileCache.cpp
/// @brief Implementation of persistent filesystem tile storage and offline directory lookup.

#include "DiskTileCache.h"

#include <cstddef>

#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "MercatorProjection.h"

namespace Mapping {

namespace {
    [[nodiscard]] constexpr bool isValidTileCoord(const TileCoord& coord) noexcept
    {
        if (coord.zoom < MercatorProjection::kMinZoom || coord.zoom > MercatorProjection::kMaxZoom) {
            return false;
        }
        const int maxTiles { 1 << coord.zoom };
        return coord.x >= 0 && coord.x < maxTiles && coord.y >= 0 && coord.y < maxTiles;
    }
} // namespace

DiskTileCache::DiskTileCache(std::filesystem::path primaryCacheDir)
    : m_primaryDir(std::move(primaryCacheDir))
{
}

void DiskTileCache::addSearchDirectory(std::filesystem::path offlineDir)
{
    std::lock_guard<std::mutex> lock(m_ioMutex);
    m_searchDirs.push_back(std::move(offlineDir));
}

std::optional<std::filesystem::path> DiskTileCache::findTileFile(const TileCoord& coord) const
{
    if (!isValidTileCoord(coord)) {
        return std::nullopt;
    }

    const std::string zStr { std::to_string(coord.zoom) };
    const std::string xStr { std::to_string(coord.x) };
    const std::string yStr { std::to_string(coord.y) };

    static constexpr std::array<std::string_view, 3> kExtensions { ".png", ".jpg", ".jpeg" };

    auto checkInDir = [&](const std::filesystem::path& base) -> std::optional<std::filesystem::path> {
        if (base.empty()) {
            return std::nullopt;
        }
        const std::filesystem::path tileDir { base / zStr / xStr };
        for (const std::string_view ext : kExtensions) {
            std::string fileName { yStr };
            fileName.append(ext);
            const std::filesystem::path candidate { tileDir / fileName };
            std::error_code ec { };
            if (std::filesystem::exists(candidate, ec) && !ec) {
                return candidate;
            }
        }
        return std::nullopt;
    };

    // 1. Check primary directory
    if (auto found = checkInDir(m_primaryDir); found.has_value()) {
        return found;
    }

    // 2. Check search directories
    for (const auto& dir : m_searchDirs) {
        if (auto found = checkInDir(dir); found.has_value()) {
            return found;
        }
    }

    return std::nullopt;
}

bool DiskTileCache::hasTile(const TileCoord& coord) const
{
    if (!isValidTileCoord(coord)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_ioMutex);
    return findTileFile(coord).has_value();
}

std::optional<TileData> DiskTileCache::getTile(const TileCoord& coord)
{
    if (!isValidTileCoord(coord)) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_ioMutex);

    const auto filePath { findTileFile(coord) };
    if (!filePath.has_value()) {
        return std::nullopt;
    }

    std::ifstream file(*filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    static constexpr std::streamoff kMaxTileFileBytes { 16 * 1024 * 1024 };
    const auto fileSize { file.tellg() };
    if (fileSize <= 0 || fileSize > kMaxTileFileBytes) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::beg);
    TileData data { };
    data.bytes.resize(static_cast<std::size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(data.bytes.data()), fileSize)) {
        return std::nullopt;
    }

    const std::string ext { filePath->extension().string() };
    if (ext == ".jpg" || ext == ".jpeg") {
        data.mimeType = "image/jpeg";
    } else {
        data.mimeType = "image/png";
    }

    data.valid = true;
    return data;
}

bool DiskTileCache::storeTile(const TileCoord& coord, const TileData& data)
{
    if (!isValidTileCoord(coord) || data.bytes.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_ioMutex);

    const std::string zStr { std::to_string(coord.zoom) };
    const std::string xStr { std::to_string(coord.x) };
    const std::string yStr { std::to_string(coord.y) };
    const std::string ext { (data.mimeType == "image/jpeg") ? ".jpg" : ".png" };

    const std::filesystem::path dirPath { m_primaryDir / zStr / xStr };
    std::error_code ec { };
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path targetFile { dirPath / (yStr + ext) };
    std::ofstream file(targetFile, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.bytes.data()), static_cast<std::streamsize>(data.bytes.size()));
    return file.good();
}

} // namespace Mapping
