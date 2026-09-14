#pragma once

/// @file RetryPolicy.h
/// @brief Configurable retry policies and backoff strategies for commands and queries.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace PelcoD {

/// @enum BackoffStrategy
/// @brief Strategy for calculating delay intervals between successive retry attempts.
enum class BackoffStrategy : std::uint8_t {
    Fixed = 0, ///< Constant delay equal to initialBackoff.
    Linear = 1, ///< Linearly increasing delay: initialBackoff * attempt.
    Exponential = 2 ///< Exponentially increasing delay: initialBackoff * (multiplier ^ (attempt - 1)).
};

/// @struct RetryConfig
/// @brief Configuration settings controlling command and query automatic retry behavior.
struct RetryConfig {
    /// @brief Maximum number of retry attempts. 0 disables retries (single-shot execution).
    std::uint32_t maxRetries { 0U };

    /// @brief Initial backoff interval before the first retry attempt.
    std::chrono::milliseconds initialBackoff { 50 };

    /// @brief Maximum cap applied to any computed backoff delay.
    std::chrono::milliseconds maxBackoff { 1000 };

    /// @brief Growth multiplier for Exponential backoff strategy (typically 1.5 - 2.0).
    double backoffMultiplier { 2.0 };

    /// @brief Selected algorithm for computing backoff intervals.
    BackoffStrategy strategy { BackoffStrategy::Exponential };

    /// @brief Whether to retry when transport sendData() returns false.
    bool retryOnTransportError { true };
};

/// @brief Computes the backoff delay for a specific retry attempt.
/// @param[in] config The active retry configuration.
/// @param[in] retryAttempt The 1-based index of the retry attempt (1 = first retry, 2 = second retry).
/// @return Calculated backoff delay in milliseconds, clamped to config.maxBackoff.
[[nodiscard]] inline std::chrono::milliseconds calculateBackoffDelay(
    const RetryConfig& config, std::uint32_t retryAttempt) noexcept
{
    if (retryAttempt == 0U || config.initialBackoff.count() <= 0) {
        return std::chrono::milliseconds { 0 };
    }

    std::chrono::milliseconds delay = config.initialBackoff;

    switch (config.strategy) {
    case BackoffStrategy::Fixed:
        delay = config.initialBackoff;
        break;

    case BackoffStrategy::Linear: {
        const auto calculatedMs = static_cast<long long>(config.initialBackoff.count()) * retryAttempt;
        delay = std::chrono::milliseconds(calculatedMs);
        break;
    }

    case BackoffStrategy::Exponential: {
        const double mult = (config.backoffMultiplier > 0.0)
            ? std::pow(config.backoffMultiplier, static_cast<double>(retryAttempt - 1U))
            : 1.0;
        const auto calculatedMs = static_cast<long long>(static_cast<double>(config.initialBackoff.count()) * mult);
        delay = std::chrono::milliseconds(calculatedMs);
        break;
    }
    }

    return std::min(delay, config.maxBackoff);
}

} // namespace PelcoD
