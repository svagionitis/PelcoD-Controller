#pragma once

/// @file SightlineQmlBridge.h
/// @brief Primary QML bridge controller exposing SightlineDevice API, models and telemetry.

#include "TrackListModel.h"
#include "TrafficLogModel.h"
#include <SightlineCore/SightlineMessages.h>
#include <SightlineQt/QSightlineDevice.h>
#include <Transport/SightlineUdpTransport.h>

#include <QElapsedTimer>
#include <QObject>
#include <QRect>
#include <QString>
#include <memory>

class SightlineQmlBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int commandPort READ commandPort WRITE setCommandPort NOTIFY commandPortChanged)
    Q_PROPERTY(int replyPort READ replyPort WRITE setReplyPort NOTIFY replyPortChanged)
    Q_PROPERTY(int cpuLoadPercent READ cpuLoadPercent NOTIFY systemStatusChanged)
    Q_PROPERTY(int coreTempC READ coreTempC NOTIFY systemStatusChanged)
    Q_PROPERTY(int uptimeSeconds READ uptimeSeconds NOTIFY systemStatusChanged)
    Q_PROPERTY(QString lastWarningMessage READ lastWarningMessage NOTIFY warningReceived)
    Q_PROPERTY(QString softwareVersion READ softwareVersion NOTIFY versionReceived)
    Q_PROPERTY(TrackListModel* trackListModel READ trackListModel CONSTANT)
    Q_PROPERTY(TrafficLogModel* trafficLogModel READ trafficLogModel CONSTANT)
    Q_PROPERTY(int activeContrastMode READ activeContrastMode NOTIFY contrastModeChanged)
    Q_PROPERTY(int activePaletteIndex READ activePaletteIndex NOTIFY paletteIndexChanged)
    Q_PROPERTY(QRect enhancementRoi READ enhancementRoi NOTIFY enhancementRoiChanged)

