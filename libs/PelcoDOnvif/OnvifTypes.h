#pragma once

/// @file OnvifTypes.h
/// @brief Common data structures, enums, and models for ONVIF Profile S client.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @struct DiscoveredDevice
/// @brief Device metadata discovered via WS-Discovery multicast probe.
struct DiscoveredDevice {
    std::string endpoint; ///< Device service URL (XAddr), e.g. http://192.168.1.100/onvif/device_service
    std::string ip; ///< IPv4 address extracted from endpoint or probe
    std::string hardware; ///< Hardware model name
    std::string name; ///< Device name
    std::string location; ///< Physical location from scope
    std::vector<std::string> scopes; ///< All discovered ONVIF scope URIs
};

/// @struct DeviceInformation
/// @brief Hardware identification metadata queried via GetDeviceInformation.
struct DeviceInformation {
    std::string manufacturer; ///< Manufacturer name
    std::string model; ///< Camera model
    std::string firmwareVersion; ///< Firmware revision string
    std::string serialNumber; ///< Factory serial number
    std::string hardwareId; ///< Hardware board identifier
};

/// @struct MediaProfile
/// @brief Media profile containing video encoder configuration.
struct MediaProfile {
    std::string token; ///< Unique profile identifier token, e.g. "Profile_1"
    std::string name; ///< User-friendly profile label, e.g. "MainStream"
    int videoWidth { 0 }; ///< Native video resolution width in pixels
    int videoHeight { 0 }; ///< Native video resolution height in pixels
    std::string videoEncoding; ///< Encoding format (e.g. "H264", "H265", "JPEG")
    std::string videoSourceToken {}; ///< Video source configuration token for imaging service
};

/// @struct StreamUriInfo
/// @brief RTSP streaming URI and session properties queried via GetStreamUri.
struct StreamUriInfo {
    std::string uri; ///< Full RTSP stream URL, e.g. rtsp://192.168.1.100:554/live/ch0
    bool invalidAfterConnect { false };
    bool invalidAfterReboot { false };
    std::string timeout; ///< Stream keep-alive timeout
};

/// @struct PtzStatus
/// @brief Current kinematics state of the pan/tilt/zoom head.
struct PtzStatus {
    double pan { 0.0 }; ///< Normalized pan position [-1.0 left to +1.0 right]
    double tilt { 0.0 }; ///< Normalized tilt position [-1.0 down to +1.0 up]
    double zoom { 0.0 }; ///< Normalized zoom position [0.0 wide to 1.0 tele]
    bool isMoving { false }; ///< True if motorized drive is actively moving
    std::string utcTime; ///< Camera UTC timestamp
};

/// @struct PtzPreset
/// @brief Stored pan/tilt/zoom preset position on the camera.
struct PtzPreset {
    std::string token {}; ///< Unique preset identifier token, e.g. "1" or "Preset_1"
    std::string name {}; ///< User-friendly preset label, e.g. "Front Gate"
    double pan { 0.0 }; ///< Normalized pan position [-1.0 to +1.0] if provided
    double tilt { 0.0 }; ///< Normalized tilt position [-1.0 to +1.0] if provided
    double zoom { 0.0 }; ///< Normalized zoom position [0.0 to 1.0] if provided
};

/// @enum FocusMoveMode
/// @brief Movement control mode for focus operations.
enum class FocusMoveMode : std::uint8_t { Continuous, Absolute, Relative };

/// @struct FocusMove
/// @brief Directional or positional focus request parameters.
struct FocusMove {
    FocusMoveMode mode { FocusMoveMode::Continuous }; ///< Continuous, Absolute, or Relative
    float continuousSpeed { 0.0f }; ///< Speed ratio [-1.0 to 1.0] (negative = near, positive = far)
    float absolutePosition { 0.0f }; ///< Target position [0.0 to 1.0]
    float relativeDistance { 0.0f }; ///< Relative displacement [-1.0 to 1.0]
    float relativeSpeed { 1.0f }; ///< Relative speed ratio [0.0 to 1.0]
};

/// @struct FocusStatus20
/// @brief Optical focus status and encoder position (Profile T).
struct FocusStatus20 {
    float position { 0.0f }; ///< Current optical focus position [0.0 to 1.0]
    std::string moveStatus { "IDLE" }; ///< Status: "IDLE", "MOVING", "UNKNOWN"
    std::string error {}; ///< Error detail if any
};

/// @struct ExposureSettings
/// @brief Exposure and iris control parameters.
struct ExposureSettings {
    std::string mode { "AUTO" }; ///< Exposure mode: "AUTO", "MANUAL"
    std::string priority { "LowNoise" }; ///< Priority: "LowNoise", "FrameRate"
    float exposureTime { 10000.0f }; ///< Exposure time in microseconds
    float gain { 0.0f }; ///< Sensor analog/digital gain in dB
    float iris { 50.0f }; ///< Iris aperture [0.0 to 100.0]
};

/// @struct ImagingPreset
/// @brief Saved optical configuration profile.
struct ImagingPreset {
    std::string token {}; ///< Unique preset token
    std::string type { "Custom" }; ///< Type: "Clear", "B/W", "Custom"
    std::string name {}; ///< User-friendly preset label
};

