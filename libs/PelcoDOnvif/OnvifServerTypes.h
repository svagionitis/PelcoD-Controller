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
    std::vector<std::string> scopes { "onvif://www.onvif.org/Profile/S", "onvif://www.onvif.org/Profile/T",
        "onvif://www.onvif.org/Profile/G" };

    /// @brief Default optical and imaging configuration.
    ImagingSettings defaultImagingSettings {};

    /// @brief Default ONVIF user accounts.
    std::vector<OnvifUser> defaultUsers { { "admin", "admin", OnvifUserLevel::Administrator },
        { "operator", "operator", OnvifUserLevel::Operator } };

    /// @brief Default network interfaces.
    std::vector<NetworkInterfaceConfig> defaultNetworkInterfaces { { "eth0", true, "eth0", "00:11:22:33:44:55", 1500,
        { true, true, "192.168.1.100", 24 } } };

    /// @brief Default network default gateway.
    std::string defaultGateway { "192.168.1.1" };

    /// @brief Default DNS configuration.
    DnsConfig defaultDns { true, { "local" }, { "8.8.8.8", "1.1.1.1" } };

    /// @brief Default NTP configuration.
    NtpConfig defaultNtp { true, { "pool.ntp.org" } };

    /// @brief System hostname.
    std::string hostname { "PelcoD-Bridge" };

    /// @brief Default relay outputs for DeviceIO service.
    std::vector<RelayOutputConfig> defaultRelayOutputs { { "Relay_1", RelayMode::Bistable, 0.0f, RelayIdleState::Open,
                                                             RelayLogicalState::Inactive },
        { "Relay_2", RelayMode::Bistable, 0.0f, RelayIdleState::Open, RelayLogicalState::Inactive } };

    /// @brief Default digital inputs for DeviceIO service.
    std::vector<DigitalInputConfig> defaultDigitalInputs { { "Input_1", RelayIdleState::Open, "Alarm", false } };

    /// @brief Default metadata stream configurations (Profile T / Profile M).
    std::vector<MetadataConfiguration> defaultMetadataConfigs { { "MetadataConfig_1", "MetadataConfiguration", 1,
        "PT60S", true, true, true, false } };

    /// @brief Default RTSP or HTTP metadata stream URI.
    std::string metadataStreamUri { "rtsp://127.0.0.1:8554/metadata" };

    /// @brief Default RTSP replay stream URI template (Profile G).
    std::string replayStreamUri { "rtsp://127.0.0.1:8554/onvif/replay" };

    /// @brief Default edge recording containers (Profile G).
    std::vector<RecordingConfig> defaultRecordings { { "Recording_1", "VideoSource_1", "Primary surveillance recording",
        "P30D", { { "Track_Video", RecordingTrackType::Video, "H.264 Video" } } } };

    /// @brief Default recording jobs (Profile G).
    std::vector<RecordingJob> defaultRecordingJobs { { "Job_1", "Recording_1", RecordingJobMode::Active, 1,
        "VideoSource_1" } };

    /// @brief Default installed X.509 certificates.
    std::vector<OnvifCertificate> defaultCertificates {};
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

    /// @brief Queries current focus status and encoder position.
    /// @param[in] videoSourceToken Video source token.
    /// @return Current FocusStatus20 structure.
    [[nodiscard]] virtual FocusStatus20 handleGetFocusStatus(const std::string& /*videoSourceToken*/)
    {
        return FocusStatus20 {};
    }

    /// @brief Moves optical focus with directional or positional mode.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] move Focus movement request.
    /// @return True if focus command was accepted.
    [[nodiscard]] virtual bool handleMoveFocusAdvanced(const std::string& videoSourceToken, const FocusMove& move)
    {
        if (move.mode == FocusMoveMode::Continuous) {
            handleMoveFocus(videoSourceToken, move.continuousSpeed);
            return true;
        }
        return false;
    }

    /// @brief Retrieves list of saved optical imaging presets.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of ImagingPreset.
    [[nodiscard]] virtual std::vector<ImagingPreset> handleGetImagingPresets(const std::string& /*videoSourceToken*/)
    {
        return {};
    }

    /// @brief Recalls and applies an optical imaging preset.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] presetToken Target preset token.
    /// @return True if preset was recalled.
    [[nodiscard]] virtual bool handleSetCurrentImagingPreset(
        const std::string& /*videoSourceToken*/, const std::string& /*presetToken*/)
    {
        return false;
    }
};

