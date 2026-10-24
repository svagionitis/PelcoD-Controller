#pragma once

/// @file OnvifServer.h
/// @brief Embedded ONVIF Profile S HTTP server and WS-Discovery responder.

#include <cstdint>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"

namespace Onvif {

class ScopeStore;
class SoapRouter;

/// @class OnvifServer
/// @brief Standalone Qt-free ONVIF Profile S and Profile T HTTP server coordinating Device, Media, PTZ, Imaging, and
/// Event services. Encapsulated via Pimpl idiom for ABI stability and minimal compilation overhead.
class OnvifServer {
public:
    /// @brief Constructs server with configuration and optional PTZ handler.
    /// @param[in] config Server networking and metadata parameters.
    /// @param[in] ptzHandler Optional handler receiving PTZ motion and preset events.
    /// @param[in] imagingHandler Optional handler receiving Profile T imaging requests.
    /// @param[in] osdHandler Optional handler receiving Profile T OSD requests.
    /// @param[in] deviceHandler Optional handler receiving Device Management operations.
    /// @param[in] deviceIoHandler Optional handler receiving Device I/O operations.
    /// @param[in] metadataHandler Optional handler receiving Metadata stream requests.
    /// @param[in] analyticsHandler Optional handler receiving Analytics rules and module requests.
    explicit OnvifServer(OnvifServerConfig config = { }, std::shared_ptr<IPtzHandler> ptzHandler = nullptr,
        std::shared_ptr<IImagingHandler> imagingHandler = nullptr, std::shared_ptr<IOsdHandler> osdHandler = nullptr,
        std::shared_ptr<IDeviceManagementHandler> deviceHandler = nullptr,
        std::shared_ptr<IDeviceIoHandler> deviceIoHandler = nullptr,
        std::shared_ptr<IMetadataHandler> metadataHandler = nullptr,
        std::shared_ptr<IAnalyticsHandler> analyticsHandler = nullptr);

    /// @brief Destructor stops HTTP service and WS-Discovery daemon.
    ~OnvifServer();

    // Non-copyable, non-movable per MISRA C++:2023
    OnvifServer(const OnvifServer&) = delete;
    OnvifServer& operator=(const OnvifServer&) = delete;
    OnvifServer(OnvifServer&&) = delete;
    OnvifServer& operator=(OnvifServer&&) = delete;

    /// @brief Starts WS-Discovery responder and HTTP SOAP service threads.
    /// @return True if the server is actively listening.
    [[nodiscard]] bool start();

    /// @brief Stops HTTP service and WS-Discovery responder threads.
    void stop();

    /// @brief Checks if server is actively listening.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Gets active device provisioning lifecycle state (CWE-798, ETSI EN 303 645 §5.1).
    /// @return Current ProvisioningState (Unprovisioned vs Provisioned).
    [[nodiscard]] ProvisioningState provisioningState() const noexcept;

    /// @brief Gets active device discovery mode (Discoverable vs NonDiscoverable).
    /// @return Active DiscoveryMode.
    [[nodiscard]] DiscoveryMode getDiscoveryMode() const noexcept;

    /// @brief Sets device discovery mode and notifies discovery server.
    /// @param[in] mode Target DiscoveryMode.
    void setDiscoveryMode(DiscoveryMode mode);

    /// @brief Gets active network protocol configurations.
    /// @return Vector of NetworkProtocolConfig.
    [[nodiscard]] std::vector<NetworkProtocolConfig> getNetworkProtocols() const;

    /// @brief Sets network protocol configurations.
    /// @param[in] protocols List of updated protocols.
    /// @return True on success.
    [[nodiscard]] bool setNetworkProtocols(const std::vector<NetworkProtocolConfig>& protocols);

    /// @brief Gets active server configuration.
    /// @return Copy of active OnvifServerConfig.
    [[nodiscard]] OnvifServerConfig getConfig() const;

    /// @brief Gets the thread-safe canonical ScopeStore.
    /// @return Shared pointer to ScopeStore instance.
    [[nodiscard]] std::shared_ptr<ScopeStore> getScopeStore() const noexcept;

    /// @brief Gets a const reference to the internal declarative SOAP operation router.
    /// @return Const reference to SoapRouter.
    [[nodiscard]] const SoapRouter& getSoapRouter() const noexcept;

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

    /// @brief Gets a thread-safe snapshot of the active PTZ handler.
    /// @return Active IPtzHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IPtzHandler> getPtzHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Imaging handler.
    /// @return Active IImagingHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IImagingHandler> getImagingHandler() const;

    /// @brief Gets a thread-safe snapshot of the active OSD handler.
    /// @return Active IOsdHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IOsdHandler> getOsdHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Device Management handler.
    /// @return Active IDeviceManagementHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IDeviceManagementHandler> getDeviceManagementHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Device I/O handler.
    /// @return Active IDeviceIoHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IDeviceIoHandler> getDeviceIoHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Metadata handler.
    /// @return Active IMetadataHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IMetadataHandler> getMetadataHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Recording handler.
    /// @return Active IRecordingHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IRecordingHandler> getRecordingHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Search handler.
    /// @return Active ISearchHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<ISearchHandler> getSearchHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Replay handler.
    /// @return Active IReplayHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IReplayHandler> getReplayHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Analytics handler.
    /// @return Active IAnalyticsHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IAnalyticsHandler> getAnalyticsHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Privacy Mask handler.
    /// @return Active IMaskHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IMaskHandler> getMaskHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Video Source Mode handler.
    /// @return Active IVideoSourceModeHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IVideoSourceModeHandler> getVideoSourceModeHandler() const;

    /// @brief Gets a thread-safe snapshot of the active Thermal handler.
    /// @return Active IThermalHandler shared pointer, or nullptr.
    [[nodiscard]] std::shared_ptr<IThermalHandler> getThermalHandler() const;

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

    /// @brief Callback returning binary JPEG image bytes for snapshot requests.
    using SnapshotProvider = std::function<std::vector<std::uint8_t>()>;

    /// @brief Sets custom frame provider for snapshot delivery.
    /// @param[in] provider Function returning JPEG encoded image bytes.
    void setSnapshotProvider(SnapshotProvider provider);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Onvif
