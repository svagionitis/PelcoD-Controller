#pragma once

/// @file MaritimeNetworkCoordinator.h
/// @brief Central coordinator managing all marine network transports and IEC 61162-460 security enforcement.

#include "Iec61162_460Firewall.h"
#include "NmeaTcpServer.h"
#include "NmeaUdpEndpoint.h"
#include "NmeaWebSocketServer.h"
#include "bam/BridgeAlertManager.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string_view>

namespace Nmea::Network {

/// @class MaritimeNetworkCoordinator
/// @brief Central coordinator managing all marine network transports and IEC 61162-460 security enforcement.
class MaritimeNetworkCoordinator {
public:
    using InboundSentenceCallback = std::function<void(std::string_view source, std::string_view sentence)>;
    using InboundCommandCallback = std::function<void(std::uint32_t clientId, std::string_view jsonCommand)>;

    MaritimeNetworkCoordinator(std::shared_ptr<Iec61162_460Firewall> firewall,
                               std::shared_ptr<Bam::BridgeAlertManager> bam,
                               std::uint16_t tcpPort = 10110U,
                               std::uint16_t udpPort = 10110U,
                               std::uint16_t wsPort = 8088U);
    ~MaritimeNetworkCoordinator();

    // Non-copyable, non-movable
    MaritimeNetworkCoordinator(const MaritimeNetworkCoordinator&) = delete;
    MaritimeNetworkCoordinator& operator=(const MaritimeNetworkCoordinator&) = delete;
    MaritimeNetworkCoordinator(MaritimeNetworkCoordinator&&) = delete;
    MaritimeNetworkCoordinator& operator=(MaritimeNetworkCoordinator&&) = delete;

    /// @brief Initializes and starts all configured network transports.
    /// @return True if all active transports started successfully.
    [[nodiscard]] bool start();

    /// @brief Gracefully shuts down all network transports and firewall monitors.
    void stop();

    /// @brief Dispatches an outbound NMEA sentence to bridge network and connected navigation displays.
    /// @param[in] sentence Formatted NMEA sentence.
    void broadcastSentence(std::string_view sentence);

    /// @brief Publishes updated vessel and gimbal telemetry to connected HTML5 web dashboards.
    /// @param[in] jsonTelemetry Formatted JSON string.
    void broadcastTelemetry(std::string_view jsonTelemetry);

    /// @brief Sets callback for verified inbound NMEA sentences from TCP/UDP clients.
    void setInboundCallback(InboundSentenceCallback cb);

    /// @brief Sets callback for inbound JSON commands from web dashboard clients.
    void setCommandCallback(InboundCommandCallback cb);

    // Component accessors
    [[nodiscard]] std::shared_ptr<Iec61162_460Firewall> firewall() const noexcept;
    [[nodiscard]] std::shared_ptr<Bam::BridgeAlertManager> alertManager() const noexcept;
    [[nodiscard]] std::shared_ptr<NmeaTcpServer> tcpServer() const noexcept;
    [[nodiscard]] std::shared_ptr<NmeaUdpEndpoint> udpEndpoint() const noexcept;
    [[nodiscard]] std::shared_ptr<NmeaWebSocketServer> webSocketServer() const noexcept;

private:
    std::shared_ptr<Iec61162_460Firewall> m_firewall {};
    std::shared_ptr<Bam::BridgeAlertManager> m_bam {};
    std::shared_ptr<NmeaTcpServer> m_tcpServer {};
    std::shared_ptr<NmeaUdpEndpoint> m_udpEndpoint {};
    std::shared_ptr<NmeaWebSocketServer> m_wsServer {};

    mutable std::mutex m_mutex {};
    InboundSentenceCallback m_sentenceCallback {};
    InboundCommandCallback m_commandCallback {};
};

} // namespace Nmea::Network
