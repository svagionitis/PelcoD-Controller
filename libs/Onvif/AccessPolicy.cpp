/// @file AccessPolicy.cpp
/// @brief ONVIF operation-to-access-class tables and the user-level permission matrix.

#include "AccessPolicy.h"

#include <array>
#include <cstddef>

namespace Onvif {

namespace {

    /// @brief One (operation, access class) table row.
    struct OpRule {
        std::string_view op {};
        AccessClass cls { AccessClass::Unrecoverable };
    };

    using C = AccessClass;

    // ONVIF Core §5.9.4 / service specifications. Keep rows grouped by class for review.
    constexpr std::array<OpRule, 45U> kDevice { {
        { "GetSystemDateAndTime", C::PreAuth },
        { "GetCapabilities", C::PreAuth },
        { "GetServices", C::PreAuth },
        { "GetWsdlUrl", C::PreAuth },
        { "GetHostname", C::PreAuth },
        { "GetEndpointReference", C::PreAuth },
        { "GetDeviceInformation", C::ReadSystem },
        { "GetScopes", C::ReadSystem },
        { "GetNetworkInterfaces", C::ReadSystem },
        { "GetNetworkDefaultGateway", C::ReadSystem },
        { "GetDNS", C::ReadSystem },
        { "GetNTP", C::ReadSystem },
        { "GetGeoLocation", C::ReadSystem },
        { "GetVideoSourceModes", C::ReadSystem },
        { "GetCertificates", C::ReadSystem },
        { "GetCertificateInformation", C::ReadSystem },
        { "GetClientCertificateMode", C::ReadSystem },
        { "GetUsers", C::ReadSystemSensitive },
        { "GetSystemLog", C::ReadSystemSecret },
        { "GetSystemSupportInformation", C::ReadSystemSecret },
        { "GetSystemBackup", C::ReadSystemSecret },
        { "SetSystemDateAndTime", C::WriteSystem },
        { "AddScopes", C::WriteSystem },
        { "RemoveScopes", C::WriteSystem },
        { "SetScopes", C::WriteSystem },
        { "CreateUsers", C::WriteSystem },
        { "SetUser", C::WriteSystem },
        { "DeleteUsers", C::WriteSystem },
        { "SetNetworkInterfaces", C::WriteSystem },
        { "SetNetworkDefaultGateway", C::WriteSystem },
        { "SetDNS", C::WriteSystem },
        { "SetNTP", C::WriteSystem },
        { "SetHostname", C::WriteSystem },
        { "CreateCertificate", C::WriteSystem },
        { "GetPkcs10Request", C::WriteSystem },
        { "LoadCertificates", C::WriteSystem },
        { "DeleteCertificates", C::WriteSystem },
        { "SetClientCertificateMode", C::WriteSystem },
        { "SetGeoLocation", C::WriteSystem },
        { "DeleteGeoLocation", C::WriteSystem },
        { "SetVideoSourceMode", C::WriteSystem },
        { "SetSystemFactoryDefault", C::Unrecoverable },
        { "SystemReboot", C::Unrecoverable },
        { "RestoreSystem", C::Unrecoverable },
        { "UpgradeSystemFirmware", C::Unrecoverable },
    } };

    // Shared by Media (ver10) and Media2 (ver20).
    constexpr std::array<OpRule, 32U> kMedia { {
        { "GetProfiles", C::ReadMedia },
        { "GetVideoSources", C::ReadMedia },
        { "GetStreamUri", C::ReadMedia },
        { "GetSnapshotUri", C::ReadMedia },
        { "GetOSDOptions", C::ReadMedia },
        { "GetOSDs", C::ReadMedia },
        { "GetOSD", C::ReadMedia },
        { "GetMetadataConfigurations", C::ReadMedia },
        { "GetMetadataConfiguration", C::ReadMedia },
        { "GetMetadataConfigurationOptions", C::ReadMedia },
        { "GetCompatibleMetadataConfigurations", C::ReadMedia },
        { "GetVideoEncoderConfigurations", C::ReadMedia },
        { "GetVideoEncoderConfigurationOptions", C::ReadMedia },
        { "GetMaskOptions", C::ReadMedia },
        { "GetMasks", C::ReadMedia },
        { "GetMask", C::ReadMedia },
        { "GetVideoSourceModes", C::ReadMedia },
        { "CreateOSD", C::Actuate },
        { "SetOSD", C::Actuate },
        { "DeleteOSD", C::Actuate },
        { "SetMetadataConfiguration", C::Actuate },
        { "CreateMask", C::Actuate },
        { "SetMask", C::Actuate },
        { "DeleteMask", C::Actuate },
        { "SetVideoSourceMode", C::Actuate },
        { "CreateProfile", C::Actuate },
        { "DeleteProfile", C::Actuate },
        { "AddConfiguration", C::Actuate },
        { "RemoveConfiguration", C::Actuate },
        { "SetVideoEncoderConfiguration", C::Actuate },
        { "StartMulticastStreaming", C::Actuate },
        { "StopMulticastStreaming", C::Actuate },
    } };

