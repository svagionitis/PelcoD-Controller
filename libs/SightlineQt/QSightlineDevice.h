#pragma once

/// @file QSightlineDevice.h
/// @brief Qt adapter providing QObject wrapper, signals, and slots for Sightline SLA protocol.

#include <SightlineCore/SightlineDevice.h>
#include <SightlineCore/SightlineMessages.h>
#include <SightlineCore/SightlineTypes.h>

#include <QByteArray>
#include <QObject>
#include <memory>

class QSightlineDevice : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs a QSightlineDevice wrapping an underlying transport channel.
    /// @param[in] transport Shared pointer to communication transport.
    /// @param[in] parent Optional QObject parent.
    explicit QSightlineDevice(std::shared_ptr<Transport::ITransport> transport, QObject* parent = nullptr);

    /// @brief Constructs a QSightlineDevice adopting an existing SightlineDevice controller.
    /// @param[in] device Unique pointer to native SightlineDevice.
    /// @param[in] parent Optional QObject parent.
    explicit QSightlineDevice(std::unique_ptr<Sightline::SightlineDevice> device, QObject* parent = nullptr);

    ~QSightlineDevice() override;

    QSightlineDevice(const QSightlineDevice&) = delete;
    QSightlineDevice& operator=(const QSightlineDevice&) = delete;
    QSightlineDevice(QSightlineDevice&&) = delete;
    QSightlineDevice& operator=(QSightlineDevice&&) = delete;

    /// @brief Checks if device transport is currently connected.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses native SightlineDevice pointer.
    [[nodiscard]] Sightline::SightlineDevice* device() const noexcept;

    /// @brief Retrieves the latest cached tracking positions.
    [[nodiscard]] std::optional<Sightline::MsgTrackingPositions> lastTrackingPositions() const;

    /// @brief Retrieves the latest cached system version information.
    [[nodiscard]] std::optional<Sightline::MsgVersionNumber> lastVersion() const;

    /// @brief Retrieves the latest cached system status snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSystemStatusMessage> lastSystemStatus() const;

    /// @brief Retrieves transport and kernel-level metrics.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

signals:
    void trackingPositionsReceived(const Sightline::MsgTrackingPositions& positions);
    void extendedPositionsReceived(const Sightline::MsgTrackingPositionsExtended& positions);
    void userWarningReceived(const Sightline::MsgUserWarningMessage& warning);
    void versionReceived(const Sightline::MsgVersionNumber& version);
    void systemStatusReceived(const Sightline::MsgSystemStatusMessage& status);
    void rawFrameReceived(bool isTx, const QByteArray& data);
    void connectionStateChanged(bool isConnected);

public slots:
    bool start();
    void stop();

    bool startTracking(
        quint8 cameraIndex, quint16 col, quint16 row, quint16 width, quint16 height, quint8 flags = 0x01U);
    bool stopTracking(quint8 cameraIndex, quint8 trackId = 0xFFU);
    bool modifyTracking(quint8 cameraIndex, quint8 trackId, quint8 mode, quint8 flags = 0U);
    bool nudgeTracking(quint8 cameraIndex, qint16 deltaCol, qint16 deltaRow);
    bool designatePrimary(quint8 cameraIndex, quint8 trackId);

    bool setStabilization(quint8 cameraIndex, quint8 mode, quint8 autoBias = 1U, quint8 maxShift = 64U);
    bool resetStabilization(quint8 cameraIndex);
    bool setStabilizationBias(quint8 cameraIndex, qint16 biasCol, qint16 biasRow, qint16 biasRotation);

    bool sendLensCommand(quint8 cameraIndex, quint8 commandType, qint16 rateOrPosition);
    bool saveParameters(quint8 commitType = 0U);
    bool resetParameters(quint8 resetType = 0U);
    bool queryVersion();
    bool enableSystemStatus(bool enable = true);
    bool queryParameters(quint8 queryId);

    bool sendRawPacket(const QByteArray& rawPacket);

private:
    void wireCallbacks();

    std::unique_ptr<Sightline::SightlineDevice> m_device;
};