/// @struct ImagingSettings
/// @brief Optical, exposure, color, and focus parameters (ONVIF Profile T Imaging Service).
struct ImagingSettings {
    float brightness { 50.0f }; ///< Brightness level [0.0 to 100.0]
    float colorSaturation { 50.0f }; ///< Color saturation level [0.0 to 100.0]
    float contrast { 50.0f }; ///< Contrast level [0.0 to 100.0]
    float sharpness { 50.0f }; ///< Sharpness level [0.0 to 100.0]
    std::string irCutFilter { "AUTO" }; ///< IR cut filter mode: "ON", "OFF", or "AUTO"
    bool backlightCompensation { false }; ///< Backlight compensation enable
    float backlightLevel { 0.0f }; ///< Backlight compensation level [0.0 to 100.0]
    bool wideDynamicRange { false }; ///< Wide Dynamic Range (WDR) enable
    float wdrLevel { 0.0f }; ///< WDR level [0.0 to 100.0]
    std::string autoFocusMode { "AUTO" }; ///< Auto-focus mode: "AUTO" or "MANUAL"
    ExposureSettings exposure {}; ///< Exposure and iris parameters
    FocusStatus20 focusStatus {}; ///< Current focus position and movement status
};

/// @struct OnvifEvent
/// @brief Real-time event notification (ONVIF PullPoint NotificationMessage).
struct OnvifEvent {
    std::string topic {}; ///< Event topic, e.g. "tns1:RuleEngine/CellMotionDetector/Motion"
    std::string sourceName {}; ///< Source item name, e.g. "VideoSourceConfigurationToken"
    std::string sourceValue {}; ///< Source item value, e.g. "VideoSourceToken_1"
    std::string dataName {}; ///< Data item name, e.g. "IsMotion" or "State"
    std::string dataValue {}; ///< Data item value, e.g. "true" or "false"
    std::string utcTime {}; ///< Notification timestamp from camera
};

/// @enum PresetTourOperation
/// @brief Execution command for an ONVIF Preset Tour.
enum class PresetTourOperation : std::uint8_t { Start, Stop, Pause };

/// @enum PresetTourState
/// @brief Operational status of an ONVIF Preset Tour.
enum class PresetTourState : std::uint8_t { Idle, Touring, Paused, Extended };

/// @struct PresetTourSpot
/// @brief A waypoint/destination preset within an ONVIF Preset Tour.
struct PresetTourSpot {
    std::string presetToken {}; ///< Target preset token, e.g. "1" or "Preset_1"
    float speed { 1.0f }; ///< Speed ratio [0.0 to 1.0]
    std::uint32_t stayTimeSeconds { 5U }; ///< Dwell duration in seconds at this preset
};

/// @struct PresetTour
/// @brief Sequence of presets visited in an automated tour (ONVIF PTZ Service).
struct PresetTour {
    std::string token {}; ///< Unique tour token identifier, e.g. "Tour_1"
    std::string name {}; ///< User-friendly tour label, e.g. "Perimeter Patrol"
    PresetTourState status { PresetTourState::Idle }; ///< Current operational status
    bool autoStart { false }; ///< Automatically start tour upon boot/profile activation
    std::vector<PresetTourSpot> spots {}; ///< Sequence of tour spots
};

/// @enum OsdType
/// @brief Type of On-Screen Display overlay.
enum class OsdType : std::uint8_t { Text, Image, Extended };

/// @enum OsdPositionType
/// @brief Standard placement zones for On-Screen Display overlays.
enum class OsdPositionType : std::uint8_t { UpperLeft, UpperRight, LowerLeft, LowerRight, Custom };

/// @struct OsdConfig
/// @brief Configuration parameters for an ONVIF On-Screen Display (OSD) overlay.
struct OsdConfig {
    std::string token {}; ///< Unique OSD token, e.g. "OSD_1"
    std::string videoSourceToken { "VideoSource_1" }; ///< Video source attachment
    OsdType type { OsdType::Text }; ///< Text or Image overlay
    OsdPositionType position { OsdPositionType::UpperLeft }; ///< Corner or custom position
    float customX { 0.0f }; ///< Normalized X coordinate [-1.0 to 1.0] if position is Custom
    float customY { 0.0f }; ///< Normalized Y coordinate [-1.0 to 1.0] if position is Custom
    std::string plainText {}; ///< Text string displayed on screen
    std::uint32_t fontSize { 24U }; ///< Rendered font point size
    bool isDateAndTime { false }; ///< If true, overlays dynamic date/time
    std::string dateFormat { "YYYY/MM/DD" }; ///< Date format string
    std::string timeFormat { "HH:mm:ss" }; ///< Time format string
};

/// @struct VideoEncoderConfig
/// @brief Media2 video encoding configuration parameters.
struct VideoEncoderConfig {
    std::string token { "VideoEncoderConfig_1" }; ///< Unique configuration token
    std::string name { "VideoEncoderConfig" }; ///< User-friendly configuration name
    std::string encoding { "H264" }; ///< Video codec ("H264", "H265", "JPEG")
    int width { 1920 }; ///< Frame width in pixels
    int height { 1080 }; ///< Frame height in pixels
    float frameRateLimit { 30.0f }; ///< Maximum encoded framerate
    int bitrateLimitKbps { 4096 }; ///< Bitrate target limit in Kbps
    int govLength { 30 }; ///< Group of Video / keyframe interval
};

