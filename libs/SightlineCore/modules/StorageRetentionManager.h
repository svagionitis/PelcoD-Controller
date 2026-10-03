#pragma once

/// @file StorageRetentionManager.h
/// @brief Manages storage quotas, pinned files, and automated FIFO circular pruning.
/// @details Protects critical recordings from deletion while unlinking older segments on low space.

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Sightline {

/// @struct FileMetadataEntry
/// @brief Represents a recorded file candidate for retention evaluation.
struct FileMetadataEntry {
    std::string filename {};                  ///< Base filename or relative path
    std::uint64_t sizeBytes { 0ULL };         ///< File size on disk in bytes
    std::uint64_t timestampUs { 0ULL };       ///< Creation/modification timestamp in microseconds
    bool isPinned { false };                  ///< Protected against automated FIFO deletion
};

/// @class StorageRetentionManager
/// @brief Implements FIFO pruning policies when storage reaches low-watermark thresholds.
class StorageRetentionManager {
public:
    StorageRetentionManager() = default;
    ~StorageRetentionManager() = default;

    /// @brief Marks a file as pinned, preventing automatic pruning.
    /// @param[in] filename Filename to protect.
    void pinFile(std::string_view filename);

    /// @brief Unpins a file, permitting automatic pruning.
    /// @param[in] filename Filename to unpin.
    void unpinFile(std::string_view filename);

    /// @brief Checks whether a file is pinned.
    /// @param[in] filename Filename to check.
    /// @return True if pinned.
    [[nodiscard]] bool isPinned(std::string_view filename) const;

    /// @brief Clears all pinned file entries.
    void clearPinned() noexcept;

    /// @brief Determines which files must be pruned to reclaim required storage bytes.
    /// @details Sorts unpinned files by timestamp ascending (oldest first) and accumulates
    ///          candidates until reclaimed bytes >= requiredReclaimBytes.
    /// @param[in] entries Available files on the storage medium.
    /// @param[in] requiredReclaimBytes Number of bytes that must be freed.
    /// @param[out] outReclaimedBytes Actual total bytes of selected candidates.
    /// @return List of filenames designated for unlinking.
    [[nodiscard]] std::vector<std::string> pruneOldest(
        const std::vector<FileMetadataEntry>& entries,
        std::uint64_t requiredReclaimBytes,
        std::uint64_t& outReclaimedBytes) const;

private:
    mutable std::mutex m_mutex;
    std::unordered_set<std::string> m_pinnedFiles;
};

} // namespace Sightline
