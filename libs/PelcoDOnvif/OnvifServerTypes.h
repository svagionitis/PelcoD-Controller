#pragma once

/// @file OnvifServerTypes.h
/// @brief Configuration and interface types for the embedded ONVIF server and WS-Discovery responder.

#include "OnvifTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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
    std::vector<std::string> scopes { "onvif://www.onvif.org/Profile/S", "onvif://www.onvif.org/Profile/T" };

    /// @brief Default optical and imaging configuration.
    ImagingSettings defaultImagingSettings {};
};

/// @brief Callback signature for publishing asynchronous ONVIF event notifications.
using EventCallback = std::function<void(const OnvifEvent& event)>;

/// @brief Callback signature for logging incoming ONVIF HTTP/SOAP requests.
using RequestLogCallback
    = std::function<void(const std::string& service, const std::string& action, const std::string& clientIp)>;

/// @class IImagingHandler
/// @brief Abstract interface for decoupling ONVIF Profile T Imaging requests from hardware.
class IImagingHandler {
public:
    virtual ~IImagingHandler() = default;

    /// @brief Retrieves current optical and imaging parameters for a video source.
    /// @param[in] videoSourceToken Video source token.
    /// @return Current ImagingSettings structure.
    [[nodiscard]] virtual ImagingSettings handleGetImagingSettings(const std::string& videoSourceToken) = 0;

    /// @brief Applies updated imaging parameters to the video source.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] settings New imaging parameters.
    /// @return True if settings were successfully applied.
    [[nodiscard]] virtual bool handleSetImagingSettings(
        const std::string& videoSourceToken, const ImagingSettings& settings)
        = 0;

    /// @brief Starts continuous optical focus movement.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] speed Normalized speed [-1.0 (near) to +1.0 (far)].
    virtual void handleMoveFocus(const std::string& videoSourceToken, float speed) = 0;

    /// @brief Halts active optical focus movement.
    /// @param[in] videoSourceToken Video source token.
    virtual void handleStopFocus(const std::string& videoSourceToken) = 0;
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

    /// @brief Retrieves all configured preset tours.
    /// @return List of PresetTour objects.
    [[nodiscard]] virtual std::vector<PresetTour> handleGetPresetTours()
    {
        return {};
    }

    /// @brief Retrieves a specific preset tour by token.
    /// @param[in] tourToken Identifier token of the tour.
    /// @return PresetTour struct or nullopt if not found.
    [[nodiscard]] virtual std::optional<PresetTour> handleGetPresetTour(const std::string& /*tourToken*/)
    {
        return std::nullopt;
    }

    /// @brief Creates a new empty preset tour and returns its assigned token.
    /// @return Assigned preset tour token.
    [[nodiscard]] virtual std::string handleCreatePresetTour()
    {
        return {};
    }

    /// @brief Modifies an existing preset tour's configuration and spots.
    /// @param[in] tour Updated tour parameters and spots.
    /// @return True if modified successfully.
    [[nodiscard]] virtual bool handleModifyPresetTour(const PresetTour& /*tour*/)
    {
        return false;
    }

    /// @brief Controls execution of a preset tour (Start, Stop, Pause).
    /// @param[in] tourToken Tour identifier token.
    /// @param[in] op Operation to execute.
    /// @return True if operation succeeded.
    [[nodiscard]] virtual bool handleOperatePresetTour(const std::string& /*tourToken*/, PresetTourOperation /*op*/)
    {
        return false;
    }

    /// @brief Deletes a preset tour.
    /// @param[in] tourToken Tour identifier token to remove.
    /// @return True if removed successfully.
    [[nodiscard]] virtual bool handleRemovePresetTour(const std::string& /*tourToken*/)
    {
        return false;
    }

    /// @brief Moves camera relatively by specified coordinate translation vector.
    /// @param[in] pan Relative pan translation [-1.0 to 1.0].
    /// @param[in] tilt Relative tilt translation [-1.0 to 1.0].
    /// @param[in] zoom Relative zoom translation [-1.0 to 1.0].
    /// @param[in] speed Movement speed ratio [0.0 to 1.0].
    /// @return True if relative move command dispatched.
    [[nodiscard]] virtual bool handleRelativeMove(float /*pan*/, float /*tilt*/, float /*zoom*/, float /*speed*/ = 1.0f)
    {
        return false;
    }

    /// @brief Directs camera head to return to configured home position.
    /// @param[in] speed Movement speed ratio [0.0 to 1.0].
    /// @return True if command dispatched.
    [[nodiscard]] virtual bool handleGotoHomePosition(float /*speed*/ = 1.0f)
    {
        return false;
    }

    /// @brief Stores current camera position as reference home position.
    /// @return True if home position saved.
    [[nodiscard]] virtual bool handleSetHomePosition()
    {
        return false;
    }

    /// @brief Dispatches ONVIF auxiliary command (wiper, washer, IR, aux relays).
    /// @param[in] auxiliaryData Raw auxiliary command string token.
    /// @return Auxiliary response string or empty if unsupported.
    [[nodiscard]] virtual std::string handleSendAuxiliaryCommand(const std::string& /*auxiliaryData*/)
    {
        return {};
    }
};

} // namespace PelcoD::Onvif
