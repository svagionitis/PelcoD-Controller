#pragma once

/// @file SightlineQmlBridge.h
/// @brief Primary QML bridge controller exposing SightlineDevice API, models and telemetry.

#include "TrackListModel.h"
#include "TrafficLogModel.h"
#include <SightlineCore/SightlineMessages.h>
#include <SightlineQt/QSightlineDevice.h>
#include <Transport/SightlineUdpTransport.h>

#include <QObject>
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

    // 2. Stabilization
    /// @brief Configure video stabilization parameters.
    /// @param cam Camera index.
    /// @param mode Stabilization mode (0=off, 1=on).
    /// @param autoBias Automatic bias correction flag.
    /// @param maxShift Maximum allowed frame shift.
    /// @return True if dispatched.
    Q_INVOKABLE bool setStabilization(int cam, int mode, int autoBias = 1, int maxShift = 64);

    /// @brief Reset video stabilization reference frame.
    /// @param cam Camera index.
    /// @return True if dispatched.
    Q_INVOKABLE bool resetStabilization(int cam);

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

    /// @brief Configure video enhancement parameters.
    /// @param cam Camera index.
    /// @param contrast Contrast adjustment.
    /// @param brightness Brightness adjustment.
    /// @param sharpening Sharpening level.
    /// @param clahe CLAHE contrast level.
    /// @return True if dispatched.
    Q_INVOKABLE bool setVideoEnhance(int cam, int contrast, int brightness, int sharpening, int clahe);

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

signals:
    void connectionChanged();
    void hostChanged();
    void commandPortChanged();
    void replyPortChanged();
    void systemStatusChanged();
    void warningReceived();
    void versionReceived();

private slots:
    void handleTrackingPositions(const Sightline::MsgTrackingPositions& pos);
    void handleUserWarning(const Sightline::MsgUserWarningMessage& warn);
    void handleVersion(const Sightline::MsgVersionNumber& ver);
    void handleSystemStatus(const Sightline::MsgSystemStatusMessage& stat);
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

    std::unique_ptr<QSightlineDevice> m_device {};
    std::unique_ptr<TrackListModel> m_trackListModel {};
    std::unique_ptr<TrafficLogModel> m_trafficLogModel {};
};
