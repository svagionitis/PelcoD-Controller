#pragma once

/// @file SightlineStreamAccumulator.h
/// @brief Thread-safe stream accumulator extracting framed Sightline SLA packets.

#include "SightlineCrc8.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Sightline {

/// @class SightlineStreamAccumulator
/// @brief Ingests byte streams, recovers sync (0x51 0xAC), extracts 1/2-byte lengths, and validates CRC-8.
class SightlineStreamAccumulator {
public:
    static constexpr std::size_t DefaultMaxBufferSize { 16384U };

    /// @brief Constructs stream accumulator with buffer size ceiling.
    /// @param[in] maxBufferSize Maximum capacity in bytes before overflow protection reset.
    explicit SightlineStreamAccumulator(std::size_t maxBufferSize = DefaultMaxBufferSize);
    ~SightlineStreamAccumulator() = default;

    SightlineStreamAccumulator(const SightlineStreamAccumulator&) = delete;
    SightlineStreamAccumulator& operator=(const SightlineStreamAccumulator&) = delete;
    SightlineStreamAccumulator(SightlineStreamAccumulator&&) = delete;
    SightlineStreamAccumulator& operator=(SightlineStreamAccumulator&&) = delete;

    /// @brief Ingests raw byte vector and extracts all complete, CRC-valid SLA packets.
    /// @param[in] data Inbound byte buffer.
    /// @param[in] validateChecksum True to reject packets failing CRC-8 validation.
    /// @return Vector of complete validated packet byte vectors.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::vector<std::uint8_t>& data, bool validateChecksum = true);

    /// @brief Ingests raw byte array and extracts all complete, CRC-valid SLA packets.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes.
    /// @param[in] validateChecksum True to reject packets failing CRC-8 validation.
    /// @return Vector of complete validated packet byte vectors.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::uint8_t* data, std::size_t size, bool validateChecksum = true);

    /// @brief Clears the internal stream buffer.
    void clear();

    /// @brief Number of corrupted or noise bytes discarded while searching for sync headers.
    [[nodiscard]] std::uint64_t discardedBytes() const noexcept;

    /// @brief Number of candidate packets rejected due to CRC-8 mismatch.
    [[nodiscard]] std::uint64_t checksumErrors() const noexcept;

    /// @brief Total number of valid packets successfully extracted and dispatched.
    [[nodiscard]] std::uint64_t packetsExtracted() const noexcept;

    /// @brief Resets all accumulator telemetry counters.
    void resetStats() noexcept;

    /// @brief Current number of buffered bytes awaiting packet completion.
    [[nodiscard]] std::size_t size() const;

    /// @brief Configured maximum buffer capacity before forced pruning.
    [[nodiscard]] std::size_t maxBufferSize() const noexcept;

private:
    const std::size_t m_maxBufferSize;
    mutable std::mutex m_mutex;
    std::vector<std::uint8_t> m_buffer;

    std::atomic<std::uint64_t> m_discardedBytes { 0U };
    std::atomic<std::uint64_t> m_checksumErrors { 0U };
    std::atomic<std::uint64_t> m_packetsExtracted { 0U };
};

} // namespace Sightline