/// @struct Point2D
/// @brief 2D coordinate pair normalized to [0.0, 1.0] for video analytics geometry.
struct Point2D {
    float x { 0.0f }; ///< Horizontal normalized coordinate [0.0, 1.0]
    float y { 0.0f }; ///< Vertical normalized coordinate [0.0, 1.0]
};

/// @struct AnalyticsRule
/// @brief Analytics rule definition for ONVIF Analytics Service (Profile M & Profile T).
struct AnalyticsRule {
    std::string name {}; ///< Rule identifier name, e.g. "MyLineTripwire", "PerimeterZone"
    std::string type {
        "CellMotionDetector"
    }; ///< Rule algorithm type ("CellMotionDetector", "LineDetector", "FieldDetector", "LoiteringDetector")
    bool enabled { true }; ///< Whether the analytics rule is actively evaluated

    // Parameters for LineDetector (Tripwire)
    Point2D lineStart { 0.0f, 0.0f }; ///< Line start point for LineDetector
    Point2D lineEnd { 0.0f, 0.0f }; ///< Line end point for LineDetector
    std::string direction { "Any" }; ///< Direction: "LeftToRight", "RightToLeft", "Any"

    // Parameters for FieldDetector and LoiteringDetector
    std::vector<Point2D> polygon {}; ///< 2D polygon vertices defining region of interest

    // Parameters for LoiteringDetector
    double dwellTimeSeconds { 5.0 }; ///< Minimum continuous dwell time in seconds to trigger alarm

    // Classification filtering (Profile M)
    std::vector<std::string> objectClasses {}; ///< Allowed classes (e.g. "Human", "Vehicle"), empty = all
    float minConfidence { 0.5f }; ///< Minimum confidence threshold for classification

    // Sensitivity for CellMotionDetector
    int sensitivity { 50 }; ///< Sensitivity percentage [0, 100]
};

/// @struct AnalyticsRuleDescription
/// @brief Description of a supported analytics rule algorithm.
struct AnalyticsRuleDescription {
    std::string ruleType {}; ///< Rule algorithm type name (e.g. "tt:LineDetector")
    std::vector<std::string> supportedParameters {}; ///< List of configurable parameters
};

/// @struct AnalyticsModule
/// @brief Configuration of an ONVIF Analytics Module (Profile M / T).
struct AnalyticsModule {
    std::string name {}; ///< Module instance identifier name
    std::string type { "MotionDetector" }; ///< Module algorithm type (e.g. "tt:MotionDetector", "tt:ObjectClassifier")
    std::map<std::string, std::string> parameters {}; ///< Key-value parameter dictionary
};

/// @struct AnalyticsModuleDescription
/// @brief Description of a supported analytics module.
struct AnalyticsModuleDescription {
    std::string moduleType {}; ///< Module algorithm type name
    std::vector<std::string> supportedParameters {}; ///< List of supported parameter keys
};

/// @enum OnvifUserLevel
/// @brief Role-based user authorization level conforming to ONVIF Device Service.
enum class OnvifUserLevel : std::uint8_t { Administrator, Operator, User, Anonymous, Extended };

/// @brief Converts an OnvifUserLevel enum to string representation.
/// @param[in] level User privilege level.
/// @return String representation (e.g. "Administrator").
[[nodiscard]] inline std::string userLevelToString(OnvifUserLevel level)
{
    switch (level) {
    case OnvifUserLevel::Administrator:
        return "Administrator";
    case OnvifUserLevel::Operator:
        return "Operator";
    case OnvifUserLevel::User:
        return "User";
    case OnvifUserLevel::Anonymous:
        return "Anonymous";
    case OnvifUserLevel::Extended:
        return "Extended";
    default:
        return "User";
    }
}

/// @brief Parses an ONVIF UserLevel string into its enum counterpart.
/// @param[in] str String representation.
/// @return OnvifUserLevel enum value.
[[nodiscard]] inline OnvifUserLevel userLevelFromString(const std::string& str)
{
    if (str == "Administrator") {
        return OnvifUserLevel::Administrator;
    }
    if (str == "Operator") {
        return OnvifUserLevel::Operator;
    }
    if (str == "Anonymous") {
        return OnvifUserLevel::Anonymous;
    }
    if (str == "Extended") {
        return OnvifUserLevel::Extended;
    }
    return OnvifUserLevel::User;
}

/// @struct OnvifUser
/// @brief User account on the ONVIF device for authentication and access control.
struct OnvifUser {
    std::string username {}; ///< User login name
    std::string password {}; ///< Password (plaintext or hash)
    OnvifUserLevel level { OnvifUserLevel::User }; ///< Role authorization level
};

/// @struct NetworkIPv4Config
/// @brief IPv4 network configuration for a network adapter interface.
struct NetworkIPv4Config {
    bool enabled { true }; ///< True if IPv4 stack is enabled
    bool dhcp { true }; ///< True if IPv4 address is assigned via DHCP
    std::string manualAddress { "192.168.1.100" }; ///< Static IPv4 address
    int prefixLength { 24 }; ///< Subnet prefix length (e.g. 24 for 255.255.255.0)
};

