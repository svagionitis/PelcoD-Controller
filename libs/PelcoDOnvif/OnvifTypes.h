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

} // namespace PelcoD::Onvif
