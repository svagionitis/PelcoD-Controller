#pragma once

/// @file PelcoDStats.h
/// @brief Telemetry and error counters for Pelco-D protocol operations.

#include <cstddef>
#include <cstdint>

namespace PelcoD {

/// @struct PelcoDProtocolStats
/// @brief Diagnostic counters and latency metrics for Pelco-D protocol commands and queries.
struct PelcoDProtocolStats {
    std::uint64_t queriesSent { 0U };
    std::uint64_t queriesCompleted { 0U };
    std::uint64_t queryTimeouts { 0U };
    std::uint64_t queryRetries { 0U };
    std::uint64_t checksumErrors { 0U };
    std::uint64_t discardedSyncBytes { 0U };
    std::size_t pendingCommands { 0U };
    double lastRttMs { 0.0 };
    double minRttMs { 0.0 };
    double maxRttMs { 0.0 };
    double avgRttMs { 0.0 };
};

} // namespace PelcoD