/// @struct NetworkInterfaceConfig
/// @brief Physical or virtual network interface configuration.
struct NetworkInterfaceConfig {
    std::string token { "eth0" }; ///< Network interface token
    bool enabled { true }; ///< Adapter enabled state
    std::string name { "eth0" }; ///< System device name
    std::string hwAddress { "00:11:22:33:44:55" }; ///< Physical MAC address
    int mtu { 1500 }; ///< Maximum transmission unit in bytes
    NetworkIPv4Config ipv4 {}; ///< IPv4 parameters
};

/// @struct DnsConfig
/// @brief Domain Name System (DNS) server configuration.
struct DnsConfig {
    bool fromDhcp { true }; ///< True if DNS servers are retrieved from DHCP
    std::vector<std::string> searchDomains {}; ///< Search domain suffixes
    std::vector<std::string> dnsServers {}; ///< Configured DNS server IP addresses
};

/// @struct NtpConfig
/// @brief Network Time Protocol (NTP) server configuration.
struct NtpConfig {
    bool fromDhcp { true }; ///< True if NTP servers are acquired from DHCP
    std::vector<std::string> manualServers {}; ///< Explicit NTP server hostnames or IPs
};

/// @enum FactoryDefaultType
/// @brief Reset operation type for SetSystemFactoryDefault.
enum class FactoryDefaultType : std::uint8_t { Hard, Soft };

/// @struct SystemDateTimeConfig
/// @brief System date, time, and timezone parameters.
struct SystemDateTimeConfig {
    std::string dateTimeType { "NTP" }; ///< "Manual" or "NTP"
    bool daylightSavings { false }; ///< True if daylight saving time active
    std::string timeZone { "UTC" }; ///< Posix or standard timezone identifier (e.g. "UTC", "GMT+2")
    int hour { 0 }; ///< UTC hour [0, 23]
    int minute { 0 }; ///< UTC minute [0, 59]
    int second { 0 }; ///< UTC second [0, 59]
    int year { 2026 }; ///< Gregorian year
    int month { 1 }; ///< Month [1, 12]
    int day { 1 }; ///< Day [1, 31]
};

/// @enum RelayLogicalState
/// @brief Logical state of a physical or virtual relay output.
enum class RelayLogicalState : std::uint8_t { Active, Inactive };

/// @enum RelayMode
/// @brief Operating mode of a relay output (Monostable pulse vs. Bistable latch).
enum class RelayMode : std::uint8_t { Monostable, Bistable };

/// @enum RelayIdleState
/// @brief Electrical unenergized contact state (Open vs Closed).
enum class RelayIdleState : std::uint8_t { Open, Closed };

/// @struct RelayOutputConfig
/// @brief Configuration and state for an ONVIF Relay Output (Profile S/T).
struct RelayOutputConfig {
    std::string token {}; ///< Unique relay token, e.g. "Relay_1"
    RelayMode mode { RelayMode::Bistable }; ///< Operating mode
    float delayTimeSeconds { 0.0f }; ///< Delay duration in seconds for Monostable mode
    RelayIdleState idleState { RelayIdleState::Open }; ///< Open or Closed when unenergized
    RelayLogicalState logicalState { RelayLogicalState::Inactive }; ///< Current state: Active or Inactive
};

/// @struct DigitalInputConfig
/// @brief Configuration and state for an ONVIF Digital Input (sensor input).
struct DigitalInputConfig {
    std::string token {}; ///< Unique input token, e.g. "Input_1"
    RelayIdleState idleState { RelayIdleState::Open }; ///< Normal contact idle state
    std::string sensorType { "Manual" }; ///< Sensor description, e.g. "Alarm", "PIR", "Manual"
    bool active { false }; ///< Current live contact state
};

/// @brief Converts RelayLogicalState enum to ONVIF string ("active" or "inactive").
[[nodiscard]] inline std::string relayLogicalStateToString(RelayLogicalState state)
{
    return (state == RelayLogicalState::Active) ? "active" : "inactive";
}

/// @brief Converts string to RelayLogicalState enum.
[[nodiscard]] inline RelayLogicalState relayLogicalStateFromString(const std::string& str)
{
    return (str == "active" || str == "Active") ? RelayLogicalState::Active : RelayLogicalState::Inactive;
}

/// @brief Converts RelayMode enum to ONVIF string ("Monostable" or "Bistable").
[[nodiscard]] inline std::string relayModeToString(RelayMode mode)
{
    return (mode == RelayMode::Monostable) ? "Monostable" : "Bistable";
}

/// @brief Converts string to RelayMode enum.
[[nodiscard]] inline RelayMode relayModeFromString(const std::string& str)
{
    return (str == "Monostable") ? RelayMode::Monostable : RelayMode::Bistable;
}

/// @brief Converts RelayIdleState enum to ONVIF string ("open" or "closed").
[[nodiscard]] inline std::string relayIdleStateToString(RelayIdleState state)
{
    return (state == RelayIdleState::Closed) ? "closed" : "open";
}

/// @brief Converts string to RelayIdleState enum.
[[nodiscard]] inline RelayIdleState relayIdleStateFromString(const std::string& str)
{
    return (str == "closed" || str == "Closed") ? RelayIdleState::Closed : RelayIdleState::Open;
}

/// @struct GeoLocation
/// @brief Geographic coordinates conforming to WGS84 for ONVIF Profile M analytics.
struct GeoLocation {
    double latitude { 0.0 }; ///< Latitude in degrees [-90.0, 90.0]
    double longitude { 0.0 }; ///< Longitude in degrees [-180.0, 180.0]
    double elevation { 0.0 }; ///< Elevation above sea level in meters
};