    constexpr std::array<OpRule, 25U> kPtz { {
        { "GetConfigurationOptions", C::ReadMedia },
        { "GetConfigurations", C::ReadMedia },
        { "GetConfiguration", C::ReadMedia },
        { "GetNodes", C::ReadMedia },
        { "GetNode", C::ReadMedia },
        { "GetPresets", C::ReadMedia },
        { "GetStatus", C::ReadMedia },
        { "GetPresetTours", C::ReadMedia },
        { "GetPresetTourOptions", C::ReadMedia },
        { "GetPresetTour", C::ReadMedia },
        { "ContinuousMove", C::Actuate },
        { "Stop", C::Actuate },
        { "AbsoluteMove", C::Actuate },
        { "GeoMove", C::Actuate },
        { "RelativeMove", C::Actuate },
        { "GotoHomePosition", C::Actuate },
        { "SetHomePosition", C::Actuate },
        { "SendAuxiliaryCommand", C::Actuate },
        { "SetPreset", C::Actuate },
        { "GotoPreset", C::Actuate },
        { "RemovePreset", C::Actuate },
        { "RemovePresetTour", C::Actuate },
        { "CreatePresetTour", C::Actuate },
        { "ModifyPresetTour", C::Actuate },
        { "OperatePresetTour", C::Actuate },
    } };

    constexpr std::array<OpRule, 10U> kImaging { {
        { "GetImagingSettings", C::ReadMedia },
        { "GetStatus", C::ReadMedia },
        { "GetOptions", C::ReadMedia },
        { "GetMoveOptions", C::ReadMedia },
        { "GetPresets", C::ReadMedia },
        { "GetCurrentPreset", C::ReadMedia },
        { "SetImagingSettings", C::Actuate },
        { "Move", C::Actuate },
        { "Stop", C::Actuate },
        { "SetCurrentPreset", C::Actuate },
    } };

    constexpr std::array<OpRule, 10U> kDeviceIo { {
        { "GetRelayOutputs", C::ReadMedia },
        { "GetRelayOutputOptions", C::ReadMedia },
        { "GetDigitalInputs", C::ReadMedia },
        { "GetDigitalInputConfigurationOptions", C::ReadMedia },
        { "GetVideoSources", C::ReadMedia },
        { "GetVideoOutputs", C::ReadMedia },
        { "GetAudioSources", C::ReadMedia },
        { "GetAudioOutputs", C::ReadMedia },
        { "SetRelayOutputSettings", C::Actuate },
        { "SetRelayOutputState", C::Actuate },
    } };

    constexpr std::array<OpRule, 3U> kEvents { {
        { "CreatePullPointSubscription", C::ReadMedia },
        { "Subscribe", C::ReadMedia },
        { "GetEventProperties", C::ReadMedia },
    } };

    constexpr std::array<OpRule, 4U> kPullPoint { {
        { "PullMessages", C::ReadMedia },
        { "Unsubscribe", C::ReadMedia },
        { "Renew", C::ReadMedia },
        { "SetSynchronizationPoint", C::ReadMedia },
    } };

    constexpr std::array<OpRule, 10U> kAnalytics { {
        { "GetSupportedRules", C::ReadMedia },
        { "GetRules", C::ReadMedia },
        { "GetSupportedAnalyticsModules", C::ReadMedia },
        { "GetAnalyticsModules", C::ReadMedia },
        { "CreateRules", C::Actuate },
        { "ModifyRules", C::Actuate },
        { "DeleteRules", C::Actuate },
        { "CreateAnalyticsModules", C::Actuate },
        { "ModifyAnalyticsModules", C::Actuate },
        { "DeleteAnalyticsModules", C::Actuate },
    } };

    constexpr std::array<OpRule, 13U> kRecording { {
        { "GetRecordings", C::ReadMedia },
        { "GetRecordingConfiguration", C::ReadMedia },
        { "GetTrackConfiguration", C::ReadMedia },
        { "GetRecordingJobs", C::ReadMedia },
        { "GetRecordingSummary", C::ReadMedia },
        { "CreateRecording", C::Actuate },
        { "SetRecordingConfiguration", C::Actuate },
        { "DeleteRecording", C::Actuate },
        { "CreateTrack", C::Actuate },
        { "DeleteTrack", C::Actuate },
        { "CreateRecordingJob", C::Actuate },
        { "SetRecordingJobMode", C::Actuate },
        { "DeleteRecordingJob", C::Actuate },
    } };

    constexpr std::array<OpRule, 8U> kSearch { {
        { "FindRecordings", C::ReadMedia },
        { "GetRecordingSearchResults", C::ReadMedia },
        { "FindEvents", C::ReadMedia },
        { "GetEventSearchResults", C::ReadMedia },
        { "EndSearch", C::ReadMedia },
        { "GetRecordingSummary", C::ReadMedia },
        { "GetRecordingInformation", C::ReadMedia },
        { "GetMediaAttributes", C::ReadMedia },
    } };

