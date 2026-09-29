#pragma once

#include "ITransport.h"
#include "LweMulticastTransport.h"
#include "LweTypes.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace Nmea::Lwe {

/// @class LweChannelManager
/// @brief Manages multiple IEC 61162-450 multicast transmission channels simultaneously.
/// @details Ingests concurrent streams (e.g. TGTD target data, SATD satellite position, NAVD gyro heading)
///          and multiplexes them into a single virtual Transport::ITransport feed with optional source filtering.
/// @note Thread-safe.
class LweChannelManager : public std::enable_shared_from_this<LweChannelManager> {
public:
    LweChannelManager();
    virtual ~LweChannelManager();

    // Non-copyable, non-movable
    LweChannelManager(const LweChannelManager&) = delete;
    LweChannelManager& operator=(const LweChannelManager&) = delete;
    LweChannelManager(LweChannelManager&&) = delete;
    LweChannelManager& operator=(LweChannelManager&&) = delete;

    /// @brief Joins an IEC 61162-450 standard transmission group.
    /// @param[in] tg Standard transmission group.
    /// @param[in] interfaceIp Local interface IP ("0.0.0.0" for default).
    /// @return True if channel created and joined successfully.
    bool joinTransmissionGroup(TransmissionGroup tg, const std::string& interfaceIp = "0.0.0.0");

    /// @brief Joins an arbitrary multicast IPv4 group and port.
    /// @param[in] groupIp Multicast address string.
    /// @param[in] port UDP port.
    /// @param[in] interfaceIp Local interface IP.
    /// @return True if channel created and joined successfully.
    bool joinCustomGroup(const std::string& groupIp, std::uint16_t port, const std::string& interfaceIp = "0.0.0.0");

    /// @brief Closes and leaves all active multicast channels.
    void leaveAll();

    /// @brief Returns the count of currently active transmission channels.
    [[nodiscard]] std::size_t channelCount() const;

    /// @brief Restricts dispatched data to only packets originating from specified IEC 61162-450 source system IDs.
    /// @param[in] allowedSystemIds Whitelist of system identifiers (e.g. {"RA0001", "GP0001"}).
    void setSourceFilter(const std::vector<std::string>& allowedSystemIds);

    /// @brief Clears source system filtering (accepts all packets).
    void clearSourceFilter();

    /// @brief Returns a virtual ITransport implementation that aggregates all active multicast channels.
    /// @details Passing this to NmeaDevice allows a single device to receive multi-channel radar, GPS, and heading
    /// feeds.
    [[nodiscard]] std::shared_ptr<Transport::ITransport> getAggregatedTransport();

private:
    class AggregatedTransport;

    void handleIncomingChannelData(const std::vector<std::uint8_t>& data);

    mutable std::mutex m_mutex {};
    std::vector<std::shared_ptr<LweMulticastTransport>> m_channels {};
    std::unordered_set<std::string> m_sourceWhitelist {};
    bool m_useSourceFilter { false };
    std::shared_ptr<AggregatedTransport> m_aggregatedTransport {};
};

} // namespace Nmea::Lwe