/// @struct BoundingBox
/// @brief Normalized bounding box for detected objects in video analytics frames.
struct BoundingBox {
    float left { 0.0f }; ///< Left normalized coordinate
    float top { 0.0f }; ///< Top normalized coordinate
    float right { 0.0f }; ///< Right normalized coordinate
    float bottom { 0.0f }; ///< Bottom normalized coordinate
};

/// @struct AnalyticsObject
/// @brief Classified object detected in a video analytics frame (Profile M).
struct AnalyticsObject {
    int objectId { 0 }; ///< Unique object tracking identifier
    std::string className { "Human" }; ///< Object class: "Human", "Vehicle", "Bike", "Bag"
    float confidence { 1.0f }; ///< Classification confidence [0.0, 1.0]
    BoundingBox boundingBox {}; ///< Spatial position on frame
    float speed { 0.0f }; ///< Estimated speed in m/s
    GeoLocation geoLocation {}; ///< Geographic location if calibrated
};

/// @struct AnalyticsFrame
/// @brief Video analytics time-slice frame containing detected objects (Profile M).
struct AnalyticsFrame {
    std::string utcTime {}; ///< Frame timestamp in ISO 8601 UTC
    int frameWidth { 1920 }; ///< Frame pixel width
    int frameHeight { 1080 }; ///< Frame pixel height
    std::vector<AnalyticsObject> objects {}; ///< Detected objects
};

/// @struct MetadataConfiguration
/// @brief Configuration defining which telemetry and analytics streams are enabled.
struct MetadataConfiguration {
    std::string token { "MetadataConfig_1" }; ///< Configuration token
    std::string name { "MetadataConfiguration" }; ///< Configuration name
    int useCount { 1 }; ///< Number of profiles referencing this configuration
    std::string sessionTimeout { "PT60S" }; ///< Streaming session keep-alive timeout
    bool ptzStatusEnabled { true }; ///< Stream PTZ status telemetry (Profile T)
    bool analyticsEnabled { true }; ///< Stream object analytics & bounding boxes (Profile M)
    bool eventsEnabled { true }; ///< Stream notification events in metadata channel
    bool geoOrientationEnabled { false }; ///< Stream compass heading / geo-orientation
};

/// @struct MetadataConfigurationOptions
/// @brief Capability options for metadata configuration.
struct MetadataConfigurationOptions {
    bool ptzStatusSupported { true }; ///< True if PTZ status streaming is supported
    bool analyticsSupported { true }; ///< True if video analytics metadata is supported
    bool eventsSupported { true }; ///< True if event notification streaming is supported
};

/// @struct MetadataStreamPayload
/// @brief Full snapshot of active metadata (PTZ status, analytics frame, and events).
struct MetadataStreamPayload {
    std::optional<PtzStatus> ptzStatus {}; ///< Current PTZ kinematics
    std::optional<AnalyticsFrame> analyticsFrame {}; ///< Detected objects
    std::vector<OnvifEvent> events {}; ///< Recent event notifications
};

/// @enum SystemLogType
/// @brief Type of system log retrieved from camera (ONVIF Device Service).
enum class SystemLogType : std::uint8_t { System, Access };

/// @brief Converts SystemLogType enum to ONVIF string representation.
[[nodiscard]] inline std::string systemLogTypeToString(SystemLogType type)
{
    return (type == SystemLogType::Access) ? "Access" : "System";
}

/// @brief Parses SystemLogType enum from string representation.
[[nodiscard]] inline SystemLogType systemLogTypeFromString(const std::string& str)
{
    return (str == "Access" || str == "access") ? SystemLogType::Access : SystemLogType::System;
}

/// @struct SystemSupportInfo
/// @brief Diagnostics and performance telemetry for device support.
struct SystemSupportInfo {
    std::uint64_t uptimeSeconds { 0 }; ///< Device uptime in seconds
    float cpuLoadPercent { 0.0f }; ///< Current CPU load [0.0, 100.0]
    std::uint32_t memoryUsedMb { 0 }; ///< Used RAM in megabytes
    std::uint32_t memoryTotalMb { 0 }; ///< Total RAM in megabytes
    std::uint32_t activeConnections { 0 }; ///< Number of open client connections
    std::string storageState { "OK" }; ///< Storage / filesystem health
    std::string rawDiagnostics {}; ///< Text diagnostic dump
};

/// @struct SystemBackupData
/// @brief Configuration archive for backup and restore operations.
struct SystemBackupData {
    std::string backupTimestamp {}; ///< ISO timestamp when backup was created
    std::string configPayloadJson {}; ///< Serialized settings JSON/XML payload
    std::string checksum {}; ///< Checksum verification string
};

// =========================================================================
// Profile G: Recording, Search & Replay Data Models
// =========================================================================

/// @enum RecordingTrackType
/// @brief Media type of a track within an ONVIF recording container (Profile G).
enum class RecordingTrackType : std::uint8_t { Video, Audio, Metadata };

/// @brief Converts RecordingTrackType to string representation.
[[nodiscard]] inline std::string recordingTrackTypeToString(RecordingTrackType type)
{
    switch (type) {
    case RecordingTrackType::Audio:
        return "Audio";
    case RecordingTrackType::Metadata:
        return "Metadata";
    case RecordingTrackType::Video:
    default:
        return "Video";
    }
}