/// @class IDeviceIoHandler
/// @brief Abstract interface for decoupling ONVIF DeviceIO & Relay Output requests from hardware.
class IDeviceIoHandler {
public:
    virtual ~IDeviceIoHandler() = default;

    /// @brief Retrieves all configured relay outputs.
    /// @return Vector of RelayOutputConfig.
    [[nodiscard]] virtual std::vector<RelayOutputConfig> handleGetRelayOutputs() = 0;

    /// @brief Retrieves configuration options for a given relay.
    /// @param[in] token Relay token.
    /// @return Vector of supported modes ("Monostable", "Bistable").
    [[nodiscard]] virtual std::vector<std::string> handleGetRelayOutputOptions(const std::string& /*token*/)
    {
        return { "Bistable", "Monostable" };
    }

    /// @brief Applies updated mode, delay time, or idle state to a relay.
    /// @param[in] token Relay token.
    /// @param[in] settings Updated configuration parameters.
    /// @return True if configuration was applied.
    [[nodiscard]] virtual bool handleSetRelayOutputSettings(const std::string& token, const RelayOutputConfig& settings)
        = 0;

    /// @brief Changes the logical state (Active/Inactive) of a relay.
    /// @param[in] token Relay token.
    /// @param[in] state Desired logical state.
    /// @return True if state was changed.
    [[nodiscard]] virtual bool handleSetRelayOutputState(const std::string& token, RelayLogicalState state) = 0;

    /// @brief Retrieves all configured digital inputs.
    /// @return Vector of DigitalInputConfig.
    [[nodiscard]] virtual std::vector<DigitalInputConfig> handleGetDigitalInputs()
    {
        return {};
    }
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

/// @class IOsdHandler
/// @brief Interface receiving ONVIF On-Screen Display (OSD) management events.
class IOsdHandler {
public:
    virtual ~IOsdHandler() = default;

    /// @brief Queries list of all configured OSD overlays for a video source.
    /// @param[in] videoSourceToken Video source token.
    /// @return List of OsdConfig structs.
    [[nodiscard]] virtual std::vector<OsdConfig> handleGetOSDs(const std::string& /*videoSourceToken*/)
    {
        return {};
    }

    /// @brief Queries a specific OSD overlay by token.
    /// @param[in] osdToken OSD identifier token.
    /// @return OsdConfig or nullopt if not found.
    [[nodiscard]] virtual std::optional<OsdConfig> handleGetOSD(const std::string& /*osdToken*/)
    {
        return std::nullopt;
    }

    /// @brief Creates a new OSD overlay configuration.
    /// @param[in] osd OSD configuration parameters.
    /// @return Assigned token string or empty on failure.
    [[nodiscard]] virtual std::string handleCreateOSD(const OsdConfig& /*osd*/)
    {
        return {};
    }

    /// @brief Modifies an existing OSD overlay.
    /// @param[in] osd Updated OSD configuration.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetOSD(const OsdConfig& /*osd*/)
    {
        return false;
    }

    /// @brief Deletes an OSD overlay.
    /// @param[in] osdToken Token of the OSD to delete.
    /// @return True if removed successfully.
    [[nodiscard]] virtual bool handleDeleteOSD(const std::string& /*osdToken*/)
    {
        return false;
    }
};

/// @class IDeviceManagementHandler
/// @brief Abstract interface receiving ONVIF Device Management (/onvif/device_service) requests.
class IDeviceManagementHandler {
public:
    virtual ~IDeviceManagementHandler() = default;

