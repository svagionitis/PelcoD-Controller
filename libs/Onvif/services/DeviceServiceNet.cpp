/// @file DeviceServiceNet.cpp
/// @brief Modular ONVIF Device Management network, certificate, and auxiliary operations.

#include "DeviceService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "XmlUtils.h"
#include "builders/DeviceXmlBuilder.h"

namespace Onvif {

SoapHandlerResult DeviceService::handleGetNetIfaces(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const auto ifaces = dev ? dev->handleGetNetworkInterfaces() : m_netInterfaces;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildNetIfacesXml(ifaces));
}

SoapHandlerResult DeviceService::handleSetNetIfaces(const SoapContext& ctx)
{
    NetworkInterfaceConfig iface;
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='InterfaceToken']").node();
    if (tokNode) {
        iface.token = tokNode.text().as_string();
    }
    const auto enNode
        = ctx.reqNode.select_node(".//*[local-name()='NetworkInterface']/*[local-name()='Enabled']").node();
    if (enNode) {
        iface.enabled = enNode.text().as_bool(true);
    }
    const auto mtuNode = ctx.reqNode.select_node(".//*[local-name()='MTU']").node();
    if (mtuNode) {
        iface.mtu = mtuNode.text().as_int(1500);
    }
    const auto ipv4EnNode = ctx.reqNode.select_node(".//*[local-name()='IPv4']/*[local-name()='Enabled']").node();
    if (ipv4EnNode) {
        iface.ipv4.enabled = ipv4EnNode.text().as_bool(true);
    }
    const auto dhcpNode = ctx.reqNode.select_node(".//*[local-name()='DHCP']").node();
    if (dhcpNode) {
        iface.ipv4.dhcp = dhcpNode.text().as_bool(false);
    }
    const auto addrNode = ctx.reqNode.select_node(".//*[local-name()='Manual']/*[local-name()='Address']").node();
    if (addrNode) {
        iface.ipv4.manualAddress = addrNode.text().as_string();
    }
    const auto pfxNode = ctx.reqNode.select_node(".//*[local-name()='Manual']/*[local-name()='PrefixLength']").node();
    if (pfxNode) {
        iface.ipv4.prefixLength = pfxNode.text().as_int(24);
    }

    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetNetworkInterfaces(iface));
    }
    auto it = std::find_if(m_netInterfaces.begin(), m_netInterfaces.end(),
        [&](const NetworkInterfaceConfig& c) { return c.token == iface.token; });
    if (it != m_netInterfaces.end()) {
        *it = iface;
    } else {
        m_netInterfaces.push_back(iface);
    }
    return SoapHandlerResult::ok("    <tds:SetNetworkInterfacesResponse>\r\n"
                                 "      <tds:RebootNeeded>false</tds:RebootNeeded>\r\n"
                                 "    </tds:SetNetworkInterfacesResponse>\r\n");
}

SoapHandlerResult DeviceService::handleGetNetProtos(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    std::vector<NetworkProtocolConfig> protos;
    if (dev) {
        protos = dev->handleGetNetworkProtocols();
    }
    if (protos.empty() && m_ctx.getNetProtocols) {
        protos = m_ctx.getNetProtocols();
    }
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildNetProtosXml(protos));
}

SoapHandlerResult DeviceService::handleSetNetProtos(const SoapContext& ctx)
{
    const auto protoNodes = ctx.reqNode.select_nodes(".//*[local-name()='NetworkProtocols']");
    if (protoNodes.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing NetworkProtocols", 400);
    }
    std::vector<NetworkProtocolConfig> updated;
    for (const auto& sel : protoNodes) {
        NetworkProtocolConfig proto;
        const auto nameNode = sel.node().select_node(".//*[local-name()='Name']").node();
        const auto enabledNode = sel.node().select_node(".//*[local-name()='Enabled']").node();
        const auto portNode = sel.node().select_node(".//*[local-name()='Port']").node();
        if (!nameNode || !portNode) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing Name or Port in NetworkProtocols", 400);
        }
        proto.name = nameNode.text().as_string();
        proto.enabled = enabledNode ? enabledNode.text().as_bool(true) : true;
        const int p = portNode.text().as_int(-1);
        if (p < 1 || p > 65535) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid network protocol port number", 400);
        }
        proto.port = p;
        updated.push_back(proto);
    }
    if (m_ctx.setNetProtocols && !m_ctx.setNetProtocols(updated)) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Failed to apply network protocols", 400);
    }
    return SoapHandlerResult::ok("    <tds:SetNetworkProtocolsResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetGateway(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const std::string gw = dev ? dev->handleGetDefaultGateway() : m_gateway;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildGatewayXml(gw));
}

