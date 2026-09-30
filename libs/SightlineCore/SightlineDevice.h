#pragma once

/// @file SightlineDevice.h
/// @brief Asynchronous controller for Sightline SLA protocol video tracking and stabilization.

#include "SightlineMessages.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "SightlineStreamAccumulator.h"
#include "SightlineTypes.h"
#include "Transport/ITransport.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Sightline {

/// @class SightlineDevice
/// @brief Manages serial/Ethernet connection, command dispatch, and asynchronous telemetry.
class SightlineDevice {
public:
    using TrackingCallback = std::function<void(const MsgTrackingPositions&)>;
    using ExtendedPositionsCallback = std::function<void(const MsgTrackingPositionsExtended&)>;
    using WarningCallback = std::function<void(const MsgUserWarningMessage&)>;
    using VersionCallback = std::function<void(const MsgVersionNumber&)>;
    using SystemStatusCallback = std::function<void(const MsgSystemStatusMessage&)>;
    using RawTrafficCallback = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;

    /// @brief Constructs a SightlineDevice wrapping the given transport channel.
    /// @param[in] transport Shared pointer to underlying communication transport.
    /// @param[in] maxAccumulatorBuffer Maximum inbound stream buffer capacity before forced flush.
    explicit SightlineDevice(
        std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer = 16384U);
    virtual ~SightlineDevice();

    SightlineDevice(const SightlineDevice&) = delete;
    SightlineDevice& operator=(const SightlineDevice&) = delete;
    SightlineDevice(SightlineDevice&&) = delete;
    SightlineDevice& operator=(SightlineDevice&&) = delete;

    /// @brief Starts communication, attaches callbacks to transport, and opens the channel.
    /// @return True if transport is open and ready.
    [[nodiscard]] bool start();

    /// @brief Stops communication, detaches callbacks, and closes the transport channel.
    void stop();

    /// @brief Checks whether the underlying transport is currently connected and open.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses the underlying transport channel.
    [[nodiscard]] std::shared_ptr<Transport::ITransport> transport() const noexcept;

    /// @brief Captures transport-layer and kernel-level communication statistics.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

    // --- Tracking Commands ---

