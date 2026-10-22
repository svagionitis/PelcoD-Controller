#pragma once

/// @file QViscaSonyDevice.h
/// @brief Qt 6 adapter wrapping SonyFCBDevice for GUI integration.

#include <SonyCameraModel.h>
#include <SonyFCBDevice.h>
#include <SonyViscaTypes.h>
#include <Transport/ITransport.h>
#include <ViscaDevice.h>
#include <ViscaFrame.h>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <mutex>

namespace ViscaApp {

/// @class QViscaSonyDevice
/// @brief Bridges Sony FCB camera VISCA operations and telemetry to Qt signals and slots.
class QViscaSonyDevice : public QObject {
    Q_OBJECT

public:
    explicit QViscaSonyDevice(std::shared_ptr<::Transport::ITransport> transport = nullptr,
        std::uint8_t cameraAddress = 1, QObject* parent = nullptr);
    ~QViscaSonyDevice() override;

    // Non-copyable, non-movable
    QViscaSonyDevice(const QViscaSonyDevice&) = delete;
    QViscaSonyDevice& operator=(const QViscaSonyDevice&) = delete;

    void setTransport(std::shared_ptr<::Transport::ITransport> transport, std::uint8_t cameraAddress = 1);
    [[nodiscard]] std::shared_ptr<::Transport::ITransport> transport() const noexcept;

    [[nodiscard]] bool connectDevice();
    void connectDeviceAsync();
    void disconnectDevice();
    [[nodiscard]] bool isConnected() const noexcept;

    [[nodiscard]] std::uint8_t cameraAddress() const noexcept;
    void setCameraAddress(std::uint8_t address);

    [[nodiscard]] Visca::Sony::SonyCameraModelType modelType() const noexcept;
    [[nodiscard]] Visca::Sony::CameraCapabilities capabilities() const noexcept;
    [[nodiscard]] Visca::Sony::SonyFCBStatus currentStatus() const noexcept;

    /// @brief Retrieves the current transport and kernel communication statistics snapshot.
    /// @return TransportStatsSnapshot containing atomic metrics and kernel telemetry.
    [[nodiscard]] ::Transport::TransportStatsSnapshot transportStats() const;

    /// @brief Retrieves the current VISCA protocol telemetry snapshot.
    /// @return ViscaProtocolStats containing socket states, completed counts, errors, and latencies.
    [[nodiscard]] Visca::ViscaProtocolStats protocolStats() const;

    [[nodiscard]] std::shared_ptr<Visca::Sony::SonyFCBDevice> coreDevice() const noexcept
    {
        return m_fcbDevice;
    }

    void setPollingInterval(int intervalMs);

signals:
    void connectionStateChanged(bool connected);
    void connectingStateChanged(bool connecting);
    void modelDiscovered(Visca::Sony::SonyCameraModelType modelType, const Visca::Sony::CameraCapabilities& caps);
    void statusUpdated(const Visca::Sony::SonyFCBStatus& status);
    void trafficLogged(bool isTx, const QByteArray& packet, const QString& description);
    void commandFailed(const QString& reason);

public slots:
    // Polling
    void pollStatus();

    // Lens & Optics
    void setZoomDirect(std::uint16_t position);
    void zoomTele(std::uint8_t speed = 4);
    void zoomWide(std::uint8_t speed = 4);
    void zoomStop();

    void setFocusAuto(bool autoMode);
    void setFocusDirect(std::uint16_t position);
    void focusOnePush();
    void setFocusNearLimit(std::uint16_t limit);

    // Exposure
    void setExposureMode(Visca::Sony::SonyExposureMode mode);
    void setShutter(std::uint8_t position);
    void setIris(std::uint8_t position);
    void setGain(std::uint8_t position);
    void setExposureCompensation(bool on, std::uint8_t position = 0);

    // White Balance
    void setWhiteBalance(Visca::Sony::SonyWhiteBalanceMode mode);
    void setRgainDirect(std::uint8_t position);
    void setBgainDirect(std::uint8_t position);
    void triggerOnePushWb();

    // Image Enhancement
    void setStabilizer(Visca::Sony::SonyStabilizerMode mode);
    void setDefog(Visca::Sony::SonyDefogMode mode);
    void setIcr(bool on);
    void setAutoIcr(bool on);

    // Hardware Gated Options
    void setDistortionCompensation(bool on);
    void setOpticalAxisGapCompensation(bool on);
    void setOperatingMode(std::uint8_t mode);
    void setLvdsMode(std::uint8_t mode);
    void setDigitalOutputMode(std::uint8_t mode);

    // Custom Registers & Raw Dispatch
    void writeRegister(std::uint8_t reg, std::uint8_t val);
    void readRegister(std::uint8_t reg);
    void sendRawHex(const QByteArray& hexData);

    // Power & Interface
    void setPower(bool on);
    void ifClear();
    void cancelSocket(int socketIndex);

private:
    void setupTrafficHook();

    std::shared_ptr<::Transport::ITransport> m_transport;
    std::uint8_t m_cameraAddress { 1 };
    std::shared_ptr<Visca::Sony::SonyFCBDevice> m_fcbDevice;

    QTimer* m_pollTimer { nullptr };
    mutable std::mutex m_mutex;
    bool m_connected { false };
};

} // namespace ViscaApp