    /// @brief Retrieves list of configured ONVIF user accounts.
    /// @return Vector of OnvifUser records.
    [[nodiscard]] virtual std::vector<OnvifUser> handleGetUsers()
    {
        return {};
    }

    /// @brief Adds new ONVIF user accounts.
    /// @param[in] users Vector of new users to create.
    /// @return True on success.
    [[nodiscard]] virtual bool handleCreateUsers(const std::vector<OnvifUser>& /*users*/)
    {
        return false;
    }

    /// @brief Updates an existing ONVIF user's password and/or role.
    /// @param[in] user Updated user record.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetUser(const OnvifUser& /*user*/)
    {
        return false;
    }

    /// @brief Removes user accounts by username.
    /// @param[in] usernames List of usernames to delete.
    /// @return True on success.
    [[nodiscard]] virtual bool handleDeleteUsers(const std::vector<std::string>& /*usernames*/)
    {
        return false;
    }

    /// @brief Retrieves network adapter interface configurations.
    /// @return Vector of NetworkInterfaceConfig records.
    [[nodiscard]] virtual std::vector<NetworkInterfaceConfig> handleGetNetworkInterfaces()
    {
        return {};
    }

    /// @brief Modifies a network interface configuration.
    /// @param[in] config Updated interface settings.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetNetworkInterfaces(const NetworkInterfaceConfig& /*config*/)
    {
        return false;
    }

    /// @brief Retrieves default network gateway address.
    /// @return Gateway IP address.
    [[nodiscard]] virtual std::string handleGetNetworkDefaultGateway()
    {
        return {};
    }

    /// @brief Sets default network gateway address.
    /// @param[in] gateway Gateway IP address.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetNetworkDefaultGateway(const std::string& /*gateway*/)
    {
        return false;
    }

    /// @brief Retrieves DNS server configuration.
    /// @return DnsConfig structure.
    [[nodiscard]] virtual DnsConfig handleGetDNS()
    {
        return {};
    }

    /// @brief Updates DNS server configuration.
    /// @param[in] dns New DNS settings.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetDNS(const DnsConfig& /*dns*/)
    {
        return false;
    }

    /// @brief Retrieves NTP server configuration.
    /// @return NtpConfig structure.
    [[nodiscard]] virtual NtpConfig handleGetNTP()
    {
        return {};
    }

    /// @brief Updates NTP server configuration.
    /// @param[in] ntp New NTP settings.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetNTP(const NtpConfig& /*ntp*/)
    {
        return false;
    }

    /// @brief Retrieves device hostname.
    /// @return Hostname string.
    [[nodiscard]] virtual std::string handleGetHostname()
    {
        return {};
    }

    /// @brief Updates device hostname.
    /// @param[in] hostname New hostname.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetHostname(const std::string& /*hostname*/)
    {
        return false;
    }

    /// @brief Configures system date, time, and timezone.
    /// @param[in] dt Updated system date and time settings.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetSystemDateAndTime(const SystemDateTimeConfig& /*dt*/)
    {
        return false;
    }

    /// @brief Resets system to factory defaults.
    /// @param[in] type Factory default reset type (Hard or Soft).
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetSystemFactoryDefault(FactoryDefaultType /*type*/)
    {
        return false;
    }

    /// @brief Dispatches system reboot command.
    /// @return Informational message confirming reboot initiation.
    [[nodiscard]] virtual std::string handleSystemReboot()
    {
        return "Rebooting";
    }

    /// @brief Retrieves the requested system or access log content.
    /// @param[in] logType Log type (System or Access).
    /// @return String containing log entries.
    [[nodiscard]] virtual std::string handleGetSystemLog(SystemLogType /*logType*/)
    {
        return {};
    }

