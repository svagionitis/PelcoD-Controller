#pragma once

/// @file TransportStats.h
/// @brief Unified kernel-level and application-level transport statistics.

#include <chrono>
#include <cstdint>
#include <optional>

namespace Transport {

/// @struct SerialKernelStats
/// @brief Kernel-level serial port telemetry and error counters.
struct SerialKernelStats {
    bool supported { false };
    std::uint64_t rxBytesDriver { 0U };
    std::uint64_t txBytesDriver { 0U };
    std::uint64_t framingErrors { 0U };
    std::uint64_t fifoOverruns { 0U };
    std::uint64_t parityErrors { 0U };
    std::uint64_t breakCount { 0U };
    std::uint32_t queuedRxBytes { 0U };
    std::uint32_t queuedTxBytes { 0U };
    bool ctsHold { false };
    bool dsrHold { false };
};

/// @struct TcpKernelStats
/// @brief Kernel-level TCP connection telemetry.
struct TcpKernelStats {
    bool supported { false };
    std::uint32_t rttUs { 0U };
    std::uint32_t rttVarUs { 0U };
    std::uint32_t minRttUs { 0U };
    std::uint64_t totalRetrans { 0U };
    std::uint32_t unackedSegments { 0U };
    std::uint32_t lostSegments { 0U };
    std::uint32_t sndCwnd { 0U };
    std::uint32_t queuedTxBytes { 0U };
    std::uint32_t queuedRxBytes { 0U };
    std::uint32_t reorderMetric { 0U };
    std::uint32_t caState { 0U };
};

/// @struct UdpKernelStats
/// @brief Kernel-level UDP socket telemetry.
struct UdpKernelStats {
    bool supported { false };
    std::uint64_t rxDroppedPackets { 0U };
    std::uint32_t queuedRxBytes { 0U };
    std::uint32_t socketRxBufferSize { 0U };
    std::uint32_t socketTxBufferSize { 0U };
};

/// @struct GenericTransportStats
/// @brief Application-level transport counter metrics.
struct GenericTransportStats {
    std::uint64_t bytesSent { 0U };
    std::uint64_t bytesReceived { 0U };
    std::uint64_t packetsSent { 0U };
    std::uint64_t packetsReceived { 0U };
    std::uint64_t txErrorCount { 0U };
    std::uint64_t rxErrorCount { 0U };
    std::uint32_t reconnectCount { 0U };
    std::chrono::steady_clock::time_point lastTxTime {};
    std::chrono::steady_clock::time_point lastRxTime {};
};

/// @struct TransportStatsSnapshot
/// @brief Aggregated diagnostic snapshot for any transport.
struct TransportStatsSnapshot {
    GenericTransportStats generic {};
    std::optional<SerialKernelStats> serial {};
    std::optional<TcpKernelStats> tcp {};
    std::optional<UdpKernelStats> udp {};
};

} // namespace Transport
