#pragma once

/// @file OnvifServer.h
/// @brief Embedded ONVIF Profile S HTTP server and WS-Discovery responder.

#include "OnvifServerTypes.h"
#include "WsDiscoveryServer.h"

#include <httplib.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace PelcoD::Onvif {

/// @class OnvifServer
/// @brief Standalone Qt-free ONVIF Profile S HTTP server coordinating Device, Media, and PTZ services.
class OnvifServer {
public:
    /// @brief Constructs server with configuration and optional PTZ handler.
    /// @param[in] config Server networking and metadata parameters.
    /// @param[in] ptzHandler Optional handler receiving PTZ motion and preset events.
    explicit OnvifServer(OnvifServerConfig config, std::shared_ptr<IPtzHandler> ptzHandler = nullptr);

    /// @brief Destructor stops HTTP service and WS-Discovery daemon.
    ~OnvifServer();

    // Non-copyable, non-movable
    OnvifServer(const OnvifServer&) = delete;
    OnvifServer& operator=(const OnvifServer&) = delete;
    OnvifServer(OnvifServer&&) = delete;
    OnvifServer& operator=(OnvifServer&&) = delete;

    /// @brief Starts WS-Discovery responder and HTTP SOAP service threads.
    /// @return True if server started successfully.
    [[nodiscard]] bool start();

    /// @brief Stops HTTP service and WS-Discovery responder threads.
    void stop();

    /// @brief Checks if server is actively listening.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Gets active server configuration.
    /// @return Copy of active OnvifServerConfig.
    [[nodiscard]] OnvifServerConfig getConfig() const;

    /// @brief Sets or replaces the active PTZ handler.
    /// @param[in] handler New IPtzHandler instance.
    void setPtzHandler(std::shared_ptr<IPtzHandler> handler);

private:
    void setupRoutes();
    void handleDeviceService(const httplib::Request& req, httplib::Response& res);
    void handleMediaService(const httplib::Request& req, httplib::Response& res);
    void handlePtzService(const httplib::Request& req, httplib::Response& res);

    [[nodiscard]] std::string resolveHost(const httplib::Request& req) const;

    OnvifServerConfig m_config;
    std::shared_ptr<IPtzHandler> m_ptzHandler;
    std::unique_ptr<WsDiscoveryServer> m_discoveryServer;
    httplib::Server m_httpServer;

    std::atomic<bool> m_running { false };
    std::thread m_httpThread {};
};

} // namespace PelcoD::Onvif
