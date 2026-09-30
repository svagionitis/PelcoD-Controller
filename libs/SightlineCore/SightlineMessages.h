#pragma once

/// @file SightlineMessages.h
/// @brief Strongly-typed Plain Old Data (POD) message structures for Sightline SLA protocol v3.11.6.

#include "SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

// ==============================================================================
// 1. System, Configuration & Diagnostic Messages
// ==============================================================================

/// @struct MsgGetParameters
/// @brief Unified generic getter request (Message ID 0x28).
struct MsgGetParameters {
    std::uint8_t queryId { 0U };
};

/// @struct MsgResetAllParameters
/// @brief System reset to factory defaults (Message ID 0x01).
struct MsgResetAllParameters {
    std::uint8_t resetType { 0U }; // 0: Reset all, 1: Keep network, 2: Save to flash
};

/// @struct MsgSaveParameters
/// @brief Commit active parameters to flash memory (Message ID 0x25).
struct MsgSaveParameters {
    std::uint8_t commitType { 0U };
};

/// @struct MsgVersionNumber
/// @brief System hardware, firmware, and capability flags reply (Message ID 0x40 / 0x00).
struct MsgVersionNumber {
    std::uint8_t hardwareType { 0U };
    std::uint8_t softwareMajor { 0U };
    std::uint8_t softwareMinor { 0U };
    std::uint8_t softwarePatch { 0U };
    std::uint32_t appBits { 0U };
    std::uint32_t boardRevision { 0U };
    std::string versionString {};
};

/// @struct MsgUserWarningMessage
/// @brief Unsolicited diagnostic notifications and error warnings (Message ID 0x86).
struct MsgUserWarningMessage {
    std::uint16_t warningCode { 0U };
    std::string message {};
};

/// @struct MsgSystemStatusMessage
/// @brief Periodic system health, CPU load, and temperature message (Message ID 0x87).
struct MsgSystemStatusMessage {
    std::uint16_t cpuLoadPercent { 0U };
    std::int16_t coreTempC { 0 };
    std::uint32_t uptimeSeconds { 0U };
    std::uint32_t errorFlags { 0U };
};

/// @struct MsgCurrentConfiguration
/// @brief Current hardware configuration, camera count, and display paths (Message ID 0x8E).
struct MsgCurrentConfiguration {
    std::uint8_t numVideoInputs { 1U };
    std::uint8_t numVideoOutputs { 1U };
    std::uint8_t numDisplays { 1U };
    std::uint8_t hardwareType { 0U };
};

/// @struct MsgSetNetworkParameters
/// @brief IP address, subnet mask, gateway, and DHCP configuration (Message ID 0x1C).
struct MsgSetNetworkParameters {
    std::uint32_t ipAddress { 0U };
    std::uint32_t subnetMask { 0U };
    std::uint32_t gateway { 0U };
    std::uint8_t dhcpEnable { 1U };
    std::uint16_t commandPort { DefaultHardwareCommandPort };
    std::uint16_t replyPort { DefaultClientReplyPort };
};

/// @struct MsgSetPortConfiguration
/// @brief Serial port mode and baud rate configuration (Message ID 0x3E).
struct MsgSetPortConfiguration {
    std::uint8_t portIndex { 0U };
    std::uint32_t baudRate { 57600U };
    std::uint8_t mode { 0U }; // 0: Command & Control, 1: Pass-through, 2: NMEA, etc.
};

/// @struct MsgCommandPassThrough
/// @brief Transparent serial/network passthrough tunnel (Message ID 0x3D).
struct MsgCommandPassThrough {
    std::uint8_t destPort { 0U };
    std::vector<std::uint8_t> data {};
};

// ==============================================================================
// 2. Video Tracking, Acquisition & Motion Analysis Messages
// ==============================================================================

/// @struct MsgStartTracking
/// @brief Primary or secondary target acquisition initiation (Message ID 0x08).
struct MsgStartTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t centerCol { 320U };
    std::uint16_t centerRow { 240U };
    std::uint16_t width { 64U };
    std::uint16_t height { 64U };
    std::uint8_t flags { 0U }; // 0x01: Primary, 0x02: Secondary, 0x04: Auto-size
};

/// @struct MsgStopTracking
/// @brief Terminate target tracking (Message ID 0x09).
struct MsgStopTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0xFFU }; // 0xFF: Stop all tracks
};

/// @struct MsgModifyTracking
/// @brief Adjust active tracking parameters or mode (Message ID 0x05).
struct MsgModifyTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t mode { 0U };
    std::uint8_t flags { 0U };
};