    /// @brief Generates comprehensive system and diagnostic support telemetry.
    /// @return SystemSupportInfo structure.
    [[nodiscard]] virtual SystemSupportInfo handleGetSystemSupportInformation()
    {
        return SystemSupportInfo {};
    }

    /// @brief Backs up system settings, returning an archive payload string.
    /// @return Serialized backup payload.
    [[nodiscard]] virtual std::string handleGetSystemBackup()
    {
        return {};
    }

    /// @brief Restores system settings from an archive payload string.
    /// @param[in] backupData Backup payload to restore.
    /// @return True on success.
    [[nodiscard]] virtual bool handleRestoreSystem(const std::string& /*backupData*/)
    {
        return false;
    }

    /// @brief Retrieves device unique endpoint reference URN/UUID.
    /// @return URN endpoint string.
    [[nodiscard]] virtual std::string handleGetEndpointReference()
    {
        return {};
    }

    /// @brief Retrieves list of installed X.509 certificates.
    /// @return Vector of OnvifCertificate records.
    [[nodiscard]] virtual std::vector<OnvifCertificate> handleGetCertificates()
    {
        return {};
    }

    /// @brief Retrieves detailed information for a specific certificate ID.
    /// @param[in] certificateId Certificate identifier.
    /// @return CertificateInformation struct or nullopt if not found.
    [[nodiscard]] virtual std::optional<CertificateInformation> handleGetCertificateInformation(
        const std::string& /*certificateId*/)
    {
        return std::nullopt;
    }

    /// @brief Generates a self-signed X.509 certificate on device.
    /// @param[in] certificateId Desired certificate token.
    /// @param[in] subject Distinguished name.
    /// @param[in] daysValid Validity period in days.
    /// @return Created OnvifCertificate.
    [[nodiscard]] virtual OnvifCertificate handleCreateCertificate(
        const std::string& certificateId, const std::string& subject, int /*daysValid*/ = 365)
    {
        OnvifCertificate cert {};
        cert.certificateId = certificateId;
        cert.info.certificateId = certificateId;
        cert.info.subject = subject.empty() ? ("CN=" + certificateId) : subject;
        cert.info.issuer = cert.info.subject;
        cert.info.keyAlgorithm = "RSA";
        cert.info.isDefault = true;
        return cert;
    }

    /// @brief Generates a PKCS#10 Certificate Signing Request (CSR).
    /// @param[in] certificateId Certificate identifier.
    /// @param[in] subject Subject DN.
    /// @return Pkcs10Request struct with Base64/PEM CSR.
    [[nodiscard]] virtual Pkcs10Request handleGetPkcs10Request(
        const std::string& certificateId, const std::string& subject)
    {
        Pkcs10Request req {};
        req.certificateId = certificateId;
        req.subject = subject.empty() ? ("CN=" + certificateId) : subject;
        req.csrBase64 = "MIIB..." + certificateId;
        return req;
    }

    /// @brief Loads or updates signed certificates onto the device.
    /// @param[in] certificates List of certificates to store.
    /// @return True on success.
    [[nodiscard]] virtual bool handleLoadCertificates(const std::vector<OnvifCertificate>& /*certificates*/)
    {
        return true;
    }

    /// @brief Deletes a certificate by token ID.
    /// @param[in] certificateId Certificate token to delete.
    /// @return True if deleted.
    [[nodiscard]] virtual bool handleDeleteCertificate(const std::string& /*certificateId*/)
    {
        return false;
    }

    /// @brief Queries client certificate authentication mode.
    /// @return ClientCertificateMode enum.
    [[nodiscard]] virtual ClientCertificateMode handleGetClientCertificateMode()
    {
        return ClientCertificateMode::Off;
    }

    /// @brief Configures client certificate authentication mode.
    /// @param[in] mode Desired client certificate mode.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetClientCertificateMode(ClientCertificateMode /*mode*/)
    {
        return true;
    }
};

/// @class IMetadataHandler
/// @brief Abstract interface decoupling ONVIF Metadata Configuration & Streaming from hardware.
class IMetadataHandler {
public:
    virtual ~IMetadataHandler() = default;

