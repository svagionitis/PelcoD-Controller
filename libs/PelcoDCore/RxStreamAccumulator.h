#pragma once

/// @file RxStreamAccumulator.h
/// @brief Thread-safe byte-stream accumulator extracting valid Pelco-D frames.

#include "PelcoDFrame.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace PelcoD {

/// @enum RxFrameExpectation
/// @brief Declares the framing expectation context for Pelco-D stream reassembly.
enum class RxFrameExpectation : std::uint8_t {
    StandardOnly = 0U,         ///< Standard 7-byte PTZ commands, telemetry, and ACKs (default).
    AwaitingQuery = 1U,        ///< 18-byte extended ASCII response expected or standard 7-byte.
    AllowGeneralResponse = 2U, ///< 4-byte general response frames permitted subject to boundary check.
    AllFrames = 3U             ///< Permit 7-byte, 18-byte, and 4-byte frames (batch parsing / splitStream).
};

/// @class RxStreamAccumulator
/// @brief Accumulates incoming byte chunks from transports, detects sync bytes,
///        validates candidate frame checksums, and emits discrete framed packets.
class RxStreamAccumulator {
public:
    static constexpr std::size_t DefaultMaxBufferSize { 4096U };
    static constexpr std::chrono::milliseconds DefaultInterByteTimeout { 25 };

    /// @brief Constructs an accumulator with configurable buffer limit and inter-byte timeout.
    /// @param[in] maxBufferSize Maximum allowed buffered bytes before overflow reset.
    /// @param[in] interByteTimeout Silence duration required before standalone 4-byte extraction.
    explicit RxStreamAccumulator(
        std::size_t maxBufferSize = DefaultMaxBufferSize,
        std::chrono::milliseconds interByteTimeout = DefaultInterByteTimeout);

    /// @brief Default destructor.
    ~RxStreamAccumulator() = default;

    // Non-copyable, non-movable
    RxStreamAccumulator(const RxStreamAccumulator&) = delete;
    RxStreamAccumulator& operator=(const RxStreamAccumulator&) = delete;
    RxStreamAccumulator(RxStreamAccumulator&&) = delete;
    RxStreamAccumulator& operator=(RxStreamAccumulator&&) = delete;

    /// @brief Appends incoming raw bytes and extracts complete validated Pelco-D frames.
    /// @param[in] data Incoming byte buffer.
    /// @param[in] expectation Expected framing context.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::vector<std::uint8_t>& data,
        RxFrameExpectation expectation = RxFrameExpectation::StandardOnly);

    /// @brief Backwards-compatible overload mapping boolean awaitingQuery flag to RxFrameExpectation.
    /// @param[in] data Incoming byte buffer.
    /// @param[in] awaitingQuery Whether device is expecting an extended 18-byte query response.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::vector<std::uint8_t>& data, bool awaitingQuery);

    /// @brief Appends incoming raw byte span and extracts complete validated Pelco-D frames.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes.
    /// @param[in] expectation Expected framing context.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::uint8_t* data, std::size_t size,
        RxFrameExpectation expectation = RxFrameExpectation::StandardOnly);

    /// @brief Backwards-compatible raw pointer overload mapping boolean flag to RxFrameExpectation.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes.
    /// @param[in] awaitingQuery Whether device is expecting an extended 18-byte query response.
    /// @return List of verified discrete frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> push(
        const std::uint8_t* data, std::size_t size, bool awaitingQuery);

    /// @brief Emits complete pending frames if inter-byte timeout has elapsed since last push.
    /// @param[in] expectation Expected framing context for evaluating buffered frames.
    /// @return Extracted frames on timeout expiration.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> flushExpired(
        RxFrameExpectation expectation = RxFrameExpectation::AllowGeneralResponse);

    /// @brief Forcibly extracts all valid frames currently buffered, ignoring timeouts.
    /// @param[in] expectation Expected framing context for evaluating buffered frames.
    /// @return Extracted valid frames.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> flush(
        RxFrameExpectation expectation = RxFrameExpectation::AllowGeneralResponse);

    /// @brief Clears accumulated internal byte buffer.
    void clear();

    /// @brief Current count of bytes buffered awaiting framing.
    [[nodiscard]] std::size_t size() const;

    /// @brief Configured maximum buffer capacity before overflow reset.
    [[nodiscard]] std::size_t maxBufferSize() const noexcept;

    /// @brief Configured inter-byte timeout duration.
    [[nodiscard]] std::chrono::milliseconds interByteTimeout() const noexcept;

    /// @brief Count of noise bytes dropped during sync byte hunting.
    [[nodiscard]] std::uint64_t discardedBytes() const noexcept;

    /// @brief Count of candidate frames rejected due to invalid checksums.
    [[nodiscard]] std::uint64_t checksumErrors() const noexcept;

    /// @brief Resets stream accumulator statistics.
    void resetStats() noexcept;

private:
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> extractLocked(
        RxFrameExpectation expectation, bool forceFlush);

    const std::size_t m_maxBufferSize;
    const std::chrono::milliseconds m_interByteTimeout;
    mutable std::mutex m_mutex;
    std::vector<std::uint8_t> m_buffer;
    std::chrono::steady_clock::time_point m_lastRxTime {};

    std::atomic<std::uint64_t> m_discardedBytes { 0U };
    std::atomic<std::uint64_t> m_checksumErrors { 0U };
};

} // namespace PelcoD
