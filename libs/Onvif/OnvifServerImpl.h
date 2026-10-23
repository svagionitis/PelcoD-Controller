#pragma once

/// @file OnvifServerImpl.h
/// @brief Internal implementation structure for OnvifServer (Pimpl idiom).

#include "OnvifServer.h"

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pugixml.hpp>

#include "Authenticator.h"
#include "CredentialStore.h"
#include "HttplibInclude.h"
#include "OnvifServerTypes.h"
#include "ScopeStore.h"
#include "SoapRouter.h"
#include "WsDiscoveryServer.h"
#include "services/IService.h"

namespace Onvif {

class SubscriptionManager;
class NotificationDispatcher;

/// @struct OnvifServer::Impl
/// @brief Private implementation state and internal operations for OnvifServer.
struct OnvifServer::Impl {
    explicit Impl(OnvifServerConfig configIn, std::shared_ptr<IPtzHandler> ptzIn,
        std::shared_ptr<IImagingHandler> imagingIn, std::shared_ptr<IOsdHandler> osdIn,
        std::shared_ptr<IDeviceManagementHandler> deviceIn, std::shared_ptr<IDeviceIoHandler> deviceIoIn,
        std::shared_ptr<IMetadataHandler> metadataIn, std::shared_ptr<IAnalyticsHandler> analyticsIn);
    ~Impl();

    // Operational lifecycle methods
    bool start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] ProvisioningState provisioningState() const noexcept;
    [[nodiscard]] DiscoveryMode getDiscoveryMode() const noexcept;
    void setDiscoveryMode(DiscoveryMode mode);
    [[nodiscard]] std::vector<NetworkProtocolConfig> getNetworkProtocols() const;
    bool setNetworkProtocols(const std::vector<NetworkProtocolConfig>& protocols);
    [[nodiscard]] OnvifServerConfig getConfig() const;
    [[nodiscard]] std::shared_ptr<ScopeStore> getScopeStore() const noexcept;
    [[nodiscard]] const SoapRouter& getSoapRouter() const noexcept;