    /// @brief Initiates target acquisition and tracking on the specified camera.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] col Center column coordinate in pixels.
    /// @param[in] row Center row coordinate in pixels.
    /// @param[in] width Initial tracking gate width in pixels.
    /// @param[in] height Initial tracking gate height in pixels.
    /// @param[in] flags Acquisition flags (e.g. primary/secondary track).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool startTracking(std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
        std::uint16_t width, std::uint16_t height, std::uint8_t flags = 0x01U);

    /// @brief Terminates target tracking.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Target ID to stop, or 0xFF for all active tracks.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool stopTracking(std::uint8_t cameraIndex, std::uint8_t trackId = 0xFFU);

    /// @brief Modifies active tracking mode or flags.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Target track ID.
    /// @param[in] mode Tracking mode parameter.
    /// @param[in] flags Option flags.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool modifyTracking(
        std::uint8_t cameraIndex, std::uint8_t trackId, std::uint8_t mode, std::uint8_t flags = 0U);

    /// @brief Applies fine sub-pixel nudge offsets to the primary track.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] deltaCol Pixel column offset to nudge.
    /// @param[in] deltaRow Pixel row offset to nudge.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool nudgeTracking(std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow);

    /// @brief Configures coordinate telemetry update rate and reporting mode.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] framePeriod Rate divisor (1 = every frame, 2 = every 2nd frame).
    /// @param[in] reportingFlags Bitmask of telemetry inclusions.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setReportingMode(
        std::uint8_t cameraIndex, std::uint8_t framePeriod, std::uint8_t reportingFlags);

    /// @brief Designates an active track as the primary track.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Track identifier to promote.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool designatePrimary(std::uint8_t cameraIndex, std::uint8_t trackId);

    /// @brief Shifts track gate position relative to target centroid.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Track identifier.
    /// @param[in] shiftCol Offset column in pixels.
    /// @param[in] shiftRow Offset row in pixels.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool shiftTrack(
        std::uint8_t cameraIndex, std::uint8_t trackId, std::int16_t shiftCol, std::int16_t shiftRow);

    /// @brief Stops a single identified track.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Track identifier to stop.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool stopTrack(std::uint8_t cameraIndex, std::uint8_t trackId);

    /// @brief Configures moving target indication (MTI) detection parameters.
    /// @param[in] msg Detection parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setDetection(const MsgSetDetectionParameters& msg);

    /// @brief Executes custom deep learning AI model detection.
    /// @param[in] msg AI detect parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool customAIDetect(const MsgCustomAIDetect& msg);

    // --- Stabilization Commands ---

    /// @brief Configures electronic image stabilization parameters.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] mode Stabilization mode (0: Off, 1: On, 2: Auto).
    /// @param[in] autoBias Automatic motion bias removal enable.
    /// @param[in] maxShift Maximum shift limit in pixels.
    /// @param[in] flags Option flags.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilization(std::uint8_t cameraIndex, std::uint8_t mode, std::uint8_t autoBias = 1U,
        std::uint8_t maxShift = 64U, std::uint8_t flags = 0U);

    /// @brief Resets stabilization smoothing filters.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool resetStabilization(std::uint8_t cameraIndex);

    /// @brief Sets stabilization motion bias offsets.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] biasCol Column bias in pixels.
    /// @param[in] biasRow Row bias in pixels.
    /// @param[in] biasRotation Rotation bias.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilizationBias(
        std::uint8_t cameraIndex, std::int16_t biasCol, std::int16_t biasRow, std::int16_t biasRotation);

    /// @brief Configures multi-sensor video fusion and blending.
    /// @param[in] msg Blend parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setBlend(const MsgSetBlendParameters& msg);

    /// @brief Configures 3D spatio-temporal noise reduction filter.
    /// @param[in] msg Noise reduction parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setNoise3D(const MsgNoise3D& msg);

    // --- Video & Display Pipeline ---

    /// @brief Configures video capture format.
    /// @param[in] msg Video parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoParams(const MsgSetVideoParameters& msg);

    /// @brief Configures video zoom, freeze, and orientation mode.
    /// @param[in] msg Video mode parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoMode(const MsgSetVideoMode& msg);

    /// @brief Configures image contrast, brightness, and CLAHE enhancement.
    /// @param[in] msg Enhancement parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoEnhance(const MsgSetVideoEnhancement& msg);

    /// @brief Configures display output placement and scaling.
    /// @param[in] msg Display parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setDisplayParams(const MsgSetDisplayParameters& msg);

    /// @brief Configures network Ethernet video streaming destination.
    /// @param[in] msg Ethernet video parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setEthernetVideo(const MsgSetEthernetVideoParameters& msg);

    /// @brief Configures H.264 video compression parameters.
    /// @param[in] msg H.264 parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setH264Params(const MsgSetH264Parameters& msg);

    /// @brief Controls onboard SD card recording.
    /// @param[in] msg SD recording parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setSDRecording(const MsgSetSDRecordingParameters& msg);

    /// @brief Starts, stops, or pauses network video streams.
    /// @param[in] streamIndex Zero-based stream index.
    /// @param[in] action Action code (0: Stop, 1: Start, 2: Pause).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool streamingControl(std::uint8_t streamIndex, std::uint8_t action);

    // --- Metadata & KLV ---

    /// @brief Updates platform telemetry for MISB KLV injection.
    /// @param[in] msg Metadata values.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadata(const MsgSetMetadataValues& msg);

    /// @brief Configures metadata injection rate.
    /// @param[in] metadataType Type of metadata.
    /// @param[in] ratePeriod Divisor period.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadataRate(std::uint8_t metadataType, std::uint8_t ratePeriod);

    /// @brief Registers external client destination for high-rate telemetry.
    /// @param[in] msg Telemetry destination parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTelemetryDest(const MsgSetTelemetryDestination& msg);

    // --- Lens Commands ---

    /// @brief Sends motorized lens focus or zoom command.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] commandType Command identifier (0: Stop, 1: Focus Far, etc).
    /// @param[in] rateOrPosition Rate or target coordinate.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendLensCommand(std::uint8_t cameraIndex, std::uint8_t commandType, std::int16_t rateOrPosition);

    /// @brief Configures auto-focus parameters and region of interest.
    /// @param[in] msg Focus parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setFocusParams(const MsgFocusParameters& msg);

    /// @brief Configures lens optical calibration parameters.
    /// @param[in] msg Lens calibration parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setLensParams(const MsgSetLensParameters& msg);

    /// @brief Controls GPIO pin direction and output states.
    /// @param[in] msg GPIO parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setGPIO(const MsgGPIO& msg);

    // --- Overlays & Reticles ---

    /// @brief Configures overlay graphics rendering mode.
    /// @param[in] msg Overlay mode parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setOverlayMode(const MsgSetOverlayMode& msg);

    /// @brief Draws a single dynamic graphics primitive on video overlay.
    /// @param[in] msg Draw object parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool drawObject(const MsgDrawObject& msg);

    /// @brief Draws multi-primitive overlay batch.
    /// @param[in] msg Draw overlay parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool drawOverlay(const MsgDrawOverlay& msg);

    // --- System & Maintenance ---

    /// @brief Saves active system parameters to persistent flash memory.
    /// @param[in] commitType Save option flags.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool saveParameters(std::uint8_t commitType = 0U);

    /// @brief Resets system parameters to factory defaults.
    /// @param[in] resetType Reset option flags.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool resetParameters(std::uint8_t resetType = 0U);

    /// @brief Configures IP network parameters.
    /// @param[in] msg Network parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setNetwork(const MsgSetNetworkParameters& msg);

    /// @brief Configures serial port baud rate and protocol mode.
    /// @param[in] msg Port parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setPortConfig(const MsgSetPortConfiguration& msg);

    // --- Query Commands ---

    /// @brief Requests hardware and software version information.
    /// @return True if query was transmitted.
    [[nodiscard]] bool queryVersion();

    /// @brief Enables or disables 1Hz periodic system health telemetry (0x80).
    /// @param[in] enable True to enable, false to disable.
    /// @return True if command was transmitted.
    [[nodiscard]] bool enableSystemStatus(bool enable = true);

    /// @brief Sends a generic parameter getter query.
    /// @param[in] queryId Setter Message ID to query.
    /// @return True if query was transmitted.
    [[nodiscard]] bool queryParameters(std::uint8_t queryId);

    // --- Subscriptions & Telemetry ---

    /// @brief Registers an observer callback for tracking coordinate telemetry.
    void setTrackingCallback(TrackingCallback cb);

    /// @brief Registers an observer callback for extended multi-track telemetry with AI classes.
    void setExtendedPositionsCallback(ExtendedPositionsCallback cb);

    /// @brief Registers an observer callback for diagnostic warnings.
    void setWarningCallback(WarningCallback cb);

    /// @brief Registers an observer callback for version replies.
    void setVersionCallback(VersionCallback cb);

    /// @brief Registers an observer callback for periodic system health status.
    void setSystemStatusCallback(SystemStatusCallback cb);

    /// @brief Registers an observer callback for raw frame traffic inspection.
    void setRawTrafficCallback(RawTrafficCallback cb);

    /// @brief Retrieves the latest cached tracking positions snapshot.
    [[nodiscard]] std::optional<MsgTrackingPositions> lastTrackingPositions() const;

    /// @brief Retrieves the latest cached version information.
    [[nodiscard]] std::optional<MsgVersionNumber> lastVersion() const;

    /// @brief Retrieves the latest cached system status snapshot.
    [[nodiscard]] std::optional<MsgSystemStatusMessage> lastSystemStatus() const;

private:
    void handleIncomingBytes(const std::vector<std::uint8_t>& data);
    void dispatchPacket(const std::vector<std::uint8_t>& packet);
    [[nodiscard]] bool sendPacket(const std::vector<std::uint8_t>& packet);

    std::shared_ptr<Transport::ITransport> m_transport;
    SightlineStreamAccumulator m_accumulator;

    std::atomic<bool> m_started { false };

    mutable std::mutex m_callbackMutex;
    TrackingCallback m_trackingCallback;
    ExtendedPositionsCallback m_extendedPositionsCallback;
    WarningCallback m_warningCallback;
    VersionCallback m_versionCallback;
    SystemStatusCallback m_systemStatusCallback;
    RawTrafficCallback m_rawTrafficCallback;

    mutable std::mutex m_cacheMutex;
    std::optional<MsgTrackingPositions> m_lastPositions;
    std::optional<MsgVersionNumber> m_lastVersion;
    std::optional<MsgSystemStatusMessage> m_lastStatus;
};

} // namespace Sightline
