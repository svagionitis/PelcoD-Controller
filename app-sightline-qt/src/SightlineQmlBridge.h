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
#include <QTimer>
#include <QVariantList>
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
    Q_PROPERTY(QVariantList activeOverlayIds READ activeOverlayIds NOTIFY activeOverlayIdsChanged)
    Q_PROPERTY(bool coolerCountdownActive READ isCoolerCountdownActive NOTIFY coolerCountdownChanged)
    Q_PROPERTY(int coolerCountdownRemaining READ coolerCountdownRemaining NOTIFY coolerCountdownChanged)

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

    /// @brief Get active overlay object identifiers registered in session.
    /// @return List of integer object IDs.
    [[nodiscard]] QVariantList activeOverlayIds() const;

    /// @brief Check if cooler countdown sequence is actively executing.
    /// @return True if cooler countdown timer is running.
    [[nodiscard]] bool isCoolerCountdownActive() const noexcept;

    /// @brief Get remaining seconds for active cooler countdown.
    /// @return Remaining seconds (0 if inactive).
    [[nodiscard]] int coolerCountdownRemaining() const noexcept;

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

    /// @brief Command camera to start precision tracking at coordinate using MISB PTS.
    /// @param cam Camera index (0-3).
    /// @param col Center column coordinate.
    /// @param row Center row coordinate.
    /// @param w Track box width.
    /// @param h Track box height.
    /// @param framePts MISB timestamp in microseconds.
    /// @return True if command dispatched successfully.
    Q_INVOKABLE bool startPrecisionTrack(int cam, int col, int row, int w, int h, qint64 framePts);

    /// @brief Command camera to force a specific coasting mode on a track.
    /// @param cam Camera index.
    /// @param trackId Track ID.
    /// @param mode Forced coasting mode (3=None, 4=FreezeUpdates, 5=FreezeSearch, 6=FreezePropagation).
    /// @return True if dispatched.
    Q_INVOKABLE bool setForcedCoast(int cam, int trackId, int mode);

    /// @brief Command camera to reinitialize primary track model.
    /// @param cam Camera index.
    /// @param trackId Track ID.
    /// @return True if dispatched.
    Q_INVOKABLE bool reinitTrack(int cam, int trackId);

    /// @brief Dynamically resize track gate with or without acquisition assist.
    /// @param cam Camera index.
    /// @param trackId Track ID.
    /// @param w New width.
    /// @param h New height.
    /// @param assist True to enable acquisition assist re-centering.
    /// @return True if dispatched.
    Q_INVOKABLE bool resizeTrack(int cam, int trackId, int w, int h, bool assist = true);

    /// @brief Cue track or designate primary near coordinates using modify mode.
    /// @param cam Camera index.
    /// @param col Column coordinate.
    /// @param row Row coordinate.
    /// @param mode Modify mode integer (0-13).
    /// @param trackId Track ID or -1 (0xFF) for unassigned.
    /// @return True if dispatched.
    Q_INVOKABLE bool cueTrackAt(int cam, int col, int row, int mode, int trackId = -1);

    /// @brief Nudge track gate in display coordinate frame (handles sensor rotation).
    /// @param cam Camera index.
    /// @param deltaCol Column pixel offset in display frame.
    /// @param deltaRow Row pixel offset in display frame.
    /// @return True if dispatched.
    Q_INVOKABLE bool nudgeDisplayTrack(int cam, int deltaCol, int deltaRow);

    /// @brief Configures algorithmic tracking parameters and modes (Message ID 0x0C).
    /// @param cam Camera index (0-3).
    /// @param mode Tracking mode (0=Vehicle, 1=Stationary, 2=Scene, 4=Static, 5=Drone, 6=Person).
    /// @param flags Feature bitmask (AcqAssist, IntelAssist, Color, ZoomScaling, UniqueTracks, AutoMode).
    /// @param maxMisses Maximum coasting misses before dropping track (default 45).
    /// @param zoomSmoothing Zoom rate filter smoothing factor (default 5).
    /// @param rollSmoothing Roll rate filter smoothing factor (default 5).
    /// @param maxPauseTime Maximum pause duration in seconds (0..20).
    /// @param acqCol Initial acquisition search column size in pixels (default 128).
    /// @param acqRow Initial acquisition search row size in pixels (default 96).
    /// @return True if dispatched.
    Q_INVOKABLE bool setTrackingParameters(int cam, int mode, int flags,
        int maxMisses = 45, int zoomSmoothing = 5, int rollSmoothing = 5,
        int maxPauseTime = 0, int acqCol = 128, int acqRow = 96);

    /// @brief Queries current tracking parameters from device.
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool queryTrackingParameters(int cam = 0);

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

    // 3. Detection & AI Classification (EAN-Detection-Modes)
    /// @brief Configure motion or blob detection parameters.
    /// @param cam Camera index.
    /// @param mode Detection mode.
    /// @param threshold Sensitivity threshold.
    /// @param minSize Minimum target size.
    /// @param maxSize Maximum target size.
    /// @return True if dispatched.
    Q_INVOKABLE bool setDetectionParams(int cam, int mode, int threshold, int minSize, int maxSize);

    /// @brief Comprehensive detection mode & sensitivity configuration (EAN Sec 2).
    Q_INVOKABLE bool setDetectionExtended(int cam, int detIdx, int mode, int sensMode, int threshold, int minSize,
        int maxSize, int bkgdThresh = 0, int watchFrames = 0, int suspScore = 0);

    /// @brief Configure advanced algorithmic tuning parameters (EAN Sec 2.1-2.4).
    Q_INVOKABLE bool setDetectionAdvanced(int cam, int updateRate, int surroundSize, int blobDir, bool use8Bit,
        int gasOriginal, int gasColor, int iouThresh, bool enableMtd, int downsample);

    /// @brief Configure directional 2-point detection line ROI (EAN Sec 3.1).
    Q_INVOKABLE bool setDetectionRoiLine(int cam, int detIdx, int roiIdx, int x1, int y1, int x2, int y2, int lineSide);

    /// @brief Configure 16x16 masked grid detection ROI (EAN Sec 3.2).
    Q_INVOKABLE bool setDetectionRoiGrid(int cam, int detIdx, int roiIdx, int blocksW, int blocksH,
        const QString& mask0, const QString& mask1, const QString& mask2, const QString& mask3,
        bool showRegions = false);

    /// @brief Triggers automated high-res detection snapshot capture (Message ID 0xAB).
    Q_INVOKABLE bool triggerDetectionSnapshot(int cam, int detIdx = 0);

    /// @brief Configure deep learning classifier parameters and custom models (EAN Sec 4.1).
    Q_INVOKABLE bool setClassifierSettings(int cam, int model, const QString& customModel, int maxPerFrame, int minDims,
        int droneMode, int pad, int updateRate);

    /// @brief Assign hardware compute resource execution (NPU vs CPU, Sync vs Async).
    Q_INVOKABLE bool setComputeAssignment(bool useNpu, bool asyncInferencing);

    /// @brief Configure KLV metric dimension & spatial horizon filters (EAN Sec 4.4.4).
    Q_INVOKABLE bool setKlvMetricBounds(int cam, double minW, double maxW, double minH, double maxH, bool aboveHorizon,
        bool belowHorizon, double minLat, double maxLat, double minLon, double maxLon);

    /// @brief Query active detection parameters.
    Q_INVOKABLE bool queryDetection(int cam, int detIdx = 0);

    /// @brief Query advanced detection parameters.
    Q_INVOKABLE bool queryAdvDetection(int cam);

    /// @brief Query detection ROI.
    Q_INVOKABLE bool queryDetectionROI(int cam, int roiIdx = 0);

    /// @brief Query KLV metric filters.
    Q_INVOKABLE bool queryKlvMetricFilters(int cam);

    /// @brief Query classifier configuration.
    Q_INVOKABLE bool queryClassifierConfig(int cam);

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

    // 6. Overlays & Graphic Primitives (Module 0x62 & 0x9C)
    /// @brief Configure reticle mode and graphics feature flags (Message ID 0x06).
    /// @param cam Camera index.
    /// @param primaryReticle Primary reticle style index.
    /// @param secondaryReticle Secondary reticle style index.
    /// @param graphicsMask Bitmask of OverlayGraphicsFlags.
    /// @return True if dispatched.
    Q_INVOKABLE bool setOverlayMode(int cam, int primaryReticle, int secondaryReticle, int graphicsMask);

    /// @brief Query overlay mode for camera (Message ID 0x06).
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool getOverlayMode(int cam = 0);

    /// @brief Draw cross graphic object (Message ID 0x9C).
    /// @param cam Camera index.
    /// @param objId Object identifier (0-255).
    /// @param x Center X coordinate in pixels.
    /// @param y Center Y coordinate in pixels.
    /// @param size Arm length/size in pixels.
    /// @param fgColor Color index (0-15).
    /// @param thickness Line thickness in pixels.
    /// @param originUpperLeft True for top-left (0,0), false for center (0,0).
    /// @return True if dispatched.
    Q_INVOKABLE bool drawCross(
        int cam, int objId, int x, int y, int size, int fgColor = 0, int thickness = 1, bool originUpperLeft = false);

    /// @brief Draw rectangle graphic object (Message ID 0x9C).
    /// @param cam Camera index.
    /// @param objId Object identifier (0-255).
    /// @param x Corner X coordinate in pixels.
    /// @param y Corner Y coordinate in pixels.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @param filled True to fill interior.
    /// @param fgColor Border/foreground color index (0-15).
    /// @param bgColor Fill/background color index (0-15).
    /// @param alpha 5-bit alpha opacity level (0-31).
    /// @param thickness Border line thickness.
    /// @param originUpperLeft True for top-left origin.
    /// @return True if dispatched.
    Q_INVOKABLE bool drawRectangle(int cam, int objId, int x, int y, int w, int h, bool filled, int fgColor = 0,
        int bgColor = 14, int alpha = 0, int thickness = 1, bool originUpperLeft = true);

    /// @brief Draw text banner graphic object (Message ID 0x9C).
    /// @param cam Camera index.
    /// @param objId Object identifier (0-255).
    /// @param x Starting X coordinate.
    /// @param y Starting Y coordinate.
    /// @param text Text string to display.
    /// @param fontId Font family ID (0=Courier, 1=Arial, etc.).
    /// @param fgColor Text color index (0-15).
    /// @param bgColor Background box color index (0-15).
    /// @param hScale Horizontal font scale (8..255).
    /// @param vScale Vertical font scale (8..255).
    /// @param originUpperLeft True for top-left origin.
    /// @return True if dispatched.
    Q_INVOKABLE bool drawText(int cam, int objId, int x, int y, const QString& text, int fontId = 0, int fgColor = 0,
        int bgColor = 14, int hScale = 32, int vScale = 32, bool originUpperLeft = true);

    /// @brief Draw dynamic KLV telemetry field graphic object (Message ID 0x9C).
    /// @param cam Camera index.
    /// @param objId Object identifier (0-255).
    /// @param x Starting X coordinate.
    /// @param y Starting Y coordinate.
    /// @param fieldTag KLV field tag index.
    /// @param formatType Value format style index.
    /// @param formatString Printf-style format template.
    /// @param fontId Font family ID.
    /// @param fgColor Text color index.
    /// @param originUpperLeft True for top-left origin.
    /// @return True if dispatched.
    Q_INVOKABLE bool drawKlvField(int cam, int objId, int x, int y, int fieldTag, int formatType,
        const QString& formatString = "%s", int fontId = 0, int fgColor = 0, bool originUpperLeft = true);

    /// @brief Draw solid blackout rectangle covering sensor image.
    /// @param cam Camera index.
    /// @param objId Object identifier.
    /// @param width Screen width in pixels (default 640).
    /// @param height Screen height in pixels (default 480).
    /// @return True if dispatched.
    Q_INVOKABLE bool drawBlackout(int cam, int objId, int width = 640, int height = 480);

    /// @brief Destroy/delete specific graphic overlay object.
    /// @param cam Camera index.
    /// @param objId Object identifier to delete.
    /// @return True if dispatched.
    Q_INVOKABLE bool destroyOverlay(int cam, int objId);

    /// @brief Destroy/delete all user overlay objects on camera.
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool destroyAllOverlays(int cam = 0);

    /// @brief Configure watermark logo opacity and offsets (Message ID 0x9B).
    /// @param cam Camera index.
    /// @param opacity Transparency alpha value (0-255).
    /// @param offsetX Offset from right edge in pixels.
    /// @param offsetY Offset from bottom edge in pixels.
    /// @return True if dispatched.
    Q_INVOKABLE bool setLogoParameters(int cam, int opacity, int offsetX, int offsetY);

    /// @brief Query watermark logo parameters (Message ID 0x9B).
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool getLogoParameters(int cam = 0);

    /// @brief Assign user TrueType font file to font slot (Message ID 0xAE).
    /// @param slotIndex Font slot index (0-15).
    /// @param fontPath Local file path or font identifier string.
    /// @return True if dispatched.
    Q_INVOKABLE bool setUserFont(int slotIndex, const QString& fontPath);

    /// @brief Query active user overlay object IDs from hardware (Message ID 0x68).
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool getOverlayObjectsIds(int cam = 0);

    /// @brief Query detailed parameters of specific overlay object (Message ID 0x6B).
    /// @param objId Object identifier.
    /// @return True if dispatched.
    Q_INVOKABLE bool getOverlayObjectParams(int objId);

    /// @brief Start automated cooler countdown sequence (EAN-Overlay-Graphics Section 10).
    /// @param cam Camera index.
    /// @param durationSeconds Countdown duration in seconds.
    /// @return True if started.
    Q_INVOKABLE bool startCoolerCountdown(int cam, int durationSeconds = 15);

    /// @brief Cancel active cooler countdown sequence.
    Q_INVOKABLE void cancelCoolerCountdown();

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
    void activeOverlayIdsChanged();
    void coolerCountdownChanged();
    void coolerCountdownFinished();
    void overlayModeReceived(int cam, int primaryReticle, int secondaryReticle, int graphicsMask);
    void overlayObjectParamsReceived(int objId, int objType, int x, int y);
    void logoParametersReceived(int cam, int opacity, int offsetX, int offsetY);
    void detectionParamsReceived(int cam, int detIdx, int mode, int sensMode, int threshold, int minSize, int maxSize);
    void advDetectionReceived(int cam, int updateRate, int surroundSize, int blobDir, bool use8Bit, int gasOriginal,
        int gasColor, int iouThresh, bool enableMtd, int downsample);
    void detectionRoiReceived(
        int cam, int detIdx, int roiIdx, int geomMode, int x1, int y1, int x2, int y2, int lineSide);
    void klvMetricFiltersReceived(int cam, double minW, double maxW, double minH, double maxH, bool aboveHorizon,
        bool belowHorizon, double minLat, double maxLat, double minLon, double maxLon);
    void trackCoastingChanged(int cam, int trackId, bool isCoasting);
    void trackingParametersReceived(int cam, int mode, int flags, int maxMisses, int zoomSmoothing,
        int rollSmoothing, int maxPauseTime, int acqCol, int acqRow);