/// @brief Parses RecordingTrackType from string representation.
[[nodiscard]] inline RecordingTrackType recordingTrackTypeFromString(const std::string& str)
{
    if (str == "Audio" || str == "audio") {
        return RecordingTrackType::Audio;
    }
    if (str == "Metadata" || str == "metadata") {
        return RecordingTrackType::Metadata;
    }
    return RecordingTrackType::Video;
}

/// @struct RecordingTrack
/// @brief A specific recorded stream track (e.g. video, audio, or metadata).
struct RecordingTrack {
    std::string trackToken { "Track_Video" }; ///< Track token
    RecordingTrackType trackType { RecordingTrackType::Video }; ///< Track type
    std::string description {}; ///< Description
};

/// @struct RecordingConfig
/// @brief Configuration and attributes of an edge recording storage container.
struct RecordingConfig {
    std::string recordingToken { "Recording_1" }; ///< Unique recording token
    std::string sourceToken { "VideoSource_1" }; ///< Bound video source
    std::string content { "Primary surveillance recording" }; ///< Descriptive label
    std::string maximumRetentionTime { "P30D" }; ///< Maximum retention time ISO 8601 duration
    std::vector<RecordingTrack> tracks {}; ///< Associated recorded tracks
};

/// @struct RecordingSummary
/// @brief Aggregated storage volume and time statistics for recordings.
struct RecordingSummary {
    std::string dataFrom {}; ///< Earliest recording timestamp (ISO 8601 UTC)
    std::string dataUntil {}; ///< Latest recording timestamp (ISO 8601 UTC)
    int numberRecordings { 0 }; ///< Total number of recording containers
    std::uint64_t totalStorageBytes { 0 }; ///< Total disk bytes consumed
};

/// @enum RecordingJobMode
/// @brief Operational mode of an automated recording job.
enum class RecordingJobMode : std::uint8_t { Active, Idle };

/// @brief Converts RecordingJobMode to string representation.
[[nodiscard]] inline std::string recordingJobModeToString(RecordingJobMode mode)
{
    return (mode == RecordingJobMode::Active) ? "Active" : "Idle";
}

/// @brief Parses RecordingJobMode from string representation.
[[nodiscard]] inline RecordingJobMode recordingJobModeFromString(const std::string& str)
{
    return (str == "Active" || str == "active") ? RecordingJobMode::Active : RecordingJobMode::Idle;
}

/// @struct RecordingJob
/// @brief Automation job associating a media source with an edge recording container.
struct RecordingJob {
    std::string jobToken { "Job_1" }; ///< Unique job token
    std::string recordingToken { "Recording_1" }; ///< Target recording token
    RecordingJobMode mode { RecordingJobMode::Active }; ///< Current job mode
    int priority { 1 }; ///< Priority integer
    std::string sourceToken { "VideoSource_1" }; ///< Video source token
};

/// @struct RecordingSearchResult
/// @brief Match item returned from a historical recording search query.
struct RecordingSearchResult {
    std::string recordingToken {}; ///< Matching recording token
    std::string trackToken {}; ///< Matching track token
    std::string earliestTime {}; ///< Earliest segment timestamp
    std::string latestTime {}; ///< Latest segment timestamp
    std::string searchState { "Completed" }; ///< State of the search
};

/// @struct RecordedEventResult
/// @brief Historical event notification found within recorded time slices.
struct RecordedEventResult {
    std::string recordingToken {}; ///< Originating recording container
    std::string eventTime {}; ///< Event timestamp (ISO 8601 UTC)
    std::string topic {}; ///< ONVIF event topic
    std::string source {}; ///< Event source name/value
    std::string data {}; ///< Event data payload
};

/// @struct ReplayConfiguration
/// @brief Session parameters and timeouts for RTSP replay streams.
struct ReplayConfiguration {
    std::string sessionTimeout { "PT60S" }; ///< Replay stream keep-alive duration
};

// =========================================================================
// Device Management: PKI Certificates & HTTPS/TLS Security Models
// =========================================================================

/// @struct CertificateInformation
/// @brief X.509 certificate metadata and validity attributes.
struct CertificateInformation {
    std::string certificateId { "Cert_1" }; ///< Unique certificate identifier
    std::string subject {}; ///< X.509 Distinguished Name subject
    std::string issuer {}; ///< Certificate Authority or self-signed issuer
    std::string validNotBefore {}; ///< Start of validity period
    std::string validNotAfter {}; ///< Expiration timestamp
    std::string keyAlgorithm { "RSA" }; ///< Public key algorithm
    bool isDefault { false }; ///< True if bound as default TLS server cert
};

/// @struct OnvifCertificate
/// @brief Complete certificate record including raw DER base64 representation.
struct OnvifCertificate {
    std::string certificateId { "Cert_1" }; ///< Certificate ID
    std::string x509DerBase64 {}; ///< Base64-encoded DER X.509 certificate data
    CertificateInformation info {}; ///< Decoded metadata
};

/// @struct Pkcs10Request
/// @brief PKCS#10 Certificate Signing Request (CSR) generated on device.
struct Pkcs10Request {
    std::string certificateId {}; ///< Associated certificate ID
    std::string subject {}; ///< Requested subject DN
    std::string csrBase64 {}; ///< Base64-encoded PKCS#10 CSR payload
};

