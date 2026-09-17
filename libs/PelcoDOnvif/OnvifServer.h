#pragma once

/// @file OnvifServer.h
/// @brief Embedded ONVIF Profile S HTTP server and WS-Discovery responder.

#include "OnvifServerTypes.h"
#include "WsDiscoveryServer.h"

#include <httplib.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace pugi {
class xml_document;
}

namespace PelcoD::Onvif {

/// @class OnvifServer
/// @brief Standalone Qt-free ONVIF Profile S and Profile T HTTP server coordinating Device, Media, PTZ, Imaging, and
/// Event services.
class OnvifServer {
public:
    /// @brief Constructs server with configuration and optional PTZ handler.
    /// @param[in] config Server networking and metadata parameters.
    /// @param[in] ptzHandler Optional handler receiving PTZ motion and preset events.
    /// @param[in] imagingHandler Optional handler receiving Profile T imaging requests.
    explicit OnvifServer(OnvifServerConfig config = {}, std::shared_ptr<IPtzHandler> ptzHandler = nullptr,
        std::shared_ptr<IImagingHandler> imagingHandler = nullptr, std::shared_ptr<IOsdHandler> osdHandler = nullptr,
        std::shared_ptr<IDeviceManagementHandler> deviceHandler = nullptr);

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

    /// @brief Sets or replaces the active Profile T Imaging handler.
    /// @param[in] handler New IImagingHandler instance.
    void setImagingHandler(std::shared_ptr<IImagingHandler> handler);

    /// @brief Sets or replaces the active OSD handler.
    /// @param[in] handler New IOsdHandler instance.
    void setOsdHandler(std::shared_ptr<IOsdHandler> handler);

    /// @brief Sets or replaces the active Device Management handler.
    /// @param[in] handler New IDeviceManagementHandler instance.
    void setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> handler);

    /// @brief Pushes an asynchronous ONVIF event to active PullPoint and push subscriptions.
    /// @param[in] event The event to publish.
    void publishEvent(const OnvifEvent& event);

    /// @brief Sets callback for monitoring incoming SOAP requests.
    /// @param[in] callback Callback invoked on every incoming SOAP transaction.
    void setRequestLogCallback(RequestLogCallback callback);

private:
    void setupRoutes();
    void handleDeviceService(const httplib::Request& req, httplib::Response& res);
    void handleMediaService(const httplib::Request& req, httplib::Response& res);
    void handleMedia2Service(const httplib::Request& req, httplib::Response& res);
    void handlePtzService(const httplib::Request& req, httplib::Response& res);
    void handleImagingService(const httplib::Request& req, httplib::Response& res);
    void handleEventService(const httplib::Request& req, httplib::Response& res);
    void handleSubscriptionService(const httplib::Request& req, httplib::Response& res);
    void handleAnalyticsService(const httplib::Request& req, httplib::Response& res);

    void processOsdRequest(
        const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix);

    [[nodiscard]] std::string resolveHost(const httplib::Request& req) const;

    struct PullPointSubscription {
        std::string id {};
        std::chrono::steady_clock::time_point terminationTime {};
        std::deque<OnvifEvent> queue {};
        std::mutex mutex {};
        std::condition_variable cv {};
    };

    struct PushSubscription {
        std::string id {};
        std::string consumerUrl {};
        std::chrono::steady_clock::time_point terminationTime {};
    };

    OnvifServerConfig m_config;
    RequestLogCallback m_logCallback {};
    std::shared_ptr<IPtzHandler> m_ptzHandler;
    std::shared_ptr<IImagingHandler> m_imagingHandler;
    std::shared_ptr<IOsdHandler> m_osdHandler;
    std::shared_ptr<IDeviceManagementHandler> m_deviceHandler;
    std::unique_ptr<WsDiscoveryServer> m_discoveryServer;
    httplib::Server m_httpServer;

    mutable std::mutex m_deviceMutex {};
    std::vector<OnvifUser> m_internalUsers {};
    std::vector<NetworkInterfaceConfig> m_internalNetworkInterfaces {};
    std::string m_internalGateway { "192.168.1.1" };
    DnsConfig m_internalDns {};
    NtpConfig m_internalNtp {};
    std::string m_internalHostname { "PelcoD-Bridge" };
    SystemDateTimeConfig m_internalDateTime {};

    mutable std::mutex m_osdMutex {};
    std::map<std::string, OsdConfig> m_internalOsds {};
    uint32_t m_nextOsdId { 1 };

    mutable std::mutex m_subMutex {};
    std::map<std::string, std::shared_ptr<PullPointSubscription>> m_subscriptions {};
    std::map<std::string, PushSubscription> m_pushSubscriptions {};
    uint32_t m_nextSubId { 1 };

    std::atomic<bool> m_running { false };
    std::thread m_httpThread {};
};

} // namespace PelcoD::Onvif
