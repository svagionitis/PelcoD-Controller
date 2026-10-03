/// @file SecureStorageSyncService.cpp
/// @brief Implementation of secure remote storage synchronization coordinator.

#include "SecureStorageSyncService.h"

#include <utility>

namespace Sightline {

SecureStorageSyncService::SecureStorageSyncService(
    std::shared_ptr<ISecureStorageSink> sink,
    SecureSyncConfig config)
    : m_sink(std::move(sink))
    , m_config(std::move(config))
{
}

void SecureStorageSyncService::setSink(std::shared_ptr<ISecureStorageSink> sink)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sink = std::move(sink);
}

void SecureStorageSyncService::setConfig(const SecureSyncConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void SecureStorageSyncService::enqueueUpload(std::string_view localPath, std::string_view remoteName)
{
    if (localPath.empty() || remoteName.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingQueue.push(SecureSyncTask { std::string(localPath), std::string(remoteName) });
}

bool SecureStorageSyncService::processNext()
{
    SecureSyncTask task {};
    std::shared_ptr<ISecureStorageSink> sink;
    std::string remoteDir {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingQueue.empty() || !m_sink) {
            return false;
        }
        task = m_pendingQueue.front();
        m_pendingQueue.pop();
        sink = m_sink;
        remoteDir = m_config.remoteDirectory;
    }

    std::string fullRemotePath = remoteDir;
    if (!fullRemotePath.empty() && fullRemotePath.back() != '/') {
        fullRemotePath += '/';
    }
    fullRemotePath += task.remoteFileName;

    const bool ok = sink->uploadFile(task.localFilePath, fullRemotePath);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (ok) {
            ++m_completedCount;
        } else {
            ++m_failedCount;
        }
    }

    return ok;
}

std::size_t SecureStorageSyncService::processAll()
{
    std::size_t count { 0U };
    while (processNext()) {
        ++count;
    }
    return count;
}

std::size_t SecureStorageSyncService::pendingCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingQueue.size();
}

std::uint64_t SecureStorageSyncService::completedCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedCount;
}

std::uint64_t SecureStorageSyncService::failedCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_failedCount;
}

void SecureStorageSyncService::clear() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<SecureSyncTask> emptyQueue {};
    std::swap(m_pendingQueue, emptyQueue);
    m_completedCount = 0ULL;
    m_failedCount = 0ULL;
}

} // namespace Sightline