/// @enum ClientCertificateMode
/// @brief TLS client certificate authentication enforcement mode.
enum class ClientCertificateMode : std::uint8_t { Off, Optional, Required };

/// @brief Converts ClientCertificateMode to string representation.
[[nodiscard]] inline std::string clientCertificateModeToString(ClientCertificateMode mode)
{
    switch (mode) {
    case ClientCertificateMode::Optional:
        return "Optional";
    case ClientCertificateMode::Required:
        return "Required";
    case ClientCertificateMode::Off:
    default:
        return "Off";
    }
}

/// @brief Parses ClientCertificateMode from string representation.
[[nodiscard]] inline ClientCertificateMode clientCertificateModeFromString(const std::string& str)
{
    if (str == "Optional" || str == "optional") {
        return ClientCertificateMode::Optional;
    }
    if (str == "Required" || str == "required") {
        return ClientCertificateMode::Required;
    }
    return ClientCertificateMode::Off;
}

// =========================================================================
// Geolocation, GeoMove & Absolute Coordinate Spaces (Profile T & M)
// =========================================================================

/// @namespace CoordinateSpace
/// @brief Standard ONVIF PTZ coordinate space URI identifiers.
namespace CoordinateSpace {
    inline constexpr const char* PositionGenericSpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace";
    inline constexpr const char* PositionSphericalSpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace";
    inline constexpr const char* VelocityGenericSpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";
    inline constexpr const char* TranslationGenericSpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace";
} // namespace CoordinateSpace

/// @struct GeoOrientation
/// @brief Spatial orientation angles relative to Earth (ONVIF Profile M / Core).
struct GeoOrientation {
    double yaw { 0.0 };    ///< Azimuth heading clockwise from True North [0.0, 360.0)
    double pitch { 0.0 };  ///< Elevation tilt angle [-90.0, +90.0] (positive = up)
    double roll { 0.0 };   ///< Bank rotation angle [-180.0, +180.0]
};

/// @struct LocationEntity
/// @brief Geolocation container for a device or video source entity (ONVIF Device Management).
struct LocationEntity {
    std::string entity { "Device" }; ///< Entity type ("Device", "VideoSource")
    std::string token {};            ///< Entity token identifier
    bool fixed { true };             ///< True if coordinates are statically configured
    GeoLocation location {};         ///< WGS84 coordinates
    GeoOrientation orientation {};   ///< 3-axis mounting orientation
};

/// @struct GeoMoveTarget
/// @brief Parameters for directing PTZ camera toward a geographic location (ONVIF PTZ GeoMove).
struct GeoMoveTarget {
    GeoLocation targetGeo {};               ///< Target WGS84 coordinate
    std::optional<float> speed {};          ///< PTZ movement speed ratio [0.0, 1.0]
    std::optional<float> areaWidth {};      ///< Target framing area width in meters (for auto-zoom)
    std::optional<float> areaHeight {};     ///< Target framing area height in meters (for auto-zoom)
};

/// @struct SphericalPosition
/// @brief Pan/tilt angles in standard spherical degrees (ONVIF PositionSphericalSpace).
struct SphericalPosition {
    double azimuthDegrees { 0.0 };   ///< Azimuth angle [0.0, 360.0) degrees
    double elevationDegrees { 0.0 }; ///< Elevation angle [-90.0, +90.0] degrees
};

/// @enum MaskType
/// @brief Visual obfuscation appearance for ONVIF Privacy Masks (Profile T).
enum class MaskType {
    Color,     ///< Solid opaque color rectangle or polygon
    Pixelated, ///< Mosaic / pixelated blur effect
    Blurred    ///< Gaussian blur effect
};

/// @brief Converts MaskType enum to ONVIF string representation.
inline std::string maskTypeToString(MaskType type)
{
    switch (type) {
    case MaskType::Pixelated: return "Pixelated";
    case MaskType::Blurred: return "Blurred";
    case MaskType::Color:
    default:
        return "Color";
    }
}

/// @brief Converts ONVIF string representation to MaskType enum.
inline MaskType stringToMaskType(const std::string& str)
{
    if (str == "Pixelated") return MaskType::Pixelated;
    if (str == "Blurred") return MaskType::Blurred;
    return MaskType::Color;
}


/// @struct MaskColor
/// @brief Color specification for solid color privacy masks.
struct MaskColor {
    int x { 0 };                     ///< Color coordinate X (e.g. Red in RGB, Y in YUV) [0..255]
    int y { 0 };                     ///< Color coordinate Y (e.g. Green in RGB, U in YUV) [0..255]
    int z { 0 };                     ///< Color coordinate Z (e.g. Blue in RGB, V in YUV) [0..255]
    std::string colorspace { "RGB" };///< Color space ("RGB", "YUV")
};

/// @struct PrivacyMask
/// @brief Privacy exclusion mask definition (ONVIF Profile T / Media2).
struct PrivacyMask {
    std::string token {};                   ///< Unique mask identifier (e.g. "Mask_1")
    std::string configurationToken {};      ///< Associated VideoSourceConfiguration token
    std::vector<Point2D> polygon {};        ///< Geometric vertices in normalized coordinates [0.0, 1.0]
    MaskType type { MaskType::Color };      ///< Mask type (Color, Pixelated, Blurred)
    MaskColor color {};                     ///< Fill color for MaskType::Color
    bool enabled { true };                  ///< Whether mask is actively rendered
};

