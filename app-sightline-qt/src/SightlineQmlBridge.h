#pragma once

/// @file SightlineQmlBridge.h
/// @brief Primary QML bridge controller exposing SightlineDevice API, models and telemetry.

#include "RecordingFileListModel.h"
#include "SightlineNucController.h"
#include "TrackListModel.h"
#include "TrafficLogModel.h"
#include <SightlineCore/SightlineMessages.h>
#include <SightlineCore/modules/RecordingValidator.h>
#include <SightlineQt/QSightlineDevice.h>
#include <Transport/SightlineUdpTransport.h>

#include <QElapsedTimer>
#include <QJsonObject>
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
    Q_PROPERTY(SightlineNucController* nuc READ nuc CONSTANT)
    Q_PROPERTY(int activeContrastMode READ activeContrastMode NOTIFY contrastModeChanged)
    Q_PROPERTY(int activePaletteIndex READ activePaletteIndex NOTIFY paletteIndexChanged)
    Q_PROPERTY(QRect enhancementRoi READ enhancementRoi NOTIFY enhancementRoiChanged)
    Q_PROPERTY(QVariantList activeOverlayIds READ activeOverlayIds NOTIFY activeOverlayIdsChanged)
    Q_PROPERTY(bool coolerCountdownActive READ isCoolerCountdownActive NOTIFY coolerCountdownChanged)
    Q_PROPERTY(int coolerCountdownRemaining READ coolerCountdownRemaining NOTIFY coolerCountdownChanged)
    Q_PROPERTY(bool isRecordingActive READ isRecordingActive NOTIFY recordingActiveChanged)
    Q_PROPERTY(int freeStorageMB READ freeStorageMB NOTIFY recordingStatusChanged)
    Q_PROPERTY(int usedStorageMB READ usedStorageMB NOTIFY recordingStatusChanged)
    Q_PROPERTY(double storageUsagePercent READ storageUsagePercent NOTIFY recordingStatusChanged)
    Q_PROPERTY(int currentBitrateKbps READ currentBitrateKbps NOTIFY recordingStatusChanged)
    Q_PROPERTY(int droppedFrames READ droppedFrames NOTIFY recordingStatusChanged)
    Q_PROPERTY(int elapsedRecordingSec READ elapsedRecordingSec NOTIFY recordingClockChanged)
    Q_PROPERTY(QString currentFilename READ currentFilename NOTIFY recordingStatusChanged)
    Q_PROPERTY(QString lastRecordingEvent READ lastRecordingEvent NOTIFY recordingEventReceived)
    Q_PROPERTY(QString lastAckStatus READ lastAckStatus NOTIFY commandAckReceived)
    Q_PROPERTY(RecordingFileListModel* recordingFileListModel READ recordingFileListModel CONSTANT)
    Q_PROPERTY(int encBitrateKbps READ encBitrateKbps NOTIFY encParamsChanged)
    Q_PROPERTY(int encGopInterval READ encGopInterval NOTIFY encParamsChanged)
    Q_PROPERTY(int encProfile READ encProfile NOTIFY encParamsChanged)
    Q_PROPERTY(int encRateControl READ encRateControl NOTIFY encParamsChanged)
    Q_PROPERTY(int encMinQp READ encMinQp NOTIFY encParamsChanged)
    Q_PROPERTY(int encMaxQp READ encMaxQp NOTIFY encParamsChanged)
    Q_PROPERTY(int encAirMb READ encAirMb NOTIFY encParamsChanged)
    Q_PROPERTY(int encSliceRows READ encSliceRows NOTIFY encParamsChanged)
    Q_PROPERTY(int netDisplayProtocol READ netDisplayProtocol NOTIFY netDisplayChanged)
    Q_PROPERTY(QString netDisplayIp READ netDisplayIp NOTIFY netDisplayChanged)
    Q_PROPERTY(int netDisplayPort READ netDisplayPort NOTIFY netDisplayChanged)
    Q_PROPERTY(int netMaxPacket READ netMaxPacket NOTIFY netDisplayChanged)
    Q_PROPERTY(int tcRateKbps READ tcRateKbps NOTIFY tcStatusChanged)
    Q_PROPERTY(int tcBurstBytes READ tcBurstBytes NOTIFY tcStatusChanged)
    Q_PROPERTY(int tcMtuBytes READ tcMtuBytes NOTIFY tcStatusChanged)
    Q_PROPERTY(QString boardIp READ boardIp NOTIFY boardNetworkChanged)
    Q_PROPERTY(QString boardNetmask READ boardNetmask NOTIFY boardNetworkChanged)
    Q_PROPERTY(QString boardGateway READ boardGateway NOTIFY boardNetworkChanged)
    Q_PROPERTY(bool boardDhcp READ boardDhcp NOTIFY boardNetworkChanged)
    Q_PROPERTY(QStringList networkInterfaces READ networkInterfaces NOTIFY networkInterfacesChanged)

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

    /// @brief Get the NUC / DPR workflow controller (EAN-NUC-and-DPR).
    /// @return Pointer to the controller; owned by the bridge, never null.
    [[nodiscard]] SightlineNucController* nuc() const noexcept;

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

    /// @brief Check if video recording is actively in progress.
    [[nodiscard]] bool isRecordingActive() const noexcept;

    /// @brief Get remaining free storage space in megabytes.
    [[nodiscard]] int freeStorageMB() const noexcept;

    /// @brief Get consumed storage space in megabytes.
    [[nodiscard]] int usedStorageMB() const noexcept;

    /// @brief Get storage consumption percentage (0.0 to 100.0).
    [[nodiscard]] double storageUsagePercent() const noexcept;

    /// @brief Get current stream encode bitrate in Kbps.
    [[nodiscard]] int currentBitrateKbps() const noexcept;

    /// @brief Get total dropped video frames counter.
    [[nodiscard]] int droppedFrames() const noexcept;

    /// @brief Get elapsed recording time in seconds.
    [[nodiscard]] int elapsedRecordingSec() const noexcept;

    /// @brief Get active recording target filename.
    [[nodiscard]] QString currentFilename() const;

    /// @brief Get last received recording event text.
    [[nodiscard]] QString lastRecordingEvent() const;

    /// @brief Get last received command ACK status description.
    [[nodiscard]] QString lastAckStatus() const;

    /// @brief Access remote storage file list model.
    [[nodiscard]] RecordingFileListModel* recordingFileListModel() const noexcept;

    /// @brief Get encoder target bitrate in Kbps.
    [[nodiscard]] int encBitrateKbps() const noexcept;

    /// @brief Get encoder GOP / intra-frame interval.
    [[nodiscard]] int encGopInterval() const noexcept;

    /// @brief Get encoder H.264 profile.
    [[nodiscard]] int encProfile() const noexcept;

    /// @brief Get encoder rate control mode.
    [[nodiscard]] int encRateControl() const noexcept;

    /// @brief Get encoder minimum QP.
    [[nodiscard]] int encMinQp() const noexcept;

    /// @brief Get encoder maximum QP.
    [[nodiscard]] int encMaxQp() const noexcept;

    /// @brief Get adaptive intra refresh macroblocks count.
    [[nodiscard]] int encAirMb() const noexcept;

    /// @brief Get slice rows count.
    [[nodiscard]] int encSliceRows() const noexcept;

    /// @brief Get Ethernet display streaming protocol.
    [[nodiscard]] int netDisplayProtocol() const noexcept;

    /// @brief Get Ethernet display streaming destination IP.
    [[nodiscard]] QString netDisplayIp() const;

    /// @brief Get Ethernet display streaming destination UDP port.
    [[nodiscard]] int netDisplayPort() const noexcept;

    /// @brief Get Ethernet display max packet size.
    [[nodiscard]] int netMaxPacket() const noexcept;

    /// @brief Get Linux traffic control rate in Kbps.
    [[nodiscard]] int tcRateKbps() const noexcept;

    /// @brief Get Linux traffic control burst in bytes.
    [[nodiscard]] int tcBurstBytes() const noexcept;

    /// @brief Get Linux traffic control MTU in bytes.
    [[nodiscard]] int tcMtuBytes() const noexcept;

    /// @brief Get board network interface IP address.
    [[nodiscard]] QString boardIp() const;

    /// @brief Get board network subnet mask.
    [[nodiscard]] QString boardNetmask() const;

    /// @brief Get board network default gateway.
    [[nodiscard]] QString boardGateway() const;

    /// @brief Get board DHCP enable flag.
    [[nodiscard]] bool boardDhcp() const noexcept;

    /// @brief Get enumerated board network interface names.
    [[nodiscard]] QStringList networkInterfaces() const;

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
    Q_INVOKABLE bool setTrackingParameters(int cam, int mode, int flags, int maxMisses = 45, int zoomSmoothing = 5,
        int rollSmoothing = 5, int maxPauseTime = 0, int acqCol = 128, int acqRow = 96);

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

    /// @brief Configures advanced H.264/H.265 compression parameters (Message ID 0x23).
    /// @param stream Video stream index (0: Net0, 1: Net1, 2: Net2).
    /// @param bitrate Target bitrate in kbps.
    /// @param gop Group of Pictures interval (0..255).
    /// @param profile Profile (0: Baseline, 1: Main, 2: High).
    /// @param rateCtrl Rate control mode (0: Legacy, 1: Variable, 2: Constrained, 3: Balanced).
    /// @param minQp Minimum quantization parameter (0..30).
    /// @param maxQp Maximum quantization parameter (0..51).
    /// @param deblock Deblocking filter mode (0: Enabled, 1: Disabled, 2: NoSliceBoundaries).
    /// @param airMb Adaptive intra refresh macroblocks.
    /// @param sliceRows Slices per frame or slice rows.
    /// @return True if dispatched.
    Q_INVOKABLE bool setH264ParamsEx(int stream, int bitrate, int gop, int profile, int rateCtrl, int minQp, int maxQp,
        int deblock = 0, int airMb = 0, int sliceRows = 0);

    /// @brief Configures Ethernet display streaming destination (Message ID 0x51).
    /// @param stream Video stream index (0: Net0, 1: Net1, 2: Net2).
    /// @param protocol Protocol enum (1: MPEG2-TS H.264, 2: MJPEG, 4: Raw, 5: RTP H.264, 6: RTP TS H.264, 7: KLV, 8:
    /// MPEG2-TS H.265, 9: RTP H.265, 10: RTP TS H.265).
    /// @param ip Destination IP address string.
    /// @param port Destination UDP/RTP port (must be even for RTP).
    /// @param maxPacket Maximum packet size in bytes (default 1400).
    /// @param maxRawPacket Maximum raw packet size in bytes (default 0).
    /// @return True if dispatched.
    Q_INVOKABLE bool setEthernetDisplay(
        int stream, int protocol, const QString& ip, int port, int maxPacket = 1400, int maxRawPacket = 0);

    /// @brief Configures Ethernet video quality, decimation, and downsampling (Message ID 0x1A).
    /// @param stream Video stream index.
    /// @param frameStep Frame decimation step (1 = 30fps, 2 = 15fps, etc.).
    /// @param frameSize Resolution mode (0: Native, 1: 720p, 2: 480p, 3: 240p, 4: Custom, 5: Downsample 2:1, etc.).
    /// @param customW Custom frame width.
    /// @param customH Custom frame height.
    /// @param quality MJPEG quality (0..100).
    /// @param foveal Foveal quality.
    /// @return True if dispatched.
    Q_INVOKABLE bool setEthernetVideo(int stream, int frameStep = 1, int frameSize = 0, int customW = 0,
        int customH = 0, int quality = 0, int foveal = 0);

    /// @brief Export SDP file for a given video network stream (EAN-RTSP Section 7).
    /// @param stream Video network stream index (0: Net0, 1: Net1).
    /// @param destinationPath Local file system path to write the SDP file.
    /// @return True if SDP file was successfully generated and written.
    Q_INVOKABLE bool exportSdpFile(int stream, const QString& destinationPath);

    /// @brief Configures Linux Traffic Control (tc) bandwidth limiter (Message ID 0x92 Key 13).
    /// @param rateKbps Rate limit in kbps (0 to disable/reset).
    /// @param burstBytes Token bucket burst size in bytes (e.g. 3000).
    /// @param mtuBytes MTU in bytes (e.g. 1500).
    /// @return True if dispatched.
    Q_INVOKABLE bool setTrafficControl(int rateKbps, int burstBytes = 3000, int mtuBytes = 1500);

    /// @brief Resets Linux Traffic Control shaping back to unlimited.
    /// @return True if dispatched.
    Q_INVOKABLE bool resetTrafficControl();

    /// @brief Applies optimized tactical low-bandwidth profile (100 kbps, 720p, frameStep 2, CBR).
    /// @param stream Video stream index.
    /// @return True if dispatched.
    Q_INVOKABLE bool applyLowBandwidth(int stream);

    /// @brief Validates transport port parity against RFC 3550 (RTP ports must be even).
    /// @param protocol Protocol enum index.
    /// @param port UDP port number.
    /// @return True if port is valid for protocol.
    Q_INVOKABLE [[nodiscard]] bool isValidPort(int protocol, int port) const noexcept;

    /// @brief Checks if protocol is RTP-based (RFC 3550 applies).
    /// @param protocol Protocol enum index.
    /// @return True if protocol is RTP.
    Q_INVOKABLE [[nodiscard]] bool isRtp(int protocol) const noexcept;

    /// @brief Queries encoder parameters for specified stream.
    /// @param stream Video stream index.
    /// @return True if query dispatched.
    Q_INVOKABLE bool queryEncoderParams(int stream = 0);

    /// @brief Queries Ethernet display streaming destination for specified stream.
    /// @param stream Video stream index.
    /// @return True if query dispatched.
    Q_INVOKABLE bool queryDisplayParams(int stream = 0);

    /// @brief Queries network parameters and interface list.
    /// @return True if query dispatched.
    Q_INVOKABLE bool queryNetworkParams();

    /// @brief Configures board network interface parameters (Message ID 0x1C).
    /// @param ip Board IPv4 address string.
    /// @param mask Subnet mask string.
    /// @param gateway Default gateway IP string.
    /// @param dhcp Enable DHCP auto-assignment.
    /// @return True if dispatched.
    Q_INVOKABLE bool setBoardNetwork(const QString& ip, const QString& mask, const QString& gateway, bool dhcp);

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

    /// @brief Starts video recording using hardened V2 protocol.
    /// @param cam Camera index.
    /// @param prefix Base filename prefix.
    /// @param format Format index (0: TS, 1: MP4, 2: Raw).
    /// @param dest Storage destination (0: MicroSD, 1: USB, 2: Network, 3: Temp).
    /// @param maxDurationSec Maximum duration in seconds per split (0 = default).
    /// @param maxBitrateKbps Bitrate ceiling in Kbps (0 = unlimited).
    /// @param autoSplit True to enable automatic rollover.
    /// @return True if successfully dispatched.
    Q_INVOKABLE bool startRecordingV2(
        int cam, const QString& prefix, int format, int dest, int maxDurationSec, int maxBitrateKbps, bool autoSplit);

    /// @brief Stops video recording on specified camera.
    /// @param cam Camera index.
    /// @return True if successfully dispatched.
    Q_INVOKABLE bool stopRecordingV2(int cam);

    /// @brief Captures a still snapshot with explicit parameters.
    /// @param cam Camera index.
    /// @param prefix Optional custom filename.
    /// @param format Format index (0: JPEG, 1: PNG, 2: TIFF16, 3: SLRAW).
    /// @param quality Quality percentage (1-100).
    /// @param includeMetadata True to request embedding geospatial KLV/XMP metadata.
    /// @return True if successfully dispatched.
    Q_INVOKABLE bool captureSnapshotV2(int cam, const QString& prefix, int format, int quality, bool includeMetadata);

    /// @brief Queries remote filesystem directory listing.
    /// @param dest Target storage device (0: MicroSD, 1: USB).
    /// @param startIndex Starting entry index.
    /// @param maxEntries Maximum entries to return.
    /// @param filter Substring filter for matching filenames.
    /// @return True if query dispatched.
    Q_INVOKABLE bool requestDirectoryListing(int dest, int startIndex, int maxEntries, const QString& filter);

    /// @brief Modifies file pin status on remote storage.
    /// @param dest Target storage device.
    /// @param filename File to pin/unpin.
    /// @param pin True to pin, false to unpin.
    /// @return True if command dispatched.
    Q_INVOKABLE bool pinStorageFile(int dest, const QString& filename, bool pin);

    /// @brief Deletes a file on remote storage.
    /// @param dest Target storage device.
    /// @param filename Target file to delete.
    /// @return True if command dispatched.
    Q_INVOKABLE bool deleteStorageFile(int dest, const QString& filename);

    /// @brief Validates filename prefix against Sightline filename rollover rules.
    /// @param prefix Proposed prefix.
    /// @return QJsonObject with "valid" (bool) and "error" (string).
    Q_INVOKABLE [[nodiscard]] QJsonObject validateFilename(const QString& prefix);

    // 6. Blending & Enhancement
    /// @brief Configure dual-camera video blending parameters (legacy subset of 0x2F).
    /// @deprecated Use applyBlendConfig() which exposes every SLASetBlendParameters_t field.
    /// @param cam1 Primary camera index.
    /// @param cam2 Secondary camera index.
    /// @param mode Blending mode.
    /// @param alpha Blend weight (0-255).
    /// @return True if dispatched.
    Q_INVOKABLE bool setBlendParams(int cam1, int cam2, int mode, int alpha);

    /// @brief Configure multi-sensor video blending and registration parameters (Message ID 0x2F).
    /// @details Positional convenience wrapper around applyBlendConfig(). Warp fields default to
    ///          "no change" (0) and the thermal window defaults to the full range (0..255).
    /// @param[in] warpIdx Warp camera index [0..3].
    /// @param[in] fixedIdx Fixed camera index [0..3].
    /// @param[in] mode Blend mode (reserved value 5 is rejected).
    /// @param[in] amt Blend amount (0 = all IR, 255 = all EO).
    /// @param[in] hue Hue for Night/Color blends.
    /// @param[in] flags BlendFlags bitmask (bits 0..1).
    /// @param[in] hotStart Thermal threshold hot start.
    /// @param[in] coldEnd Thermal threshold cold end.
    /// @param[in] vertical Vertical warp shift (int8).
    /// @param[in] horizontal Horizontal warp shift (int8).
    /// @param[in] rotation Warp rotation (1..255 maps to -5..5 deg; 0 = no change).
    /// @param[in] zoom Warp zoom (0 = no change).
    /// @param[in] hzoom Warp horizontal zoom (0 = no change).
    /// @return True if the packet was validated and dispatched.
    Q_INVOKABLE bool setBlendParameters(int warpIdx, int fixedIdx, int mode, int amt, int hue = 0, int flags = 0,
        int hotStart = 0, int coldEnd = 255, int vertical = 0, int horizontal = 0, int rotation = 0, int zoom = 0,
        int hzoom = 0);

    /// @brief Apply a complete 0x2F blend configuration from a keyed QML map.
    /// @details Recognised keys: warpIndex, fixedIndex, mode, amt, hue, flags, hotStart, coldEnd,
    ///          absolute (bool), zoomMultiplier (0..7), vertical, horizontal, rotation, zoom, hzoom,
    ///          reset (bool), usePresetAlign (bool), presetAlignIndex (0..4 or 10..14).
    ///          Missing keys take safe defaults (see packBlendConfig()).
    /// @param[in] cfg Keyed blend configuration.
    /// @return True if connected, the configuration was valid, and the packet was dispatched.
    Q_INVOKABLE [[nodiscard]] bool applyBlendConfig(const QVariantMap& cfg);

    /// @brief Query active multi-sensor blend parameters (Message ID 0x30).
    /// @return True if dispatched.
    Q_INVOKABLE bool getBlendParameters();

    /// @brief Configure fine-tune alignment offsets and automated registration (Message ID 0xB9).
    /// @details Values are normalised/clamped by packBlendAlign() before dispatch.
    /// @param[in] index Alignment slot [0..4].
    /// @param[in] vertical Vertical offset in pixels (int16).
    /// @param[in] horizontal Horizontal offset in pixels (int16).
    /// @param[in] rotate Rotation in degrees * 128 (any integer; wrapped to [0, 360) deg).
    /// @param[in] zoom Zoom * 4096 (clamped to 0.01x..15.99x).
    /// @param[in] hzoom Horizontal zoom * 4096 (clamped to 0.01x..15.99x).
    /// @return True if dispatched.
    Q_INVOKABLE bool setBlendAlign(int index, int vertical, int horizontal, int rotate, int zoom, int hzoom);

    /// @brief Query blend alignment parameters (Message ID 0x28 query 0xB9).
    /// @param[in] index Alignment slot [0..4].
    /// @return True if dispatched.
    Q_INVOKABLE bool getBlendAlign(int index = 0);

    /// @brief Configure 4-point projective homography calibration (Message ID 0x95).
    /// @details Negative coordinates are clamped to 0 (device semantics); all zeros resets the slot.
    /// @param[in] index Alignment slot [0..4].
    /// @param[in] points Four maps with keys leftCol, leftRow, rightCol, rightRow.
    /// @return True if dispatched.
    Q_INVOKABLE bool setFourAlignPoints(int index, const QVariantList& points);

    /// @brief Query 4-point projective calibration (Message ID 0x28 query 0x95).
    /// @param[in] index Alignment slot [0..4].
    /// @return True if dispatched.
    Q_INVOKABLE bool getFourAlignPoints(int index = 0);

    /// @brief Configure multi-camera multiple alignment (Message ID 0x74).
    /// @param[in] nAlignments Number of valid slots [0..5].
    /// @param[in] alignments Up to five maps with keys vertical, horizontal, rotate, zoom, hzoom (0..255).
    /// @return True if dispatched.
    Q_INVOKABLE bool setMultipleAlignment(int nAlignments, const QVariantList& alignments);

    /// @brief Query multiple alignment parameters (Message ID 0x28 query 0x74).
    /// @return True if dispatched.
    Q_INVOKABLE bool getMultipleAlignment();

    /// @brief Check whether a value is a defined (non-reserved) BlendMode.
    /// @param[in] mode Candidate mode value.
    /// @return True for 0..4 and 6..12; false for reserved 5 and out-of-range values.
    [[nodiscard]] static bool isValidBlendMode(int mode) noexcept;

    /// @brief Check whether a value is a valid 0x2F preset alignment index.
    /// @param[in] index Candidate index.
    /// @return True for 0..4 (SLABlendAlign_t slots) and 10..14 (SLAFourAlignPoints_t slots).
    [[nodiscard]] static bool isValidPresetIdx(int index) noexcept;

    /// @brief Validate and pack a keyed QML blend configuration into a 0x2F message.
    /// @details Every narrowing conversion is clamped (or masked for bitfields) before the cast,
    ///          satisfying CERT INT31-C. Defaults: mode 1, amt 128, fixed 1, coldEnd 255,
    ///          warp fields 0 ("no change"), incremental offsets, no reset, no preset.
    /// @param[in] cfg Keyed configuration (see applyBlendConfig()).
    /// @param[out] out Packed message; only written when the function returns true.
    /// @return False if mode is reserved/out of range or the preset index is invalid while enabled.
    [[nodiscard]] static bool packBlendConfig(const QVariantMap& cfg, Sightline::MsgSetBlendParameters& out);

    /// @brief Validate and pack 0xB9 blend-align values.
    /// @details Rotation (deg * 128) is wrapped into [0, 46080); zoom/hzoom are clamped to
    ///          [41, 65495] (0.01x..15.99x * 4096); offsets are clamped to int16; index to [0..4].
    /// @param[in] index Alignment slot.
    /// @param[in] vertical Vertical offset in pixels.
    /// @param[in] horizontal Horizontal offset in pixels.
    /// @param[in] rotate Rotation in degrees * 128.
    /// @param[in] zoom Zoom * 4096.
    /// @param[in] hzoom Horizontal zoom * 4096.
    /// @return Packed, range-safe message.
    [[nodiscard]] static Sightline::MsgBlendAlign packBlendAlign(
        int index, int vertical, int horizontal, int rotate, int zoom, int hzoom) noexcept;

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

    /// @brief Transmit aircraft/sensor positioning metadata for KLV injection (Message ID 0x13).
    Q_INVOKABLE bool setMetadata(double lat, double lon, double alt, double heading, double pitch, double roll,
        double hfov = 30.0, double vfov = 20.0, double az = 0.0, double el = 0.0, int displayId = 2);

    /// @brief Configure static mission and security classification metadata (Message ID 0x14).
    Q_INVOKABLE bool setMetadataStatic(int type, const QString& value, int displayId = 2);

    /// @brief Configure frame center and ground projection / OLS DTED terrain mode (Message ID 0x15).
    Q_INVOKABLE bool setMetadataFrame(double centerLat, double centerLon, double centerEl, double frameWidth,
        double slantRange, bool enableOlsDted = false, int displayId = 2);

    /// @brief Configure KLV metadata transmission rate and enabled local sets (Message ID 0x62).
    Q_INVOKABLE bool setMetadataRate(quint64 enables, int frameStep, int displayId = 2);

    /// @brief Configure custom or extended MISB tag data (Message ID 0x96).
    Q_INVOKABLE bool setTagData(int tagId, int tagSubId, const QString& hexData, int displayId = 2);

    /// @brief Configure MISB tag update decimation rate (Message ID 0x97).
    Q_INVOKABLE bool setTagDataRate(int tagId, int frameStep, int displayId = 2);

    /// @brief Configure MISB tag source selector multiplexer (Message ID 0x98).
    Q_INVOKABLE bool setTagSourceSelector(int tagId, int selector, int displayId = 2);

    /// @brief Configure Cursor-on-Target (CoT) XML telemetry streaming (Message ID 0xB0).
    Q_INVOKABLE bool setCursorOnTarget(int mode, const QString& ipAddress, int port, int rate, int displayId = 2);

    /// @brief Configure VMTI target image chips (Message ID 0xAD).
    Q_INVOKABLE bool setVmtiChips(
        int mode, int format, int sizeType, int sizeHint, int maxPerFrame, int minFramesBetween, int displayId = 2);

    /// @brief Configure VMTI fields and ontology series update rate (Message ID 0xBF).
    Q_INVOKABLE bool setVmtiFields(int fieldsMask, int ontologyRate, int displayId = 2);

    /// @brief Inject dynamic ancillary text metadata into KLV elementary stream (Message ID 0xAC).
    Q_INVOKABLE bool setAncillaryText(
        const QString& source, const QString& originator, const QString& message, int displayId = 2);

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
    void trackingParametersReceived(int cam, int mode, int flags, int maxMisses, int zoomSmoothing, int rollSmoothing,
        int maxPauseTime, int acqCol, int acqRow);
    void recordingActiveChanged(bool active);
    void recordingStatusChanged();
    void recordingClockChanged();
    void recordingEventReceived(const QString& eventMsg);
    void commandAckReceived(int seqId, int status, const QString& statusStr);
    void encParamsChanged();
    void netDisplayChanged();
    void netVideoChanged();
    void tcStatusChanged();
    void boardNetworkChanged();
    void networkInterfacesChanged();
    void encoderParamsReceived(int stream, int bitrateKbps, int gop, int flags, int minQp, int maxQp);
    void displayParamsReceived(int stream, int protocol, const QString& ip, int port, int maxPacket);
    void trafficControlReceived(int rateKbps, int burstBytes, int mtuBytes);
    void blendParametersReceived(const QVariantMap& params);
    void currentBlendParamsReceived(const QVariantMap& params);
    void fourAlignPointsReceived(int index, const QVariantList& points);
    void blendAlignReceived(const QVariantMap& align);
    /// @brief Emitted on 0x74/0x75 multiple-alignment telemetry.
    /// @param nAlignments Number of valid slots reported by the device [0..5].
    /// @param alignments Five slot maps (vertical, horizontal, rotate, zoom, hzoom).
    void multipleAlignmentReceived(int nAlignments, const QVariantList& alignments);

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
    void handleCommandAck(const Sightline::MsgCommandAck& ack);
    void handleRecordingEvent(const Sightline::MsgFileRecordingEvent& ev);
    void handleRecordingStatus(const Sightline::MsgCurrentRecordingStatusV2& stat);
    void handleDirListingReply(const Sightline::MsgDirectoryListingReply& rep);
    void handleH264Params(const Sightline::MsgSetH264Parameters& p);
    void handleEthernetDisplay(const Sightline::MsgSetEthernetDisplayParameters& p);
    void handleEthernetVideo(const Sightline::MsgSetEthernetVideoParameters& p);
    void handleNetworkParams(const Sightline::MsgSetNetworkParameters& p);
    void handleNetworkList(const Sightline::MsgCurrentNetworkList& l);
    void handleSystemValue(const Sightline::MsgSystemValue& val);
    void handleBlendParams(const Sightline::MsgSetBlendParameters& p);
    void handleCurrentBlendParams(const Sightline::MsgCurrentBlendParameters& p);
    void handleFourAlignPoints(const Sightline::MsgFourAlignPoints& p);
    void handleBlendAlign(const Sightline::MsgBlendAlign& a);
    void handleMultipleAlignment(const Sightline::MsgSetMultipleAlignment& m);
    void onRecordingClockTick();
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

    bool m_isRecordingActive { false };
    int m_freeStorageMB { 0 };
    int m_usedStorageMB { 0 };
    double m_storageUsagePercent { 0.0 };
    int m_currentBitrateKbps { 0 };
    int m_droppedFrames { 0 };
    int m_elapsedRecordingSec { 0 };
    QString m_currentFilename {};
    QString m_lastRecordingEvent {};
    QString m_lastAckStatus {};

    int m_encBitrateKbps { 4000 };
    int m_encGopInterval { 30 };
    int m_encProfile { 2 };
    int m_encRateControl { 0 };
    int m_encMinQp { 0 };
    int m_encMaxQp { 28 };
    int m_encAirMb { 0 };
    int m_encSliceRows { 0 };

    int m_netDisplayProtocol { 1 };
    QString m_netDisplayIp { "127.0.0.1" };
    int m_netDisplayPort { 15004 };
    int m_netMaxPacket { 1400 };

    int m_tcRateKbps { 0 };
    int m_tcBurstBytes { 3000 };
    int m_tcMtuBytes { 1500 };

    QString m_boardIp { "192.168.0.107" };
    QString m_boardNetmask { "255.255.0.0" };
    QString m_boardGateway { "192.168.0.1" };
    bool m_boardDhcp { false };
    QStringList m_networkInterfaces { "eth0" };

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
    std::unique_ptr<QTimer> m_recordingClockTimer {};
    std::unique_ptr<QSightlineDevice> m_device {};
    std::unique_ptr<TrackListModel> m_trackListModel {};
    std::unique_ptr<TrafficLogModel> m_trafficLogModel {};
    std::unique_ptr<RecordingFileListModel> m_recordingFileListModel {};
    std::unique_ptr<SightlineNucController> m_nuc {}; ///< NUC / DPR workflow; sends via m_device
};