/// @struct MsgNudgeTrackingCoordinate
/// @brief Sub-pixel manual trim adjustment for primary target track (Message ID 0x0A).
struct MsgNudgeTrackingCoordinate {
    std::uint8_t cameraIndex { 0U };
    std::int16_t deltaCol { 0 };
    std::int16_t deltaRow { 0 };
};

/// @struct MsgCoordinateReportingMode
/// @brief Configure rate and telemetry contents for tracking coordinates (Message ID 0x0B).
struct MsgCoordinateReportingMode {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t framePeriod { 1U }; // 1 = every frame (30/60 Hz), 2 = every 2nd frame
    std::uint8_t reportingFlags { 0x03U }; // Primary + All
};

/// @struct MsgSetTrackingParameters
/// @brief Algorithmic parameters for search window, motion models, and gating (Message ID 0x0C).
struct MsgSetTrackingParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U };
    std::uint16_t acquisitionSearchCol { 128U };
    std::uint16_t acquisitionSearchRow { 96U };
    std::uint8_t flags { 0U };
};

/// @struct MsgDesignateSelectedTrackPrimary
/// @brief Elevates an existing secondary track to primary status (Message ID 0x32).
struct MsgDesignateSelectedTrackPrimary {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
};

/// @struct MsgShiftSelectedTrack
/// @brief Shifts track gate position relative to target centroid (Message ID 0x33).
struct MsgShiftSelectedTrack {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::int16_t shiftCol { 0 };
    std::int16_t shiftRow { 0 };
};

/// @struct MsgStopSelectedTrack
/// @brief Stops a single identified track (Message ID 0x3C).
struct MsgStopSelectedTrack {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
};

/// @struct MsgTrackingPosition
/// @brief Single primary track position and scene motion telemetry (Message ID 0x43).
struct MsgTrackingPosition {
    std::uint8_t cameraIndex { 0U };
    double col { 0.0 };
    double row { 0.0 };
    double translationCol { 0.0 };
    double translationRow { 0.0 };
    double rotationDeg { 0.0 };
    double scale { 1.0 };
    std::uint8_t confidence { 0U };
    std::uint8_t trackFlags { 0U };
};

/// @struct MsgTrackingPositions
/// @brief High-rate target position and velocity telemetry for all tracks (Message ID 0x51).
struct MsgTrackingPositions {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
};

/// @struct MsgTrackingPositionsExtended
/// @brief Multi-track report with deep learning classifier classifications (Message ID 0xA0).
struct MsgTrackingPositionsExtended {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
    std::vector<std::uint8_t> classIds {};
};

/// @struct MsgTrackTrails
/// @brief Historic positions trail for rendering target movement path (Message ID 0x9D).
struct MsgTrackTrails {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::vector<std::pair<double, double>> historyPoints {};
};

/// @struct MsgSetDetectionParameters
/// @brief Moving Target Indication (MTI) sensitivity and thresholds (Message ID 0x2D).
struct MsgSetDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U }; // 0: Off, 1: MTI, 2: Vehicle, 3: Person, 4: Maritime
    std::uint8_t threshold { 20U };
    std::uint16_t minTargetSize { 4U };
    std::uint16_t maxTargetSize { 200U };
};

/// @struct MsgCustomAIDetect
/// @brief AI inference engine configuration and model execution (Message ID 0xBA).
struct MsgCustomAIDetect {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t modelId { 0U };
    std::uint8_t confidenceThreshold { 50U };
    std::uint8_t nmsThreshold { 45U };
};

// ==============================================================================
// 3. Video Stabilization, Alignment & Enhancement Messages
// ==============================================================================

/// @struct MsgSetStabilizationParameters
/// @brief Electronic video image stabilization control (Message ID 0x02).
struct MsgSetStabilizationParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 1U }; // 0: Off, 1: On, 2: Auto
    std::uint8_t autoBias { 1U };
    std::uint8_t maxShift { 64U };
    std::uint8_t flags { 0U };
};

/// @struct MsgResetStabilizationParameters
/// @brief Reset internal stabilization motion smoothing filters (Message ID 0x04).
struct MsgResetStabilizationParameters {
    std::uint8_t cameraIndex { 0U };
};

/// @struct MsgSetStabilizationBias
/// @brief Motion bias correction values for stabilization (Message ID 0x12).
struct MsgSetStabilizationBias {
    std::uint8_t cameraIndex { 0U };
    std::int16_t biasCol { 0 };
    std::int16_t biasRow { 0 };
    std::int16_t biasRotation { 0 };
};

/// @struct MsgSetRegistrationParameters
/// @brief Frame-to-frame image registration parameters (Message ID 0x0E).
struct MsgSetRegistrationParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t searchRange { 32U };
    std::uint8_t pyramidLevels { 3U };
    std::uint8_t flags { 0U };
};

