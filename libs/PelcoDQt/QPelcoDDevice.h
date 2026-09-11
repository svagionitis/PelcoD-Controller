#pragma once

/// @file QPelcoDDevice.h
/// @brief Qt 6 QObject adapter wrapping PelcoDDevice for GUI integration.

#include "DeviceStatus.h"
#include "ITransport.h"
#include "PelcoDDevice.h"
#include "PelcoDTypes.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>
#include <thread>
#include <vector>

namespace PelcoDQt {

/// @class QPelcoDDevice
/// @brief Bridges core PelcoDDevice events and operations to Qt signals and slots.
class QPelcoDDevice : public QObject {
    Q_OBJECT

public:
    explicit QPelcoDDevice(
        std::shared_ptr<PelcoD::ITransport> transport = nullptr, std::uint8_t address = 1U, QObject* parent = nullptr);
    ~QPelcoDDevice() override;

    void setTransport(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address = 1U);
    [[nodiscard]] bool connectDevice();
    /// @brief Initiates the TCP/Serial connect on a background thread.
    ///        Returns immediately; result is signalled via connectionStateChanged().
    void connectDeviceAsync();
    void disconnectDevice();
    [[nodiscard]] bool isConnected() const noexcept;

    [[nodiscard]] PelcoD::DeviceStatus currentStatus() const;
    [[nodiscard]] PelcoD::DeviceInfo deviceInfo() const;
    [[nodiscard]] PelcoD::PelcoDDevice* coreDevice() const noexcept;

    void setTelemetryPolling(bool enable, int intervalMs = 1000);
    void setQueryTimeoutMs(int timeoutMs);

signals:
    void statusUpdated(const PelcoD::DeviceStatus& status);
    void trafficLogged(bool isTx, const QByteArray& packet, const QString& description);
    void connectionStateChanged(bool connected);
    /// @brief Emitted when an async connect attempt starts (true) or finishes (false).
    void connectingStateChanged(bool connecting);
    void queryTimeoutOccurred(const QString& queryTag);

public slots:
    // Motion
    void panLeft(int speed);
    void panRight(int speed);
    void tiltUp(int speed);
    void tiltDown(int speed);
    void stopMotion();
    void move(int panDir, int panSpeed, int tiltDir, int tiltSpeed);

    // Zoom, Focus, Iris
    void zoomTele();
    void zoomWide();
    void zoomStop();
    void focusNear();
    void focusFar();
    void focusStop();
    void irisOpen();
    void irisClose();
    void irisStop();

    // Absolute Positioning
    void setPanAngle(int centidegrees);
    void setTiltAngle(int centidegrees);
    void setZoomPosition(int position);

    // Presets
    void setPreset(int presetId);
    void clearPreset(int presetId);
    void goToPreset(int presetId);
    void flip180();
    void zeroPan();

    // Aux & Zones
    void setAuxiliary(int auxId);
    void clearAuxiliary(int auxId);
    void setZoneStart(int zoneId);
    void setZoneEnd(int zoneId);
    void setZoneScan(bool enable);

    // Patterns
    void recordPatternStart(int patternId);
    void recordPatternStop();
    void runPattern(int patternId);

    // Speeds & Options
    void setZoomSpeed(int speed);
    void setFocusSpeed(int speed);
    void setAutoFocus(int mode);
    void setAutoIris(int mode);
    void setAgc(int mode);
    void setBacklightComp(bool enable);
    void setAutoWhiteBalance(bool enable);
    void setShutterSpeed(int speed);
    void setGain(int gain);
    void setAutoIrisLevel(int level);
    void setAutoIrisPeak(int peak);

    // System & Queries
    void resetDefaults();
    void remoteReset();
    void queryPan();
    void queryTilt();
    void queryZoom();
    void queryDeviceType();
    void queryGeneral();
    void queryAll();

    void sendRawHex(const QByteArray& hexData);
    void sendRawHexPacket(const QString& hex);

private:
    [[nodiscard]] static QString describePacket(bool isTx, const std::vector<std::uint8_t>& frame);

    std::shared_ptr<PelcoD::ITransport> m_transport;
    std::unique_ptr<PelcoD::PelcoDDevice> m_device;
    std::uint8_t m_address { 1U };
    std::thread m_connectThread;
};

} // namespace PelcoDQt
