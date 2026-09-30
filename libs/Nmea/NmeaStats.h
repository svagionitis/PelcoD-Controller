#pragma once

/// @file NmeaStats.h
/// @brief Telemetry structures capturing NMEA 0183/2000 protocol metrics.

#include <cstdint>

namespace Nmea {

/// @struct NmeaProtocolStats
/// @brief High-resolution counters and telemetry for NMEA sentence processing.
struct NmeaProtocolStats {
    /// @brief Total discrete sentences successfully framed from inbound stream.
    std::uint64_t sentencesReceived { 0U };

    /// @brief Total sentences successfully decoded and matched to parser handlers.
    std::uint64_t sentencesParsed { 0U };

    /// @brief Total sentences transmitted outbound.
    std::uint64_t sentencesSent { 0U };

    /// @brief Sentences rejected due to failed 8-bit XOR checksum validation.
    std::uint64_t checksumErrors { 0U };

    /// @brief Unaligned or corrupted bytes discarded while hunting sentence delimiters.
    std::uint64_t discardedBytes { 0U };
};

} // namespace Nmea
