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
    std::string deviceIoXAddr {}; ///< DeviceIO service endpoint
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
    // Device Management & Security Extensions
    // =========================================================================

    /// @brief Queries list of ONVIF user accounts configured on the camera.
    /// @return Vector of OnvifUser objects.
    [[nodiscard]] std::vector<OnvifUser> getUsers();

    /// @brief Creates one or more new ONVIF user accounts on the device.
    /// @param[in] users Vector of new users to create.
    /// @return True on success.
    bool createUsers(const std::vector<OnvifUser>& users);

    /// @brief Modifies an existing ONVIF user's password and/or authorization level.
    /// @param[in] user Updated user parameters.
    /// @return True on success.
    bool setUser(const OnvifUser& user);

    /// @brief Deletes ONVIF user accounts by username.
    /// @param[in] usernames List of usernames to remove.
    /// @return True on success.
    bool deleteUsers(const std::vector<std::string>& usernames);

    /// @brief Queries list of network adapter interface configurations.
    /// @return Vector of NetworkInterfaceConfig records.
    [[nodiscard]] std::vector<NetworkInterfaceConfig> getNetworkInterfaces();

    /// @brief Updates network interface parameters.
    /// @param[in] config Updated interface settings.
    /// @return True on success.
    bool setNetworkInterfaces(const NetworkInterfaceConfig& config);

    /// @brief Queries default network gateway IPv4 address.
    /// @return Gateway IP address string.
    [[nodiscard]] std::string getNetworkDefaultGateway();

    /// @brief Sets default network gateway address.
    /// @param[in] gateway Gateway IP address string.
    /// @return True on success.
    bool setNetworkDefaultGateway(const std::string& gateway);

    /// @brief Queries DNS server configuration.
    /// @return DnsConfig or nullopt on communication failure.
    [[nodiscard]] std::optional<DnsConfig> getDNS();

    /// @brief Sets DNS server configuration.
    /// @param[in] dns New DNS configuration.
    /// @return True on success.
    bool setDNS(const DnsConfig& dns);

    /// @brief Queries NTP server configuration.
    /// @return NtpConfig or nullopt on communication failure.
    [[nodiscard]] std::optional<NtpConfig> getNTP();

    /// @brief Sets NTP server configuration.
    /// @param[in] ntp New NTP configuration.
    /// @return True on success.
    bool setNTP(const NtpConfig& ntp);

    /// @brief Queries device hostname.
    /// @return Hostname string.
    [[nodiscard]] std::string getHostname();

    /// @brief Sets device hostname.
    /// @param[in] hostname New hostname string.
    /// @return True on success.
    bool setHostname(const std::string& hostname);

    /// @brief Updates camera system date, time, and timezone.
    /// @param[in] dateTime Date and time parameters.
    /// @return True on success.
    bool setSystemDateAndTime(const SystemDateTimeConfig& dateTime);

    /// @brief Resets device to factory default settings.
    /// @param[in] type FactoryDefaultType (Hard or Soft).
    /// @return True on success.
    bool setSystemFactoryDefault(FactoryDefaultType type = FactoryDefaultType::Soft);

    /// @brief Queries device scopes.
    /// @return Vector of scope URIs.
    [[nodiscard]] std::vector<std::string> getScopes();

    /// @brief Adds configurable scopes to device.
    /// @param[in] scopes List of scope URIs to add.
    /// @return True on success.
    bool addScopes(const std::vector<std::string>& scopes);

    /// @brief Removes configurable scopes from device.
    /// @param[in] scopes List of scope URIs to remove.
    /// @return True on success.
    bool removeScopes(const std::vector<std::string>& scopes);

    /// @brief Sets/replaces configurable scopes on device.
    /// @param[in] scopes Complete list of scope URIs.
    /// @return True on success.
    bool setScopes(const std::vector<std::string>& scopes);

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
    // OSD & Media Service Extensions
    // =========================================================================

    /// @brief Queries list of configured OSD overlays for a video source configuration.
    /// @param[in] videoSourceConfigurationToken Configuration token (or empty for all).
    /// @return List of OsdConfig records.
    [[nodiscard]] std::vector<OsdConfig> getOSDs(
        const std::string& videoSourceConfigurationToken = "VideoSourceConfig_1");

    /// @brief Queries a specific OSD overlay by token.
    /// @param[in] osdToken OSD identifier token.
    /// @return OsdConfig or nullopt on failure.
    [[nodiscard]] std::optional<OsdConfig> getOSD(const std::string& osdToken);

    /// @brief Creates a new OSD overlay configuration on camera.
    /// @param[in] osd OSD configuration parameters.
    /// @return Assigned token string or empty on failure.
    [[nodiscard]] std::string createOSD(const OsdConfig& osd);

    /// @brief Modifies an existing OSD overlay.
    /// @param[in] osd Updated OSD configuration.
    /// @return True on success.
    bool setOSD(const OsdConfig& osd);

    /// @brief Deletes an OSD overlay.
    /// @param[in] osdToken Token of the OSD to delete.
    /// @return True if removed successfully.
    bool deleteOSD(const std::string& osdToken);

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

    /// @brief Sends an ONVIF auxiliary command (wiper, washer, IR, aux relays).
    /// @param[in] profileToken Media profile token.
    /// @param[in] auxiliaryData Auxiliary command string token (e.g. "tt:Wiper|On", "Aux1On").
    /// @return Returned auxiliary response string or nullopt on failure.
    [[nodiscard]] std::optional<std::string> sendAuxiliaryCommand(
        const std::string& profileToken, const std::string& auxiliaryData);

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

    /// @brief Queries current focus status and encoder position.
    /// @param[in] videoSourceToken Video source token.
    /// @return Current FocusStatus20 or nullopt on failure.
    [[nodiscard]] std::optional<FocusStatus20> getFocusStatus(const std::string& videoSourceToken);

    /// @brief Moves optical focus using continuous velocity.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] speed Speed ratio [-1.0 to 1.0].
    /// @return True if command was accepted.
    bool moveFocusContinuous(const std::string& videoSourceToken, float speed);

    /// @brief Moves optical focus to absolute position.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] position Target position [0.0 to 1.0].
    /// @param[in] speed Velocity ratio [0.0 to 1.0].
    /// @return True if command was accepted.
    bool moveFocusAbsolute(const std::string& videoSourceToken, float position, float speed = 1.0f);

    /// @brief Moves optical focus relative to current position.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] distance Displacement [-1.0 to 1.0].
    /// @param[in] speed Velocity ratio [0.0 to 1.0].
    /// @return True if command was accepted.
    bool moveFocusRelative(const std::string& videoSourceToken, float distance, float speed = 1.0f);

    /// @brief Retrieves list of saved optical imaging presets.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of ImagingPreset.
    [[nodiscard]] std::vector<ImagingPreset> getImagingPresets(const std::string& videoSourceToken);

    /// @brief Recalls an optical imaging preset.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] presetToken Target preset token.
    /// @return True if preset recalled.
    bool setCurrentImagingPreset(const std::string& videoSourceToken, const std::string& presetToken);

    // =========================================================================
    // Device I/O & Relay Outputs Service (Profile S & T)
    // =========================================================================

    /// @brief Queries all configured relay outputs.
    /// @return Vector of RelayOutputConfig.
    [[nodiscard]] std::vector<RelayOutputConfig> getRelayOutputs();

    /// @brief Queries configuration options for a relay output.
    /// @param[in] relayToken Relay token (e.g. "Relay_1").
    /// @return Vector of supported relay modes.
    [[nodiscard]] std::vector<std::string> getRelayOutputOptions(const std::string& relayToken);

    /// @brief Configures relay output parameters (mode, delay, idle state).
    /// @param[in] relayToken Target relay token.
    /// @param[in] settings Updated configuration.
    /// @return True if update accepted.
    bool setRelayOutputSettings(const std::string& relayToken, const RelayOutputConfig& settings);

    /// @brief Changes the logical state of a relay output.
    /// @param[in] relayToken Target relay token.
    /// @param[in] state Target state (Active or Inactive).
    /// @return True if state change accepted.
    bool setRelayOutputState(const std::string& relayToken, RelayLogicalState state);

    /// @brief Queries all digital inputs.
    /// @return Vector of DigitalInputConfig.
    [[nodiscard]] std::vector<DigitalInputConfig> getDigitalInputs();

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
    // Metadata Service (Profile T & M)
    // =========================================================================

    /// @brief Queries list of all Metadata Configurations available on device.
    /// @return Vector of MetadataConfiguration structures.
    [[nodiscard]] std::vector<MetadataConfiguration> getMetadataConfigurations();

    /// @brief Queries a specific Metadata Configuration by token.
    /// @param[in] configToken Configuration token.
    /// @return MetadataConfiguration structure or nullopt on failure.
    [[nodiscard]] std::optional<MetadataConfiguration> getMetadataConfiguration(const std::string& configToken);

    /// @brief Updates parameters of an existing Metadata Configuration.
    /// @param[in] config Updated MetadataConfiguration structure.
    /// @return True on success.
    bool setMetadataConfiguration(const MetadataConfiguration& config);

    /// @brief Queries metadata streaming capability options.
    /// @param[in] configToken Configuration token.
    /// @param[in] profileToken Profile token (optional).
    /// @return MetadataConfigurationOptions or nullopt on failure.
    [[nodiscard]] std::optional<MetadataConfigurationOptions> getMetadataConfigurationOptions(
        const std::string& configToken, const std::string& profileToken = "");

    /// @brief Fetches live snapshot of current ONVIF metadata stream (telemetry, detected objects, events).
    /// @param[in] streamUri Direct HTTP metadata stream URI (e.g. /onvif/metadata_stream).
    /// @return MetadataStreamPayload or nullopt on failure.
    [[nodiscard]] std::optional<MetadataStreamPayload> getMetadataStream(const std::string& streamUri = "");

    // =========================================================================
    // System Maintenance & Device Service Extensions
    // =========================================================================

    /// @brief Queries system or access logs from camera.
    /// @param[in] logType Type of log (System or Access).
    /// @return String containing log entries or nullopt on failure.
    [[nodiscard]] std::optional<std::string> getSystemLog(SystemLogType logType);

    /// @brief Retrieves detailed system diagnostics and support information.
    /// @return SystemSupportInfo or nullopt on failure.
    [[nodiscard]] std::optional<SystemSupportInfo> getSystemSupportInformation();

    /// @brief Downloads a system backup archive from device.
    /// @return Serialized backup payload string or nullopt on failure.
    [[nodiscard]] std::optional<std::string> getSystemBackup();

    /// @brief Restores system settings on device using a backup archive payload.
    /// @param[in] backupData Backup archive payload.
    /// @return True on success.
    bool restoreSystem(const std::string& backupData);

    /// @brief Queries unique endpoint reference identifier GUID/UUID from device.
    /// @return Endpoint reference string or nullopt on failure.
    [[nodiscard]] std::optional<std::string> getEndpointReference();

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

    /// @brief Parses SendAuxiliaryCommand XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted auxiliary response string or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseSendAuxiliaryCommandResponse(const std::string& xml);

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

    /// @brief Parses GetOSDs XML response into list of OsdConfig objects.
    /// @param[in] xml Raw response XML.
    /// @return Vector of parsed OsdConfig objects.
    [[nodiscard]] static std::vector<OsdConfig> parseOsdListResponse(const std::string& xml);

    /// @brief Parses GetOSD XML response into an OsdConfig object.
    /// @param[in] xml Raw response XML.
    /// @return Extracted OsdConfig or nullopt on parse failure.
    [[nodiscard]] static std::optional<OsdConfig> parseOsdResponse(const std::string& xml);

    /// @brief Parses CreateOSD XML response.
    /// @param[in] xml Raw response XML.
    /// @return Assigned OSD token or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseCreateOsdResponse(const std::string& xml);

    /// @brief Parses GetUsers XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of OnvifUser records.
    [[nodiscard]] static std::vector<OnvifUser> parseUsersResponse(const std::string& xml);

    /// @brief Parses GetNetworkInterfaces XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of NetworkInterfaceConfig records.
    [[nodiscard]] static std::vector<NetworkInterfaceConfig> parseNetworkInterfacesResponse(const std::string& xml);

    /// @brief Parses GetNetworkDefaultGateway XML response.
    /// @param[in] xml Raw response XML.
    /// @return Gateway address string.
    [[nodiscard]] static std::string parseNetworkDefaultGatewayResponse(const std::string& xml);

    /// @brief Parses GetDNS XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted DnsConfig or nullopt on failure.
    [[nodiscard]] static std::optional<DnsConfig> parseDnsResponse(const std::string& xml);

    /// @brief Parses GetNTP XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted NtpConfig or nullopt on failure.
    [[nodiscard]] static std::optional<NtpConfig> parseNtpResponse(const std::string& xml);

    /// @brief Parses GetHostname XML response.
    /// @param[in] xml Raw response XML.
    /// @return Hostname string.
    [[nodiscard]] static std::string parseHostnameResponse(const std::string& xml);

    /// @brief Parses GetScopes XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of scope URIs.
    [[nodiscard]] static std::vector<std::string> parseScopesResponse(const std::string& xml);

    /// @brief Parses GetStatus (FocusStatus20) XML response.
    /// @param[in] xml Raw response XML.
    /// @return FocusStatus20 or nullopt.
    [[nodiscard]] static std::optional<FocusStatus20> parseFocusStatusResponse(const std::string& xml);

    /// @brief Parses GetPresets XML response for imaging presets.
    /// @param[in] xml Raw response XML.
    /// @return Vector of ImagingPreset.
    [[nodiscard]] static std::vector<ImagingPreset> parseImagingPresetsResponse(const std::string& xml);

    /// @brief Parses GetRelayOutputs XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of RelayOutputConfig.
    [[nodiscard]] static std::vector<RelayOutputConfig> parseRelayOutputsResponse(const std::string& xml);

    /// @brief Parses GetDigitalInputs XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of DigitalInputConfig.
    [[nodiscard]] static std::vector<DigitalInputConfig> parseDigitalInputsResponse(const std::string& xml);

    /// @brief Parses GetMetadataConfigurations XML response.
    /// @param[in] xml Raw response XML.
    /// @return Vector of MetadataConfiguration structures.
    [[nodiscard]] static std::vector<MetadataConfiguration> parseMetadataConfigurationsResponse(const std::string& xml);

    /// @brief Parses GetMetadataConfiguration XML response.
    /// @param[in] xml Raw response XML.
    /// @return MetadataConfiguration or nullopt on failure.
    [[nodiscard]] static std::optional<MetadataConfiguration> parseMetadataConfigurationResponse(
        const std::string& xml);

    /// @brief Parses GetMetadataConfigurationOptions XML response.
    /// @param[in] xml Raw response XML.
    /// @return MetadataConfigurationOptions or nullopt on failure.
    [[nodiscard]] static std::optional<MetadataConfigurationOptions> parseMetadataConfigurationOptionsResponse(
        const std::string& xml);

    /// @brief Parses Profile M / Profile T MetadataStream XML document.
    /// @param[in] xml Raw response XML.
    /// @return MetadataStreamPayload with extracted PTZ status, detected objects, and events.
    [[nodiscard]] static std::optional<MetadataStreamPayload> parseMetadataStreamResponse(const std::string& xml);

    /// @brief Parses GetSystemLog XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted log text or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseSystemLogResponse(const std::string& xml);

    /// @brief Parses GetSystemSupportInformation XML response.
    /// @param[in] xml Raw response XML.
    /// @return Extracted SystemSupportInfo or nullopt on failure.
    [[nodiscard]] static std::optional<SystemSupportInfo> parseSystemSupportInformationResponse(const std::string& xml);

    /// @brief Parses GetSystemBackup XML response.
    /// @param[in] xml Raw response XML.
    /// @return Backup archive payload string or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseSystemBackupResponse(const std::string& xml);

    /// @brief Parses GetEndpointReference XML response.
    /// @param[in] xml Raw response XML.
    /// @return Endpoint GUID string or nullopt on failure.
    [[nodiscard]] static std::optional<std::string> parseEndpointReferenceResponse(const std::string& xml);

private:
    std::string m_deviceEndpoint {};
    SecurityCredentials m_credentials {};
    OnvifCapabilities m_capabilities {};
    OnvifHttpClient m_httpClient {};
};

} // namespace PelcoD::Onvif
