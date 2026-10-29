/// @file MaritimeNetworkCoordinator.cpp
/// @brief Implementation of MaritimeNetworkCoordinator for multi-transport management and IEC 61162-460 security.

#include <cstdint>

#include "MaritimeNetworkCoordinator.h"
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace Nmea::Network {

MaritimeNetworkCoordinator::MaritimeNetworkCoordinator(std::shared_ptr<Iec61162_460Firewall> firewall,
    std::shared_ptr<Bam::BridgeAlertManager> bam, std::uint16_t tcpPort, std::uint16_t udpPort, std::uint16_t wsPort)
    : m_firewall { std::move(firewall) }
    , m_bam { std::move(bam) }
    , m_tcpServer { std::make_shared<NmeaTcpServer>(tcpPort) }
    , m_udpEndpoint { std::make_shared<NmeaUdpEndpoint>(udpPort) }
    , m_wsServer { std::make_shared<NmeaWebSocketServer>(wsPort) }
{
    if (m_firewall && m_bam) {
        m_firewall->attachAlertManager(m_bam);
    }

    // Configure TCP server inbound callback
    m_tcpServer->setSentenceCallback(
        [this](std::uint32_t /*clientId*/, std::string_view clientIp, std::string_view sentence) {
            if (m_firewall && !m_firewall->inspectInbound(SecurityZone::GeneralShipLan, clientIp, "", sentence)) {
                return; // Dropped by firewall
            }
            InboundSentenceCallback cb {};
            {
                std::lock_guard<std::mutex> lock { m_mutex };
                cb = m_sentenceCallback;
            }
            if (cb) {
                cb("TCP", sentence);
            }
        });

    // Configure UDP endpoint inbound callback
    m_udpEndpoint->setDatagramCallback(
        [this](const std::vector<std::uint8_t>& data, std::string_view senderIp, std::uint16_t /*senderPort*/) {
            const std::string_view sentence { reinterpret_cast<const char*>(data.data()), data.size() };
            if (m_firewall && !m_firewall->inspectInbound(SecurityZone::BridgeNetwork, senderIp, "", sentence)) {
                return; // Dropped by firewall
            }
            InboundSentenceCallback cb {};
            {
                std::lock_guard<std::mutex> lock { m_mutex };
                cb = m_sentenceCallback;
            }
            if (cb) {
                cb("UDP", sentence);
            }
        });

    // Configure WebSocket server command callback
    m_wsServer->setMessageCallback([this](std::uint32_t clientId, std::string_view jsonCommand) {
        InboundCommandCallback cb {};
        {
            std::lock_guard<std::mutex> lock { m_mutex };
            cb = m_commandCallback;
        }
        if (cb) {
            cb(clientId, jsonCommand);
        }
    });
}

MaritimeNetworkCoordinator::~MaritimeNetworkCoordinator()
{
    stop();
}

bool MaritimeNetworkCoordinator::start()
{
    bool success { true };
    if (m_tcpServer && !m_tcpServer->start()) {
        success = false;
    }
    if (m_udpEndpoint && !m_udpEndpoint->open()) {
        success = false;
    }
    if (m_wsServer && !m_wsServer->start()) {
        success = false;
    }
    return success;
}

void MaritimeNetworkCoordinator::stop()
{
    if (m_tcpServer) {
        m_tcpServer->stop();
    }
    if (m_udpEndpoint) {
        m_udpEndpoint->close();
    }
    if (m_wsServer) {
        m_wsServer->stop();
    }
}

void MaritimeNetworkCoordinator::broadcastSentence(std::string_view sentence)
{
    if (m_firewall && !m_firewall->inspectOutbound(SecurityZone::BridgeNetwork, sentence)) {
        return;
    }

    if (m_tcpServer) {
        static_cast<void>(m_tcpServer->broadcastSentence(sentence));
    }
    if (m_udpEndpoint) {
        static_cast<void>(m_udpEndpoint->sendSentence(sentence));
    }
}

void MaritimeNetworkCoordinator::broadcastTelemetry(std::string_view jsonTelemetry)
{
    if (m_wsServer) {
        m_wsServer->broadcastJson(jsonTelemetry);
    }
}

void MaritimeNetworkCoordinator::setInboundCallback(InboundSentenceCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_sentenceCallback = std::move(cb);
}

void MaritimeNetworkCoordinator::setCommandCallback(InboundCommandCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_commandCallback = std::move(cb);
}

std::shared_ptr<Iec61162_460Firewall> MaritimeNetworkCoordinator::firewall() const noexcept
{
    return m_firewall;
}

std::shared_ptr<Bam::BridgeAlertManager> MaritimeNetworkCoordinator::alertManager() const noexcept
{
    return m_bam;
}

std::shared_ptr<NmeaTcpServer> MaritimeNetworkCoordinator::tcpServer() const noexcept
{
    return m_tcpServer;
}

std::shared_ptr<NmeaUdpEndpoint> MaritimeNetworkCoordinator::udpEndpoint() const noexcept
{
    return m_udpEndpoint;
}

std::shared_ptr<NmeaWebSocketServer> MaritimeNetworkCoordinator::webSocketServer() const noexcept
{
    return m_wsServer;
}

} // namespace Nmea::Network