public:
    /// @brief Construct a new SightlineQmlBridge instance.
    /// @details Initializes internal models, timers, and device holder.
    /// @param parent Optional parent QObject.
    explicit SightlineQmlBridge(QObject* parent = nullptr);

    /// @brief Destructor.
    /// @details Cleanly shuts down transport and device connections.
    ~SightlineQmlBridge() override;

    /// @brief Check if transport is actively connected.
    /// @return True if connected, false otherwise.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Get the configured destination host IP address.
    /// @return Host IP string.
    [[nodiscard]] QString host() const;

    /// @brief Set the destination host IP address.
    /// @param host IP address string.
    void setHost(const QString& host);

    /// @brief Get the configured UDP command TX port.
    /// @return Command port number.
    [[nodiscard]] int commandPort() const noexcept;

    /// @brief Set the UDP command TX port.
    /// @param port Command port number.
    void setCommandPort(int port);

    /// @brief Get the configured UDP telemetry RX reply port.
    /// @return Reply port number.
    [[nodiscard]] int replyPort() const noexcept;

    /// @brief Set the UDP telemetry RX reply port.
    /// @param port Reply port number.
    void setReplyPort(int port);

    /// @brief Get the latest reported CPU load percentage.
    /// @return CPU load percentage (0-100).
    [[nodiscard]] int cpuLoadPercent() const noexcept;

    /// @brief Get the latest reported onboard core temperature in Celsius.
    /// @return Core temperature in degrees Celsius.
    [[nodiscard]] int coreTempC() const noexcept;

    /// @brief Get the latest reported system uptime in seconds.
    /// @return System uptime in seconds.
    [[nodiscard]] int uptimeSeconds() const noexcept;

    /// @brief Get the most recently received user warning message string.
    /// @return Warning text message.
    [[nodiscard]] QString lastWarningMessage() const;

    /// @brief Get the reported software version string.
    /// @return Software version text.
    [[nodiscard]] QString softwareVersion() const;

    /// @brief Get pointer to the TrackListModel.
    /// @return Pointer to track list model.
    [[nodiscard]] TrackListModel* trackListModel() const noexcept;

    /// @brief Get pointer to the TrafficLogModel.
    /// @return Pointer to traffic log model.
    [[nodiscard]] TrafficLogModel* trafficLogModel() const noexcept;

    /// @brief Get the active contrast mode for primary camera.
    /// @return ContrastMode enum integer.
    [[nodiscard]] int activeContrastMode() const noexcept;

    /// @brief Get the active false color palette index for primary camera.
    /// @return False color palette index.
    [[nodiscard]] int activePaletteIndex() const noexcept;

    /// @brief Get the active enhancement region of interest rectangle.
    /// @return Enhancement ROI rectangle.
    [[nodiscard]] QRect enhancementRoi() const noexcept;

    // --- QML Invokable Operations ---

    /// @brief Establish UDP connection to Sightline hardware.
    /// @details Binds RX reply port and sets destination command endpoint.
    /// @param host Host IP address.
    /// @param cmdPort Command TX port (default 14001).
    /// @param replyPort Reply RX port (default 14002).
    /// @return True on socket initialization success.
    Q_INVOKABLE bool connectUdp(const QString& host, int cmdPort = 14001, int replyPort = 14002);

    /// @brief Disconnect UDP transport and reset models.
    Q_INVOKABLE void disconnectDevice();

    // 1. Tracking
    /// @brief Command camera to start tracking target at coordinate.
    /// @param cam Camera index (0-3).
    /// @param col Center column coordinate.
    /// @param row Center row coordinate.
    /// @param w Track box width.
    /// @param h Track box height.
    /// @param flags Acquisition mode flags.
    /// @return True if command dispatched successfully.
    Q_INVOKABLE bool startTracking(int cam, int col, int row, int w, int h, int flags = 0x01);

    /// @brief Command camera to stop tracking a specific track or all tracks.
    /// @param cam Camera index.
    /// @param trackId Track ID or 0xFF for all tracks.
    /// @return True if dispatched.
    Q_INVOKABLE bool stopTracking(int cam, int trackId = 0xFF);

    /// @brief Modify parameters or mode of an existing track.
    /// @param cam Camera index.
    /// @param trackId Track ID.
    /// @param mode New track mode.
    /// @param flags Mode flags.
    /// @return True if dispatched.
    Q_INVOKABLE bool modifyTracking(int cam, int trackId, int mode, int flags = 0);

    /// @brief Nudge track gate position by pixel offset.
    /// @param cam Camera index.
    /// @param deltaCol Column pixel offset.
    /// @param deltaRow Row pixel offset.
    /// @return True if dispatched.
    Q_INVOKABLE bool nudgeTracking(int cam, int deltaCol, int deltaRow);

    /// @brief Designate a track as the primary track.
    /// @param cam Camera index.
    /// @param trackId Track ID.
    /// @return True if dispatched.
    Q_INVOKABLE bool designatePrimary(int cam, int trackId);

    // 2. Stabilization & Registration (EAN-Stabilization)
    /// @brief Configure video stabilization parameters.
    /// @param cam Camera index.
    /// @param mode Stabilization mode (0=off, 1=on).
    /// @param autoBias Automatic bias correction flag.
    /// @param maxShift Maximum allowed frame shift.
    /// @return True if dispatched.
    Q_INVOKABLE bool setStabilization(int cam, int mode, int autoBias = 1, int maxShift = 64);

    /// @brief Configure full video stabilization parameters (Message ID 0x02).
    /// @param cam Camera channel index.
    /// @param mode Bitmask mode flags.
    /// @param rate Recenter drift rate 0..255.
    /// @param maxDispOffset Maximum display offset limit.
    /// @param maxAngle Maximum rotation angle limit in degrees.
    /// @param maxStabOff Maximum stabilization offset limit.
    /// @param edgeY Background edge color Y.
    /// @param edgeU Background edge color U.
    /// @param edgeV Background edge color V.
    /// @return True if dispatched.
    Q_INVOKABLE bool setStabilizationFull(
        int cam, int mode, int rate, int maxDispOffset, int maxAngle, int maxStabOff, int edgeY, int edgeU, int edgeV);

    /// @brief Reset video stabilization reference frame (Message ID 0x04).
    /// @param cam Camera index.
    /// @param resetType 0: All filters, 1: Display filter, 2: Auto bias filter.
    /// @return True if dispatched.
    Q_INVOKABLE bool resetStabilization(int cam, int resetType = 0);

    /// @brief Configure frame registration parameters (Message ID 0x9E).
    /// @param cam Camera index.
    /// @param maxTranslation Max translation in pixels/frame.
    /// @param maxRotation Max rotation in degrees/frame (0..10).
    /// @param zoomRange Max zoom range in %/frame (0..10).
    /// @param left Left band of edge pixels to ignore.
    /// @param right Right band of edge pixels to ignore.
    /// @param top Top band of edge pixels to ignore.
    /// @param bottom Bottom band of edge pixels to ignore.
    /// @param updateRate Model update rate (100: moving, 10: low drift staring).
    /// @param flags Registration flags.
    /// @return True if dispatched.
    Q_INVOKABLE bool setRegistration(int cam, int maxTranslation, int maxRotation, int zoomRange, int left, int right,
        int top, int bottom, int updateRate, int flags);

    /// @brief Configure stabilization motion bias (Message ID 0x9F).
    /// @param cam Camera index.
    /// @param biasCol Per-frame column adjustment in pixels.
    /// @param biasRow Per-frame row adjustment in pixels.
    /// @param autoBias Enable automatic bias correction.
    /// @param updateRate Auto bias update rate.
    /// @return True if dispatched.
    Q_INVOKABLE bool setStabilizationBias(int cam, int biasCol, int biasRow, int autoBias = 1, int updateRate = 50);

    /// @brief Apply operational profile preset from EAN-Stabilization.
    /// @param cam Camera index.
    /// @param preset 0: Airborne Gimbal, 1: Fixed/Ground PTZ, 2: Moving Vehicle.
    /// @return True if dispatched.
    Q_INVOKABLE bool applyStabilizationPreset(int cam, int preset);

    /// @brief Calculate and set gimbal feedforward manual bias per EAN Section 3.1 & 3.2.
    /// @param cam Camera index.
    /// @param panLeft Pan angular rate (positive left, negative right) in deg/s.
    /// @param tiltUp Tilt angular rate (positive up, negative down) in deg/s.
    /// @param hRes Camera horizontal resolution in pixels.
    /// @param vRes Camera vertical resolution in pixels.
    /// @param hFov Camera horizontal field of view in degrees.
    /// @param vFov Camera vertical field of view in degrees.
    /// @param fps Camera frame rate in fps.
    /// @return Calculated column bias in pixels.
    Q_INVOKABLE int setGimbalFeedforwardBias(
        int cam, double panLeft, double tiltUp, int hRes, int vRes, double hFov, double vFov, double fps);

    /// @brief Toggles Registration Ignored Edges overlay on video (Message ID 0x06).
    /// @param cam Camera index.
    /// @param enable Show yellow ignored edges box on video.
    /// @return True if dispatched.
    Q_INVOKABLE bool setIgnoredEdgesOverlay(int cam, bool enable);

    // 3. Detection & AI Classification
    /// @brief Configure motion or blob detection parameters.
    /// @param cam Camera index.
    /// @param mode Detection mode.
    /// @param threshold Sensitivity threshold.
    /// @param minSize Minimum target size.
    /// @param maxSize Maximum target size.
    /// @return True if dispatched.
    Q_INVOKABLE bool setDetectionParams(int cam, int mode, int threshold, int minSize, int maxSize);

    /// @brief Configure custom AI deep learning detector parameters.
    /// @param cam Camera index.
    /// @param modelId Loaded AI model identifier.
    /// @param confThresh Confidence threshold (0-100).
    /// @param nmsThresh Non-maximum suppression threshold (0-100).
    /// @return True if dispatched.
    Q_INVOKABLE bool customAIDetect(int cam, int modelId, int confThresh, int nmsThresh);

    // 4. Focus & Lens
    /// @brief Send lens focus or zoom command.
    /// @param cam Camera index.
    /// @param cmdType Lens command type.
    /// @param rateOrPos Speed rate or target position.
    /// @return True if dispatched.
    Q_INVOKABLE bool sendLensCommand(int cam, int cmdType, int rateOrPos);

    // 5. Video & Compression
    /// @brief Configure raw video input format and frame rate.
    /// @param cam Camera index.
    /// @param format Video input format enum.
    /// @param width Frame width.
    /// @param height Frame height.
    /// @param fps Frame rate in Hz.
    /// @return True if dispatched.
    Q_INVOKABLE bool setVideoParams(int cam, int format, int width, int height, int fps);

    /// @brief Configure H.264/H.265 compression bitrate and GOP parameters.
    /// @param stream Video stream index.
    /// @param bitrate Target bitrate in kbps.
    /// @param gop Group of Pictures interval.
    /// @param quality Quality factor.
    /// @param rateCtrl Rate control mode.
    /// @return True if dispatched.
    Q_INVOKABLE bool setH264Params(int stream, int bitrate, int gop, int quality, int rateCtrl);

    /// @brief Control video streaming start or stop.
    /// @param stream Stream index.
    /// @param action Stream action (0=stop, 1=start).
    /// @return True if dispatched.
    Q_INVOKABLE bool streamingControl(int stream, int action);

    /// @brief Control SD card onboard video recording.
    /// @param state Recording state (0=stop, 1=record, 2=snapshot).
    /// @param cam Camera index.
    /// @param prefix File name prefix.
    /// @return True if dispatched.
    Q_INVOKABLE bool setSDRecording(int state, int cam, const QString& prefix);

    // 6. Blending & Enhancement
    /// @brief Configure dual-camera video blending parameters.
    /// @param cam1 Primary camera index.
    /// @param cam2 Secondary camera index.
    /// @param mode Blending mode.
    /// @param alpha Blend weight (0-255).
    /// @return True if dispatched.
    Q_INVOKABLE bool setBlendParams(int cam1, int cam2, int mode, int alpha);

    /// @brief Configure video enhancement parameters (basic).
    /// @param cam Camera index.
    /// @param contrast Contrast adjustment.
    /// @param brightness Brightness adjustment.
    /// @param sharpening Sharpening level.
    /// @param clahe CLAHE contrast level.
    /// @return True if dispatched.
    Q_INVOKABLE bool setVideoEnhance(int cam, int contrast, int brightness, int sharpening, int clahe);

    /// @brief Configure contrast mode and sharpening bounds (Message ID 0x21).
    /// @param cam Camera index.
    /// @param mode Contrast mode (0..8).
    /// @param strength Contrast strength parameter (0..127).
    /// @param blend Alpha blend (0..255).
    /// @param sharpen Sharpen level (0..15).
    /// @param radius Sharpen radius in pixels (1, 2, or 3).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setEnhancementMode(int cam, int mode, int strength, int blend, int sharpen, int radius);

    /// @brief Configure temporal denoise and motion masking (Message ID 0x21).
    /// @param cam Camera index.
    /// @param rate Denoise averaging rate (0..255).
    /// @param motionMask Enable motion masking flag.
    /// @param motionMaskType Mask type (0: Aerial, 1: Staring).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setDenoiseParameters(int cam, int rate, bool motionMask, int motionMaskType);

    /// @brief Configure histogram equalization and luminance (Message ID 0x21).
    /// @param cam Camera index.
    /// @param featureBased Feature-based histogram mode.
    /// @param sqrtHist Square-root histogram weighting.
    /// @param aveRate Temporal averaging rate (0..255).
    /// @param maxPct Max percent bin clip (0..255).
    /// @param brightness Brightness offset (0..255).
    /// @param contrast Contrast scale (0..255).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setHistogramControls(
        int cam, bool featureBased, bool sqrtHist, int aveRate, int maxPct, int brightness, int contrast);

    /// @brief Configure atmospheric scintillation preset (Message ID 0x21).
    /// @param cam Camera index.
    /// @param mode Scintillation preset mode (0..3).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setScintillationMode(int cam, int mode);

    /// @brief Configure Gaussian smoothing and LAP contour suppression (Message ID 0x21).
    /// @param cam Camera index.
    /// @param gaussianBlur Gaussian blur level (0..6).
    /// @param lapMinDiff LAP suppression threshold (0..255).
    /// @param colorEnhance Color enhancement level (0..255).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setGaussianAndLap(int cam, int gaussianBlur, int lapMinDiff, int colorEnhance);

    /// @brief Configure video enhancement region of interest (Message ID 0x21).
    /// @param cam Camera index.
    /// @param row ROI upper bounding row.
    /// @param col ROI left bounding column.
    /// @param height ROI bounding box height.
    /// @param width ROI bounding box width.
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setEnhancementRoi(int cam, int row, int col, int height, int width);

    /// @brief Configure custom spatial convolution kernel (Message ID 0x21).
    /// @param cam Camera index.
    /// @param kernelSize NxN kernel dimension (3, 5, 7, 9).
    /// @param weights List of integer kernel weights.
    /// @param normalize Whether to normalize kernel sum.
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setCustomConvolution(int cam, int kernelSize, const QVariantList& weights, bool normalize);

    /// @brief Set thermal false color palette (Message ID 0x16).
    /// @param cam Camera index.
    /// @param paletteIndex Palette index (0..41, or 127 for user custom palette).
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setFalseColorPalette(int cam, int paletteIndex);

    /// @brief Upload 256x3 YUV user lookup table to hardware (Message ID 0x72).
    /// @param paletteIndex Palette slot index (0..3).
    /// @param yuvData Exactly 768 unsigned bytes of Y, U, V tuples.
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool uploadUserPalette(int paletteIndex, const QByteArray& yuvData);

    /// @brief Load binary user palette file (256x3 YUV).
    /// @param filePath Path to binary palette file (.lut / .bin).
    /// @return Raw 768-byte palette buffer.
    Q_INVOKABLE QByteArray loadUserPaletteFile(const QString& filePath);

    /// @brief Save binary user palette file (256x3 YUV).
    /// @param filePath Target file path.
    /// @param yuvData 768-byte palette buffer.
    /// @return True on success.
    Q_INVOKABLE bool saveUserPaletteFile(const QString& filePath, const QByteArray& yuvData);

    /// @brief Configure optical lens radial distortion correction parameters (Message ID 0x6E).
    /// @param cam Camera index.
    /// @param k1 Radial barrel/pincushion coefficient 1.
    /// @param k2 Radial coefficient 2.
    /// @param centerOffsetX Horizontal optical center offset.
    /// @param centerOffsetY Vertical optical center offset.
    /// @return True if dispatched or updated.
    Q_INVOKABLE bool setLensDistortion(int cam, double k1, double k2, double centerOffsetX, double centerOffsetY);

    /// @brief Save named enhancement configuration preset to persistent storage.
    /// @param name Preset name.
    /// @param settings Dictionary of enhancement parameters.
    /// @return True on success.
    Q_INVOKABLE bool saveEnhancementPreset(const QString& name, const QVariantMap& settings);

    /// @brief Load named enhancement configuration preset from persistent storage.
    /// @param name Preset name.
    /// @return Parameter map or empty on error.
    Q_INVOKABLE QVariantMap loadEnhancementPreset(const QString& name);

    /// @brief Get list of available enhancement preset names.
    /// @return List of preset name strings.
    Q_INVOKABLE QStringList getEnhancementPresets();

    // --- Backwards compatibility wrappers ---
    Q_INVOKABLE bool setEnhanceFull(int cam, int mode, int sharpen, int blend, int enhanceParam, int denoise, int flags,
        int histAveRate, int histMaxPct, int roiRow, int roiCol, int roiHigh, int roiWide, int gaussian, int lapMinDiff,
        int colorEnhance, int brightness, int contrast, int scintillation, int sharpenRadius);
    Q_INVOKABLE bool setCustomConvolution(int cam, const QVariantList& weights, bool normalize);
    Q_INVOKABLE bool setFalseColor(int cam, int paletteIndex);
    Q_INVOKABLE bool setUserPaletteLut(int paletteIndex, const QVariantList& yuvValues);
    Q_INVOKABLE QVariantList loadPaletteFile(const QString& filePath);
    Q_INVOKABLE bool savePaletteFile(const QString& filePath, const QVariantList& yuvValues);
    Q_INVOKABLE bool saveEnhancePreset(const QString& name, const QVariantMap& settings);
    Q_INVOKABLE QVariantMap loadEnhancePreset(const QString& name);
    Q_INVOKABLE QStringList getEnhancePresets();

    /// @brief Configure 3D noise reduction filter.
    /// @param cam Camera index.
    /// @param enable Noise reduction enable flag.
    /// @param temporal Temporal filtering strength.
    /// @param spatial Spatial filtering strength.
    /// @return True if dispatched.
    Q_INVOKABLE bool setNoise3D(int cam, int enable, int temporal, int spatial);

    // 7. Telemetry & Metadata
    /// @brief Configure telemetry reporting rate and message masks.
    /// @param cam Camera index.
    /// @param period Reporting interval period.
    /// @param flags Telemetry selection flags.
    /// @return True if dispatched.
    Q_INVOKABLE bool setReportingMode(int cam, int period, int flags);

    /// @brief Transmit aircraft/sensor positioning metadata for KLV injection.
    /// @param lat Latitude in degrees.
    /// @param lon Longitude in degrees.
    /// @param alt Altitude in meters.
    /// @param heading Platform heading in degrees.
    /// @param pitch Platform pitch in degrees.
    /// @param roll Platform roll in degrees.
    /// @return True if dispatched.
    Q_INVOKABLE bool setMetadata(double lat, double lon, double alt, double heading, double pitch, double roll);

    /// @brief Configure Cursor-on-Target (CoT) XML telemetry streaming.
    /// @param enable Enable/disable CoT generation.
    /// @param port Destination UDP broadcast port.
    /// @param uid Platform UID string.
    /// @param type Cursor-on-Target entity type string.
    /// @return True if dispatched.
    Q_INVOKABLE bool setCursorOnTarget(int enable, int port, const QString& uid, const QString& type);

    // 8. System & Raw Inspection
    /// @brief Commit active parameters to onboard non-volatile flash.
    /// @param commitType Save type option.
    /// @return True if dispatched.
    Q_INVOKABLE bool saveParameters(int commitType = 0);

    /// @brief Reset onboard parameters to factory defaults.
    /// @param resetType Reset option.
    /// @return True if dispatched.
    Q_INVOKABLE bool resetParameters(int resetType = 0);

    /// @brief Query firmware version and system capabilities from device.
    /// @return True if dispatched.
    Q_INVOKABLE bool queryVersion();

    /// @brief Dispatch raw hexadecimal payload directly to device.
    /// @param hexString Hex string representing packet bytes.
    /// @return True if dispatched.
    Q_INVOKABLE bool sendRawHex(const QString& hexString);

    /// @brief Dispatch generic parameter query (Message ID 0x28).
    /// @param queryId Target setter Message ID to query.
    /// @return True if dispatched.
    Q_INVOKABLE bool queryParameters(int queryId);

    /// @brief Automatically query getter parameters for the specified module tab index.
    /// @param tabIndex Selected sidebar tab index.
    Q_INVOKABLE void queryModuleParameters(int tabIndex);

