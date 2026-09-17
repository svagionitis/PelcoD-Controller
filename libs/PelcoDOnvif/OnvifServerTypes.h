#pragma once

/// @file OnvifServerTypes.h
/// @brief Configuration and interface types for the embedded ONVIF server and WS-Discovery responder.

#include "OnvifTypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @struct OnvifServerConfig
/// @brief Configuration settings for the embedded ONVIF HTTP server and WS-Discovery service.
struct OnvifServerConfig {
    /// @brief IP address or interface to bind to (e.g. "0.0.0.0").
    std::string bindAddress { "0.0.0.0" };

    /// @brief HTTP server listening port (default: 8080).
    int port { 8080 };

    /// @brief Human-readable name of the advertised ONVIF device.
    std::string deviceName { "Pelco-D ONVIF Bridge" };

    /// @brief Manufacturer string reported in GetDeviceInformation.
    std::string manufacturer { "PelcoD-Controller" };

    /// @brief Model identifier reported in GetDeviceInformation.
    std::string model { "Virtual-PTZ-Bridge" };

    /// @brief Firmware version string reported in GetDeviceInformation.
    std::string firmwareVersion { "1.0.0" };

    /// @brief Serial number reported in GetDeviceInformation.
    std::string serialNumber { "SN-PELCOD-001" };

    /// @brief Hardware revision identifier.
    std::string hardwareId { "1.0" };

    /// @brief Default RTSP stream URI returned in Media GetStreamUri.
    std::string rtspStreamUri { "rtsp://127.0.0.1:8554/live" };

    /// @brief Unique UUID string identifying this device endpoint (auto-generated if empty).
    std::string serviceUuid {};

    /// @brief Additional WS-Discovery scope URIs.
    std::vector<std::string> scopes {};
};

/// @class IPtzHandler
/// @brief Abstract interface for decoupling ONVIF PTZ requests from hardware/controller implementation.
class IPtzHandler {
public:
    virtual ~IPtzHandler() = default;

    /// @brief Handles continuous pan, tilt, and zoom velocity commands.
    /// @param[in] panSpeed Normalized horizontal velocity [-1.0 (left) to 1.0 (right)].
    /// @param[in] tiltSpeed Normalized vertical velocity [-1.0 (down) to 1.0 (up)].
    /// @param[in] zoomSpeed Normalized zoom velocity [-1.0 (wide) to 1.0 (tele)].
    virtual void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed) = 0;

    /// @brief Handles absolute positioning commands.
    /// @param[in] pan Target pan position in degrees or normalized coordinate.
    /// @param[in] tilt Target tilt position in degrees or normalized coordinate.
    /// @param[in] zoom Target zoom position in normalized range [0.0, 1.0].
    virtual void handleAbsoluteMove(float pan, float tilt, float zoom) = 0;

    /// @brief Handles stop motion requests.
    /// @param[in] stopPanTilt True if pan and tilt motion should cease.
    /// @param[in] stopZoom True if zoom motion should cease.
    virtual void handleStop(bool stopPanTilt, bool stopZoom) = 0;

    /// @brief Stores current position as a named preset.
    /// @param[in] name Optional user-friendly preset name.
    /// @param[in] token Optional or existing preset token.
    /// @return Assigned preset token string, or empty on failure.
    [[nodiscard]] virtual std::string handleSetPreset(const std::string& name, const std::string& token) = 0;

    /// @brief Recalls and moves camera to the designated preset.
    /// @param[in] token Preset identifier token.
    /// @return True if command successfully dispatched.
    [[nodiscard]] virtual bool handleGotoPreset(const std::string& token) = 0;

    /// @brief Removes a previously stored preset.
    /// @param[in] token Preset identifier token to delete.
    /// @return True if removed successfully.
    [[nodiscard]] virtual bool handleRemovePreset(const std::string& token) = 0;

    /// @brief Retrieves list of currently active presets.
    /// @return Vector of preset tokens and names.
    [[nodiscard]] virtual std::vector<PtzPreset> handleGetPresets() = 0;

    /// @brief Retrieves the current PTZ position and moving status.
    /// @return Current status snapshot.
    [[nodiscard]] virtual PtzStatus handleGetStatus() = 0;
};

} // namespace PelcoD::Onvif
