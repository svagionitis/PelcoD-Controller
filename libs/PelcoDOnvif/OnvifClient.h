#pragma once

/// @file OnvifClient.h
/// @brief High-level ONVIF Profile S Client supporting Device, Media, and PTZ services.

#include "OnvifHttpClient.h"
#include "OnvifSecurity.h"
#include "OnvifTypes.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @struct OnvifCapabilities
/// @brief Service endpoints discovered via GetCapabilities.
struct OnvifCapabilities {
    std::string deviceXAddr {}; ///< Device service endpoint
    std::string mediaXAddr {}; ///< Media service endpoint
    std::string ptzXAddr {}; ///< PTZ service endpoint
    std::string eventsXAddr {}; ///< Events service endpoint
    std::string imagingXAddr {}; ///< Imaging service endpoint
};

/// @class OnvifClient
/// @brief Client communicating with ONVIF Profile S IP cameras.
class OnvifClient {
public:
    /// @brief Constructs client with device endpoint and credentials.
    /// @param[in] deviceEndpoint Device service URL (e.g. http://192.168.1.100/onvif/device_service).
    /// @param[in] credentials Authentication credentials.
    explicit OnvifClient(std::string deviceEndpoint, SecurityCredentials credentials = {});

    /// @brief Virtual destructor.
    virtual ~OnvifClient() = default;

    /// @brief Updates credentials.
    /// @param[in] credentials New authentication credentials.
    void setCredentials(SecurityCredentials credentials);

    /// @brief Gets current credentials.
    /// @return Current authentication credentials.
    [[nodiscard]] const SecurityCredentials& credentials() const;

    /// @brief Sets device service endpoint.
    /// @param[in] endpoint New device endpoint URL.
    void setDeviceEndpoint(std::string endpoint);

    /// @brief Gets current device service endpoint.
    /// @return Endpoint URL.
    [[nodiscard]] const std::string& deviceEndpoint() const;

    /// @brief Sets HTTP timeout.
    /// @param[in] timeout Request timeout.
    void setTimeout(std::chrono::milliseconds timeout);

    // =========================================================================
    // Device Service
    // =========================================================================

    /// @brief Queries camera capabilities to resolve Media and PTZ endpoints.
    /// @return Discovered capabilities or nullopt on communication failure.
    [[nodiscard]] std::optional<OnvifCapabilities> getCapabilities();

    /// @brief Queries camera hardware identification metadata.
    /// @return DeviceInformation struct or nullopt on failure.
    [[nodiscard]] std::optional<DeviceInformation> getDeviceInformation();

    /// @brief Queries camera UTC time and synchronizes client clock offset.
    /// @return True if system date/time query succeeded.
    bool synchronizeSystemTime();

    // =========================================================================
    // Media Service
    // =========================================================================

    /// @brief Queries available media profiles (resolutions, tokens, codecs).
    /// @return List of MediaProfile records.
    [[nodiscard]] std::vector<MediaProfile> getProfiles();

    /// @brief Resolves RTSP stream URI for a given media profile.
    /// @param[in] profileToken Media profile token (e.g. "Profile_1").
    /// @param[in] injectCredentials If true, embeds user:pass into rtsp:// URL.
    /// @return StreamUriInfo or nullopt on failure.
    [[nodiscard]] std::optional<StreamUriInfo> getStreamUri(
        const std::string& profileToken, bool injectCredentials = true);

    // =========================================================================
    // PTZ Service
    // =========================================================================

    /// @brief Sends continuous pan/tilt/zoom velocity command.
    /// @param[in] profileToken Media profile token.
    /// @param[in] panSpeed Normalized horizontal velocity [-1.0 (left) to +1.0 (right)].
    /// @param[in] tiltSpeed Normalized vertical velocity [-1.0 (down) to +1.0 (up)].
    /// @param[in] zoomSpeed Normalized zoom velocity [-1.0 (wide) to +1.0 (tele)].
    /// @return True if command was acknowledged by camera.
    bool continuousMove(const std::string& profileToken, double panSpeed, double tiltSpeed, double zoomSpeed = 0.0);

    /// @brief Stops active pan/tilt or zoom motion.
    /// @param[in] profileToken Media profile token.
    /// @param[in] stopPanTilt If true, stops pan and tilt motorized drive.
    /// @param[in] stopZoom If true, stops motorized zoom drive.
    /// @return True if stop command succeeded.
    bool stop(const std::string& profileToken, bool stopPanTilt = true, bool stopZoom = true);

    /// @brief Queries current pan/tilt/zoom position and movement status.
    /// @param[in] profileToken Media profile token.
    /// @return Current PtzStatus or nullopt on failure.
    [[nodiscard]] std::optional<PtzStatus> getStatus(const std::string& profileToken);

    /// @brief Moves PTZ head to absolute normalized coordinates.
    /// @param[in] profileToken Media profile token.
    /// @param[in] pan Normalized pan [-1.0, +1.0].
    /// @param[in] tilt Normalized tilt [-1.0, +1.0].
    /// @param[in] zoom Normalized zoom [0.0, 1.0].
    /// @return True if absolute move command succeeded.
    bool absoluteMove(const std::string& profileToken, double pan, double tilt, double zoom);

    // =========================================================================
    // XML Envelope & Parsing Helpers (Public for testing)
    // =========================================================================

    /// @brief Wraps SOAP body content in SOAP 1.2 envelope with WS-Security header.
    /// @param[in] bodyContent XML elements for the SOAP Body.
    /// @return Full XML SOAP envelope string.
    [[nodiscard]] std::string wrapSoapEnvelope(const std::string& bodyContent) const;

    /// @brief Parses GetCapabilities XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted OnvifCapabilities.
    [[nodiscard]] static std::optional<OnvifCapabilities> parseCapabilitiesResponse(const std::string& xml);

    /// @brief Parses GetDeviceInformation XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted DeviceInformation.
    [[nodiscard]] static std::optional<DeviceInformation> parseDeviceInformationResponse(const std::string& xml);

    /// @brief Parses GetProfiles XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of MediaProfile objects.
    [[nodiscard]] static std::vector<MediaProfile> parseProfilesResponse(const std::string& xml);

    /// @brief Parses GetStreamUri XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted StreamUriInfo.
    [[nodiscard]] static std::optional<StreamUriInfo> parseStreamUriResponse(const std::string& xml);

    /// @brief Parses GetStatus PTZ XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted PtzStatus.
    [[nodiscard]] static std::optional<PtzStatus> parsePtzStatusResponse(const std::string& xml);

private:
    std::string m_deviceEndpoint {};
    SecurityCredentials m_credentials {};
    OnvifCapabilities m_capabilities {};
    OnvifHttpClient m_httpClient {};
};

} // namespace PelcoD::Onvif
