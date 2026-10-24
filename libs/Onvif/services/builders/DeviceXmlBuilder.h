#pragma once

/// @file DeviceXmlBuilder.h
/// @brief Serializer for ONVIF Device Service SOAP XML responses.

#include <cstddef>
#include <cstdint>

#include <deque>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"
#include "OnvifTypes.h"

namespace Onvif {

/// @class DeviceXmlBuilder
/// @brief Formatter for ONVIF Device Management (tds) XML response payloads.
class DeviceXmlBuilder {
public:
    DeviceXmlBuilder() = default;
    ~DeviceXmlBuilder() = default;

    /// @brief Serializes GetDeviceInformationResponse XML.
    [[nodiscard]] static std::string buildDeviceInfoXml(const OnvifServerConfig& config);

    /// @brief Serializes GetSystemDateAndTimeResponse XML.
    [[nodiscard]] static std::string buildSysDateXml(const SystemDateTimeConfig& dt);

    /// @brief Serializes GetCapabilitiesResponse XML.
    [[nodiscard]] static std::string buildCapsXml(const OnvifServerConfig& config, const std::string& host,
        std::int32_t port, std::size_t numRelays, const std::vector<std::string>& categories,
        bool thermalSupported = false, bool profileGSupported = false);

    /// @brief Serializes GetScopesResponse XML.
    [[nodiscard]] static std::string buildScopesXml(
        const std::string& devName, const std::string& model, const std::vector<std::string>& configurableScopes);

    /// @brief Serializes GetUsersResponse XML.
    [[nodiscard]] static std::string buildUsersXml(const std::vector<OnvifUser>& users);

    /// @brief Serializes GetNetworkInterfacesResponse XML.
    [[nodiscard]] static std::string buildNetIfacesXml(const std::vector<NetworkInterfaceConfig>& ifaces);

    /// @brief Serializes GetNetworkProtocolsResponse XML.
    [[nodiscard]] static std::string buildNetProtosXml(const std::vector<NetworkProtocolConfig>& protos);

    /// @brief Serializes GetNetworkDefaultGatewayResponse XML.
    [[nodiscard]] static std::string buildGatewayXml(const std::string& gateway);

    /// @brief Serializes GetDNSResponse XML.
    [[nodiscard]] static std::string buildDnsXml(const DnsConfig& dns);

    /// @brief Serializes GetNTPResponse XML.
    [[nodiscard]] static std::string buildNtpXml(const NtpConfig& ntp);

    /// @brief Serializes GetHostnameResponse XML.
    [[nodiscard]] static std::string buildHostnameXml(const std::string& hostname);

    /// @brief Serializes GetSystemLogResponse XML.
    [[nodiscard]] static std::string buildSysLogsXml(const std::string& logData);

    /// @brief Serializes GetCertificatesResponse XML.
    [[nodiscard]] static std::string buildCertsXml(const std::vector<OnvifCertificate>& certs);

    /// @brief Serializes GetCertificateInformationResponse XML.
    [[nodiscard]] static std::string buildCertInfoXml(const std::string& certId, const CertificateInformation& info);

    /// @brief Serializes GetGeoLocationResponse XML.
    [[nodiscard]] static std::string buildGeoLocationXml(const LocationEntity& loc);
};

} // namespace Onvif