/// @struct MsgSetBlendParameters
/// @brief Dual-camera blending and alignment (EO + IR fusion) (Message ID 0x2F).
struct MsgSetBlendParameters {
    std::uint8_t primaryCamera { 0U };
    std::uint8_t secondaryCamera { 1U };
    std::uint8_t blendMode { 0U }; // 0: Alpha, 1: Picture-in-Picture, 2: False Color
    std::uint8_t alphaPercent { 50U };
};

/// @struct MsgNoise3D
/// @brief 3D Spatio-temporal noise reduction filter settings (Message ID 0xAF).
struct MsgNoise3D {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t enable { 1U };
    std::uint8_t temporalStrength { 50U };
    std::uint8_t spatialStrength { 30U };
};

// ==============================================================================
// 4. Video Pipeline, Display & Streaming Messages
// ==============================================================================

/// @struct MsgSetVideoParameters
/// @brief Video input standard and capture configuration (Message ID 0x10).
struct MsgSetVideoParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t inputFormat { 0U }; // 0: Auto, 1: 1080p, 2: 720p, 3: NTSC, 4: PAL
    std::uint16_t width { 1920U };
    std::uint16_t height { 1080U };
    std::uint8_t frameRate { 30U };
};

/// @struct MsgSetVideoMode
/// @brief Controls video freeze, zoom, and orientation (Message ID 0x1F).
struct MsgSetVideoMode {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t freeze { 0U };
    std::uint8_t digitalZoom { 100U }; // 100 = 1.0x, 200 = 2.0x
    std::uint8_t mirror { 0U };
    std::uint8_t flip { 0U };
};

/// @struct MsgSetVideoEnhancement
/// @brief Contrast, brightness, sharpening, and CLAHE (Message ID 0x21).
struct MsgSetVideoEnhancement {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t contrast { 50U };
    std::uint8_t brightness { 50U };
    std::uint8_t sharpening { 0U };
    std::uint8_t claheEnable { 0U };
};

/// @struct MsgSetDisplayParameters
/// @brief Video output display scaling and rendering options (Message ID 0x16).
struct MsgSetDisplayParameters {
    std::uint8_t displayIndex { 0U };
    std::uint8_t cameraIndex { 0U };
    std::uint16_t xOffset { 0U };
    std::uint16_t yOffset { 0U };
    std::uint16_t displayWidth { 1920U };
    std::uint16_t displayHeight { 1080U };
};

/// @struct MsgSetEthernetVideoParameters
/// @brief Network video streaming protocol, destination, and payload (Message ID 0x1A).
struct MsgSetEthernetVideoParameters {
    std::uint8_t streamIndex { 0U };
    std::uint32_t destIpAddress { 0U };
    std::uint16_t destPort { 15004U }; // Default MPEG2-TS port
    std::uint8_t protocol { 0U }; // 0: MPEG2-TS UDP, 1: RTP H.264, 2: RTSP
    std::uint16_t ttl { 64U };
};

/// @struct MsgSetH264Parameters
/// @brief Encoder bitrate, GOP structure, and profile settings (Message ID 0x23).
struct MsgSetH264Parameters {
    std::uint8_t streamIndex { 0U };
    std::uint32_t targetBitrateBps { 4000000U }; // 4 Mbps
    std::uint16_t gopLength { 30U };
    std::uint8_t qualityLevel { 1U };
    std::uint8_t rateControl { 0U }; // 0: CBR, 1: VBR
};

/// @struct MsgSetSDRecordingParameters
/// @brief Onboard SD card video recording control (Message ID 0x1E).
struct MsgSetSDRecordingParameters {
    std::uint8_t recordingState { 0U }; // 0: Stop, 1: Start, 2: Snapshot
    std::uint8_t cameraIndex { 0U };
    std::string filenamePrefix {};
};

/// @struct MsgStreamingControl
/// @brief Start / pause / stop network video stream pipelines (Message ID 0x90).
struct MsgStreamingControl {
    std::uint8_t streamIndex { 0U };
    std::uint8_t action { 1U }; // 0: Stop, 1: Start, 2: Pause
};

// ==============================================================================
// 5. Metadata, KLV & Telemetry Messages (MISB / STANAG 4609)
// ==============================================================================

/// @struct MsgSetMetadataValues
/// @brief Dynamic platform and target telemetry for KLV insertion (Message ID 0x13).
struct MsgSetMetadataValues {
    double platformLatitudeDeg { 0.0 };
    double platformLongitudeDeg { 0.0 };
    double platformAltitudeMeters { 0.0 };
    double platformHeadingDeg { 0.0 };
    double platformPitchDeg { 0.0 };
    double platformRollDeg { 0.0 };
    double sensorHorizontalFovDeg { 0.0 };
    double sensorVerticalFovDeg { 0.0 };
};

