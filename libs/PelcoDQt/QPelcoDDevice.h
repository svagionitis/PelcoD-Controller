#pragma once

/// @file QPelcoDDevice.h
/// @brief Qt 6 QObject adapter wrapping PelcoDDevice for GUI integration.

#include "DeviceStatus.h"
#include "ITransport.h"
#include "PelcoDDevice.h"
#include "PelcoDTypes.h"
#include "RetryPolicy.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QString>
#include <QThreadPool>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
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

    /// @brief Retrieves the current transport and kernel communication statistics snapshot.
    /// @return TransportStatsSnapshot containing atomic metrics and kernel telemetry.
    [[nodiscard]] Transport::TransportStatsSnapshot transportStats() const;

    /// @brief Retrieves the current Pelco-D protocol telemetry snapshot.
    /// @return PelcoDProtocolStats containing query metrics, retry counts, and RTT.
    [[nodiscard]] PelcoD::PelcoDProtocolStats protocolStats() const;
    [[nodiscard]] PelcoD::PelcoDDevice* coreDevice() const noexcept;
    [[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> sharedCoreDevice() const noexcept
    {
        return m_device;
    }

    /// @brief Executes a callable or PelcoDDevice member function on the underlying core device if valid.
    template <typename Func, typename... Args> void invokeCore(Func&& func, Args&&... args)
    {
        if (m_device) {
            static_cast<void>(std::invoke(std::forward<Func>(func), m_device.get(), std::forward<Args>(args)...));
        }
    }

    void setTelemetryPolling(bool enable, int intervalMs = 1000);
    void setQueryTimeoutMs(int timeoutMs);

    void setRetryConfig(const PelcoD::RetryConfig& config);
    void setRetryConfig(int maxRetries, int initialBackoffMs = 50, int maxBackoffMs = 1000,
        double backoffMultiplier = 2.0, int strategy = 2);
    [[nodiscard]] PelcoD::RetryConfig retryConfig() const;

signals:
    void statusUpdated(const PelcoD::DeviceStatus& status);
    void trafficLogged(bool isTx, const QByteArray& packet, const QString& description);
    void connectionStateChanged(bool connected);
    /// @brief Emitted when an async connect attempt starts (true) or finishes (false).
    void connectingStateChanged(bool connecting);
    void queryTimeoutOccurred(const QString& queryTag);
    void queryRetryAttempted(const QString& queryTag, int attempt, int maxRetries, int delayMs);
    void commandDropped(const QByteArray& frame, PelcoD::SendResult reason);

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
    void setPhaseDelayMode(int state);
    void adjustWhiteBalanceRB(int value);
    void adjustWhiteBalanceMG(int value);
    void setMagnification(int value, bool relative = false);
    void setBaudRate(int baud);
    void setZeroPosition();

    // System & Queries
    void resetDefaults();
    void remoteReset();
    void queryPan();
    void queryTilt();
    void queryZoom();
    void queryMagnification();
    void queryDeviceType();
    void queryGeneral();
    void queryDiagnostics();
    void queryAll();

    void sendRawHex(const QByteArray& hexData);
    void sendRawHexPacket(const QString& hex);

protected:
    /// @brief Factory method creating the underlying core PelcoDDevice instance.
    /// @param[in] transport Transport interface.
    /// @param[in] address Device bus address.
    /// @return Shared pointer to newly created PelcoD::PelcoDDevice instance.
    [[nodiscard]] virtual std::shared_ptr<PelcoD::PelcoDDevice> createCoreDevice(
        std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address);

    /// @brief Subclass hook called after callbacks are bound to core device.
    virtual void onCallbacksInitialized()
    {
    }

    std::shared_ptr<PelcoD::ITransport> m_transport {};
    std::shared_ptr<PelcoD::PelcoDDevice> m_device {};
    std::uint8_t m_address { 1U };
    PelcoD::ScopedConnectionList m_deviceConnections {};

private:
    [[nodiscard]] static QString describePacket(bool isTx, const std::vector<std::uint8_t>& frame);
    void initDeviceCallbacks();

    std::atomic<std::uint64_t> m_connectGeneration { 0U };
};

} // namespace PelcoDQt
