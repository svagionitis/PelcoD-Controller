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
    explicit QViscaSonyDevice(std::shared_ptr<::Transport::ITransport> transport = nullptr, uint8_t cameraAddress = 1,
        QObject* parent = nullptr);
    ~QViscaSonyDevice() override;

    // Non-copyable, non-movable
    QViscaSonyDevice(const QViscaSonyDevice&) = delete;
    QViscaSonyDevice& operator=(const QViscaSonyDevice&) = delete;

    void setTransport(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress = 1);
    [[nodiscard]] std::shared_ptr<::Transport::ITransport> transport() const noexcept;

    [[nodiscard]] bool connectDevice();
    void connectDeviceAsync();
    void disconnectDevice();
    [[nodiscard]] bool isConnected() const noexcept;

    [[nodiscard]] uint8_t cameraAddress() const noexcept;
    void setCameraAddress(uint8_t address);

    [[nodiscard]] Visca::Sony::SonyCameraModelType modelType() const noexcept;
    [[nodiscard]] Visca::Sony::CameraCapabilities capabilities() const noexcept;
    [[nodiscard]] Visca::Sony::SonyFCBStatus currentStatus() const noexcept;

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
    void setZoomDirect(uint16_t position);
    void zoomTele(uint8_t speed = 4);
    void zoomWide(uint8_t speed = 4);
    void zoomStop();

    void setFocusAuto(bool autoMode);
    void setFocusDirect(uint16_t position);
    void focusOnePush();
    void setFocusNearLimit(uint16_t limit);

    // Exposure
    void setExposureMode(Visca::Sony::SonyExposureMode mode);
    void setShutter(uint8_t position);
    void setIris(uint8_t position);
    void setGain(uint8_t position);
    void setExposureCompensation(bool on, uint8_t position = 0);

    // White Balance
    void setWhiteBalance(Visca::Sony::SonyWhiteBalanceMode mode);
    void setRgainDirect(uint8_t position);
    void setBgainDirect(uint8_t position);
    void triggerOnePushWb();

    // Image Enhancement
    void setStabilizer(Visca::Sony::SonyStabilizerMode mode);
    void setDefog(Visca::Sony::SonyDefogMode mode);
    void setIcr(bool on);
    void setAutoIcr(bool on);

    // Hardware Gated Options
    void setDistortionCompensation(bool on);
    void setOpticalAxisGapCompensation(bool on);
    void setOperatingMode(uint8_t mode);
    void setLvdsMode(uint8_t mode);
    void setDigitalOutputMode(uint8_t mode);

    // Custom Registers & Raw Dispatch
    void writeRegister(uint8_t reg, uint8_t val);
    void readRegister(uint8_t reg);
    void sendRawHex(const QByteArray& hexData);

    // Power & Interface
    void setPower(bool on);
    void ifClear();
    void cancelSocket(int socketIndex);

private:
    void setupTrafficHook();

    std::shared_ptr<::Transport::ITransport> m_transport;
    uint8_t m_cameraAddress { 1 };
    std::shared_ptr<Visca::Sony::SonyFCBDevice> m_fcbDevice;

    QTimer* m_pollTimer { nullptr };
    mutable std::mutex m_mutex;
    bool m_connected { false };
};

} // namespace ViscaApp