    // Handler accessors
    void setPtzHandler(std::shared_ptr<IPtzHandler> handler);
    [[nodiscard]] std::shared_ptr<IPtzHandler> getPtzHandler() const;
    void setImagingHandler(std::shared_ptr<IImagingHandler> handler);
    [[nodiscard]] std::shared_ptr<IImagingHandler> getImagingHandler() const;
    void setOsdHandler(std::shared_ptr<IOsdHandler> handler);
    [[nodiscard]] std::shared_ptr<IOsdHandler> getOsdHandler() const;
    void setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> handler);
    [[nodiscard]] std::shared_ptr<IDeviceManagementHandler> getDeviceManagementHandler() const;
    void setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> handler);
    [[nodiscard]] std::shared_ptr<IDeviceIoHandler> getDeviceIoHandler() const;
    void setMetadataHandler(std::shared_ptr<IMetadataHandler> handler);
    [[nodiscard]] std::shared_ptr<IMetadataHandler> getMetadataHandler() const;
    void setRecordingHandler(std::shared_ptr<IRecordingHandler> handler);
    [[nodiscard]] std::shared_ptr<IRecordingHandler> getRecordingHandler() const;
    void setSearchHandler(std::shared_ptr<ISearchHandler> handler);
    [[nodiscard]] std::shared_ptr<ISearchHandler> getSearchHandler() const;
    void setReplayHandler(std::shared_ptr<IReplayHandler> handler);
    [[nodiscard]] std::shared_ptr<IReplayHandler> getReplayHandler() const;
    void setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> handler);
    [[nodiscard]] std::shared_ptr<IAnalyticsHandler> getAnalyticsHandler() const;
    void setMaskHandler(std::shared_ptr<IMaskHandler> handler);
    [[nodiscard]] std::shared_ptr<IMaskHandler> getMaskHandler() const;
    void setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> handler);
    [[nodiscard]] std::shared_ptr<IVideoSourceModeHandler> getVideoSourceModeHandler() const;
    void setThermalHandler(std::shared_ptr<IThermalHandler> handler);
    [[nodiscard]] std::shared_ptr<IThermalHandler> getThermalHandler() const;

    void logSystemMessage(const std::string& level, const std::string& msg);
    void publishEvent(const OnvifEvent& event);
    void setRequestLogCallback(RequestLogCallback callback);
    void setSnapshotProvider(OnvifServer::SnapshotProvider provider);

    // Internal HTTP & SOAP pipeline methods
    void setupRoutes();
    void handleSoapEndpoint(const httplib::Request& req, httplib::Response& res, std::string_view serviceName,
        std::string_view fallbackNs = { });
    void handleMetadataStream(const httplib::Request& req, httplib::Response& res);
    [[nodiscard]] std::string generateMetadataStreamXml(const MetadataStreamPayload& payload) const;
    [[nodiscard]] std::string resolveHost(const httplib::Request& req) const;

    struct SoapRequest {
        pugi::xml_node bodyNode { };
        pugi::xml_node reqNode { };
        std::string opName { };
    };

    [[nodiscard]] std::optional<SoapRequest> parseSoapRequest(
        const httplib::Request& req, httplib::Response& res, std::string_view serviceName, pugi::xml_document& doc);

    void appendAccessLog(std::string_view serviceName, const std::string& opName, const std::string& remoteAddr);
    void sendSoapResponse(httplib::Response& res, const std::string& bodyXml, int status = 200);

    [[nodiscard]] bool authorize(const httplib::Request& req, httplib::Response& res, std::string_view serviceName,
        const std::string& opName, const pugi::xml_document& doc);
    [[nodiscard]] bool authorizeHttp(const httplib::Request& req, httplib::Response& res, std::string_view what);
    void rejectAuth(const httplib::Request& req, httplib::Response& res, AuthOutcome outcome, std::string_view what);
    void rejectForbidden(
        const httplib::Request& req, httplib::Response& res, const Principal& who, std::string_view what);
    void appendAuthFailLog(const std::string& remoteAddr, std::string_view what, std::string_view reason);

    // -------------------------------------------------------------------------
    // Server State Members
    // -------------------------------------------------------------------------
    OnvifServerConfig config;
    RequestLogCallback logCallback { };
    mutable std::mutex handlerMutex { };
    std::shared_ptr<IPtzHandler> ptzHandler { };
    std::shared_ptr<IImagingHandler> imagingHandler { };
    std::shared_ptr<IOsdHandler> osdHandler { };
    std::shared_ptr<IDeviceManagementHandler> deviceHandler { };
    std::shared_ptr<IDeviceIoHandler> deviceIoHandler { };
    std::shared_ptr<IMetadataHandler> metadataHandler { };
    std::shared_ptr<IRecordingHandler> recordingHandler { };
    std::shared_ptr<ISearchHandler> searchHandler { };
    std::shared_ptr<IReplayHandler> replayHandler { };
    std::shared_ptr<IAnalyticsHandler> analyticsHandler { };
    std::shared_ptr<IMaskHandler> maskHandler { };
    std::shared_ptr<IVideoSourceModeHandler> videoSourceModeHandler { };
    std::shared_ptr<IThermalHandler> thermalHandler { };
    std::shared_ptr<ScopeStore> scopeStore { };
    std::unique_ptr<WsDiscoveryServer> discoveryServer { };
    httplib::Server httpServer;

    mutable std::mutex deviceMutex { };
    std::shared_ptr<CredentialStore> credStore { };
    std::unique_ptr<Authenticator> authenticator { };
    std::vector<NetworkInterfaceConfig> internalNetworkInterfaces { };
    std::vector<NetworkProtocolConfig> internalNetworkProtocols { };
    std::string internalGateway { "192.168.1.1" };
    DnsConfig internalDns { };
    NtpConfig internalNtp { };
    std::string internalHostname { "PelcoD-Bridge" };
    SystemDateTimeConfig internalDateTime { };

    mutable std::mutex certMutex { };
    std::vector<OnvifCertificate> internalCertificates { };
    ClientCertificateMode internalClientCertMode { ClientCertificateMode::Off };

    mutable std::mutex imagingMutex { };
    FocusStatus20 internalFocusStatus { };
    std::vector<ImagingPreset> internalImagingPresets { };
    std::string currentImagingPresetToken { };

    mutable std::mutex deviceIoMutex { };
    std::vector<RelayOutputConfig> internalRelayOutputs { };
    std::vector<DigitalInputConfig> internalDigitalInputs { };

    mutable std::mutex metadataMutex { };
    std::vector<MetadataConfiguration> internalMetadataConfigs { };

    mutable std::mutex recordingMutex { };
    std::vector<RecordingConfig> internalRecordings { };
    std::vector<RecordingJob> internalRecordingJobs { };
    ReplayConfiguration internalReplayConfig { };

    mutable std::mutex searchMutex { };
    std::map<std::string, std::vector<RecordingSearchResult>> recordingSearches { };
    std::map<std::string, std::vector<RecordedEventResult>> eventSearches { };
    std::uint32_t nextSearchId { 1 };

    mutable std::mutex analyticsMutex { };
    std::vector<AnalyticsRule> internalRules { };
    std::vector<AnalyticsModule> internalModules { };

    mutable std::mutex logMutex { };
    std::deque<std::string> systemLogs { };
    std::deque<std::string> accessLogs { };

    std::chrono::steady_clock::time_point startTime { std::chrono::steady_clock::now() };

    mutable std::mutex osdMutex { };
    std::map<std::string, OsdConfig> internalOsds { };
    std::uint32_t nextOsdId { 1 };

    mutable std::mutex maskMutex { };
    std::vector<PrivacyMask> internalMasks { };
    MaskOptions internalMaskOptions { };
    std::uint32_t nextMaskId { 1 };

    mutable std::mutex videoSourceModeMutex { };
    std::vector<VideoSourceMode> internalVideoSourceModes { };

    mutable std::mutex thermalMutex { };
    RadiometryConfig internalRadiometryConfig { };
    std::vector<RadiometrySpot> internalRadiometrySpots { };
    std::vector<RadiometryBox> internalRadiometryBoxes { };
    std::vector<ColorPalette> internalColorPalettes { };
    std::string internalActiveColorPalette { "WhiteHot" };
    ThermalCapabilities internalThermalCaps { };

    std::shared_ptr<SubscriptionManager> subManager { };
    std::shared_ptr<NotificationDispatcher> dispatcher { };

    mutable std::mutex mediaMutex { };
    std::vector<ServerMediaProfile> internalProfiles { };
    std::vector<VideoSourceConfig> internalVideoSources { };
    std::vector<VideoEncoderConfig> internalVideoEncoders { };
    std::uint32_t nextProfileId { 2 };

    mutable std::mutex ptzConfigMutex { };
    std::vector<PTZConfiguration> internalPtzConfigs { };

    std::atomic<ProvisioningState> provState { ProvisioningState::Unprovisioned };
    std::atomic<DiscoveryMode> discoveryMode { DiscoveryMode::Discoverable };
    mutable std::mutex snapshotMutex { };
    OnvifServer::SnapshotProvider snapshotProvider { };

    std::atomic<bool> running { false };
    std::thread httpThread { };
    SoapRouter soapRouter { };
    std::vector<std::unique_ptr<IService>> services { };
    Principal lastAuthPrincipal { };
};

} // namespace Onvif
