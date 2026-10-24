/// @file DeviceXmlBuilder.cpp
#include "DeviceXmlBuilder.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "XmlUtils.h"

namespace Onvif {

std::string DeviceXmlBuilder::buildDeviceInfoXml(const OnvifServerConfig& config)
{
    std::ostringstream ss;
    ss << "    <tds:GetDeviceInformationResponse>\r\n"
       << "      <tds:Manufacturer>" << Xml::escapeXmlText(config.manufacturer) << "</tds:Manufacturer>\r\n"
       << "      <tds:Model>" << Xml::escapeXmlText(config.model) << "</tds:Model>\r\n"
       << "      <tds:FirmwareVersion>" << Xml::escapeXmlText(config.firmwareVersion) << "</tds:FirmwareVersion>\r\n"
       << "      <tds:SerialNumber>" << Xml::escapeXmlText(config.serialNumber) << "</tds:SerialNumber>\r\n"
       << "      <tds:HardwareId>" << Xml::escapeXmlText(config.hardwareId) << "</tds:HardwareId>\r\n"
       << "    </tds:GetDeviceInformationResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildSysDateXml(const SystemDateTimeConfig& dt)
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm utcTm { };
#ifdef _WIN32
    gmtime_s(&utcTm, &nowTime);
#else
    gmtime_r(&nowTime, &utcTm);
#endif
    const std::string dtType = dt.dateTimeType.empty() ? "Manual" : dt.dateTimeType;
    const std::string tzStr = dt.timeZone.empty() ? "UTC" : dt.timeZone;
    const bool dst = dt.daylightSavings;

    std::ostringstream ss;
    ss << "    <tds:GetSystemDateAndTimeResponse>\r\n"
       << "      <tds:SystemDateAndTime>\r\n"
       << "        <tt:DateTimeType>" << dtType << "</tt:DateTimeType>\r\n"
       << "        <tt:DaylightSavings>" << (dst ? "true" : "false") << "</tt:DaylightSavings>\r\n"
       << "        <tt:TimeZone><tt:TZ>" << tzStr << "</tt:TZ></tt:TimeZone>\r\n"
       << "        <tt:UTCDateTime>\r\n"
       << "          <tt:Time><tt:Hour>" << utcTm.tm_hour << "</tt:Hour><tt:Minute>" << utcTm.tm_min
       << "</tt:Minute><tt:Second>" << utcTm.tm_sec << "</tt:Second></tt:Time>\r\n"
       << "          <tt:Date><tt:Year>" << (utcTm.tm_year + 1900) << "</tt:Year><tt:Month>" << (utcTm.tm_mon + 1)
       << "</tt:Month><tt:Day>" << utcTm.tm_mday << "</tt:Day></tt:Date>\r\n"
       << "        </tt:UTCDateTime>\r\n"
       << "      </tds:SystemDateAndTime>\r\n"
       << "    </tds:GetSystemDateAndTimeResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildCapsXml(const OnvifServerConfig& config, const std::string& host, int port,
    std::size_t numRelays, const std::vector<std::string>& categories, bool thermalSupported, bool profileGSupported)
{
    static_cast<void>(config);
    const bool hasFilter = !categories.empty();
    const bool includeAll = !hasFilter || (std::find(categories.begin(), categories.end(), "All") != categories.end());

    auto shouldInclude = [&](const std::string& cat) {
        if (includeAll) {
            return true;
        }
        return std::find(categories.begin(), categories.end(), cat) != categories.end();
    };

    const std::string safeHost = Xml::escapeXml(host);
    std::ostringstream ss;
    ss << "    <tds:GetCapabilitiesResponse>\r\n"
       << "      <tds:Capabilities>\r\n";

    if (shouldInclude("Device")) {
        ss << "        <tt:Device>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/device_service</tt:XAddr>\r\n"
           << "          <tt:Network>\r\n"
           << "            <tt:IPFilter>false</tt:IPFilter>\r\n"
           << "            <tt:ZeroConfiguration>false</tt:ZeroConfiguration>\r\n"
           << "            <tt:IPVersion6>false</tt:IPVersion6>\r\n"
           << "            <tt:DynDNS>false</tt:DynDNS>\r\n"
           << "          </tt:Network>\r\n"
           << "          <tt:System>\r\n"
           << "            <tt:DiscoveryResolve>false</tt:DiscoveryResolve>\r\n"
           << "            <tt:DiscoveryBye>true</tt:DiscoveryBye>\r\n"
           << "            <tt:RemoteDiscovery>false</tt:RemoteDiscovery>\r\n"
           << "            <tt:SystemBackup>true</tt:SystemBackup>\r\n"
           << "            <tt:SystemLogging>true</tt:SystemLogging>\r\n"
           << "            <tt:FirmwareUpgrade>false</tt:FirmwareUpgrade>\r\n"
           << "            <tt:SupportedVersions>\r\n"
           << "              <tt:Major>2</tt:Major>\r\n"
           << "              <tt:Minor>4</tt:Minor>\r\n"
           << "            </tt:SupportedVersions>\r\n"
           << "          </tt:System>\r\n"
           << "          <tt:IO>\r\n"
           << "            <tt:InputConnectors>1</tt:InputConnectors>\r\n"
           << "            <tt:RelayOutputs>" << numRelays << "</tt:RelayOutputs>\r\n"
           << "          </tt:IO>\r\n"
           << "          <tt:Security>\r\n"
           << "            <tt:TLS1.1>false</tt:TLS1.1>\r\n"
           << "            <tt:TLS1.2>true</tt:TLS1.2>\r\n"
           << "            <tt:OnboardKeyGeneration>false</tt:OnboardKeyGeneration>\r\n"
           << "            <tt:AccessPolicyConfig>true</tt:AccessPolicyConfig>\r\n"
           << "            <tt:X.509Token>false</tt:X.509Token>\r\n"
           << "            <tt:SAMLToken>false</tt:SAMLToken>\r\n"
           << "            <tt:KerberosToken>false</tt:KerberosToken>\r\n"
           << "            <tt:RELToken>false</tt:RELToken>\r\n"
           << "          </tt:Security>\r\n"
           << "        </tt:Device>\r\n";
    }

    if (shouldInclude("Media")) {
        ss << "        <tt:Media>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/media_service</tt:XAddr>\r\n"
           << "          <tt:StreamingCapabilities>\r\n"
           << "            <tt:RTPMulticast>false</tt:RTPMulticast>\r\n"
           << "            <tt:RTP_TCP>true</tt:RTP_TCP>\r\n"
           << "            <tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>\r\n"
           << "          </tt:StreamingCapabilities>\r\n"
           << "        </tt:Media>\r\n";
    }

    if (shouldInclude("Events")) {
        ss << "        <tt:Events>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/event_service</tt:XAddr>\r\n"
           << "          <tt:WSSubscriptionPolicySupport>true</tt:WSSubscriptionPolicySupport>\r\n"
           << "          <tt:WSPullPointSupport>true</tt:WSPullPointSupport>\r\n"
           << "          "
              "<tt:WSPausableSubscriptionManagerInterfaceSupport>false</"
              "tt:WSPausableSubscriptionManagerInterfaceSupport>\r\n"
           << "        </tt:Events>\r\n";
    }

    if (shouldInclude("Imaging")) {
        ss << "        <tt:Imaging>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/imaging_service</tt:XAddr>\r\n"
           << "        </tt:Imaging>\r\n";
    }

    if (shouldInclude("PTZ")) {
        ss << "        <tt:PTZ>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/ptz_service</tt:XAddr>\r\n"
           << "        </tt:PTZ>\r\n";
    }

    if (shouldInclude("Analytics")) {
        ss << "        <tt:Analytics>\r\n"
           << "          <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/analytics_service</tt:XAddr>\r\n"
           << "          <tt:RuleSupport>true</tt:RuleSupport>\r\n"
           << "          <tt:AnalyticsModuleSupport>true</tt:AnalyticsModuleSupport>\r\n"
           << "        </tt:Analytics>\r\n";
    }

    if (includeAll || shouldInclude("Extension")) {
        ss << "        <tt:Extension>\r\n"
           << "          <tt:DeviceIO>\r\n"
           << "            <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/deviceio_service</tt:XAddr>\r\n"
           << "            <tt:VideoSources>1</tt:VideoSources>\r\n"
           << "            <tt:RelayOutputs>" << numRelays << "</tt:RelayOutputs>\r\n"
           << "          </tt:DeviceIO>\r\n";
        if (profileGSupported) {
            ss << "          <tt:Recording>\r\n"
               << "            <tt:XAddr>http://" << safeHost << ":" << port
               << "/onvif/recording_service</tt:XAddr>\r\n"
               << "            <tt:Receiver>false</tt:Receiver>\r\n"
               << "            <tt:MediaProfileSummary>false</tt:MediaProfileSummary>\r\n"
               << "          </tt:Recording>\r\n"
               << "          <tt:Search>\r\n"
               << "            <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/search_service</tt:XAddr>\r\n"
               << "            <tt:MetadataSearch>true</tt:MetadataSearch>\r\n"
               << "          </tt:Search>\r\n"
               << "          <tt:Replay>\r\n"
               << "            <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/replay_service</tt:XAddr>\r\n"
               << "          </tt:Replay>\r\n";
        }
        ss << "          <tt:Media2>\r\n"
           << "            <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/media2_service</tt:XAddr>\r\n"
           << "          </tt:Media2>\r\n";
        if (thermalSupported) {
            ss << "          <tt:Thermal>\r\n"
               << "            <tt:XAddr>http://" << safeHost << ":" << port << "/onvif/thermal_service</tt:XAddr>\r\n"
               << "          </tt:Thermal>\r\n";
        }
        ss << "        </tt:Extension>\r\n";
    }

    ss << "      </tds:Capabilities>\r\n"
       << "    </tds:GetCapabilitiesResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildScopesXml(
    const std::string& devName, const std::string& model, const std::vector<std::string>& configurableScopes)
{
    std::ostringstream ss;
    ss << "    <tds:GetScopesResponse>\r\n"
       << "      "
          "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/type/video_encoder</"
          "tt:ScopeItem></tds:Scopes>\r\n"
       << "      "
          "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/type/ptz</tt:ScopeItem></"
          "tds:Scopes>\r\n"
       << "      <tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/name/"
       << Xml::escapeXmlText(devName) << "</tt:ScopeItem></tds:Scopes>\r\n"
       << "      <tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/hardware/"
       << Xml::escapeXmlText(model) << "</tt:ScopeItem></tds:Scopes>\r\n";
    for (const auto& scope : configurableScopes) {
        ss << "      <tds:Scopes><tt:ScopeDef>Configurable</tt:ScopeDef><tt:ScopeItem>" << Xml::escapeXmlText(scope)
           << "</tt:ScopeItem></tds:Scopes>\r\n";
    }
    ss << "    </tds:GetScopesResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildUsersXml(const std::vector<OnvifUser>& users)
{
    std::ostringstream ss;
    ss << "    <tds:GetUsersResponse>\r\n";
    for (const auto& u : users) {
        ss << "      <tds:User>\r\n"
           << "        <tt:Username>" << Xml::escapeXmlText(u.username) << "</tt:Username>\r\n"
           << "        <tt:UserLevel>" << userLevelToString(u.level) << "</tt:UserLevel>\r\n"
           << "      </tds:User>\r\n";
    }
    ss << "    </tds:GetUsersResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildNetIfacesXml(const std::vector<NetworkInterfaceConfig>& ifaces)
{
    std::ostringstream ss;
    ss << "    <tds:GetNetworkInterfacesResponse>\r\n";
    for (const auto& iface : ifaces) {
        ss << "      <tds:NetworkInterfaces token=\"" << Xml::escapeXmlAttr(iface.token) << "\">\r\n"
           << "        <tt:Enabled>" << (iface.enabled ? "true" : "false") << "</tt:Enabled>\r\n"
           << "        <tt:Info>\r\n"
           << "          <tt:Name>" << Xml::escapeXmlText(iface.name) << "</tt:Name>\r\n"
           << "          <tt:HwAddress>" << Xml::escapeXmlText(iface.hwAddress) << "</tt:HwAddress>\r\n"
           << "          <tt:MTU>" << iface.mtu << "</tt:MTU>\r\n"
           << "        </tt:Info>\r\n"
           << "        <tt:IPv4>\r\n"
           << "          <tt:Enabled>" << (iface.ipv4.enabled ? "true" : "false") << "</tt:Enabled>\r\n"
           << "          <tt:Config>\r\n"
           << "            <tt:Manual>\r\n"
           << "              <tt:Address>" << Xml::escapeXmlText(iface.ipv4.manualAddress) << "</tt:Address>\r\n"
           << "              <tt:PrefixLength>" << iface.ipv4.prefixLength << "</tt:PrefixLength>\r\n"
           << "            </tt:Manual>\r\n"
           << "            <tt:DHCP>" << (iface.ipv4.dhcp ? "true" : "false") << "</tt:DHCP>\r\n"
           << "          </tt:Config>\r\n"
           << "        </tt:IPv4>\r\n"
           << "      </tds:NetworkInterfaces>\r\n";
    }
    ss << "    </tds:GetNetworkInterfacesResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildNetProtosXml(const std::vector<NetworkProtocolConfig>& protos)
{
    std::ostringstream ss;
    ss << "    <tds:GetNetworkProtocolsResponse>\r\n";
    for (const auto& proto : protos) {
        ss << "      <tds:NetworkProtocols>\r\n"
           << "        <tt:Name>" << Xml::escapeXmlText(proto.name) << "</tt:Name>\r\n"
           << "        <tt:Enabled>" << (proto.enabled ? "true" : "false") << "</tt:Enabled>\r\n"
           << "        <tt:Port>" << proto.port << "</tt:Port>\r\n"
           << "      </tds:NetworkProtocols>\r\n";
    }
    ss << "    </tds:GetNetworkProtocolsResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildGatewayXml(const std::string& gateway)
{
    std::ostringstream ss;
    ss << "    <tds:GetNetworkDefaultGatewayResponse>\r\n"
       << "      <tds:NetworkGateway>\r\n"
       << "        <tt:IPv4Address>" << Xml::escapeXmlText(gateway) << "</tt:IPv4Address>\r\n"
       << "      </tds:NetworkGateway>\r\n"
       << "    </tds:GetNetworkDefaultGatewayResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildDnsXml(const DnsConfig& dns)
{
    std::ostringstream ss;
    ss << "    <tds:GetDNSResponse>\r\n"
       << "      <tds:DNSInformation>\r\n"
       << "        <tt:FromDHCP>" << (dns.fromDhcp ? "true" : "false") << "</tt:FromDHCP>\r\n";
    for (const auto& sd : dns.searchDomains) {
        ss << "        <tt:SearchDomain>" << Xml::escapeXmlText(sd) << "</tt:SearchDomain>\r\n";
    }
    for (const auto& server : dns.dnsServers) {
        ss << "        <tt:DNSManual>\r\n"
           << "          <tt:Type>IPv4</tt:Type>\r\n"
           << "          <tt:IPv4Address>" << Xml::escapeXmlText(server) << "</tt:IPv4Address>\r\n"
           << "        </tt:DNSManual>\r\n";
    }
    ss << "      </tds:DNSInformation>\r\n"
       << "    </tds:GetDNSResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildNtpXml(const NtpConfig& ntp)
{
    std::ostringstream ss;
    ss << "    <tds:GetNTPResponse>\r\n"
       << "      <tds:NTPInformation>\r\n"
       << "        <tt:FromDHCP>" << (ntp.fromDhcp ? "true" : "false") << "</tt:FromDHCP>\r\n";
    for (const auto& srv : ntp.manualServers) {
        ss << "        <tt:NTPManual>\r\n"
           << "          <tt:Type>DNS</tt:Type>\r\n"
           << "          <tt:DNSname>" << Xml::escapeXmlText(srv) << "</tt:DNSname>\r\n"
           << "        </tt:NTPManual>\r\n";
    }
    ss << "      </tds:NTPInformation>\r\n"
       << "    </tds:GetNTPResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildHostnameXml(const std::string& hostname)
{
    std::ostringstream ss;
    ss << "    <tds:GetHostnameResponse>\r\n"
       << "      <tds:HostnameInformation>\r\n"
       << "        <tt:FromDHCP>false</tt:FromDHCP>\r\n"
       << "        <tt:Name>" << Xml::escapeXmlText(hostname) << "</tt:Name>\r\n"
       << "      </tds:HostnameInformation>\r\n"
       << "    </tds:GetHostnameResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildSysLogsXml(const std::string& logData)
{
    std::ostringstream ss;
    ss << "    <tds:GetSystemLogResponse>\r\n"
       << "      <tds:SystemLog>\r\n"
       << "        <tt:String>" << Xml::escapeXmlText(logData) << "</tt:String>\r\n"
       << "      </tds:SystemLog>\r\n"
       << "    </tds:GetSystemLogResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildCertsXml(const std::vector<OnvifCertificate>& certs)
{
    std::ostringstream ss;
    ss << "    <tds:GetCertificatesResponse>\r\n";
    for (const auto& cert : certs) {
        ss << "      <tds:NvtCertificate>\r\n"
           << "        <tt:CertificateID>" << Xml::escapeXmlText(cert.certificateId) << "</tt:CertificateID>\r\n"
           << "        <tt:Certificate>\r\n"
           << "          <tt:Data>" << Xml::escapeXmlText(cert.x509DerBase64) << "</tt:Data>\r\n"
           << "        </tt:Certificate>\r\n"
           << "      </tds:NvtCertificate>\r\n";
    }
    ss << "    </tds:GetCertificatesResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildCertInfoXml(const std::string& certId, const CertificateInformation& info)
{
    std::ostringstream ss;
    ss << "    <tds:GetCertificateInformationResponse>\r\n"
       << "      <tds:CertificateInformation>\r\n"
       << "        <tt:CertificateID>" << Xml::escapeXmlText(info.certificateId.empty() ? certId : info.certificateId)
       << "</tt:CertificateID>\r\n"
       << "        <tt:IssuerDN>" << Xml::escapeXmlText(info.issuer) << "</tt:IssuerDN>\r\n"
       << "        <tt:SubjectDN>" << Xml::escapeXmlText(info.subject) << "</tt:SubjectDN>\r\n"
       << "        <tt:Validity>\r\n"
       << "          <tt:From>" << Xml::escapeXmlText(info.validNotBefore) << "</tt:From>\r\n"
       << "          <tt:Until>" << Xml::escapeXmlText(info.validNotAfter) << "</tt:Until>\r\n"
       << "        </tt:Validity>\r\n"
       << "        <tt:Extension>\r\n"
       << "          <tt:KeyUsage>" << Xml::escapeXmlText(info.keyAlgorithm) << "</tt:KeyUsage>\r\n"
       << "        </tt:Extension>\r\n"
       << "      </tds:CertificateInformation>\r\n"
       << "    </tds:GetCertificateInformationResponse>\r\n";
    return ss.str();
}

std::string DeviceXmlBuilder::buildGeoLocationXml(const LocationEntity& loc)
{
    std::ostringstream ss;
    ss << "    <tds:GetGeoLocationResponse>\r\n"
       << "      <tds:Location Entity=\"" << Xml::escapeXmlAttr(loc.entity) << "\" Token=\""
       << Xml::escapeXmlAttr(loc.token) << "\" Fixed=\"" << (loc.fixed ? "true" : "false") << "\">\r\n"
       << "        <tt:GeoLocation lat=\"" << std::fixed << std::setprecision(6) << loc.location.latitude << "\" lon=\""
       << loc.location.longitude << "\" elevation=\"" << std::setprecision(2) << loc.location.elevation << "\"/>\r\n"
       << "        <tt:GeoOrientation yaw=\"" << std::setprecision(2) << loc.orientation.yaw << "\" pitch=\""
       << loc.orientation.pitch << "\" roll=\"" << loc.orientation.roll << "\"/>\r\n"
       << "      </tds:Location>\r\n"
       << "    </tds:GetGeoLocationResponse>\r\n";
    return ss.str();
}

} // namespace Onvif
