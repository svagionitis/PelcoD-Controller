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
    using StabilizationCallback = std::function<void(const MsgSetStabilizationParameters&)>;
    using RegistrationCallback = std::function<void(const MsgSetRegistrationParameters&)>;
    using StabilizationBiasCallback = std::function<void(const MsgSetStabilizationBias&)>;
    using RawTrafficCallback = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;
    using OverlayModeCallback = std::function<void(const MsgSetOverlayMode&)>;
    using OverlayObjectsIdsCallback = std::function<void(const MsgCurrentOverlayObjectsIds&)>;
    using OverlayObjectParamsCallback = std::function<void(const MsgCurrentOverlayObjectParameters&)>;
    using LogoParametersCallback = std::function<void(const MsgLogoParameters&)>;
    using DetectionCallback = std::function<void(const MsgSetDetectionParameters&)>;
    using AdvDetectionCallback = std::function<void(const MsgAdvancedDetectionParameters&)>;
    using DetectionRoiCallback = std::function<void(const MsgDetectionROI&)>;
    using KlvMetricFiltersCallback = std::function<void(const MsgKlvMetricFilters&)>;
    using TrackingParamsCallback = std::function<void(const MsgSetTrackingParameters&)>;
    using CommandAckCallback = std::function<void(const MsgCommandAck&)>;
    using RecordingEventCallback = std::function<void(const MsgFileRecordingEvent&)>;
    using RecordingStatusV2Callback = std::function<void(const MsgCurrentRecordingStatusV2&)>;
    using DirListingReplyCallback = std::function<void(const MsgDirectoryListingReply&)>;
    using H264ParamsCallback = std::function<void(const MsgSetH264Parameters&)>;
    using EthernetDisplayCallback = std::function<void(const MsgSetEthernetDisplayParameters&)>;
    using EthernetVideoCallback = std::function<void(const MsgSetEthernetVideoParameters&)>;
    using NetworkParamsCallback = std::function<void(const MsgSetNetworkParameters&)>;
    using NetworkListCallback = std::function<void(const MsgCurrentNetworkList&)>;
    using SystemValueCallback = std::function<void(const MsgSystemValue&)>;
    using BlendParamsCallback = std::function<void(const MsgSetBlendParameters&)>;
    using CurrentBlendParamsCallback = std::function<void(const MsgCurrentBlendParameters&)>;
    using FourAlignPointsCallback = std::function<void(const MsgFourAlignPoints&)>;
    using BlendAlignCallback = std::function<void(const MsgBlendAlign&)>;
    using MultipleAlignmentCallback = std::function<void(const MsgSetMultipleAlignment&)>;

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

    /// @brief Initiates precision acquisition on past frame with MISB timestamp.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] col Center column coordinate in pixels.
    /// @param[in] row Center row coordinate in pixels.
    /// @param[in] width Target box width.
    /// @param[in] height Target box height.
    /// @param[in] framePtsUs Microsecond MISB Precision Time Stamp.
    /// @param[in] flags Acquisition flags (default 0x01 primary).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool startPrecisionTrack(
        std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
        std::uint16_t width, std::uint16_t height, std::uint64_t framePtsUs,
        std::uint8_t flags = 0x01U);

    /// @brief Sets forced coasting override mode for a target track.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Target track identifier.
    /// @param[in] mode Forced coasting mode (None, FreezeUpdates, FreezeSearch, FreezePropagation).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setForcedCoast(
        std::uint8_t cameraIndex, std::uint8_t trackId, ForcedCoastingMode mode);

    /// @brief Re-acquires target model at current centroid position.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Target track identifier.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool reinitTrack(std::uint8_t cameraIndex, std::uint8_t trackId);

    /// @brief Resizes target track box dynamically.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackId Target track identifier.
    /// @param[in] width New track box width.
    /// @param[in] height New track box height.
    /// @param[in] acqAssist Enable acquisition assist search optimization.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool resizeTrack(
        std::uint8_t cameraIndex, std::uint8_t trackId,
        std::uint16_t width, std::uint16_t height, bool acqAssist = false);

    /// @brief Modifies track using target cueing operation and NearVal radius.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] col Target column coordinate.
    /// @param[in] row Target row coordinate.
    /// @param[in] mode Algorithmic modify mode (EAN Appendix C).
    /// @param[in] trackId Optional target track identifier (default 0).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool cueTrackAt(
        std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
        ModifyMode mode, std::uint8_t trackId = 0U);

    /// @brief Applies fine sub-pixel nudge offsets to the primary track.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] deltaCol Pixel column offset to nudge.
    /// @param[in] deltaRow Pixel row offset to nudge.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool nudgeTracking(std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow);

    /// @brief Nudges track gate in rotated display coordinates.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] deltaCol Pixel column offset to nudge.
    /// @param[in] deltaRow Pixel row offset to nudge.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool nudgeDisplayTrack(
        std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow);

    /// @brief Configures coordinate telemetry update rate and reporting mode.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] framePeriod Rate divisor (1 = every frame, 2 = every 2nd frame).
    /// @param[in] reportingFlags Bitmask of telemetry inclusions.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setReportingMode(
        std::uint8_t cameraIndex, std::uint8_t framePeriod, std::uint8_t reportingFlags);

    /// @brief Configures algorithmic tracking parameters and modes (Message ID 0x0C).
    /// @param[in] msg Algorithmic tracking parameters and feature flags.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTrackingParameters(const MsgSetTrackingParameters& msg);

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

    /// @brief Configures advanced detection parameters (Message ID 0x76).
    /// @param[in] msg Advanced detection parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setAdvancedDetection(const MsgAdvancedDetectionParameters& msg);

    /// @brief Configures detection region of interest (Message ID 0x7C).
    /// @param[in] msg Detection ROI parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setDetectionROI(const MsgDetectionROI& msg);

    /// @brief Configures Video Moving Target Indication (Message ID 0x84).
    /// @param[in] msg VMTI parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVMTI(const MsgSetVMTI& msg);

    /// @brief Triggers automated high-res detection snapshot capture (Message ID 0xAB).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] detectionIndex Target detection index (0 or 1).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool triggerDetectionSnapshot(std::uint8_t cameraIndex, std::uint8_t detectionIndex = 0U);

    /// @brief Configures KLV spatial and metric dimension filters (Message ID 0xC1).
    /// @param[in] msg Metric filters struct (EAN Sec 4.4.4).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setKlvMetricFilters(const MsgKlvMetricFilters& msg);

    /// @brief Configures classifier settings and compute assignment (Message ID 0xA9).
    /// @param[in] msg Classifier configuration struct.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setClassifierConfig(const MsgClassifierConfig& msg);

    /// @brief Configures classifier compute resource assignment (NPU vs CPU, Sync vs Async).
    /// @param[in] useNpu True to force classification to NPU via NPU_CONTROL.
    /// @param[in] asyncInferencing True to run asynchronously via CLASSIFY_ASYNC.
    /// @return True if commands were successfully transmitted.
    [[nodiscard]] bool setComputeResources(bool useNpu, bool asyncInferencing);

    /// @brief Queries active detection parameters (Message ID 0x2E).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] detIdx Detection index (0 or 1).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryDetectionParams(std::uint8_t cameraIndex = 0U, std::uint8_t detIdx = 0U);

    /// @brief Queries advanced detection parameters (Message ID 0x28 query 0x76).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryAdvDetection(std::uint8_t cameraIndex = 0U);

    /// @brief Queries active detection ROI (Message ID 0x28 query 0x7C).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] roiIndex ROI slot index (0..3).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryDetectionROI(std::uint8_t cameraIndex = 0U, std::uint8_t roiIndex = 0U);

    /// @brief Queries active VMTI configuration (Message ID 0x28 query 0x84).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryVMTI(std::uint8_t cameraIndex = 0U);

    /// @brief Queries tracking box luminance statistics (Message ID 0x28 query 0x78).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] trackId Target track ID.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryTrackingPixelStats(std::uint8_t cameraIndex = 0U, std::uint8_t trackId = 0U);

    /// @brief Queries KLV metric dimension filters (Message ID 0x28 query 0xC1).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryKlvMetricFilters(std::uint8_t cameraIndex = 0U);

    /// @brief Queries classifier configuration (Message ID 0x28 query 0xA9).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool queryClassifierConfig(std::uint8_t cameraIndex = 0U);

    /// @brief Executes custom deep learning AI model detection.
    /// @param[in] msg AI detect parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool customAIDetect(const MsgCustomAIDetect& msg);

    // --- Stabilization Commands ---

    /// @brief Configures electronic image stabilization parameters.
    /// @param[in] msg Stabilization parameters struct.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilization(const MsgSetStabilizationParameters& msg);

    /// @brief Configures electronic image stabilization parameters.
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] mode Stabilization mode (0: Off, 1: On, 2: Auto).
    /// @param[in] rate Update rate / smoothing filter.
    /// @param[in] maxStabOff Maximum stabilization offset in pixels.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilization(
        std::uint8_t cameraIndex, std::uint8_t mode, std::uint8_t rate = 50U, std::uint8_t maxStabOff = 0U);

    /// @brief Resets stabilization smoothing filters (Message ID 0x04).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] resetType Filter reset mode (0: all, 1: display, 2: auto bias).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool resetStabilization(std::uint8_t cameraIndex, std::uint8_t resetType = 0U);

    /// @brief Sets stabilization motion bias offsets (Message ID 0x9F).
    /// @param[in] msg Stabilization bias structure.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilizationBias(const MsgSetStabilizationBias& msg);

    /// @brief Sets stabilization motion bias offsets (Message ID 0x9F).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] biasCol Column bias in pixels.
    /// @param[in] biasRow Row bias in pixels.
    /// @param[in] autoBias Enable automatic bias.
    /// @param[in] updateRate Auto bias update rate.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setStabilizationBias(std::uint8_t cameraIndex, std::int16_t biasCol, std::int16_t biasRow,
        std::uint8_t autoBias = 1U, std::uint8_t updateRate = 50U);

    /// @brief Configures frame registration parameters (Message ID 0x9E).
    /// @param[in] msg Registration parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setRegistration(const MsgSetRegistrationParameters& msg);

    /// @brief Queries active stabilization parameters (Message ID 0x03).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getStabilization(std::uint8_t cameraIndex = 0U);

    /// @brief Queries active registration parameters (Message ID 0x28 query 0x9E).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getRegistration(std::uint8_t cameraIndex = 0U);

    /// @brief Queries active stabilization bias (Message ID 0x28 query 0x9F).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getStabilizationBias(std::uint8_t cameraIndex = 0U);

    /// @brief Configures multi-sensor video fusion and blending.
    /// @param[in] msg Blend parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setBlend(const MsgSetBlendParameters& msg);

    /// @brief Queries active multi-sensor blend parameters (Message ID 0x30).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getBlendParameters();

    /// @brief Configures 4-point projective homography calibration (Message ID 0x95).
    /// @param[in] msg Four align points parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setFourAlignPoints(const MsgFourAlignPoints& msg);

    /// @brief Queries 4-point projective calibration (Message ID 0x28 query 0x95).
    /// @param[in] index Alignment slot index [0..4].
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getFourAlignPoints(std::uint8_t index = 0U);

    /// @brief Configures fine-tune alignment offsets and automated registration (Message ID 0xB9).
    /// @param[in] msg Blend align parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setBlendAlign(const MsgBlendAlign& msg);

    /// @brief Queries blend alignment parameters (Message ID 0x28 query 0xB9).
    /// @param[in] index Alignment slot index [0..4].
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getBlendAlign(std::uint8_t index = 0U);

    /// @brief Configures multi-camera multiple alignment (Message ID 0x74).
    /// @param[in] msg Multiple alignment parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMultipleAlignment(const MsgSetMultipleAlignment& msg);

    /// @brief Queries multiple alignment parameters (Message ID 0x28 query 0x74).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getMultipleAlignment();

    // --- Video & Display Pipeline ---

    /// @brief Configures video capture format.
    /// @param[in] msg Video parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoParams(const MsgSetVideoParameters& msg);

    /// @brief Configures digital camera high-bit-depth auto gain and dynamic range parameters (Message ID 0x70).
    /// @param[in] msg Digital camera AGC parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setDigitalCameraParams(const MsgDigitalCameraParameters& msg);

    /// @brief Queries digital camera parameters (Message ID 0x28 query 0x70).
    /// @param[in] cameraIndex Target camera index.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getDigitalCameraParams(std::uint8_t cameraIndex = 0U);

    /// @brief Configures video zoom, freeze, and orientation mode.
    /// @param[in] msg Video mode parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoMode(const MsgSetVideoMode& msg);

    /// @brief Configures image contrast, brightness, and CLAHE enhancement.
    /// @param[in] msg Enhancement parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoEnhance(const MsgSetVideoEnhancement& msg);

    /// @brief Configures full SLA video enhancement parameters (Message ID 0x21).
    /// @param[in] msg Full enhancement parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVideoEnhanceFull(const MsgSetVideoEnhancementFull& msg);

    /// @brief Configures false color thermal palette (Message ID 0x16).
    /// @param[in] cameraIndex Target camera index (0-based, or 255 for all).
    /// @param[in] palette Predefined false color palette mode.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setFalseColor(std::uint8_t cameraIndex, FalseColorPalette palette);

    /// @brief Ingests custom pseudo-color lookup table (LUT) for thermal sensors (Message ID 0x72).
    /// @param[in] msg User palette table.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setUserPalette(const MsgUserPalette& msg);

    /// @brief Ingests custom pseudo-color lookup table from isotherm builder (Message ID 0x72).
    /// @param[in] builder Configured isotherm LUT builder.
    /// @param[in] paletteIndex Palette slot index (0..3).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setIsothermPalette(
        const SightlineIsothermBuilder& builder, std::uint8_t paletteIndex = 0U);

    /// @brief Configures display output placement and scaling.
    /// @param[in] msg Display parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setDisplayParams(const MsgSetDisplayParameters& msg);

    /// @brief Configures network Ethernet video streaming destination.
    /// @param[in] msg Ethernet video parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setEthernetVideo(const MsgSetEthernetVideoParameters& msg);

    /// @brief Configures network Ethernet display stream destination and protocol.
    /// @param[in] msg Ethernet display parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setEthernetDisplay(const MsgSetEthernetDisplayParameters& msg);

    /// @brief Configures H.264 video compression parameters.
    /// @param[in] msg H.264 parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setH264Params(const MsgSetH264Parameters& msg);

    /// @brief Configures Linux Traffic Control (tc) bandwidth limiter (Message ID 0x92, Key 13).
    /// @param[in] rateKbps Maximum rate in kilobits per second.
    /// @param[in] burstBytes Burst in bytes (e.g. 3000).
    /// @param[in] mtuBytes MTU in bytes (e.g. 1500).
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTrafficControl(
        std::uint32_t rateKbps, std::uint32_t burstBytes = 3000U, std::uint32_t mtuBytes = 1500U);

    /// @brief Queries H.264 compression parameters for specified network display (Message ID 0x24).
    /// @param[in] displayId Network Display ID (Net0 = 0x0002, Net1 = 0x0080, Net2 = 0x0200).
    /// @return True if query was successfully transmitted.
    [[nodiscard]] bool getH264Params(std::uint16_t displayId = 0x0002U);

    /// @brief Queries active Ethernet display streaming parameters (Message ID 0x39).
    /// @param[in] displayId Network Display ID.
    /// @return True if query was successfully transmitted.
    [[nodiscard]] bool getEthernetDisplay(std::uint16_t displayId = 0x0002U);

    /// @brief Queries active Ethernet video parameters (Message ID 0x1B).
    /// @param[in] displayId Network Display ID.
    /// @return True if query was successfully transmitted.
    [[nodiscard]] bool getEthernetVideo(std::uint16_t displayId = 0x0002U);

    /// @brief Queries board network parameters for specified interface (Message ID 0x1D).
    /// @param[in] index Zero-based interface index.
    /// @return True if query was successfully transmitted.
    [[nodiscard]] bool getNetworkParams(std::uint8_t index = 0U);

    /// @brief Queries available network interfaces (Message ID 0x28 query 0x67).
    /// @return True if query was successfully transmitted.
    [[nodiscard]] bool getNetworkList();

    /// @brief Controls onboard SD card recording.
    /// @param[in] msg SD recording parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setSDRecording(const MsgSetSDRecordingParameters& msg);

    /// @brief Configures hardened video recording parameters with sequence tracking (0xC4).
    /// @param[in] msg Hardened recording parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setFileRecordingV2(const MsgSetFileRecordingParamsV2& msg);

    /// @brief Triggers hardened snapshot capture with explicit sensor routing (0xC5).
    /// @param[in] msg Hardened snapshot request.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool doSnapShotV2(const MsgDoSnapShotV2& msg);

    /// @brief Transmits a command acknowledgment packet (0xC3).
    /// @param[in] msg CommandAck structure.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendCmdAck(const MsgCommandAck& msg);

    /// @brief Transmits a file recording event notification (0xC6).
    /// @param[in] msg File recording event structure.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendRecordingEvent(const MsgFileRecordingEvent& msg);

    /// @brief Transmits comprehensive recording health telemetry (0xC7).
    /// @param[in] msg Recording health telemetry structure.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendRecordingStatusV2(const MsgCurrentRecordingStatusV2& msg);

    /// @brief Queries remote file catalog (Message ID 0xC8).
    /// @param[in] msg Directory query parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool getDirectoryListing(const MsgGetDirectoryListing& msg);

    /// @brief Transmits remote file catalog response (Message ID 0xC9).
    /// @param[in] msg Directory listing reply structure.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendDirListingReply(const MsgDirectoryListingReply& msg);

    /// @brief Transmits file storage management operation (Message ID 0xCA).
    /// @param[in] msg Storage operation parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool sendFileStorageMgmt(const MsgFileStorageManagement& msg);

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

    /// @brief Sets static mission and classification metadata.
    /// @param[in] msg Static metadata values.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadataStatic(const MsgMetadataStaticValues& msg);

    /// @brief Sets KLV frame values including terrain / OLS DTED mode.
    /// @param[in] msg Frame metadata values.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadataFrame(const MsgSetMetadataFrameValues& msg);

    /// @brief Injects user-constructed KLV blob into elementary stream.
    /// @param[in] msg KLV data blob.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setKlvData(const MsgSetKlvData& msg);

    /// @brief Configures metadata injection rate.
    /// @param[in] msg Metadata rate parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadataRate(const MsgSetMetadataRate& msg);

    /// @brief Configures metadata injection rate (convenience overload).
    /// @param[in] enables Bitmask of enabled telemetry packets.
    /// @param[in] frameStep Decimation rate (1 = full rate).
    /// @param[in] displayId Network display ID.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setMetadataRate(
        std::uint64_t enables, std::uint8_t frameStep, std::uint16_t displayId = 0x0002U);

    /// @brief Injects external target detections into VMTI Tag 74.
    /// @param[in] msg VMTI targets.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVmti(const MsgSetVmti& msg);

    /// @brief Injects appended user metadata (Tag 100).
    /// @param[in] msg Appended metadata.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setAppendedMetadata(const MsgAppendedMetadata& msg);

    /// @brief Configures custom MISB tag data value.
    /// @param[in] msg Tag data.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTagData(const MsgTagData& msg);

    /// @brief Configures custom MISB tag data rate.
    /// @param[in] msg Tag data rate.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTagDataRate(const MsgTagDataRate& msg);

    /// @brief Configures source selector for MISB tags.
    /// @param[in] msg Tag source selector.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setTagSourceSelector(const MsgTagSourceSelector& msg);

    /// @brief Injects dynamic ancillary text into KLV elementary stream.
    /// @param[in] msg Ancillary text message.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setAncillaryText(const MsgAncillaryTextMetadata& msg);

    /// @brief Configures VMTI target image chips.
    /// @param[in] msg VMTI chips parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVmtiChips(const MsgVmtiChips& msg);

    /// @brief Configures Cursor-on-Target XML tactical broadcast.
    /// @param[in] msg CoT parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setCursorOnTarget(const MsgCursorOnTarget& msg);

    /// @brief Configures VMTI fields and Ontology update rate.
    /// @param[in] msg VMTI fields parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setVmtiFields(const MsgVmtiFields& msg);

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

    /// @brief Configures overlay graphics rendering mode (Message ID 0x06).
    /// @param[in] msg Overlay mode parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool setOverlayMode(const MsgSetOverlayMode& msg);

    /// @brief Queries current overlay rendering mode (Message ID 0x07 / 0x28).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return True if query was transmitted.
    [[nodiscard]] bool getOverlayMode(std::uint8_t cameraIndex = 0U);

    /// @brief Draws a single dynamic graphics primitive on video overlay (Message ID 0x9C).
    /// @param[in] msg Draw overlay parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool drawOverlay(const MsgDrawOverlay& msg);

    /// @brief Draws a batch of overlay graphic primitives in sequential packets (Message ID 0x9C).
    /// @param[in] objects Vector of graphic overlay commands.
    /// @return True if packets were successfully transmitted.
    [[nodiscard]] bool drawOverlayBatch(const std::vector<MsgDrawOverlay>& objects);

    /// @brief Draws a cross reticle overlay on screen (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object identifier (1..199).
    /// @param[in] centerX Center X coordinate in pixels.
    /// @param[in] centerY Center Y coordinate in pixels.
    /// @param[in] size Arm length in pixels.
    /// @param[in] fgColor Foreground palette color.
    /// @param[in] thickness Line thickness in pixels.
    /// @param[in] originUpperLeft True if origin is upper-left, false if center.
    /// @return True if command was transmitted.
    [[nodiscard]] bool drawCross(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t centerX,
        std::int16_t centerY, std::uint16_t size, OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        std::uint16_t thickness = 1U, bool originUpperLeft = false);

    /// @brief Draws an outlined or solid filled rectangle on screen (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object identifier (1..199).
    /// @param[in] x Top-left X coordinate in pixels.
    /// @param[in] y Top-left Y coordinate in pixels.
    /// @param[in] width Rectangle width in pixels.
    /// @param[in] height Rectangle height in pixels.
    /// @param[in] filled True for solid filled rectangle, false for outline.
    /// @param[in] fgColor Foreground/border palette color.
    /// @param[in] bgColor Background/fill palette color.
    /// @param[in] alpha Opacity/transparency level (0 = opaque, 1..31 = translucent).
    /// @param[in] thickness Border line thickness in pixels.
    /// @param[in] originUpperLeft True if origin is upper-left.
    /// @return True if command was transmitted.
    [[nodiscard]] bool drawRectangle(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
        std::uint16_t width, std::uint16_t height, bool filled = false,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        OverlayPaletteColor bgColor = OverlayPaletteColor::TransparentBgOrTurquoiseFg, std::uint8_t alpha = 0U,
        std::uint16_t thickness = 1U, bool originUpperLeft = true);

    /// @brief Draws a static or dynamic text string on screen (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object identifier (1..199).
    /// @param[in] x Starting X coordinate in pixels.
    /// @param[in] y Starting Y coordinate in pixels.
    /// @param[in] text String content (up to 64 bytes).
    /// @param[in] fontId System or user font slot.
    /// @param[in] fgColor Text font color.
    /// @param[in] bgColor Text background/shadow color.
    /// @param[in] hScale Horizontal scale (32 = 100%).
    /// @param[in] vScale Vertical scale (32 = 100%).
    /// @param[in] originUpperLeft True if origin is upper-left.
    /// @return True if command was transmitted.
    [[nodiscard]] bool drawText(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
        const std::string& text, OverlayFontId fontId = OverlayFontId::Courier,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        OverlayPaletteColor bgColor = OverlayPaletteColor::TransparentBgOrTurquoiseFg, std::uint8_t hScale = 32U,
        std::uint8_t vScale = 32U, bool originUpperLeft = true);

    /// @brief Draws a dynamic KLV telemetry field badge (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object identifier (1..199).
    /// @param[in] x Starting X coordinate in pixels.
    /// @param[in] y Starting Y coordinate in pixels.
    /// @param[in] fieldTag Telemetry field tag to bind.
    /// @param[in] formatType Formatting style for the telemetry value.
    /// @param[in] formatString C-style template string (e.g. "%s" or "Slant: %f m").
    /// @param[in] fontId Font slot.
    /// @param[in] fgColor Text font color.
    /// @param[in] originUpperLeft True if origin is upper-left.
    /// @return True if command was transmitted.
    [[nodiscard]] bool drawKlvField(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
        KlvFieldTag fieldTag, KlvFormatType formatType, const std::string& formatString = "%s",
        OverlayFontId fontId = OverlayFontId::Courier, OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        bool originUpperLeft = true);

    /// @brief Draws an opaque blackout rectangle covering the video display (EAN Sec 10).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object identifier (1..199).
    /// @param[in] width Display width in pixels.
    /// @param[in] height Display height in pixels.
    /// @return True if command was transmitted.
    [[nodiscard]] bool drawBlackout(
        std::uint8_t cameraIndex, std::uint8_t objectId, std::uint16_t width = 640U, std::uint16_t height = 480U);

    /// @brief Destroys a single graphic overlay object by ID (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Object identifier to delete (1..199).
    /// @return True if command was transmitted.
    [[nodiscard]] bool destroyOverlay(std::uint8_t cameraIndex, std::uint8_t objectId);

    /// @brief Destroys all user graphic overlay objects on the target camera (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return True if command was transmitted.
    [[nodiscard]] bool destroyAllOverlays(std::uint8_t cameraIndex = 0U);

    /// @brief Draws a single dynamic graphics primitive on video overlay (legacy Message ID 0x3B).
    /// @param[in] msg Draw object parameters.
    /// @return True if command was successfully transmitted.
    [[nodiscard]] bool drawObject(const MsgDrawObject& msg);

    /// @brief Configures logo watermark display parameters (Message ID 0x9B).
    /// @param[in] msg Logo parameters struct.
    /// @return True if command was transmitted.
    [[nodiscard]] bool setLogoParameters(const MsgLogoParameters& msg);

    /// @brief Queries logo watermark display configuration (Message ID 0x28 query 0x9B).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return True if query was transmitted.
    [[nodiscard]] bool getLogoParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Assigns a TrueType font file path to a font slot (Message ID 0xAE).
    /// @param[in] msg User font parameters struct.
    /// @return True if command was transmitted.
    [[nodiscard]] bool setUserFont(const MsgUserFont& msg);

    /// @brief Assigns a TrueType font file path to a font slot (Message ID 0xAE).
    /// @param[in] slotIndex Font slot index (0..15).
    /// @param[in] fontFileName Path or filename of the TTF font on the device.
    /// @return True if command was transmitted.
    [[nodiscard]] bool setUserFont(std::uint8_t slotIndex, const std::string& fontFileName);

    /// @brief Queries list of all active user overlay objects (Message ID 0x28 query 0x68).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return True if query was transmitted.
    [[nodiscard]] bool getOverlayObjectsIds(std::uint8_t cameraIndex = 0U);

    /// @brief Queries parameters and geometry of a specific overlay object (Message ID 0x28 query 0x6B).
    /// @param[in] objectId Target object identifier (1..199).
    /// @return True if query was transmitted.
    [[nodiscard]] bool getOverlayObjectParams(std::uint8_t objectId);

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

    /// @brief Registers an observer callback for electronic stabilization telemetry.
    void setStabilizationCallback(StabilizationCallback cb);

    /// @brief Registers an observer callback for image registration telemetry.
    void setRegistrationCallback(RegistrationCallback cb);

    /// @brief Registers an observer callback for stabilization motion bias telemetry.
    void setStabilizationBiasCallback(StabilizationBiasCallback cb);

    /// @brief Registers an observer callback for raw frame traffic inspection.
    void setRawTrafficCallback(RawTrafficCallback cb);

    /// @brief Registers an observer callback for overlay mode telemetry.
    void setOverlayCallback(OverlayModeCallback cb);

    /// @brief Registers an observer callback for active overlay object IDs bitmasks.
    void setObjectsIdsCallback(OverlayObjectsIdsCallback cb);

    /// @brief Registers an observer callback for overlay object parameters queries.
    void setObjectParamsCallback(OverlayObjectParamsCallback cb);

    /// @brief Registers an observer callback for logo watermark parameters.
    void setLogoCallback(LogoParametersCallback cb);

    /// @brief Registers an observer callback for active detection parameters.
    void setDetectionCallback(DetectionCallback cb);

    /// @brief Registers an observer callback for advanced detection parameters.
    void setAdvDetectionCallback(AdvDetectionCallback cb);

    /// @brief Registers an observer callback for detection region of interest.
    void setDetectionRoiCallback(DetectionRoiCallback cb);

    /// @brief Registers an observer callback for KLV metric dimension filters.
    void setKlvMetricFiltersCb(KlvMetricFiltersCallback cb);

    /// @brief Registers an observer callback for algorithmic tracking parameters.
    void setTrackingParamsCallback(TrackingParamsCallback cb);

    /// @brief Registers an observer callback for command acknowledgment replies.
    void setCommandAckCallback(CommandAckCallback cb);

    /// @brief Retrieves the latest cached command acknowledgment reply.
    [[nodiscard]] std::optional<MsgCommandAck> lastCommandAck() const;

    /// @brief Registers an observer callback for file recording events (0xC6).
    void setRecordingEventCb(RecordingEventCallback cb);

    /// @brief Registers an observer callback for recording status telemetry (0xC7).
    void setRecordingStatusCb(RecordingStatusV2Callback cb);

    /// @brief Retrieves the latest cached file recording event.
    [[nodiscard]] std::optional<MsgFileRecordingEvent> lastRecordingEvent() const;

    /// @brief Retrieves the latest cached recording status telemetry.
    [[nodiscard]] std::optional<MsgCurrentRecordingStatusV2> lastRecordingStatus() const;

    /// @brief Registers an observer callback for remote directory catalog replies (0xC9).
    void setDirListingReplyCb(DirListingReplyCallback cb);

    /// @brief Retrieves the latest cached remote directory catalog reply.
    [[nodiscard]] std::optional<MsgDirectoryListingReply> lastDirListingReply() const;

    /// @brief Retrieves the latest cached tracking positions snapshot.
    [[nodiscard]] std::optional<MsgTrackingPositions> lastTrackingPositions() const;

    /// @brief Retrieves the latest cached algorithmic tracking parameters.
    [[nodiscard]] std::optional<MsgSetTrackingParameters> lastTrackingParameters() const;

    /// @brief Retrieves the latest cached version information.
    [[nodiscard]] std::optional<MsgVersionNumber> lastVersion() const;

    /// @brief Retrieves the latest cached system status snapshot.
    [[nodiscard]] std::optional<MsgSystemStatusMessage> lastSystemStatus() const;

    /// @brief Retrieves the latest cached stabilization parameters snapshot.
    [[nodiscard]] std::optional<MsgSetStabilizationParameters> lastStabilization() const;

    /// @brief Retrieves the latest cached registration parameters snapshot.
    [[nodiscard]] std::optional<MsgSetRegistrationParameters> lastRegistration() const;

    /// @brief Retrieves the latest cached stabilization bias snapshot.
    [[nodiscard]] std::optional<MsgSetStabilizationBias> lastStabilizationBias() const;

    /// @brief Retrieves the latest cached overlay mode configuration.
    [[nodiscard]] std::optional<MsgSetOverlayMode> lastOverlayMode() const;

    /// @brief Retrieves the latest cached active overlay object IDs bitmask.
    [[nodiscard]] std::optional<MsgCurrentOverlayObjectsIds> lastOverlayObjectsIds() const;

    /// @brief Retrieves the latest cached logo watermark parameters.
    [[nodiscard]] std::optional<MsgLogoParameters> lastLogoParameters() const;

    /// @brief Retrieves the latest cached detection parameters snapshot.
    [[nodiscard]] std::optional<MsgSetDetectionParameters> lastDetectionParams() const;

    /// @brief Retrieves the latest cached advanced detection parameters.
    [[nodiscard]] std::optional<MsgAdvancedDetectionParameters> lastAdvDetection() const;

    /// @brief Retrieves the latest cached detection ROI parameters.
    [[nodiscard]] std::optional<MsgDetectionROI> lastDetectionROI() const;

    /// @brief Retrieves the latest cached KLV metric filters.
    [[nodiscard]] std::optional<MsgKlvMetricFilters> lastKlvMetricFilters() const;

    /// @brief Registers an observer callback for H.264 encoder parameters (0x56).
    void setH264ParamsCallback(H264ParamsCallback cb);

    /// @brief Registers an observer callback for Ethernet display stream destination (0x52).
    void setEthernetDisplayCb(EthernetDisplayCallback cb);

    /// @brief Registers an observer callback for Ethernet video parameters (0x48).
    void setEthernetVideoCb(EthernetVideoCallback cb);

    /// @brief Registers an observer callback for network parameters (0x49).
    void setNetworkParamsCb(NetworkParamsCallback cb);

    /// @brief Registers an observer callback for network list (0x67).
    void setNetworkListCb(NetworkListCallback cb);

    /// @brief Registers an observer callback for system register values (0x93).
    void setSystemValueCallback(SystemValueCallback cb);

    /// @brief Registers an observer callback for blend parameters (0x2F / 0x4D).
    void setBlendParamsCb(BlendParamsCallback cb);

    /// @brief Registers an observer callback for full current blend parameters (0x4D).
    void setCurrentBlendParamsCb(CurrentBlendParamsCallback cb);

    /// @brief Registers an observer callback for 4-point alignment homography (0x95).
    void setFourAlignPointsCb(FourAlignPointsCallback cb);

    /// @brief Registers an observer callback for blend alignment parameters (0xB9).
    void setBlendAlignCb(BlendAlignCallback cb);

    /// @brief Registers an observer callback for multiple alignment parameters (0x74 / 0x75).
    void setMultipleAlignmentCb(MultipleAlignmentCallback cb);

    /// @brief Retrieves the latest cached blend parameters snapshot.
    [[nodiscard]] std::optional<MsgSetBlendParameters> lastBlendParams() const;

    /// @brief Retrieves the latest cached current blend parameters snapshot (19 bytes).
    [[nodiscard]] std::optional<MsgCurrentBlendParameters> lastCurrentBlendParams() const;

    /// @brief Retrieves the latest cached 4-point projective calibration snapshot.
    [[nodiscard]] std::optional<MsgFourAlignPoints> lastFourAlignPoints() const;

    /// @brief Retrieves the latest cached blend alignment snapshot.
    [[nodiscard]] std::optional<MsgBlendAlign> lastBlendAlign() const;

    /// @brief Retrieves the latest cached multiple alignment snapshot.
    [[nodiscard]] std::optional<MsgSetMultipleAlignment> lastMultipleAlignment() const;

    /// @brief Retrieves the latest cached H.264 encoder parameters snapshot.
    [[nodiscard]] std::optional<MsgSetH264Parameters> lastH264Params() const;

    /// @brief Retrieves the latest cached Ethernet display streaming destination snapshot.
    [[nodiscard]] std::optional<MsgSetEthernetDisplayParameters> lastEthernetDisplay() const;

    /// @brief Retrieves the latest cached Ethernet video quality and sizing snapshot.
    [[nodiscard]] std::optional<MsgSetEthernetVideoParameters> lastEthernetVideo() const;

    /// @brief Retrieves the latest cached network settings snapshot.
    [[nodiscard]] std::optional<MsgSetNetworkParameters> lastNetworkParams() const;

    /// @brief Retrieves the latest cached network interfaces list.
    [[nodiscard]] std::optional<MsgCurrentNetworkList> lastNetworkList() const;

    /// @brief Retrieves the latest cached system value.
    [[nodiscard]] std::optional<MsgSystemValue> lastSystemValue() const;

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
    OverlayModeCallback m_overlayModeCallback;
    OverlayObjectsIdsCallback m_objectsIdsCallback;
    OverlayObjectParamsCallback m_objectParamsCallback;
    LogoParametersCallback m_logoCallback;
    DetectionCallback m_detectionCallback;
    AdvDetectionCallback m_advDetectionCallback;
    DetectionRoiCallback m_detectionRoiCallback;
    KlvMetricFiltersCallback m_klvMetricFiltersCallback;
    TrackingParamsCallback m_trackingParamsCallback;
    CommandAckCallback m_commandAckCallback;
    RecordingEventCallback m_recordingEventCb;
    RecordingStatusV2Callback m_recordingStatusCb;
    DirListingReplyCallback m_dirListingReplyCb;

    mutable std::mutex m_cacheMutex;
    std::optional<MsgTrackingPositions> m_lastPositions;
    std::optional<MsgSetTrackingParameters> m_lastTrackingParams;
    std::optional<MsgVersionNumber> m_lastVersion;
    std::optional<MsgSystemStatusMessage> m_lastStatus;
    std::optional<MsgSetStabilizationParameters> m_lastStabilization;
    std::optional<MsgSetRegistrationParameters> m_lastRegistration;
    std::optional<MsgSetStabilizationBias> m_lastStabilizationBias;
    std::optional<MsgSetOverlayMode> m_lastOverlayMode;
    std::optional<MsgCurrentOverlayObjectsIds> m_lastOverlayObjectsIds;
    std::optional<MsgLogoParameters> m_lastLogoParameters;
    std::optional<MsgSetDetectionParameters> m_lastDetectionParams;
    std::optional<MsgAdvancedDetectionParameters> m_lastAdvDetection;
    std::optional<MsgDetectionROI> m_lastDetectionROI;
    std::optional<MsgKlvMetricFilters> m_lastKlvMetricFilters;
    std::optional<MsgCommandAck> m_lastCommandAck;
    std::optional<MsgFileRecordingEvent> m_lastRecordingEvent;
    std::optional<MsgCurrentRecordingStatusV2> m_lastRecordingStatus;
    std::optional<MsgDirectoryListingReply> m_lastDirListingReply;
    StabilizationCallback m_stabilizationCallback;
    RegistrationCallback m_registrationCallback;
    StabilizationBiasCallback m_stabilizationBiasCallback;
    H264ParamsCallback m_h264ParamsCallback;
    EthernetDisplayCallback m_ethDisplayCallback;
    EthernetVideoCallback m_ethVideoCallback;
    NetworkParamsCallback m_netParamsCallback;
    NetworkListCallback m_netListCallback;
    SystemValueCallback m_systemValueCallback;
    BlendParamsCallback m_blendParamsCb;
    CurrentBlendParamsCallback m_currentBlendParamsCb;
    FourAlignPointsCallback m_fourAlignPointsCb;
    BlendAlignCallback m_blendAlignCb;
    MultipleAlignmentCallback m_multipleAlignmentCb;

    std::optional<MsgSetH264Parameters> m_lastH264Params;
    std::optional<MsgSetEthernetDisplayParameters> m_lastEthernetDisplay;
    std::optional<MsgSetEthernetVideoParameters> m_lastEthernetVideo;
    std::optional<MsgSetNetworkParameters> m_lastNetworkParams;
    std::optional<MsgCurrentNetworkList> m_lastNetworkList;
    std::optional<MsgSystemValue> m_lastSystemValue;
    std::optional<MsgSetBlendParameters> m_lastBlendParams;
    std::optional<MsgCurrentBlendParameters> m_lastCurrentBlendParams;
    std::optional<MsgFourAlignPoints> m_lastFourAlignPoints;
    std::optional<MsgBlendAlign> m_lastBlendAlign;
    std::optional<MsgSetMultipleAlignment> m_lastMultipleAlignment;
};

} // namespace Sightline
