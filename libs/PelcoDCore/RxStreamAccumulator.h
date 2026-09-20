#pragma once

/// @file RxStreamAccumulator.h
/// @brief Thread-safe byte-stream accumulator extracting valid Pelco-D frames.

#include "PelcoDFrame.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace PelcoD {

/// @class RxStreamAccumulator
/// @brief Accumulates incoming byte chunks from transports, detects sync bytes,
///        validates candidate frame checksums, and emits discrete framed packets.
class RxStreamAccumulator {
public:
    static constexpr std::size_t DefaultMaxBufferSize { 4096U };

    explicit RxStreamAccumulator(std::size_t maxBufferSize = DefaultMaxBufferSize);
    ~RxStreamAccumulator() = default;

    // Non-copyable, non-movable
    RxStreamAccumulator(const RxStreamAccumulator&) = delete;
    RxStreamAccumulator& operator=(const RxStreamAccumulator&) = delete;
    RxStreamAccumulator(RxStreamAccumulator&&) = delete;
    RxStreamAccumulator& operator=(RxStreamAccumulator&&) = delete;

    /// @brief Appends incoming raw bytes and extracts all complete, validated Pelco-D frames.
    /// @param[in] data Incoming byte buffer.
    /// @param[in] awaitingQuery Whether device is expecting an extended 18-byte query response.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::vector<std::uint8_t>& data, bool awaitingQuery = false);

    /// @brief Appends incoming raw byte span and extracts all complete, validated Pelco-D frames.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes.
    /// @param[in] awaitingQuery Whether device is expecting an extended 18-byte query response.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::uint8_t* data, std::size_t size, bool awaitingQuery = false);

    /// @brief Clears accumulated internal byte buffer.
    void clear();

    /// @brief Current count of bytes buffered awaiting framing.
    [[nodiscard]] std::size_t size() const;

    /// @brief Configured maximum buffer capacity before overflow reset.
    [[nodiscard]] std::size_t maxBufferSize() const noexcept;

private:
    const std::size_t m_maxBufferSize;
    mutable std::mutex m_mutex;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace PelcoD
