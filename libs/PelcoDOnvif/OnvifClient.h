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

    /// @brief Sends a hardware/system reboot request to the camera.
    /// @return True if system reboot command was accepted.
    bool systemReboot();

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

    /// @brief Resolves HTTP JPEG snapshot URI for grabbing a still image.
    /// @param[in] profileToken Media profile token (e.g. "Profile_1").
    /// @param[in] injectCredentials If true, embeds user:pass into URL.
    /// @return Snapshot URI string or nullopt on failure.
    [[nodiscard]] std::optional<std::string> getSnapshotUri(
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

    /// @brief Sends relative translation command to step PTZ position by a delta.
    /// @param[in] profileToken Media profile token.
    /// @param[in] panTranslation Normalized pan offset step [-1.0 to +1.0].
    /// @param[in] tiltTranslation Normalized tilt offset step [-1.0 to +1.0].
    /// @param[in] zoomTranslation Normalized zoom offset step [-1.0 to +1.0].
    /// @return True if relative move command succeeded.
    bool relativeMove(
        const std::string& profileToken, double panTranslation, double tiltTranslation, double zoomTranslation = 0.0);

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

    /// @brief Moves PTZ head to configured camera home position.
    /// @param[in] profileToken Media profile token.
    /// @return True if goto home position command succeeded.
    bool gotoHomePosition(const std::string& profileToken);

    /// @brief Saves current PTZ position as the camera home position.
    /// @param[in] profileToken Media profile token.
    /// @return True if set home position command succeeded.
    bool setHomePosition(const std::string& profileToken);

    /// @brief Queries list of stored presets for a media profile.
    /// @param[in] profileToken Media profile token.
    /// @return Vector of stored PtzPreset items.
    [[nodiscard]] std::vector<PtzPreset> getPresets(const std::string& profileToken);

    /// @brief Saves current position as a preset or updates an existing preset.
    /// @param[in] profileToken Media profile token.
    /// @param[in] presetName Optional label for the preset.
    /// @param[in] presetToken Optional token to overwrite.
    /// @return Assigned preset token or nullopt on failure.
    [[nodiscard]] std::optional<std::string> setPreset(
        const std::string& profileToken, const std::string& presetName = "", const std::string& presetToken = "");

    /// @brief Recalls and moves PTZ head to a stored preset position.
    /// @param[in] profileToken Media profile token.
    /// @param[in] presetToken Preset identifier token.
    /// @param[in] speed Normalized speed [0.0 to 1.0].
    /// @return True if goto preset command succeeded.
    bool gotoPreset(const std::string& profileToken, const std::string& presetToken, double speed = 1.0);

    /// @brief Removes a preset from camera storage.
    /// @param[in] profileToken Media profile token.
    /// @param[in] presetToken Preset identifier token.
    /// @return True if preset removal succeeded.
    bool removePreset(const std::string& profileToken, const std::string& presetToken);

    /// @brief Queries list of configured preset tours for a media profile.
    /// @param[in] profileToken Media profile token.
    /// @return Vector of PresetTour items.
    [[nodiscard]] std::vector<PresetTour> getPresetTours(const std::string& profileToken);

    /// @brief Queries full details and tour spots for a specific preset tour.
    /// @param[in] profileToken Media profile token.
    /// @param[in] tourToken Identifier token of the preset tour.
    /// @return PresetTour struct or nullopt on failure.
    [[nodiscard]] std::optional<PresetTour> getPresetTour(
        const std::string& profileToken, const std::string& tourToken);

    /// @brief Creates a new empty preset tour on the camera.
    /// @param[in] profileToken Media profile token.
    /// @return Assigned preset tour token or nullopt on failure.
    [[nodiscard]] std::optional<std::string> createPresetTour(const std::string& profileToken);

    /// @brief Modifies configuration, dwell times, and preset sequence of a preset tour.
    /// @param[in] profileToken Media profile token.
    /// @param[in] tour Preset tour structure with updated spots.
    /// @return True if modification succeeded.
    bool modifyPresetTour(const std::string& profileToken, const PresetTour& tour);

    /// @brief Controls execution of an ONVIF Preset Tour (Start, Stop, Pause).
    /// @param[in] profileToken Media profile token.
    /// @param[in] tourToken Identifier token of the preset tour.
    /// @param[in] op Operation to execute.
    /// @return True if operation command succeeded.
    bool operatePresetTour(const std::string& profileToken, const std::string& tourToken, PresetTourOperation op);

    /// @brief Deletes a preset tour from camera storage.
    /// @param[in] profileToken Media profile token.
    /// @param[in] tourToken Identifier token of the preset tour.
    /// @return True if deletion succeeded.
    bool removePresetTour(const std::string& profileToken, const std::string& tourToken);

    // =========================================================================
    // Imaging Service (Profile T)
    // =========================================================================

    /// @brief Queries current optical and imaging settings for a video source.
    /// @param[in] videoSourceToken Video source token (e.g. from MediaProfile).
    /// @return ImagingSettings or nullopt on communication/SOAP failure.
    [[nodiscard]] std::optional<ImagingSettings> getImagingSettings(const std::string& videoSourceToken);

    /// @brief Updates optical, color, exposure, and focus settings on the camera.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] settings New imaging parameters to apply.
    /// @param[in] forcePersistence If true, saves changes to non-volatile memory.
    /// @return True if imaging settings update was accepted.
    bool setImagingSettings(
        const std::string& videoSourceToken, const ImagingSettings& settings, bool forcePersistence = true);

    /// @brief Starts continuous motorized optical focus movement.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] speed Normalized focus speed [-1.0 (near) to +1.0 (far)].
    /// @return True if focus move command was accepted.
    bool moveFocus(const std::string& videoSourceToken, float speed);

    /// @brief Stops motorized optical focus movement.
    /// @param[in] videoSourceToken Video source token.
    /// @return True if focus stop command was accepted.
    bool stopFocus(const std::string& videoSourceToken);

    // =========================================================================
    // Event Service (PullPoint - Profile T)
    // =========================================================================

    /// @brief Creates a PullPoint event subscription on the camera.
    /// @return Subscription reference URL or nullopt on failure.
    [[nodiscard]] std::optional<std::string> createPullPointSubscription();

    /// @brief Polls queued notification events from an active subscription.
    /// @param[in] subscriptionUrl Subscription reference URL from createPullPointSubscription.
    /// @param[in] timeoutSeconds Maximum seconds camera holds request before returning.
    /// @param[in] messageLimit Maximum number of event messages to retrieve.
    /// @return Vector of parsed OnvifEvent items.
    [[nodiscard]] std::vector<OnvifEvent> pullMessages(
        const std::string& subscriptionUrl, int timeoutSeconds = 5, int messageLimit = 10);

    /// @brief Unsubscribes and closes an active PullPoint event subscription.
    /// @param[in] subscriptionUrl Subscription reference URL.
    /// @return True if unsubscription succeeded.
    bool unsubscribe(const std::string& subscriptionUrl);

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

    /// @brief Parses GetSnapshotUri XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted snapshot URI string.
    [[nodiscard]] static std::optional<std::string> parseSnapshotUriResponse(const std::string& xml);

    /// @brief Parses GetStatus PTZ XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted PtzStatus.
    [[nodiscard]] static std::optional<PtzStatus> parsePtzStatusResponse(const std::string& xml);

    /// @brief Parses GetPresets PTZ XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of parsed PtzPreset objects.
    [[nodiscard]] static std::vector<PtzPreset> parsePresetsResponse(const std::string& xml);

    /// @brief Parses SetPreset PTZ XML response.
    /// @param[in] xml Raw response XML.
    /// @return Assigned preset token or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseSetPresetResponse(const std::string& xml);

    /// @brief Parses GetPresetTours XML response into list of PresetTour objects.
    /// @param[in] xml Raw response XML.
    /// @return Vector of parsed PresetTour objects.
    [[nodiscard]] static std::vector<PresetTour> parsePresetToursResponse(const std::string& xml);

    /// @brief Parses GetPresetTour XML response into a full PresetTour structure.
    /// @param[in] xml Raw response XML.
    /// @return Extracted PresetTour or nullopt on parse failure.
    [[nodiscard]] static std::optional<PresetTour> parsePresetTourResponse(const std::string& xml);

    /// @brief Parses CreatePresetTour XML response.
    /// @param[in] xml Raw response XML.
    /// @return Assigned tour token or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseCreatePresetTourResponse(const std::string& xml);

    /// @brief Parses SystemReboot Device XML response.
    /// @param[in] xml Raw response XML.
    /// @return Reboot message string or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseSystemRebootResponse(const std::string& xml);

    /// @brief Parses GetImagingSettings XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted ImagingSettings or nullopt on parse failure.
    [[nodiscard]] static std::optional<ImagingSettings> parseImagingSettingsResponse(const std::string& xml);

    /// @brief Parses CreatePullPointSubscription XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted subscription reference URL or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseCreatePullPointSubscriptionResponse(const std::string& xml);

    /// @brief Parses PullMessagesResponse XML response into event list.
    /// @param[in] xml Raw response XML.
    /// @return Vector of parsed OnvifEvent items.
    [[nodiscard]] static std::vector<OnvifEvent> parsePullMessagesResponse(const std::string& xml);

private:
    std::string m_deviceEndpoint {};
    SecurityCredentials m_credentials {};
    OnvifCapabilities m_capabilities {};
    OnvifHttpClient m_httpClient {};
};

} // namespace PelcoD::Onvif
