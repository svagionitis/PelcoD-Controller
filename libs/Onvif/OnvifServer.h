#pragma once

/// @file OnvifServer.h
/// @brief Embedded ONVIF Profile S HTTP server and WS-Discovery responder.

#include "OnvifServerTypes.h"
#include "WsDiscoveryServer.h"

#include <cstdint>
#include <httplib.h>
#include <pugixml.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace Onvif {

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
        std::shared_ptr<IDeviceManagementHandler> deviceHandler = nullptr,
        std::shared_ptr<IDeviceIoHandler> deviceIoHandler = nullptr,
        std::shared_ptr<IMetadataHandler> metadataHandler = nullptr,
        std::shared_ptr<IAnalyticsHandler> analyticsHandler = nullptr);

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

    /// @brief Sets or replaces the active Device I/O handler.
    /// @param[in] handler New IDeviceIoHandler instance.
    void setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> handler);

    /// @brief Sets or replaces the active Metadata handler.
    /// @param[in] handler New IMetadataHandler instance.
    void setMetadataHandler(std::shared_ptr<IMetadataHandler> handler);

    /// @brief Sets or replaces the active Recording handler (Profile G).
    /// @param[in] handler New IRecordingHandler instance.
    void setRecordingHandler(std::shared_ptr<IRecordingHandler> handler);

    /// @brief Sets or replaces the active Search handler (Profile G).
    /// @param[in] handler New ISearchHandler instance.
    void setSearchHandler(std::shared_ptr<ISearchHandler> handler);

    /// @brief Sets or replaces the active Replay handler (Profile G).
    /// @param[in] handler New IReplayHandler instance.
    void setReplayHandler(std::shared_ptr<IReplayHandler> handler);

    /// @brief Sets or replaces the active Analytics handler (Profile M & Profile T).
    /// @param[in] handler New IAnalyticsHandler instance.
    void setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> handler);

    /// @brief Sets or replaces the active Privacy Mask handler (Profile T / Media2).
    /// @param[in] handler New IMaskHandler instance.
    void setMaskHandler(std::shared_ptr<IMaskHandler> handler);

    /// @brief Sets or replaces the active Video Source Mode handler (Profile T / Media2).
    /// @param[in] handler New IVideoSourceModeHandler instance.
    void setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> handler);

    /// @brief Sets or replaces the active Thermal & Radiometry handler.
    /// @param[in] handler New IThermalHandler instance.
    void setThermalHandler(std::shared_ptr<IThermalHandler> handler);

    /// @brief Records an operational message into the internal system log buffer.
    /// @param[in] level Log level ("INFO", "WARNING", "ERROR").
    /// @param[in] msg Log message.
    void logSystemMessage(const std::string& level, const std::string& msg);

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
    void handleDeviceIoService(const httplib::Request& req, httplib::Response& res);
    void handleEventService(const httplib::Request& req, httplib::Response& res);
    void handleSubscriptionService(const httplib::Request& req, httplib::Response& res);
    void handleAnalyticsService(const httplib::Request& req, httplib::Response& res);
    void handleMetadataStream(const httplib::Request& req, httplib::Response& res);
    void handleRecordingService(const httplib::Request& req, httplib::Response& res);
    void handleSearchService(const httplib::Request& req, httplib::Response& res);
    void handleReplayService(const httplib::Request& req, httplib::Response& res);
    void handleThermalService(const httplib::Request& req, httplib::Response& res);

    void processOsdRequest(
        const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix);
    void processMetadataRequest(
        const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix);
    void processMaskRequest(
        const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix);
    void processVideoSourceModeRequest(
        const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix);

    [[nodiscard]] std::string generateMetadataStreamXml(const MetadataStreamPayload& payload) const;
    [[nodiscard]] std::string resolveHost(const httplib::Request& req) const;

    struct SoapRequest {
        pugi::xml_node bodyNode {};
        pugi::xml_node reqNode {};
        std::string opName {};
    };

    [[nodiscard]] std::optional<SoapRequest> parseSoapRequest(
        const httplib::Request& req, httplib::Response& res, std::string_view serviceName, pugi::xml_document& doc);

    void appendAccessLog(std::string_view serviceName, const std::string& opName, const std::string& remoteAddr);

    void sendSoapResponse(httplib::Response& res, const std::string& bodyXml, int status = 200);

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
    std::shared_ptr<IDeviceIoHandler> m_deviceIoHandler;
    std::shared_ptr<IMetadataHandler> m_metadataHandler;
    std::shared_ptr<IRecordingHandler> m_recordingHandler;
    std::shared_ptr<ISearchHandler> m_searchHandler;
    std::shared_ptr<IReplayHandler> m_replayHandler;
    std::shared_ptr<IAnalyticsHandler> m_analyticsHandler;
    std::shared_ptr<IMaskHandler> m_maskHandler;
    std::shared_ptr<IVideoSourceModeHandler> m_videoSourceModeHandler;
    std::shared_ptr<IThermalHandler> m_thermalHandler;
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

    mutable std::mutex m_certMutex {};
    std::vector<OnvifCertificate> m_internalCertificates {};
    ClientCertificateMode m_internalClientCertMode { ClientCertificateMode::Off };

    mutable std::mutex m_imagingMutex {};
    FocusStatus20 m_internalFocusStatus {};
    std::vector<ImagingPreset> m_internalImagingPresets {};
    std::string m_currentImagingPresetToken {};

    mutable std::mutex m_deviceIoMutex {};
    std::vector<RelayOutputConfig> m_internalRelayOutputs {};
    std::vector<DigitalInputConfig> m_internalDigitalInputs {};

    mutable std::mutex m_metadataMutex {};
    std::vector<MetadataConfiguration> m_internalMetadataConfigs {};

    mutable std::mutex m_recordingMutex {};
    std::vector<RecordingConfig> m_internalRecordings {};
    std::vector<RecordingJob> m_internalRecordingJobs {};
    ReplayConfiguration m_internalReplayConfig {};

    mutable std::mutex m_searchMutex {};
    std::map<std::string, std::vector<RecordingSearchResult>> m_recordingSearches {};
    std::map<std::string, std::vector<RecordedEventResult>> m_eventSearches {};
    std::uint32_t m_nextSearchId { 1 };

    mutable std::mutex m_analyticsMutex {};
    std::vector<AnalyticsRule> m_internalRules {};
    std::vector<AnalyticsModule> m_internalModules {};

    mutable std::mutex m_logMutex {};
    std::deque<std::string> m_systemLogs {};
    std::deque<std::string> m_accessLogs {};

    std::chrono::steady_clock::time_point m_startTime { std::chrono::steady_clock::now() };

    mutable std::mutex m_osdMutex {};
    std::map<std::string, OsdConfig> m_internalOsds {};
    std::uint32_t m_nextOsdId { 1 };

    mutable std::mutex m_maskMutex {};
    std::vector<PrivacyMask> m_internalMasks {};
    MaskOptions m_internalMaskOptions {};
    std::uint32_t m_nextMaskId { 1 };

    mutable std::mutex m_videoSourceModeMutex {};
    std::vector<VideoSourceMode> m_internalVideoSourceModes {};

    mutable std::mutex m_thermalMutex {};
    RadiometryConfig m_internalRadiometryConfig {};
    std::vector<RadiometrySpot> m_internalRadiometrySpots {};
    std::vector<RadiometryBox> m_internalRadiometryBoxes {};
    std::vector<ColorPalette> m_internalColorPalettes {};
    std::string m_internalActiveColorPalette { "WhiteHot" };
    ThermalCapabilities m_internalThermalCaps {};

    mutable std::mutex m_subMutex {};
    std::map<std::string, std::shared_ptr<PullPointSubscription>> m_subscriptions {};
    std::map<std::string, PushSubscription> m_pushSubscriptions {};
    std::uint32_t m_nextSubId { 1 };

    std::atomic<bool> m_running { false };
    std::thread m_httpThread {};
};

} // namespace Onvif
 
namespace PelcoD {
namespace Onvif = ::Onvif;
} // namespace PelcoD
