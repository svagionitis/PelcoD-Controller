/// @file StorageRetentionManager.cpp
/// @brief Implementation of storage retention manager and FIFO circular pruning.

#include "StorageRetentionManager.h"

#include <algorithm>

namespace Sightline {

void StorageRetentionManager::pinFile(std::string_view filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pinnedFiles.insert(std::string(filename));
}

void StorageRetentionManager::unpinFile(std::string_view filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pinnedFiles.erase(std::string(filename));
}

bool StorageRetentionManager::isPinned(std::string_view filename) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pinnedFiles.find(std::string(filename)) != m_pinnedFiles.end();
}

void StorageRetentionManager::clearPinned() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pinnedFiles.clear();
}

std::vector<std::string> StorageRetentionManager::pruneOldest(
    const std::vector<FileMetadataEntry>& entries,
    std::uint64_t requiredReclaimBytes,
    std::uint64_t& outReclaimedBytes) const
{
    outReclaimedBytes = 0ULL;
    if (requiredReclaimBytes == 0ULL || entries.empty()) {
        return {};
    }

    std::vector<const FileMetadataEntry*> candidates {};
    candidates.reserve(entries.size());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : entries) {
            const bool inPinnedSet = (m_pinnedFiles.find(entry.filename) != m_pinnedFiles.end());
            if (!entry.isPinned && !inPinnedSet) {
                candidates.push_back(&entry);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const FileMetadataEntry* a, const FileMetadataEntry* b) noexcept {
            return a->timestampUs < b->timestampUs;
        });

    std::vector<std::string> toPrune {};
    for (const auto* cand : candidates) {
        toPrune.push_back(cand->filename);
        outReclaimedBytes += cand->sizeBytes;
        if (outReclaimedBytes >= requiredReclaimBytes) {
            break;
        }
    }

    return toPrune;
}

} // namespace Sightline
