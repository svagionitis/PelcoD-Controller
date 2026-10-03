#pragma once

/// @file TransactionalStorageWriter.h
/// @brief Crash-resilient transactional storage writer with atomic commit and fMP4 encapsulation.
/// @details Protects media recordings from power loss using staging files, sync barriers, and self-describing MP4 fragments.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace Sightline {

/// @class TransactionalStorageWriter
/// @brief Provides atomic file commit semantics and fragmented MP4 (fMP4) structure generation.
class TransactionalStorageWriter {
public:
    TransactionalStorageWriter();
    ~TransactionalStorageWriter();

    TransactionalStorageWriter(const TransactionalStorageWriter&) = delete;
    TransactionalStorageWriter& operator=(const TransactionalStorageWriter&) = delete;
    TransactionalStorageWriter(TransactionalStorageWriter&&) noexcept;
    TransactionalStorageWriter& operator=(TransactionalStorageWriter&&) noexcept;

    /// @brief Begins a transactional write session to a temporary staging file.
    /// @param[in] destinationPath Final intended target file path.
    /// @return True if staging file was successfully opened.
    [[nodiscard]] bool startTransaction(std::string_view destinationPath);

    /// @brief Writes a block of binary data to the staging file.
    /// @param[in] data Pointer to buffer.
    /// @param[in] size Size in bytes.
    /// @return True if all bytes were written successfully.
    [[nodiscard]] bool writeChunk(const std::uint8_t* data, std::size_t size);

    /// @brief Writes a vector of bytes to the staging file.
    /// @param[in] data Binary payload.
    /// @return True if written successfully.
    [[nodiscard]] bool writeChunk(const std::vector<std::uint8_t>& data);

    /// @brief Issues an operating system sync barrier to force cached blocks to disk.
    /// @details Invokes FlushFileBuffers on Windows or fsync on Linux/POSIX.
    /// @return True if sync barrier succeeded.
    [[nodiscard]] bool syncBarrier();

    /// @brief Atomically commits the staging file to the destination path.
    /// @details Flushes, closes the handle, and atomically renames the temporary file.
    /// @return True if commit succeeded.
    [[nodiscard]] bool commit();

    /// @brief Aborts the active transaction, closing and unlinking the staging file.
    void abort();

    /// @brief Returns whether a transaction is currently open.
    /// @return True if active.
    [[nodiscard]] bool isActive() const noexcept;

    /// @brief Returns the active staging file path.
    /// @return Staging path string.
    [[nodiscard]] std::string stagingPath() const;

    /// @brief Returns the target destination path.
    /// @return Destination path string.
    [[nodiscard]] std::string destinationPath() const;

    /// @brief Returns cumulative bytes written in current transaction.
    /// @return Written byte count.
    [[nodiscard]] std::uint64_t bytesWritten() const noexcept;

    // --- Fragmented MP4 (fMP4) Construction Utilities ---

    /// @brief Builds a standard MP4 File Type Box (ftyp).
    /// @return Binary ftyp box with 'isom' major brand.
    [[nodiscard]] static std::vector<std::uint8_t> buildFtypBox();

    /// @brief Builds an initial Movie Box (moov) declaring fragmented video track.
    /// @param[in] width Video horizontal resolution.
    /// @param[in] height Video vertical resolution.
    /// @param[in] timescale Clock frequency (default 90000 Hz).
    /// @return Binary moov box with mvex extensions.
    [[nodiscard]] static std::vector<std::uint8_t> buildMoovHeader(
        std::uint32_t width = 1920U, std::uint32_t height = 1080U, std::uint32_t timescale = 90000U);

    /// @brief Builds a self-describing Fragmented MP4 fragment (moof + mdat boxes).
    /// @param[in] seq Fragment sequence number (1-based).
    /// @param[in] payload Encapsulated video sample data (e.g. H.264/H.265 NALUs).
    /// @param[in] durationTicks Duration of the fragment in timescale ticks.
    /// @return Binary payload containing concatenated moof and mdat boxes.
    [[nodiscard]] static std::vector<std::uint8_t> buildFragment(
        std::uint32_t seq, const std::vector<std::uint8_t>& payload, std::uint32_t durationTicks);

private:
    std::string m_destinationPath {};
    std::string m_stagingPath {};
    std::FILE* m_fileHandle { nullptr };
    std::uint64_t m_bytesWritten { 0ULL };
};

} // namespace Sightline