SoapHandlerResult DeviceService::handleSetGateway(const SoapContext& ctx)
{
    const auto gwNode = ctx.reqNode.select_node(".//*[local-name()='IPv4Address']").node();
    const std::string gw = gwNode ? gwNode.text().as_string() : "";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetDefaultGateway(gw));
    }
    m_gateway = gw;
    return SoapHandlerResult::ok("    <tds:SetNetworkDefaultGatewayResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetDns(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const DnsConfig dns = dev ? dev->handleGetDNS() : m_dns;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildDnsXml(dns));
}

SoapHandlerResult DeviceService::handleSetDns(const SoapContext& ctx)
{
    DnsConfig dns;
    const auto dhcpNode = ctx.reqNode.select_node(".//*[local-name()='FromDHCP']").node();
    if (dhcpNode) {
        dns.fromDhcp = dhcpNode.text().as_bool(false);
    }
    const auto sdNodes = ctx.reqNode.select_nodes(".//*[local-name()='SearchDomain']");
    for (const auto& sel : sdNodes) {
        dns.searchDomains.push_back(sel.node().text().as_string());
    }
    const auto srvNodes = ctx.reqNode.select_nodes(".//*[local-name()='DNSManual']/*[local-name()='IPv4Address']");
    for (const auto& sel : srvNodes) {
        dns.dnsServers.push_back(sel.node().text().as_string());
    }

    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetDNS(dns));
    }
    m_dns = dns;
    return SoapHandlerResult::ok("    <tds:SetDNSResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetNtp(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const NtpConfig ntp = dev ? dev->handleGetNTP() : m_ntp;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildNtpXml(ntp));
}

SoapHandlerResult DeviceService::handleSetNtp(const SoapContext& ctx)
{
    NtpConfig ntp;
    const auto dhcpNode = ctx.reqNode.select_node(".//*[local-name()='FromDHCP']").node();
    if (dhcpNode) {
        ntp.fromDhcp = dhcpNode.text().as_bool(false);
    }
    const auto ntpNodes = ctx.reqNode.select_nodes(".//*[local-name()='NTPManual']/*[local-name()='DNSname']");
    for (const auto& sel : ntpNodes) {
        ntp.manualServers.push_back(sel.node().text().as_string());
    }
    const auto ipNodes = ctx.reqNode.select_nodes(".//*[local-name()='NTPManual']/*[local-name()='IPv4Address']");
    for (const auto& sel : ipNodes) {
        ntp.manualServers.push_back(sel.node().text().as_string());
    }

    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetNTP(ntp));
    }
    m_ntp = ntp;
    return SoapHandlerResult::ok("    <tds:SetNTPResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetHostname(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const std::string hn = dev ? dev->handleGetHostname() : m_hostname;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildHostnameXml(hn));
}

