#pragma once

/// @file QSightlineDevice.h
/// @brief Qt adapter providing QObject wrapper, signals, and slots for Sightline SLA protocol.

#include <SightlineCore/SightlineDevice.h>
#include <SightlineCore/SightlineMessages.h>
#include <SightlineCore/SightlineTypes.h>
#include <SightlineCore/modules/SightlineBlending.h>
#include <SightlineCore/modules/SightlineCompression.h>
#include <SightlineCore/modules/SightlineGeneral.h>
#include <SightlineCore/modules/SightlineNetwork.h>
#include <SightlineCore/modules/SightlineRecording.h>

#include <QByteArray>
#include <QObject>
#include <map>
#include <memory>
#include <utility>
#include <vector>

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

    /// @brief Retrieves the latest cached detection parameters snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetDetectionParameters> lastDetectionParams() const;

    /// @brief Retrieves the latest cached advanced detection parameters.
    [[nodiscard]] std::optional<Sightline::MsgAdvancedDetectionParameters> lastAdvDetection() const;

    /// @brief Retrieves the latest cached detection ROI parameters.
    [[nodiscard]] std::optional<Sightline::MsgDetectionROI> lastDetectionROI() const;

    /// @brief Retrieves the latest cached KLV metric filters.
    [[nodiscard]] std::optional<Sightline::MsgKlvMetricFilters> lastKlvMetricFilters() const;

    /// @brief Retrieves the latest cached algorithmic tracking parameters.
    [[nodiscard]] std::optional<Sightline::MsgSetTrackingParameters> lastTrackingParameters() const;

    /// @brief Retrieves the latest cached command acknowledgment.
    [[nodiscard]] std::optional<Sightline::MsgCommandAck> lastCommandAck() const;

    /// @brief Retrieves the latest cached recording event notification.
    [[nodiscard]] std::optional<Sightline::MsgFileRecordingEvent> lastRecordingEvent() const;

    /// @brief Retrieves the latest cached recording status telemetry.
    [[nodiscard]] std::optional<Sightline::MsgCurrentRecordingStatusV2> lastRecordingStatus() const;

    /// @brief Retrieves the latest cached directory listing reply.
    [[nodiscard]] std::optional<Sightline::MsgDirectoryListingReply> lastDirListingReply() const;

    /// @brief Retrieves the latest cached H.264/H.265 compression parameters.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgSetH264Parameters> lastH264Params() const;

    /// @brief Retrieves the latest cached Ethernet display configuration.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgSetEthernetDisplayParameters> lastEthernetDisplay() const;

    /// @brief Retrieves the latest cached Ethernet video parameters.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgSetEthernetVideoParameters> lastEthernetVideo() const;

    /// @brief Retrieves the latest cached network interface parameters.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgSetNetworkParameters> lastNetworkParams() const;

    /// @brief Retrieves the latest cached network interface list.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgCurrentNetworkList> lastNetworkList() const;

    /// @brief Retrieves the latest cached system value.
    /// @return Optional message struct if received.
    [[nodiscard]] std::optional<Sightline::MsgSystemValue> lastSystemValue() const;

    /// @brief Retrieves the latest cached blend parameters snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetBlendParameters> lastBlendParams() const;

    /// @brief Retrieves the latest cached current blend parameters snapshot.
    [[nodiscard]] std::optional<Sightline::MsgCurrentBlendParameters> lastCurrentBlendParams() const;

    /// @brief Retrieves the latest cached 4-point projective homography snapshot.
    [[nodiscard]] std::optional<Sightline::MsgFourAlignPoints> lastFourAlignPoints() const;

    /// @brief Retrieves the latest cached blend alignment snapshot.
    [[nodiscard]] std::optional<Sightline::MsgBlendAlign> lastBlendAlign() const;

    /// @brief Retrieves the latest cached multiple alignment snapshot.
    [[nodiscard]] std::optional<Sightline::MsgSetMultipleAlignment> lastMultipleAlignment() const;

    /// @brief Retrieves transport and kernel-level metrics.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

