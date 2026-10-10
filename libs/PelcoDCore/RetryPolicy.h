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
    if (retryAttempt == 0U || config.initialBackoff.count() <= 0 || config.maxBackoff.count() <= 0) {
        return std::chrono::milliseconds { 0 };
    }

    const std::int64_t capMs = config.maxBackoff.count();
    const std::int64_t initialMs = config.initialBackoff.count();

    if (initialMs >= capMs) {
        return config.maxBackoff;
    }

    std::chrono::milliseconds delay { config.initialBackoff };

    switch (config.strategy) {
    case BackoffStrategy::Fixed:
        delay = config.initialBackoff;
        break;

    case BackoffStrategy::Linear: {
        const auto maxAttemptsBeforeCap = static_cast<std::uint64_t>(capMs) / static_cast<std::uint64_t>(initialMs);
        if (static_cast<std::uint64_t>(retryAttempt) >= maxAttemptsBeforeCap) {
            delay = config.maxBackoff;
        } else {
            const auto calculatedMs = initialMs * static_cast<std::int64_t>(retryAttempt);
            delay = std::chrono::milliseconds { calculatedMs };
        }
        break;
    }

    case BackoffStrategy::Exponential: {
        double mult = 1.0;
        if (!std::isnan(config.backoffMultiplier) && config.backoffMultiplier > 0.0) {
            mult = std::pow(config.backoffMultiplier, static_cast<double>(retryAttempt - 1U));
        }

        if (std::isnan(mult) || mult <= 0.0) {
            delay = config.initialBackoff;
        } else if (std::isinf(mult)) {
            delay = config.maxBackoff;
        } else {
            const double scaledMs = static_cast<double>(initialMs) * mult;
            if (std::isnan(scaledMs) || scaledMs <= 0.0) {
                delay = config.initialBackoff;
            } else if (std::isinf(scaledMs) || scaledMs >= static_cast<double>(capMs)) {
                delay = config.maxBackoff;
            } else {
                delay = std::chrono::milliseconds { static_cast<std::int64_t>(scaledMs) };
            }
        }
        break;
    }
    }

    return std::min(delay, config.maxBackoff);
}

} // namespace PelcoD
