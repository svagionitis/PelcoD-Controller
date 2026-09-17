#pragma once

/// @file QOnvifServer.h
/// @brief Qt 6 QObject adapter wrapping OnvifServer and PelcoDPtzAdapter for GUI/application management.

#include "PelcoDOnvif/OnvifServer.h"
#include "PelcoDOnvif/OnvifServerTypes.h"
#include "PelcoDOnvif/PelcoDPtzAdapter.h"
#include "QPelcoDDevice.h"

#include <QObject>
#include <QString>
#include <memory>
#include <mutex>

namespace PelcoD::Qt {

/// @class QOnvifServer
/// @brief Manages the embedded ONVIF Profile S/T HTTP and WS-Discovery server within a Qt event loop.
/// @details Exposes Qt signals/slots for starting, stopping, configuring the server, and routing incoming
/// PTZ/Imaging commands to a QPelcoDDevice.
class QOnvifServer : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs QOnvifServer with default or provided configuration.
    /// @param[in] config Initial server network and identity configuration.
    /// @param[in] parent Optional parent QObject.
    explicit QOnvifServer(PelcoD::Onvif::OnvifServerConfig config = {}, QObject* parent = nullptr);

    /// @brief Destructor ensures background server threads are gracefully stopped.
    ~QOnvifServer() override;

    // Non-copyable, non-movable
    QOnvifServer(const QOnvifServer&) = delete;
    QOnvifServer& operator=(const QOnvifServer&) = delete;
    QOnvifServer(QOnvifServer&&) = delete;
    QOnvifServer& operator=(QOnvifServer&&) = delete;

    /// @brief Checks whether the ONVIF HTTP/SOAP server is actively listening.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Retrieves the active server configuration.
    /// @return Active OnvifServerConfig.
    [[nodiscard]] PelcoD::Onvif::OnvifServerConfig config() const;

    /// @brief Updates server configuration (requires restart if server is running).
    /// @param[in] config Updated configuration settings.
    void setConfig(const PelcoD::Onvif::OnvifServerConfig& config);

    /// @brief Binds a QPelcoDDevice instance to forward incoming ONVIF PTZ commands.
    /// @param[in] device Pointer to QPelcoDDevice (can be nullptr to disable PTZ bridging).
    void bindDevice(PelcoDQt::QPelcoDDevice* device);

    /// @brief Returns the full HTTP endpoint URL (e.g. "http://192.168.1.100:8080/onvif/device_service").
    /// @return Full endpoint URL string.
    [[nodiscard]] QString endpointUrl() const;

public slots:
    /// @brief Starts the ONVIF HTTP and WS-Discovery services.
    /// @return True if started successfully.
    bool start();

    /// @brief Stops the running ONVIF services.
    void stop();

    /// @brief Restarts the ONVIF services with current configuration.
    /// @return True if restarted successfully.
    bool restart();

signals:
    /// @brief Emitted when the server successfully starts listening.
    /// @param[in] endpointUrl Advertised base endpoint URL.
    void serverStarted(const QString& endpointUrl);

    /// @brief Emitted when the server stops.
    void serverStopped();

    /// @brief Emitted when an error occurs during startup or runtime.
    /// @param[in] message Diagnostic error message.
    void errorOccurred(const QString& message);

    /// @brief Emitted when an incoming SOAP request is received.
    /// @param[in] service Target service name (e.g. "Device", "Media", "PTZ").
    /// @param[in] action Requested SOAP action name.
    /// @param[in] clientIp Originating client IP address.
    void requestLogged(const QString& service, const QString& action, const QString& clientIp);

private:
    PelcoD::Onvif::OnvifServerConfig m_config {};
    std::shared_ptr<PelcoD::Onvif::PelcoDPtzAdapter> m_ptzAdapter { nullptr };
    std::unique_ptr<PelcoD::Onvif::OnvifServer> m_server { nullptr };
    mutable std::mutex m_mutex {};
};

} // namespace PelcoD::Qt