signals:
    void trackingPositionsReceived(const Sightline::MsgTrackingPositions& positions);
    void trackingParametersReceived(const Sightline::MsgSetTrackingParameters& params);
    void extendedPositionsReceived(const Sightline::MsgTrackingPositionsExtended& positions);
    void trackCoastingChanged(quint8 cameraIndex, quint8 trackId, bool isCoasting);
    void primaryTrackUpdated(quint8 cameraIndex, const Sightline::TrackCoordinate& track);
    void primaryTrackChanged(quint8 cameraIndex, quint8 trackId);
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
    void detectionReceived(const Sightline::MsgSetDetectionParameters& params);
    void advDetectionReceived(const Sightline::MsgAdvancedDetectionParameters& params);
    void detectionRoiReceived(const Sightline::MsgDetectionROI& roi);
    void klvMetricFiltersReceived(const Sightline::MsgKlvMetricFilters& filters);
    void commandAckReceived(const Sightline::MsgCommandAck& ack);
    void recordingEventReceived(const Sightline::MsgFileRecordingEvent& event);
    void recordingStatusReceived(const Sightline::MsgCurrentRecordingStatusV2& status);
    void dirListingReplyReceived(const Sightline::MsgDirectoryListingReply& reply);
    void h264ParamsReceived(const Sightline::MsgSetH264Parameters& params);
    void ethernetDisplayReceived(const Sightline::MsgSetEthernetDisplayParameters& params);
    void ethernetVideoReceived(const Sightline::MsgSetEthernetVideoParameters& params);
    void networkParamsReceived(const Sightline::MsgSetNetworkParameters& params);
    void networkListReceived(const Sightline::MsgCurrentNetworkList& list);
    void systemValueReceived(const Sightline::MsgSystemValue& val);
    void blendParametersReceived(const Sightline::MsgSetBlendParameters& params);
    void currentBlendParamsReceived(const Sightline::MsgCurrentBlendParameters& params);
    void fourAlignPointsReceived(const Sightline::MsgFourAlignPoints& points);
    void blendAlignReceived(const Sightline::MsgBlendAlign& align);
    void multipleAlignmentReceived(const Sightline::MsgSetMultipleAlignment& params);
    void rawFrameReceived(bool isTx, const QByteArray& data);
    void connectionStateChanged(bool isConnected);

