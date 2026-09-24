#include "DiskTileCache.h"

#include <fstream>

namespace Mapping {

DiskTileCache::DiskTileCache(std::filesystem::path primaryCacheDir)
    : m_primaryDir(std::move(primaryCacheDir)) {}

void DiskTileCache::addSearchDirectory(std::filesystem::path offlineDir) {
    std::lock_guard<std::mutex> lock(m_ioMutex);
    m_searchDirs.push_back(std::move(offlineDir));
}

std::optional<std::filesystem::path> DiskTileCache::findTileFile(const TileCoord& coord) const noexcept {
    const std::string zStr = std::to_string(coord.zoom);
    const std::string xStr = std::to_string(coord.x);
    const std::string yStr = std::to_string(coord.y);

    const std::vector<std::string> extensions { ".png", ".jpg", ".jpeg" };

    // Lambda to check directory
    auto checkInDir = [&](const std::filesystem::path& base) -> std::optional<std::filesystem::path> {
        const std::filesystem::path tileDir = base / zStr / xStr;
        for (const auto& ext : extensions) {
            std::filesystem::path candidate = tileDir / (yStr + ext);
            std::error_code ec;
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

bool DiskTileCache::hasTile(const TileCoord& coord) const noexcept {
    std::lock_guard<std::mutex> lock(m_ioMutex);
    return findTileFile(coord).has_value();
}

std::optional<TileData> DiskTileCache::getTile(const TileCoord& coord) {
    std::lock_guard<std::mutex> lock(m_ioMutex);

    const auto filePath = findTileFile(coord);
    if (!filePath.has_value()) {
        return std::nullopt;
    }

    std::ifstream file(*filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const auto fileSize = file.tellg();
    if (fileSize <= 0) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::beg);
    TileData data;
    data.bytes.resize(static_cast<std::size_t>(fileSize));

    if (!file.read(reinterpret_cast<char*>(data.bytes.data()), fileSize)) {
        return std::nullopt;
    }

    const std::string ext = filePath->extension().string();
    if (ext == ".jpg" || ext == ".jpeg") {
        data.mimeType = "image/jpeg";
    } else {
        data.mimeType = "image/png";
    }

    data.valid = true;
    return data;
}

bool DiskTileCache::storeTile(const TileCoord& coord, const TileData& data) {
    if (data.bytes.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_ioMutex);

    const std::string zStr = std::to_string(coord.zoom);
    const std::string xStr = std::to_string(coord.x);
    const std::string yStr = std::to_string(coord.y);
    const std::string ext = (data.mimeType == "image/jpeg") ? ".jpg" : ".png";

    const std::filesystem::path dirPath = m_primaryDir / zStr / xStr;
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path targetFile = dirPath / (yStr + ext);
    std::ofstream file(targetFile, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.bytes.data()), static_cast<std::streamsize>(data.bytes.size()));
    return file.good();
}

} // namespace Mapping
