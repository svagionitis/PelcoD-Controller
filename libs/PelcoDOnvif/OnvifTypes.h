#pragma once

/// @file OnvifTypes.h
/// @brief Common data structures, enums, and models for ONVIF Profile S client.

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

} // namespace PelcoD::Onvif