public slots:
    bool start();
    void stop();

    bool startTracking(
        quint8 cameraIndex, quint16 col, quint16 row, quint16 width, quint16 height, quint8 flags = 0x01U);
    bool startPrecisionTrack(
        quint8 cameraIndex, quint16 col, quint16 row, quint16 width, quint16 height, quint64 framePts);
    bool stopTracking(quint8 cameraIndex, quint8 trackId = 0xFFU);
    bool modifyTracking(quint8 cameraIndex, quint8 trackId, quint8 mode, quint8 flags = 0U);
    bool nudgeTracking(quint8 cameraIndex, qint16 deltaCol, qint16 deltaRow);
    bool nudgeDisplayTrack(quint8 cameraIndex, qint16 deltaCol, qint16 deltaRow);
    bool designatePrimary(quint8 cameraIndex, quint8 trackId);
    bool setForcedCoast(quint8 cameraIndex, quint8 trackId, Sightline::ForcedCoastingMode mode);
    bool reinitTrack(quint8 cameraIndex, quint8 trackId);
    bool resizeTrack(quint8 cameraIndex, quint8 trackId, quint16 width, quint16 height, bool assist = true);
    bool cueTrackAt(
        quint8 cameraIndex, quint16 col, quint16 row, Sightline::ModifyMode mode, quint8 trackId = 0xFFU);
    bool setTrackingParameters(const Sightline::MsgSetTrackingParameters& params);

    // Automated Target Detection & Analytics (EAN-Detection-Modes)
    bool setDetection(const Sightline::MsgSetDetectionParameters& msg);
    bool setAdvancedDetection(const Sightline::MsgAdvancedDetectionParameters& msg);
    bool setDetectionROI(const Sightline::MsgDetectionROI& msg);
    bool setVMTI(const Sightline::MsgSetVMTI& msg);
    bool triggerDetectionSnapshot(quint8 cameraIndex, quint8 detectionIndex = 0U);
    bool setKlvMetricFilters(const Sightline::MsgKlvMetricFilters& msg);
    bool setClassifierConfig(const Sightline::MsgClassifierConfig& msg);
    bool setComputeResources(bool useNpu, bool asyncInferencing);
    bool queryDetectionParams(quint8 cameraIndex = 0U, quint8 detIdx = 0U);
    bool queryAdvDetection(quint8 cameraIndex = 0U);
    bool queryDetectionROI(quint8 cameraIndex = 0U, quint8 roiIndex = 0U);
    bool queryVMTI(quint8 cameraIndex = 0U);
    bool queryTrackingPixelStats(quint8 cameraIndex = 0U, quint8 trackId = 0U);
    bool queryKlvMetricFilters(quint8 cameraIndex = 0U);
    bool queryClassifierConfig(quint8 cameraIndex = 0U);

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

    // Hardened Recording & Media Storage
    bool setFileRecordingV2(const Sightline::MsgSetFileRecordingParamsV2& msg);
    bool doSnapshotV2(const Sightline::MsgDoSnapShotV2& msg);
    bool getDirectoryListing(const Sightline::MsgGetDirectoryListing& msg);
    bool sendFileStorageMgmt(const Sightline::MsgFileStorageManagement& msg);

    // Encoding & Network Streaming
    bool setH264Params(const Sightline::MsgSetH264Parameters& params);
    bool setEthernetDisplay(const Sightline::MsgSetEthernetDisplayParameters& params);
    bool setEthernetVideo(const Sightline::MsgSetEthernetVideoParameters& params);
    bool setNetworkParams(const Sightline::MsgSetNetworkParameters& params);
    bool setTrafficControl(quint32 rateKbps, quint32 burstBytes = 3000U, quint32 mtuBytes = 1500U);
    bool getH264Params(quint16 displayId = 0x0002U);
    bool getEthernetDisplay(quint16 displayId = 0x0002U);
    bool getEthernetVideo(quint16 displayId = 0x0002U);
    bool getNetworkParams(quint8 index = 0U);
    bool getNetworkList();

    // Blending & Multi-Sensor Alignment
    bool setBlend(const Sightline::MsgSetBlendParameters& msg);
    bool getBlendParameters();
    bool setFourAlignPoints(const Sightline::MsgFourAlignPoints& msg);
    bool getFourAlignPoints(quint8 index = 0U);
    bool setBlendAlign(const Sightline::MsgBlendAlign& msg);
    bool getBlendAlign(quint8 index = 0U);
    bool setMultipleAlignment(const Sightline::MsgSetMultipleAlignment& msg);
    bool getMultipleAlignment();

    bool sendRawPacket(const QByteArray& rawPacket);

private:
    void wireCallbacks();
    void processTrackTelemetry(quint8 cameraIndex, const std::vector<Sightline::TrackCoordinate>& tracks);

    std::unique_ptr<Sightline::SightlineDevice> m_device;
    std::map<std::pair<quint8, quint8>, bool> m_knownCoastingState {};
    std::map<quint8, quint8> m_knownPrimaryTrack {};
};

Q_DECLARE_METATYPE(Sightline::TrackCoordinate)
Q_DECLARE_METATYPE(Sightline::MsgCommandAck)
Q_DECLARE_METATYPE(Sightline::MsgFileRecordingEvent)
Q_DECLARE_METATYPE(Sightline::MsgCurrentRecordingStatusV2)
Q_DECLARE_METATYPE(Sightline::MsgDirectoryListingReply)
Q_DECLARE_METATYPE(Sightline::DirListEntry)
Q_DECLARE_METATYPE(Sightline::MsgSetH264Parameters)
Q_DECLARE_METATYPE(Sightline::MsgSetEthernetDisplayParameters)
Q_DECLARE_METATYPE(Sightline::MsgSetEthernetVideoParameters)
Q_DECLARE_METATYPE(Sightline::MsgSetNetworkParameters)
Q_DECLARE_METATYPE(Sightline::MsgCurrentNetworkList)
Q_DECLARE_METATYPE(Sightline::MsgSystemValue)
Q_DECLARE_METATYPE(Sightline::MsgSetBlendParameters)
Q_DECLARE_METATYPE(Sightline::MsgCurrentBlendParameters)
Q_DECLARE_METATYPE(Sightline::MsgFourAlignPoints)
Q_DECLARE_METATYPE(Sightline::MsgBlendAlign)
Q_DECLARE_METATYPE(Sightline::MsgSetMultipleAlignment)