private slots:
    void handleTrackingPositions(const Sightline::MsgTrackingPositions& pos);
    void handleTrackingPositionsExtended(const Sightline::MsgTrackingPositionsExtended& ext);
    void handleTrackingParameters(const Sightline::MsgSetTrackingParameters& p);
    void handleUserWarning(const Sightline::MsgUserWarningMessage& warn);
    void handleVersion(const Sightline::MsgVersionNumber& ver);
    void handleSystemStatus(const Sightline::MsgSystemStatusMessage& stat);
    void handleStabilizationParams(const Sightline::MsgSetStabilizationParameters& p);
    void handleRegistrationParams(const Sightline::MsgSetRegistrationParameters& p);
    void handleStabilizationBias(const Sightline::MsgSetStabilizationBias& b);
    void handleRawFrame(bool isTx, const QByteArray& data);
    void handleOverlayMode(const Sightline::MsgSetOverlayMode& m);
    void handleOverlayObjectsIds(const Sightline::MsgCurrentOverlayObjectsIds& ids);
    void handleOverlayObjectParams(const Sightline::MsgCurrentOverlayObjectParameters& p);
    void handleLogoParameters(const Sightline::MsgLogoParameters& l);
    void handleDetectionParams(const Sightline::MsgSetDetectionParameters& det);
    void handleAdvDetection(const Sightline::MsgAdvancedDetectionParameters& adv);
    void handleDetectionROI(const Sightline::MsgDetectionROI& roi);
    void handleKlvMetricFilters(const Sightline::MsgKlvMetricFilters& filters);
    void onCoolerTimerTick();

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
    Sightline::MsgSetTrackingParameters m_cachedTrackingParams[4] {};
    int m_activePaletteIndex[4] { 0, 0, 0, 0 };
    QByteArray m_activeUserPalette[4] {};
    QRect m_cachedRoi[4] {};
    double m_lensK1[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensK2[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensCenterX[4] { 0.0, 0.0, 0.0, 0.0 };
    double m_lensCenterY[4] { 0.0, 0.0, 0.0, 0.0 };

    std::unique_ptr<QTimer> m_coolerTimer {};
    int m_coolerRemaining { 0 };
    int m_coolerCamera { 0 };
    QVariantList m_activeOverlayIds {};

    QElapsedTimer m_connectionTimer {};
    std::unique_ptr<QSightlineDevice> m_device {};
    std::unique_ptr<TrackListModel> m_trackListModel {};
    std::unique_ptr<TrafficLogModel> m_trafficLogModel {};
};