/// @struct MsgMetadataStaticValues
/// @brief Static mission identifiers and classification markings (Message ID 0x14).
struct MsgMetadataStaticValues {
    std::string missionId {};
    std::string platformTailNumber {};
    std::string securityClassification { "UNCLASSIFIED" };
};

/// @struct MsgSetMetadataRate
/// @brief Telemetry output rate and MISB packet frequency (Message ID 0x62).
struct MsgSetMetadataRate {
    std::uint8_t metadataType { 0U }; // 0: Synchronous KLV, 1: Asynchronous
    std::uint8_t ratePeriod { 1U }; // Rate divisor
};

/// @struct MsgSetTelemetryDestination
/// @brief Register external client IP/port for dedicated telemetry stream (Message ID 0x64).
struct MsgSetTelemetryDestination {
    std::uint8_t clientIndex { 0U }; // 0 to 3 (up to 4 clients)
    std::uint32_t clientIpAddress { 0U };
    std::uint16_t clientPort { 14002U };
    std::uint8_t flags { 0x01U }; // 0x01: Enable telemetry
};

/// @struct MsgCursorOnTarget
/// @brief Cursor-on-Target (CoT) XML tactical message broadcast (Message ID 0xB0).
struct MsgCursorOnTarget {
    std::uint8_t enable { 1U };
    std::uint16_t broadcastPort { 1870U };
    std::string uid {};
    std::string cotType { "a-f-G-E-V-C" }; // Military vehicle symbol
};

// ==============================================================================
// 6. Optics, Lens & Hardware Messages
// ==============================================================================

/// @struct MsgLensCommand
/// @brief Motorized lens positioning and focus commands (Message ID 0xB2).
struct MsgLensCommand {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t commandType { 0U }; // 0: Stop, 1: Focus Far, 2: Focus Near, 3: Zoom In, 4: Zoom Out
    std::int16_t rateOrPosition { 0 };
};

/// @struct MsgFocusParameters
/// @brief Auto-focus and region-of-interest tuning (Message ID 0xB3).
struct MsgFocusParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t focusMode { 0U };
    std::uint16_t roiX { 0U };
    std::uint16_t roiY { 0U };
    std::uint16_t roiWidth { 0U };
    std::uint16_t roiHeight { 0U };
};

/// @struct MsgSetLensParameters
/// @brief Calibrated focal length and optical distortion parameters (Message ID 0x6E / 0xB1).
struct MsgSetLensParameters {
    std::uint8_t cameraIndex { 0U };
    double minFocalLengthMm { 4.3 };
    double maxFocalLengthMm { 129.0 };
    double horizontalFovWideDeg { 65.0 };
    double horizontalFovTeleDeg { 2.3 };
};

/// @struct MsgGPIO
/// @brief General Purpose I/O pin control and state queries (Message ID 0xB6).
struct MsgGPIO {
    std::uint8_t pinMask { 0xFFU };
    std::uint8_t pinValues { 0x00U };
    std::uint8_t directionMask { 0xFFU }; // 1 = Output, 0 = Input
};

// ==============================================================================
// 7. Reticles, Overlays & Graphics Messages
// ==============================================================================

/// @struct MsgSetOverlayMode
/// @brief Configures overlay graphics rendering options (Message ID 0x06).
struct MsgSetOverlayMode {
    std::uint8_t displayIndex { 0U };
    std::uint8_t reticleMode { 1U }; // 0: None, 1: Crosshair, 2: Box, 3: Custom
    std::uint8_t trackingBoxMode { 1U }; // 0: Off, 1: Bounding Box, 2: Centroid
    std::uint8_t telemetryTextMode { 1U };
};

/// @struct MsgDrawObject
/// @brief Dynamic custom graphics object rendering on video overlay (Message ID 0x3B).
struct MsgDrawObject {
    std::uint8_t displayIndex { 0U };
    std::uint8_t objectId { 0U };
    std::uint8_t shapeType { 0U }; // 0: Line, 1: Rectangle, 2: Circle, 3: Text
    std::uint16_t x { 0U };
    std::uint16_t y { 0U };
    std::uint16_t width { 0U };
    std::uint16_t height { 0U };
    std::uint32_t colorRgba { 0x00FF00FFU }; // Green opaque default
    std::string text {};
};

/// @struct MsgDrawOverlay
/// @brief Advanced multi-primitive overlay graphics update (Message ID 0x9C).
struct MsgDrawOverlay {
    std::uint8_t displayIndex { 0U };
    std::uint8_t clearDisplay { 0U };
    std::vector<MsgDrawObject> objects {};
};

} // namespace Sightline
