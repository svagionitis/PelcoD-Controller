#pragma once

/// @file WsDiscoveryServer.h
/// @brief WS-Discovery multicast responder daemon for ONVIF device discovery.

#include "OnvifServerTypes.h"
#include <PelcoDTransport/SocketUtils.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD::Onvif {

/// @class WsDiscoveryServer
/// @brief Listens on UDP multicast 239.255.255.250:3702 for Probe messages and replies with ProbeMatches.
class WsDiscoveryServer {
public:
    /// @brief Constructs WS-Discovery server with given configuration.
    /// @param[in] config Server configuration containing device parameters and port.
    explicit WsDiscoveryServer(OnvifServerConfig config);

    /// @brief Destructor stops listening thread and cleans up socket resources.
    ~WsDiscoveryServer();

    // Non-copyable, non-movable
    WsDiscoveryServer(const WsDiscoveryServer&) = delete;
    WsDiscoveryServer& operator=(const WsDiscoveryServer&) = delete;
    WsDiscoveryServer(WsDiscoveryServer&&) = delete;
    WsDiscoveryServer& operator=(WsDiscoveryServer&&) = delete;

    /// @brief Starts the background multicast listening thread and broadcasts Hello.
    /// @return True if socket bound and thread started successfully.
    [[nodiscard]] bool start();

    /// @brief Broadcasts Bye and halts the background thread.
    void stop();

    /// @brief Checks if the responder thread is currently active.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Formats a ProbeMatches SOAP envelope string for a given probe request.
    /// @param[in] relatesToMessageId Incoming probe MessageID header.
    /// @param[in] localIp IPv4 address where the HTTP service is reachable.
    /// @return Complete SOAP XML response payload.
    [[nodiscard]] std::string createProbeMatchesPayload(
        const std::string& relatesToMessageId, const std::string& localIp) const;

    /// @brief Formats a Hello announcement SOAP XML message.
    /// @param[in] localIp IPv4 address where the HTTP service is reachable.
    /// @return Complete SOAP XML Hello payload.
    [[nodiscard]] std::string createHelloPayload(const std::string& localIp) const;

    /// @brief Formats a Bye departure SOAP XML message.
    /// @return Complete SOAP XML Bye payload.
    [[nodiscard]] std::string createByePayload() const;

private:
    void runListener();

    OnvifServerConfig m_config;
    std::atomic<bool> m_running { false };
    std::thread m_thread {};

    Net::SocketHandle m_sockFd { Net::InvalidSocket };
};

} // namespace PelcoD::Onvif
