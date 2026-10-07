#pragma once

/// @file SubscriptionManager.h
/// @brief Lifecycle, lease enforcement, and quota management for ONVIF event subscriptions.
/// @details Remediates review finding C5 (CWE-400 resource exhaustion, lease expiration).

#include "OnvifServerTypes.h"
#include "OnvifTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @struct SubCreateResult
/// @brief Result of a subscription creation request.
struct SubCreateResult {
    bool success { false };
    std::string id {};
    std::string terminationTimeUtc {};
    std::string errorReason {};
};

/// @class SubscriptionManager
/// @brief Thread-safe manager for PullPoint and Push event subscriptions.
/// @details Enforces maximum subscription quotas, parses and clamps ISO 8601 leases, bounds
///          per-subscriber event queues, automatically sweeps expired subscriptions, and provides
///          strict lookup without fallback leaks.
class SubscriptionManager {
public:
    /// @brief Constructs the manager with the given notification settings.
    /// @param[in] config Operational notification configuration.
    explicit SubscriptionManager(NotificationConfig config);

    ~SubscriptionManager() = default;

    SubscriptionManager(const SubscriptionManager&) = delete;
    SubscriptionManager& operator=(const SubscriptionManager&) = delete;
    SubscriptionManager(SubscriptionManager&&) = delete;
    SubscriptionManager& operator=(SubscriptionManager&&) = delete;

    /// @brief Creates a new PullPoint subscription with enforced quotas and lease limits.
    /// @param[in] termTimeStr Optional ISO 8601 duration or dateTime.
    /// @return SubCreateResult with assigned ID and ISO 8601 termination timestamp.
    [[nodiscard]] SubCreateResult createPullSub(std::string_view termTimeStr);

    /// @brief Creates a new Push subscription for a verified consumer URL.
    /// @param[in] consumerUrl Verified target push URL.
    /// @param[in] termTimeStr Optional ISO 8601 duration or dateTime.
    /// @return SubCreateResult with assigned ID and ISO 8601 termination timestamp.
    [[nodiscard]] SubCreateResult createPushSub(std::string_view consumerUrl, std::string_view termTimeStr);

    /// @brief Extends the lease termination time for an existing active subscription.
    /// @param[in] subId Subscription token ID.
    /// @param[in] termTimeStr Optional requested ISO 8601 lease extension.
    /// @param[out] outNewTermTime ISO 8601 string of the new termination timestamp.
    /// @return True if subscription was found and renewed; false if unknown or expired.
    [[nodiscard]] bool renewSub(std::string_view subId, std::string_view termTimeStr, std::string& outNewTermTime);

    /// @brief Removes an active subscription.
    /// @param[in] subId Subscription token ID.
    /// @return True if subscription was found and erased.
    bool unsubscribe(std::string_view subId);

    /// @brief Pulls queued events for a PullPoint subscription.
    /// @param[in] subId Subscription token ID.
    /// @param[in] limit Maximum events to retrieve.
    /// @param[in] timeoutSec Maximum seconds to wait if queue is empty.
    /// @param[out] outEvents Retrieved events.
    /// @param[out] outCurTime Current UTC timestamp string.
    /// @param[out] outTermTime Subscription termination timestamp string.
    /// @param[in] serverRunning Server running flag to abort wait on shutdown.
    /// @return True on success; false if subscription ID is unknown or expired.
    [[nodiscard]] bool pullMessages(std::string_view subId, int limit, int timeoutSec,
        std::vector<OnvifEvent>& outEvents, std::string& outCurTime, std::string& outTermTime,
        const std::atomic<bool>& serverRunning);

    /// @brief Publishes an event to all active PullPoint queues and collects push URLs.
    /// @param[in] event The event to publish.
    /// @param[out] outPushUrls Collected list of URLs for active push subscribers.
    void publishEvent(const OnvifEvent& event, std::vector<std::string>& outPushUrls);

    /// @brief Sweeps and removes all expired subscriptions.
    /// @return Number of evicted subscriptions.
    std::size_t sweepExpired();

    /// @brief Wakes up all waiting PullPoint subscribers (used during server shutdown).
    void notifyAll();

    /// @brief Returns the count of active PullPoint subscriptions.
    /// @return Current PullPoint subscription count.
    [[nodiscard]] std::size_t getPullSubCount() const;

    /// @brief Returns the count of active Push subscriptions.
    /// @return Current Push subscription count.
    [[nodiscard]] std::size_t getPushSubCount() const;

    /// @brief Parses an ISO 8601 duration or seconds string, clamping to min/max lease.
    /// @param[in] termStr Requested duration string (e.g. "PT10M", "PT30S").
    /// @return Clamped duration in seconds.
    [[nodiscard]] std::chrono::seconds parseLease(std::string_view termStr) const;

private:
    struct PullPointSub {
        std::string id {};
        std::chrono::steady_clock::time_point terminationTime {};
        std::deque<OnvifEvent> queue {};
        std::mutex mutex {};
        std::condition_variable cv {};
    };

    struct PushSub {
        std::string id {};
        std::string consumerUrl {};
        std::chrono::steady_clock::time_point terminationTime {};
    };

    NotificationConfig m_config;
    mutable std::mutex m_mutex {};
    std::map<std::string, std::shared_ptr<PullPointSub>> m_pullSubs {};
    std::map<std::string, PushSub> m_pushSubs {};
    std::uint32_t m_nextSubId { 1U };
};

} // namespace Onvif
