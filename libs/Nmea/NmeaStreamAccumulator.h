#pragma once

/// @file NmeaStreamAccumulator.h
/// @brief Thread-safe byte-stream accumulator extracting framed NMEA 0183 sentences.

#include "NmeaChecksum.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Nmea {

/// @class NmeaStreamAccumulator
/// @brief Accumulates incoming byte chunks from transports, detects delimiters, and emits discrete sentences.
class NmeaStreamAccumulator {
public:
    static constexpr std::size_t DefaultMaxBufferSize { 4096U };

    /// @brief Constructs stream accumulator with maximum buffer capacity.
    /// @param[in] maxBufferSize Maximum memory capacity before clearing corrupted buffers.
    explicit NmeaStreamAccumulator(std::size_t maxBufferSize = DefaultMaxBufferSize);
    ~NmeaStreamAccumulator() = default;

    // Non-copyable, non-movable for thread-safety
    NmeaStreamAccumulator(const NmeaStreamAccumulator&) = delete;
    NmeaStreamAccumulator& operator=(const NmeaStreamAccumulator&) = delete;
    NmeaStreamAccumulator(NmeaStreamAccumulator&&) = delete;
    NmeaStreamAccumulator& operator=(NmeaStreamAccumulator&&) = delete;

    /// @brief Ingests raw byte vector and extracts all complete NMEA sentences.
    /// @param[in] data Incoming byte buffer.
    /// @param[in] validateChecksum If true, discards sentences failing 8-bit XOR checksum.
    /// @return List of extracted sentences.
    [[nodiscard]] std::vector<std::string> push(const std::vector<std::uint8_t>& data, bool validateChecksum = true);

    /// @brief Ingests raw byte array and extracts all complete NMEA sentences.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes.
    /// @param[in] validateChecksum If true, discards sentences failing 8-bit XOR checksum.
    /// @return List of extracted sentences.
    [[nodiscard]] std::vector<std::string> push(
        const std::uint8_t* data, std::size_t size, bool validateChecksum = true);

    /// @brief Ingests string chunk and extracts all complete NMEA sentences.
    /// @param[in] text String chunk.
    /// @param[in] validateChecksum If true, discards sentences failing 8-bit XOR checksum.
    /// @return List of extracted sentences.
    [[nodiscard]] std::vector<std::string> push(std::string_view text, bool validateChecksum = true);

    /// @brief Clears internal byte buffer.
    void clear();

    /// @brief Current number of buffered bytes awaiting sentence termination.
    [[nodiscard]] std::size_t size() const;

    /// @brief Configured maximum buffer capacity.
    [[nodiscard]] std::size_t maxBufferSize() const noexcept;

private:
    const std::size_t m_maxBufferSize;
    mutable std::mutex m_mutex;
    std::string m_buffer;
};

} // namespace Nmea