    constexpr std::array<OpRule, 3U> kReplay { {
        { "GetReplayUri", C::ReadMedia },
        { "GetReplayConfiguration", C::ReadMedia },
        { "SetReplayConfiguration", C::Actuate },
    } };

    constexpr std::array<OpRule, 11U> kThermal { {
        { "GetRadiometryConfigurationOptions", C::ReadMedia },
        { "GetRadiometryConfiguration", C::ReadMedia },
        { "GetRadiometrySpots", C::ReadMedia },
        { "GetRadiometryBoxes", C::ReadMedia },
        { "GetColorPalettes", C::ReadMedia },
        { "SetRadiometryConfiguration", C::Actuate },
        { "SetRadiometrySpots", C::Actuate },
        { "SetRadiometryBoxes", C::Actuate },
        { "SetColorPalette", C::Actuate },
        { "TriggerNUC", C::Actuate },
        { "ManualNUC", C::Actuate },
    } };

    /// @brief Looks up an operation in a service table (fail closed).
    template <std::size_t N>
    [[nodiscard]] constexpr AccessClass lookup(const std::array<OpRule, N>& table, std::string_view op) noexcept
    {
        for (const OpRule& rule : table) {
            if (rule.op == op) {
                return rule.cls;
            }
        }
        return C::Unrecoverable;
    }

    /// @brief True if every row names an operation (guards against over-sized std::array bounds,
    ///        whose value-initialised rows would otherwise default to AccessClass::PreAuth).
    template <std::size_t N>
    [[nodiscard]] constexpr bool allRowsFilled(const std::array<OpRule, N>& table) noexcept
    {
        for (const OpRule& rule : table) {
            if (rule.op.empty()) {
                return false;
            }
        }
        return true;
    }

    static_assert(allRowsFilled(kDevice) && allRowsFilled(kMedia) && allRowsFilled(kPtz) && allRowsFilled(kImaging)
            && allRowsFilled(kDeviceIo) && allRowsFilled(kEvents) && allRowsFilled(kPullPoint)
            && allRowsFilled(kAnalytics) && allRowsFilled(kRecording) && allRowsFilled(kSearch)
            && allRowsFilled(kReplay) && allRowsFilled(kThermal),
        "AccessPolicy table size does not match its row count");

    /// @brief Strips an XML namespace prefix ("tptz:Stop" -> "Stop").
    [[nodiscard]] constexpr std::string_view stripPrefix(std::string_view op) noexcept
    {
        const std::size_t colon { op.find(':') };
        return (colon == std::string_view::npos) ? op : op.substr(colon + 1U);
    }

    /// @brief Dispatches to the service's table; unknown services are fail-closed.
    [[nodiscard]] AccessClass lookupService(std::string_view service, std::string_view op) noexcept
    {
        AccessClass cls { C::Unrecoverable };
        if (service == "Device") {
            cls = lookup(kDevice, op);
        } else if ((service == "Media") || (service == "Media2")) {
            cls = lookup(kMedia, op);
        } else if (service == "PTZ") {
            cls = lookup(kPtz, op);
        } else if (service == "Imaging") {
            cls = lookup(kImaging, op);
        } else if (service == "DeviceIO") {
            cls = lookup(kDeviceIo, op);
        } else if (service == "Events") {
            cls = lookup(kEvents, op);
        } else if (service == "PullPoint") {
            cls = lookup(kPullPoint, op);
        } else if (service == "Analytics") {
            cls = lookup(kAnalytics, op);
        } else if (service == "Recording") {
            cls = lookup(kRecording, op);
        } else if (service == "Search") {
            cls = lookup(kSearch, op);
        } else if (service == "Replay") {
            cls = lookup(kReplay, op);
        } else if (service == "Thermal") {
            cls = lookup(kThermal, op);
        } else {
            return C::Unrecoverable;
        }
        // GetServiceCapabilities is PRE_AUTH on every ONVIF service.
        if ((cls == C::Unrecoverable) && (op == "GetServiceCapabilities")) {
            cls = C::PreAuth;
        }
        return cls;
    }

} // namespace

AccessClass AccessPolicy::classify(std::string_view service, std::string_view operation) noexcept
{
    const std::string_view op { stripPrefix(operation) };
    if (op.empty()) {
        return C::Unrecoverable;
    }
    return lookupService(service, op);
}

bool AccessPolicy::permits(OnvifUserLevel level, AccessClass cls) noexcept
{
    bool allowed { false };
    switch (level) {
    case OnvifUserLevel::Administrator:
        allowed = true;
        break;
    case OnvifUserLevel::Operator:
        allowed = (cls != C::ReadSystemSecret) && (cls != C::WriteSystem) && (cls != C::Unrecoverable);
        break;
    case OnvifUserLevel::User:
        allowed = (cls == C::PreAuth) || (cls == C::ReadSystem) || (cls == C::ReadMedia);
        break;
    case OnvifUserLevel::Anonymous:
    case OnvifUserLevel::Extended:
    default:
        allowed = (cls == C::PreAuth);
        break;
    }
    return allowed;
}

} // namespace Onvif
