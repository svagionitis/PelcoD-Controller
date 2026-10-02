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

    /// @brief Retrieves the latest cached stabilization parameters snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetStabilizationParameters> lastStabilization() const;

    /// @brief Retrieves the latest cached registration parameters snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetRegistrationParameters> lastRegistration() const;

    /// @brief Retrieves the latest cached stabilization bias snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetStabilizationBias> lastStabilizationBias() const;

    /// @brief Retrieves the latest cached overlay mode configuration.
    [[nodiscard]] std::optional<Sightline::MsgSetOverlayMode> lastOverlayMode() const;

    /// @brief Retrieves the latest cached active overlay object IDs bitmask.
    [[nodiscard]] std::optional<Sightline::MsgCurrentOverlayObjectsIds> lastOverlayObjectsIds() const;

    /// @brief Retrieves the latest cached logo watermark parameters.
    [[nodiscard]] std::optional<Sightline::MsgLogoParameters> lastLogoParameters() const;

    /// @brief Retrieves transport and kernel-level metrics.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

signals:
    void trackingPositionsReceived(const Sightline::MsgTrackingPositions& positions);
    void extendedPositionsReceived(const Sightline::MsgTrackingPositionsExtended& positions);
    void userWarningReceived(const Sightline::MsgUserWarningMessage& warning);
    void versionReceived(const Sightline::MsgVersionNumber& version);
    void systemStatusReceived(const Sightline::MsgSystemStatusMessage& status);
    void stabilizationReceived(const Sightline::MsgSetStabilizationParameters& params);
    void registrationReceived(const Sightline::MsgSetRegistrationParameters& params);
    void stabilizationBiasReceived(const Sightline::MsgSetStabilizationBias& bias);
    void overlayModeReceived(const Sightline::MsgSetOverlayMode& mode);
    void overlayObjectsIdsReceived(const Sightline::MsgCurrentOverlayObjectsIds& ids);
    void overlayObjectParamsReceived(const Sightline::MsgCurrentOverlayObjectParameters& params);
    void logoParametersReceived(const Sightline::MsgLogoParameters& logo);
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

    bool setStabilization(quint8 cameraIndex, quint8 mode, quint8 rate = 50U, quint8 maxShift = 0U);
    bool setStabilization(const Sightline::MsgSetStabilizationParameters& msg);
    bool resetStabilization(quint8 cameraIndex, quint8 resetType = 0U);
    bool setStabilizationBias(const Sightline::MsgSetStabilizationBias& msg);
    bool setStabilizationBias(
        quint8 cameraIndex, qint16 biasCol, qint16 biasRow, quint8 autoBias = 1U, quint8 updateRate = 50U);
    bool setRegistration(const Sightline::MsgSetRegistrationParameters& msg);
    bool getStabilization(quint8 cameraIndex = 0U);
    bool getRegistration(quint8 cameraIndex = 0U);
    bool getStabilizationBias(quint8 cameraIndex = 0U);

    bool sendLensCommand(quint8 cameraIndex, quint8 commandType, qint16 rateOrPosition);
    bool saveParameters(quint8 commitType = 0U);
    bool resetParameters(quint8 resetType = 0U);
    bool queryVersion();
    bool enableSystemStatus(bool enable = true);
    bool queryParameters(quint8 queryId);

    // Overlays & Symbology
    bool setOverlayMode(const Sightline::MsgSetOverlayMode& msg);
    bool getOverlayMode(quint8 cameraIndex = 0U);
    bool drawOverlay(const Sightline::MsgDrawOverlay& msg);
    bool drawOverlayBatch(const std::vector<Sightline::MsgDrawOverlay>& objects);
    bool drawCross(quint8 cameraIndex, quint8 objectId, qint16 centerX, qint16 centerY, quint16 size,
        Sightline::OverlayPaletteColor fgColor = Sightline::OverlayPaletteColor::White, quint16 thickness = 1U,
        bool originUpperLeft = false);
    bool drawRectangle(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y, quint16 width, quint16 height,
        bool filled = false, Sightline::OverlayPaletteColor fgColor = Sightline::OverlayPaletteColor::White,
        Sightline::OverlayPaletteColor bgColor = Sightline::OverlayPaletteColor::TransparentBgOrTurquoiseFg,
        quint8 alpha = 0U, quint16 thickness = 1U, bool originUpperLeft = true);
    bool drawText(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y, const QString& text,
        Sightline::OverlayFontId fontId = Sightline::OverlayFontId::Courier,
        Sightline::OverlayPaletteColor fgColor = Sightline::OverlayPaletteColor::White,
        Sightline::OverlayPaletteColor bgColor = Sightline::OverlayPaletteColor::TransparentBgOrTurquoiseFg,
        quint8 hScale = 32U, quint8 vScale = 32U, bool originUpperLeft = true);
    bool drawKlvField(quint8 cameraIndex, quint8 objectId, qint16 x, qint16 y, Sightline::KlvFieldTag fieldTag,
        Sightline::KlvFormatType formatType, const QString& formatString = "%s",
        Sightline::OverlayFontId fontId = Sightline::OverlayFontId::Courier,
        Sightline::OverlayPaletteColor fgColor = Sightline::OverlayPaletteColor::White, bool originUpperLeft = true);
    bool drawBlackout(quint8 cameraIndex, quint8 objectId, quint16 width = 640U, quint16 height = 480U);
    bool destroyOverlay(quint8 cameraIndex, quint8 objectId);
    bool destroyAllOverlays(quint8 cameraIndex = 0U);
    bool setLogoParameters(const Sightline::MsgLogoParameters& msg);
    bool getLogoParameters(quint8 cameraIndex = 0U);
    bool setUserFont(quint8 slotIndex, const QString& fontFileName);
    bool getOverlayObjectsIds(quint8 cameraIndex = 0U);
    bool getOverlayObjectParams(quint8 objectId);

    bool sendRawPacket(const QByteArray& rawPacket);

private:
    void wireCallbacks();

    std::unique_ptr<Sightline::SightlineDevice> m_device;
};
