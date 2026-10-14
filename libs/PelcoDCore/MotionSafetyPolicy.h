#pragma once

/// @file MotionSafetyPolicy.h
/// @brief Configuration policy governing motion safety, dead-man watchdog, and fail-safe stops.

#include <chrono>
#include <cstdint>

namespace PelcoD {

/// @struct MotionSafetyPolicy
/// @brief Configurable rules enforcing PTZ motion limits and fail-safe actions.
struct MotionSafetyPolicy final {
    /// @brief Maximum duration allowed between motion refresh commands before auto-stopping (0 ms = disabled).
    std::chrono::milliseconds deadManTimeout { 0 };

    /// @brief Maximum continuous duration allowed for uninterrupted motion on any single axis (0 ms = disabled).
    std::chrono::milliseconds maxContinuousDuration { 0 };

    /// @brief Whether to transmit an emergency Stop frame upon transport disconnection.
    bool stopOnDisconnect { true };

    /// @brief Whether to transmit an emergency Stop frame prior to session shutdown.
    bool stopOnShutdown { true };

    /// @brief Whether to purge all queued motion commands when transport enters Disconnected/Error state.
    bool purgeQueueOnDisconnect { true };

    /// @brief Number of best-effort emergency stop frame retransmissions during shutdown/disconnect.
    std::uint8_t failSafeStopRetries { 1U };
};

} // namespace PelcoD
