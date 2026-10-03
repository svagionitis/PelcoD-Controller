#pragma once

/// @file SecureStorageSyncService.h
/// @brief Enterprise remote storage synchronization service for offloading recordings over SFTP/FTPS.
/// @details Queues completed recording files and offloads them to a secure remote repository via an abstract sink.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>

namespace Sightline {

/// @struct SecureSyncTask
/// @brief Upload candidate representing a local recording and target remote destination.
struct SecureSyncTask {
    std::string localFilePath {};     ///< Absolute or relative path on local storage medium
    std::string remoteFileName {};    ///< Destination filename on remote server
};

/// @struct SecureSyncConfig
/// @brief Connection and authentication parameters for remote storage repository.
struct SecureSyncConfig {
    std::string serverHost {};        ///< Remote server hostname or IP address
    std::uint16_t serverPort { 22U }; ///< Remote port (default 22 for SFTP, 990 for FTPS)
    std::string remoteDirectory {};   ///< Base destination directory path on server
    std::string authKeyPath {};       ///< Local filesystem path to private key / certificate
    bool useTls { true };             ///< True for FTPS (TLS 1.3), false for standard SFTP SSH
};

/// @class ISecureStorageSink
/// @brief Abstract transfer sink interface decoupling service logic from network protocol libraries.
class ISecureStorageSink {
public:
    virtual ~ISecureStorageSink() = default;

    /// @brief Transmits a local file to the designated remote location.
    /// @param[in] localPath Path to local source file.
    /// @param[in] remotePath Target path on remote host.
    /// @return True if transfer succeeded and integrity verified.
    [[nodiscard]] virtual bool uploadFile(
        std::string_view localPath, std::string_view remotePath) = 0;
};

/// @class SecureStorageSyncService
/// @brief Asynchronous queue coordinator for offloading recordings without impacting video acquisition.
class SecureStorageSyncService {
public:
    /// @brief Constructs sync service with optional sink and configuration.
    /// @param[in] sink Storage sink implementation (can be null if set later).
    /// @param[in] config Repository connection parameters.
    explicit SecureStorageSyncService(
        std::shared_ptr<ISecureStorageSink> sink = nullptr,
        SecureSyncConfig config = {});
    ~SecureStorageSyncService() = default;

    SecureStorageSyncService(const SecureStorageSyncService&) = delete;
    SecureStorageSyncService& operator=(const SecureStorageSyncService&) = delete;
    SecureStorageSyncService(SecureStorageSyncService&&) = delete;
    SecureStorageSyncService& operator=(SecureStorageSyncService&&) = delete;

    /// @brief Attaches or updates the active secure transfer sink.
    /// @param[in] sink Shared pointer to storage sink.
    void setSink(std::shared_ptr<ISecureStorageSink> sink);

    /// @brief Updates repository connection configuration.
    /// @param[in] config Configuration settings.
    void setConfig(const SecureSyncConfig& config);

    /// @brief Enqueues a local recording for remote offloading.
    /// @param[in] localPath Path to local file.
    /// @param[in] remoteName Target filename on remote repository.
    void enqueueUpload(std::string_view localPath, std::string_view remoteName);

    /// @brief Processes the next pending upload task in the queue.
    /// @return True if a task was processed successfully, false if queue empty or transfer failed.
    [[nodiscard]] bool processNext();

    /// @brief Processes all currently enqueued upload tasks synchronously.
    /// @return Number of successfully transferred files.
    [[nodiscard]] std::size_t processAll();

    /// @brief Returns the number of upload tasks remaining in the queue.
    /// @return Pending task count.
    [[nodiscard]] std::size_t pendingCount() const noexcept;

    /// @brief Returns total number of successfully uploaded files.
    /// @return Completed count.
    [[nodiscard]] std::uint64_t completedCount() const noexcept;

    /// @brief Returns total number of failed upload attempts.
    /// @return Failed count.
    [[nodiscard]] std::uint64_t failedCount() const noexcept;

    /// @brief Clears pending upload queue and resets counters.
    void clear() noexcept;

private:
    std::shared_ptr<ISecureStorageSink> m_sink;
    SecureSyncConfig m_config;
    mutable std::mutex m_mutex;
    std::queue<SecureSyncTask> m_pendingQueue;
    std::uint64_t m_completedCount { 0ULL };
    std::uint64_t m_failedCount { 0ULL };
};

} // namespace Sightline
