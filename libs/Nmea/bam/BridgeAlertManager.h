#pragma once

/// @file BridgeAlertManager.h
/// @brief Bridge Alert Management (BAM) lifecycle coordinator complying with IEC 62923-1 / IEC 62923-2 and IEC 61162-1.

#include "BridgeAlertTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Nmea::Bam {

/// @struct BamConfig
/// @brief Operational configuration for Bridge Alert Management state machine and escalation timers.
struct BamConfig {
    std::string talkerId { "BN" };                           ///< Talker ID (default "BN" for Bridge Navigation)
    std::chrono::seconds silenceTimeoutSec { 30 };           ///< Temporary silence timeout before re-annunciation
    std::chrono::seconds escalationTimeoutSec { 120 };       ///< Warning/alarm escalation timeout if unacknowledged
    std::chrono::seconds heartbeatIntervalSec { 60 };        ///< HBT supervision transmission interval
    std::chrono::seconds cyclicAlertListIntervalSec { 30 };  ///< ALC cyclic list transmission interval
    bool legacyAlrEnabled { true };                          ///< Generate legacy $--ALR sentences alongside $--ALF
};

/// @class BridgeAlertManager
/// @brief Thread-safe alert lifecycle engine handling registration, state transitions,
///        silence/escalation timeouts, ARC/ACK command handling, and sentence broadcasts.
class BridgeAlertManager {
public:
    using BroadcastCallback = std::function<void(const std::string& sentence)>;
    using AlertCallback = std::function<void(const AlertRecord&)>;

    explicit BridgeAlertManager(const BamConfig& config = {});
    virtual ~BridgeAlertManager() = default;

    // Non-copyable, non-movable
    BridgeAlertManager(const BridgeAlertManager&) = delete;
    BridgeAlertManager& operator=(const BridgeAlertManager&) = delete;
    BridgeAlertManager(BridgeAlertManager&&) = delete;
    BridgeAlertManager& operator=(BridgeAlertManager&&) = delete;

    /// @brief Registers or raises an alert in the BAM state machine.
    /// @param[in] alertId Unique alert identifier.
    /// @param[in] instance Alert instance number (default 1).
    /// @param[in] priority Alert priority level (Emergency, Alarm, Warning, Caution).
    /// @param[in] category Functional alert category (CategoryA, CategoryB, CategoryC).
    /// @param[in] title Short alert title.
    /// @param[in] text Detailed description / cause.
    /// @return Current alert record after state transition.
    AlertRecord registerAlert(
        std::uint32_t alertId,
        std::uint32_t instance,
        AlertPriority priority,
        AlertCategory category,
        std::string title,
        std::string text);

    /// @brief Clears / rectifies an active alert condition.
    /// @param[in] alertId Alert identifier.
    /// @param[in] instance Alert instance number.
    /// @return True if alert existed and was rectified.
    bool rectifyAlert(std::uint32_t alertId, std::uint32_t instance = 1U);

    /// @brief Acknowledges an active or rectified alert.
    /// @param[in] alertId Alert identifier.
    /// @param[in] instance Alert instance number.
    /// @return True if alert transitioned to acknowledged/normal.
    bool acknowledgeAlert(std::uint32_t alertId, std::uint32_t instance = 1U);

    /// @brief Temporarily silences an active unacknowledged alert.
    /// @param[in] alertId Alert identifier.
    /// @param[in] instance Alert instance number.
    /// @return True if alert transitioned to silenced.
    bool silenceAlert(std::uint32_t alertId, std::uint32_t instance = 1U);

    /// @brief Transfers operational responsibility of an alert.
    /// @param[in] alertId Alert identifier.
    /// @param[in] instance Alert instance number.
    /// @return True if alert transitioned to responsibility transferred.
    bool transferAlert(std::uint32_t alertId, std::uint32_t instance = 1U);

    /// @brief Processes an inbound $--ARC Alert Command Request sentence.
    /// @param[in] arc Parsed ArcData command.
    /// @return True if target alert was found and updated.
    bool processArcCommand(const ArcData& arc);

    /// @brief Processes an inbound legacy $--ACK Alarm Acknowledge sentence.
    /// @param[in] ack Parsed AckData command.
    /// @return True if target alert was found and acknowledged.
    bool processAckCommand(const AckData& ack);

    /// @brief Evaluates silence timeouts, escalations, and periodic broadcasts.
    /// @param[in] now Steady-clock time reference.
    void pollEscalations(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Generates an $--HBT heartbeat sentence.
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] std::string generateHeartbeat();

    /// @brief Generates an $--ALC cyclic alert list sentence.
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] std::string generateAlertList();

    /// @brief Generates an $--ALF sentence for a specific alert record.
    /// @param[in] record Alert record to serialize.
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] std::string generateAlf(const AlertRecord& record);

    /// @brief Generates a legacy $--ALR sentence for an alert record.
    /// @param[in] record Alert record to serialize.
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] std::string generateLegacyAlr(const AlertRecord& record);

    /// @brief Retrieves all currently registered alerts.
    [[nodiscard]] std::vector<AlertRecord> activeAlerts() const;

    /// @brief Queries an alert record by ID and instance.
    /// @param[in] alertId Target alert ID.
    /// @param[in] instance Target instance (default 1).
    /// @return AlertRecord if present, std::nullopt otherwise.
    [[nodiscard]] std::optional<AlertRecord> alertById(std::uint32_t alertId, std::uint32_t instance = 1U) const;

    /// @brief Configures BAM engine parameters.
    void setConfig(const BamConfig& config);

    /// @brief Returns current BAM configuration.
    [[nodiscard]] BamConfig config() const;

    /// @brief Sets callback invoked when BAM generates sentences for broadcast.
    void setBroadcastCallback(BroadcastCallback cb);

    /// @brief Sets callback invoked on alert state transitions.
    void setStateCallback(AlertCallback cb);

private:
    struct AlertKey {
        std::uint32_t id { 0U };
        std::uint32_t instance { 1U };

        bool operator<(const AlertKey& other) const noexcept
        {
            if (id != other.id) {
                return id < other.id;
            }
            return instance < other.instance;
        }
    };

    void broadcastSentence(const std::string& sentence);
    void notifyAlertChange(const AlertRecord& record);

    mutable std::mutex m_mutex {};
    BamConfig m_config {};

    std::map<AlertKey, AlertRecord> m_alerts {};

    std::uint8_t m_hbtSeqId { 0U };
    std::uint8_t m_alcSeqId { 0U };
    std::chrono::steady_clock::time_point m_lastHbtTime {};
    std::chrono::steady_clock::time_point m_lastAlcTime {};

    BroadcastCallback m_broadcastCb {};
    AlertCallback m_alertCb {};
};

} // namespace Nmea::Bam