/// @struct MaskOptions
/// @brief Privacy mask capabilities and limits supported by device (Media2 GetMaskOptions).
struct MaskOptions {
    int maxMasks { 8 };                                   ///< Maximum simultaneous masks
    int maxPoints { 8 };                                  ///< Maximum vertices per polygon mask
    std::vector<MaskType> supportedTypes {
        MaskType::Color, MaskType::Pixelated, MaskType::Blurred
    };                                                    ///< Supported visual mask styles
    std::vector<std::string> supportedColorSpaces { "RGB" }; ///< Supported color spaces
    bool rectangleSupported { true };                     ///< Rectangle masks supported
    bool polygonSupported { true };                       ///< Polygonal masks supported
};

/// @struct VideoSourceMode
/// @brief Sensor capture resolution and framerate mode (ONVIF Profile T / Media2).
struct VideoSourceMode {
    std::string token {};                   ///< Mode identifier (e.g. "Mode_1080p60", "Mode_4K30")
    bool enabled { false };                 ///< True if this mode is currently active
    float maxFramerate { 30.0f };           ///< Maximum framerate supported in this mode
    int width { 1920 };                     ///< Maximum image width in pixels
    int height { 1080 };                    ///< Maximum image height in pixels
    std::vector<std::string> encodings { "H264", "H265" }; ///< Supported video encodings
    bool reboot { false };                  ///< True if applying mode triggers hardware reboot
    std::string description {};             ///< Human-readable mode label
};

/// @enum RadiometryAlarmType
/// @brief Type of radiometric temperature threshold alarm.
enum class RadiometryAlarmType {
    HighTemperature,    ///< Triggered when temperature rises above threshold
    LowTemperature      ///< Triggered when temperature falls below threshold
};

/// @struct RadiometryConfig
/// @brief Radiometric compensation parameters for temperature calculation (ONVIF Thermal Service).
struct RadiometryConfig {
    float emissivity { 0.95f };              ///< Surface emissivity [0.01, 1.00] (0.95 for human skin/matte)
    float distance { 5.0f };                 ///< Distance to target in meters
    float reflectedTemperature { 20.0f };    ///< Ambient reflected temperature in Celsius
    float atmosphericTemperature { 20.0f };  ///< Ambient atmospheric temperature in Celsius
    float relativeHumidity { 50.0f };        ///< Relative humidity percentage [0.0, 100.0]
    float windowTransmission { 1.0f };       ///< Window/optics transmission factor [0.01, 1.00]
};

/// @struct RadiometrySpot
/// @brief Single point spotmeter temperature measurement (ONVIF Thermal Service).
struct RadiometrySpot {
    std::string token {};                    ///< Unique spot token (e.g. "Spot_1")
    Point2D position { 0.5f, 0.5f };         ///< Normalized coordinates [0.0, 1.0]
    std::string label { "Spot 1" };          ///< Human-readable label
    float temperature { 25.0f };             ///< Measured temperature in Celsius
};

/// @struct RadiometryBox
/// @brief Rectangular area temperature measurement zone (ONVIF Thermal Service).
struct RadiometryBox {
    std::string token {};                    ///< Unique box token (e.g. "Box_1")
    Point2D topLeft { 0.25f, 0.25f };        ///< Normalized top-left coordinates [0.0, 1.0]
    Point2D bottomRight { 0.75f, 0.75f };    ///< Normalized bottom-right coordinates [0.0, 1.0]
    std::string label { "Area 1" };          ///< Human-readable label
    float minTemperature { 22.0f };          ///< Minimum measured temperature in Celsius
    float maxTemperature { 38.5f };          ///< Maximum measured temperature in Celsius
    float avgTemperature { 30.2f };          ///< Average measured temperature in Celsius
};

/// @struct RadiometryAlarmConfig
/// @brief Radiometric temperature threshold alarm configuration (ONVIF Thermal Service).
struct RadiometryAlarmConfig {
    std::string token {};                    ///< Associated spot or box token
    float thresholdTemperature { 50.0f };    ///< Alarm temperature threshold in Celsius
    float hysteresis { 2.0f };               ///< Alarm hysteresis in Celsius
    RadiometryAlarmType alarmType { RadiometryAlarmType::HighTemperature }; ///< High or Low alarm
    bool enabled { true };                   ///< Whether alarm evaluation is active
};

/// @struct ColorPalette
/// @brief Thermal false-color visualization palette (ONVIF Thermal Service).
struct ColorPalette {
    std::string token {};                    ///< Palette token (e.g. "WhiteHot", "Ironbow")
    std::string name {};                     ///< Display name
    bool isDefault { false };                ///< Whether this is the camera's default palette
};

/// @struct ThermalCapabilities
/// @brief Device capabilities supported by the ONVIF Thermal Service.
struct ThermalCapabilities {
    bool radiometry { true };                ///< Radiometric temperature measurement supported
    bool colorPalette { true };              ///< False-color palette selection supported
    bool nuc { true };                       ///< Non-Uniformity Correction (shutter) supported
    bool cooler { false };                   ///< Stirling or pulse-tube cooler supported
};

} // namespace PelcoD::Onvif