signals:
    void connectionChanged();
    void hostChanged();
    void commandPortChanged();
    void replyPortChanged();
    void systemStatusChanged();
    void warningReceived();
    void versionReceived();
    void moduleQueryDispatched(int tabIndex);
    void contrastModeChanged();
    void paletteIndexChanged();
    void enhancementRoiChanged();
    void enhancementModeChanged(int cam, int mode, int strength, int blend, int sharpen, int radius);
    void denoiseChanged(int cam, int rate, bool motionMask, int motionMaskType);
    void histogramChanged(
        int cam, bool featureBased, bool sqrtHist, int aveRate, int maxPct, int brightness, int contrast);
    void scintillationChanged(int cam, int mode);
    void gaussianAndLapChanged(int cam, int gaussianBlur, int lapMinDiff, int colorEnhance);
    void enhancementRoiUpdated(int cam, int row, int col, int height, int width);
    void falseColorPaletteChanged(int cam, int paletteIndex);
    void userPaletteUploaded(int paletteIndex, const QByteArray& yuvData);
    void lensDistortionUpdated(int cam, double k1, double k2, double centerOffsetX, double centerOffsetY);
    void stabilizationChanged(int cam, int mode, int rate, int maxDispOffset, int maxAngle, int maxStabOff);
    void registrationChanged(int cam, int maxTranslation, int maxRotation, int zoomRange, int left, int right, int top,
        int bottom, int updateRate);
    void stabilizationBiasChanged(int cam, int biasCol, int biasRow, int autoBias, int updateRate);

