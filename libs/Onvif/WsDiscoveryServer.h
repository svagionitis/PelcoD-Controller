#pragma once

/// @file WsDiscoveryServer.h
/// @brief WS-Discovery multicast responder daemon for ONVIF device discovery.

#include "OnvifServerTypes.h"
#include "WsDiscoveryCommon.h"
#include "WsDiscoveryValidator.h"
#include <Transport/SocketUtils.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Onvif {

namespace Net = Transport::Net;

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

    /// @brief Sets device discovery mode (Discoverable vs NonDiscoverable).
    /// @param[in] mode Target discovery mode.
    void setDiscoveryMode(DiscoveryMode mode) noexcept;

    /// @brief Gets current device discovery mode.
    /// @return Active discovery mode.
    [[nodiscard]] DiscoveryMode getDiscoveryMode() const noexcept;

    /// @brief Gets active metadata version.
    /// @return Incremental 32-bit metadata version counter.
    [[nodiscard]] std::uint32_t getMetadataVersion() const noexcept;

    /// @brief Collects all active device scope URIs.
    /// @return Vector of active scope URIs.
    [[nodiscard]] std::vector<std::string> getActiveScopes() const;

    /// @brief Broadcasts a multicast Hello announcement packet.
    void broadcastHello();

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

    /// @brief Thread-safely updates configurable scopes in discovery responder.
    /// @param[in] scopes List of active scope URIs.
    void updateScopes(const std::vector<std::string>& scopes);

private:
    void runListener();

    mutable std::mutex m_scopeMutex {};
    OnvifServerConfig m_config;
    std::atomic<bool> m_running { false };
    std::atomic<DiscoveryMode> m_discoveryMode { DiscoveryMode::Discoverable };
    std::uint32_t m_metadataVersion { 1U };
    mutable AppSequenceTracker m_appSequence {};
    std::thread m_thread {};

    Net::SocketHandle m_sockFd { Net::InvalidSocket };
    DiscoveryRateLimiter m_rateLimiter;
};

} // namespace Onvif