    /// @brief Retrieves all metadata configurations.
    /// @return Vector of MetadataConfiguration records.
    [[nodiscard]] virtual std::vector<MetadataConfiguration> handleGetMetadataConfigurations() = 0;

    /// @brief Retrieves a specific metadata configuration by token.
    /// @param[in] token Configuration identifier.
    /// @return MetadataConfiguration or nullopt if not found.
    [[nodiscard]] virtual std::optional<MetadataConfiguration> handleGetMetadataConfiguration(
        const std::string& /*token*/)
    {
        return std::nullopt;
    }

    /// @brief Modifies a metadata configuration.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetMetadataConfiguration(const MetadataConfiguration& /*config*/)
    {
        return false;
    }

    /// @brief Retrieves options and capabilities for metadata configuration.
    /// @param[in] configToken Configuration token.
    /// @param[in] profileToken Profile token.
    /// @return MetadataConfigurationOptions structure.
    [[nodiscard]] virtual MetadataConfigurationOptions handleGetMetadataConfigurationOptions(
        const std::string& /*configToken*/, const std::string& /*profileToken*/ = "")
    {
        return MetadataConfigurationOptions {};
    }

    /// @brief Produces a live snapshot of active metadata (PTZ telemetry, analytics objects, events).
    /// @param[in] profileToken Media profile token.
    /// @return MetadataStreamPayload structure.
    [[nodiscard]] virtual MetadataStreamPayload handleGetCurrentMetadata(const std::string& /*profileToken*/ = "")
    {
        return MetadataStreamPayload {};
    }
};

/// @class IRecordingHandler
/// @brief Abstract interface for handling ONVIF Profile G Recording Service requests.
class IRecordingHandler {
public:
    virtual ~IRecordingHandler() = default;

    /// @brief Retrieves all configured recording containers.
    /// @return Vector of RecordingConfig structures.
    [[nodiscard]] virtual std::vector<RecordingConfig> handleGetRecordings() = 0;

    /// @brief Creates a new edge recording container.
    /// @param[in] config Recording container properties.
    /// @return Assigned recording token string.
    [[nodiscard]] virtual std::string handleCreateRecording(const RecordingConfig& config) = 0;

    /// @brief Deletes a recording container by token.
    /// @param[in] recordingToken Recording token to delete.
    /// @return True on success.
    [[nodiscard]] virtual bool handleDeleteRecording(const std::string& recordingToken) = 0;

    /// @brief Retrieves configuration attributes for a specific recording container.
    /// @param[in] recordingToken Recording token.
    /// @return RecordingConfig or nullopt if not found.
    [[nodiscard]] virtual std::optional<RecordingConfig> handleGetRecordingConfiguration(
        const std::string& recordingToken)
        = 0;

    /// @brief Updates configuration attributes of an existing recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetRecordingConfiguration(
        const std::string& recordingToken, const RecordingConfig& config)
        = 0;

    /// @brief Retrieves aggregated storage volume and time window metrics.
    /// @return RecordingSummary structure.
    [[nodiscard]] virtual RecordingSummary handleGetRecordingSummary() = 0;

    /// @brief Retrieves list of active automated recording jobs.
    /// @return Vector of RecordingJob structures.
    [[nodiscard]] virtual std::vector<RecordingJob> handleGetRecordingJobs() = 0;

    /// @brief Creates a new automated recording job.
    /// @param[in] job Recording job definition.
    /// @return Assigned job token string.
    [[nodiscard]] virtual std::string handleCreateRecordingJob(const RecordingJob& job) = 0;

    /// @brief Deletes a recording job by token.
    /// @param[in] jobToken Job token to delete.
    /// @return True on success.
    [[nodiscard]] virtual bool handleDeleteRecordingJob(const std::string& jobToken) = 0;