private slots:
    void handleTrackingPositions(const Sightline::MsgTrackingPositions& pos);
    void handleUserWarning(const Sightline::MsgUserWarningMessage& warn);
    void handleVersion(const Sightline::MsgVersionNumber& ver);
    void handleSystemStatus(const Sightline::MsgSystemStatusMessage& stat);
    void handleStabilizationParams(const Sightline::MsgSetStabilizationParameters& p);
    void handleRegistrationParams(const Sightline::MsgSetRegistrationParameters& p);
    void handleStabilizationBias(const Sightline::MsgSetStabilizationBias& b);
    void handleRawFrame(bool isTx, const QByteArray& data);

private:
    QString m_host { "127.0.0.1" };
    int m_commandPort { 14001 };
    int m_replyPort { 14002 };

    int m_cpuLoadPercent { 0 };
    int m_coreTempC { 0 };
    int m_uptimeSeconds { 0 };
    QString m_lastWarningMessage {};
    QString m_softwareVersion { "Disconnected" };

    Sightline::MsgSetVideoEnhancementFull m_cachedEnhancement[4] {};
    int m_activePaletteIndex[4] { 0, 0, 0, 0 };
    QByteArray m_activeUserPalette[4] {};
    QRect m_cachedRoi[4] {};
    double m_lensK1[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensK2[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensCenterX[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensCenterY[4] { 0.0, 0.0, 0.0, 0.0 };

    QElapsedTimer m_connectionTimer {};
    std::unique_ptr<QSightlineDevice> m_device {};
    std::unique_ptr<TrackListModel> m_trackListModel {};
    std::unique_ptr<TrafficLogModel> m_trafficLogModel {};
};