SoapHandlerResult DeviceService::handleSetHostname(const SoapContext& ctx)
{
    const auto hnNode = ctx.reqNode.select_node(".//*[local-name()='Name']").node();
    const std::string hn = hnNode ? hnNode.text().as_string() : "";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetHostname(hn));
    }
    m_hostname = hn;
    return SoapHandlerResult::ok("    <tds:SetHostnameResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetDiscMode(const SoapContext&)
{
    const DiscoveryMode mode = m_ctx.getDiscoveryMode ? m_ctx.getDiscoveryMode() : DiscoveryMode::Discoverable;
    const std::string modeStr = (mode == DiscoveryMode::NonDiscoverable) ? "NonDiscoverable" : "Discoverable";
    std::ostringstream ss;
    ss << "    <tds:GetDiscoveryModeResponse>\r\n"
       << "      <tds:DiscoveryMode>" << modeStr << "</tds:DiscoveryMode>\r\n"
       << "    </tds:GetDiscoveryModeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleSetDiscMode(const SoapContext& ctx)
{
    const auto modeNode = ctx.reqNode.select_node(".//*[local-name()='DiscoveryMode']").node();
    const std::string_view modeStr = modeNode ? modeNode.text().as_string() : "";
    DiscoveryMode target = DiscoveryMode::Discoverable;
    if (modeStr == "Discoverable") {
        target = DiscoveryMode::Discoverable;
    } else if (modeStr == "NonDiscoverable") {
        target = DiscoveryMode::NonDiscoverable;
    } else {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid discovery mode", 400);
    }
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetDiscoveryMode(target));
    }
    if (m_ctx.setDiscoveryMode) {
        m_ctx.setDiscoveryMode(target);
    }
    return SoapHandlerResult::ok("    <tds:SetDiscoveryModeResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetWsdlUrl(const SoapContext& ctx)
{
    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    std::ostringstream ss;
    ss << "    <tds:GetWsdlUrlResponse>\r\n"
       << "      <tds:WsdlUrl>http://" << Xml::escapeXml(host) << ":" << m_ctx.config.port
       << "/onvif/device.wsdl</tds:WsdlUrl>\r\n"
       << "    </tds:GetWsdlUrlResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleGetEndpointRef(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    std::string ep = dev ? dev->handleGetEndpointReference() : "";
    if (ep.empty()) {
        ep = m_ctx.config.serviceUuid.empty() ? ("urn:uuid:" + m_ctx.config.serialNumber) : m_ctx.config.serviceUuid;
    }
    std::ostringstream ss;
    ss << "    <tds:GetEndpointReferenceResponse>\r\n"
       << "      <tds:GUID>" << Xml::escapeXmlText(ep) << "</tds:GUID>\r\n"
       << "    </tds:GetEndpointReferenceResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleGetSystemLog(const SoapContext& ctx)
{
    const auto logTypeNode = ctx.reqNode.select_node(".//*[local-name()='LogType']").node();
    const std::string logTypeStr = logTypeNode ? logTypeNode.text().as_string() : "System";
    const SystemLogType logType = (logTypeStr == "Access") ? SystemLogType::Access : SystemLogType::System;
    std::string logData;
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        logData = dev->handleGetSystemLog(logType);
    }
    if (logData.empty() && m_ctx.getSystemLog) {
        logData = m_ctx.getSystemLog(logType);
    }
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildSysLogsXml(logData));
}

SoapHandlerResult DeviceService::handleGetSysSupport(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    SystemSupportInfo info;
    if (dev) {
        info = dev->handleGetSystemSupportInfo();
    }
    if (info.rawDiagnostics.empty()) {
        info.rawDiagnostics = "ONVIF System Diagnostics: OK\nModel: " + m_ctx.config.model;
    }
    std::ostringstream ss;
    ss << "    <tds:GetSystemSupportInformationResponse>\r\n"
       << "      <tds:SupportInformation>\r\n"
       << "        <tt:String>" << Xml::escapeXmlText(info.rawDiagnostics) << "</tt:String>\r\n"
       << "      </tds:SupportInformation>\r\n"
       << "    </tds:GetSystemSupportInformationResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleGetSysBackup(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    std::string backup = dev ? dev->handleGetSystemBackup() : "";
    if (backup.empty()) {
        backup = "{\"hostname\":\"" + m_hostname + "\"}";
    }
    std::ostringstream ss;
    ss << "    <tds:GetSystemBackupResponse>\r\n"
       << "      <tds:BackupFiles><tt:Data>" << backup << "</tt:Data></tds:BackupFiles>\r\n"
       << "    </tds:GetSystemBackupResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleRestoreSys(const SoapContext& ctx)
{
    const auto dataNode = ctx.reqNode.select_node(".//*[local-name()='BackupFiles']/*[local-name()='Data']").node();
    const std::string backup = dataNode ? dataNode.text().as_string() : "";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const bool ok = dev ? dev->handleRestoreSystem(backup) : true;
    std::ostringstream ss;
    ss << "    <tds:RestoreSystemResponse>\r\n"
       << "      <tds:Message>" << (ok ? "Restored" : "Failed") << "</tds:Message>\r\n"
       << "    </tds:RestoreSystemResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleGetVideoModes(const SoapContext& ctx)
{
    std::vector<VideoSourceMode> modes;
    const auto handler = m_ctx.getVideoSourceModeHandler ? m_ctx.getVideoSourceModeHandler() : nullptr;
    if (handler) {
        const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        modes = handler->handleGetVideoSourceModes(tok);
    }
    if (modes.empty()) {
        VideoSourceMode m1;
        m1.token = "Mode_1080p60";
        m1.enabled = true;
        m1.maxFramerate = 60.0f;
        m1.width = 1920;
        m1.height = 1080;
        m1.encodings = { "H264", "H265" };
        m1.reboot = false;
        modes.push_back(m1);
    }
    std::ostringstream ss;
    ss << "    <tds:GetVideoSourceModesResponse>\r\n";
    for (const auto& m : modes) {
        ss << "      <tds:VideoSourceModes token=\"" << Xml::escapeXmlAttr(m.token) << "\">\r\n"
           << "        <tds:MaxFramerate>" << m.maxFramerate << "</tds:MaxFramerate>\r\n"
           << "        <tds:MaxResolution><tds:Width>" << m.width << "</tds:Width><tds:Height>" << m.height
           << "</tds:Height></tds:MaxResolution>\r\n"
           << "        <tds:Encodings>" << (m.encodings.empty() ? "H264" : m.encodings.front())
           << "</tds:Encodings>\r\n"
           << "        <tds:Reboot>" << (m.reboot ? "true" : "false") << "</tds:Reboot>\r\n"
           << "      </tds:VideoSourceModes>\r\n";
    }
    ss << "    </tds:GetVideoSourceModesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleSetVideoMode(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceModeToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    bool rebootNeeded = false;
    const auto handler = m_ctx.getVideoSourceModeHandler ? m_ctx.getVideoSourceModeHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetVideoSourceMode("VideoSource_1", tok, rebootNeeded));
    }
    std::ostringstream ss;
    ss << "    <tds:SetVideoSourceModeResponse><tds:Reboot>" << (rebootNeeded ? "true" : "false")
       << "</tds:Reboot></tds:SetVideoSourceModeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

} // namespace Onvif