    /// @brief Modifies recording job mode (Active / Idle).
    /// @param[in] jobToken Job token.
    /// @param[in] mode Desired mode.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetRecordingJobMode(const std::string& jobToken, RecordingJobMode mode) = 0;

    /// @brief Retrieves all tracks associated with a recording.
    /// @param[in] recordingToken Recording token.
    /// @return Vector of RecordingTrack structures.
    [[nodiscard]] virtual std::vector<RecordingTrack> handleGetTracks(const std::string& recordingToken)
    {
        (void)recordingToken;
        return {};
    }

    /// @brief Creates a track within a recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] track Track parameters.
    /// @return Assigned track token string.
    [[nodiscard]] virtual std::string handleCreateTrack(const std::string& recordingToken, const RecordingTrack& track)
        = 0;

    /// @brief Deletes a track from a recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] trackToken Track token to delete.
    /// @return True on success.
    [[nodiscard]] virtual bool handleDeleteTrack(const std::string& recordingToken, const std::string& trackToken) = 0;
};

/// @class ISearchHandler
/// @brief Abstract interface for handling ONVIF Profile G Search Service requests.
class ISearchHandler {
public:
    virtual ~ISearchHandler() = default;

    /// @brief Initiates historical recording search query.
    /// @param[in] scope Search scope / filter.
    /// @param[in] maxMatches Maximum match count.
    /// @param[in] keepAliveTime Session keepalive string.
    /// @return Unique search session token string.
    [[nodiscard]] virtual std::string handleFindRecordings(
        const std::string& scope, int maxMatches, const std::string& keepAliveTime)
        = 0;

    /// @brief Polls results for an active recording search query.
    /// @param[in] searchToken Search session token.
    /// @return Vector of RecordingSearchResult matches.
    [[nodiscard]] virtual std::vector<RecordingSearchResult> handleGetRecordingSearchResults(
        const std::string& searchToken)
        = 0;

    /// @brief Initiates historical recorded events search query.
    /// @param[in] startUtc Start timestamp (ISO 8601 UTC).
    /// @param[in] endUtc End timestamp (ISO 8601 UTC).
    /// @param[in] maxMatches Maximum match count.
    /// @return Unique search session token string.
    [[nodiscard]] virtual std::string handleFindEvents(
        const std::string& startUtc, const std::string& endUtc, int maxMatches)
        = 0;

    /// @brief Polls results for an active event search query.
    /// @param[in] searchToken Search session token.
    /// @return Vector of RecordedEventResult matches.
    [[nodiscard]] virtual std::vector<RecordedEventResult> handleGetEventSearchResults(const std::string& searchToken)
        = 0;

    /// @brief Closes an active search query session.
    /// @param[in] searchToken Search session token.
    /// @return True on success.
    [[nodiscard]] virtual bool handleEndSearch(const std::string& searchToken) = 0;
};

/// @class IReplayHandler
/// @brief Abstract interface for handling ONVIF Profile G Replay Service requests.
class IReplayHandler {
public:
    virtual ~IReplayHandler() = default;

    /// @brief Generates an RTSP replay URI for playback of a recorded track.
    /// @param[in] recordingToken Target recording token.
    /// @param[in] trackToken Target track token.
    /// @return RTSP stream replay URI.
    [[nodiscard]] virtual std::string handleGetReplayUri(
        const std::string& recordingToken, const std::string& trackToken)
        = 0;

    /// @brief Retrieves current replay session configuration.
    /// @return ReplayConfiguration structure.
    [[nodiscard]] virtual ReplayConfiguration handleGetReplayConfiguration() = 0;

    /// @brief Configures replay session timeouts.
    /// @param[in] config Updated replay parameters.
    /// @return True on success.
    [[nodiscard]] virtual bool handleSetReplayConfiguration(const ReplayConfiguration& config) = 0;
};

} // namespace PelcoD::Onvif
