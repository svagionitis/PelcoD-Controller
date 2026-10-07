/// @file ServiceRegistry.cpp
/// @brief Implementation of ONVIF service registry and DOM response builder.

#include "ServiceRegistry.h"
#include "XmlUtils.h"
#include <sstream>

namespace Onvif {

void ServiceRegistry::addService(const ServiceDescriptor& desc)
{
    m_services.push_back(desc);
}

void ServiceRegistry::clear() noexcept
{
    m_services.clear();
}

const std::vector<ServiceDescriptor>& ServiceRegistry::getServices() const noexcept
{
    return m_services;
}

bool ServiceRegistry::parseIncludeCap(const pugi::xml_node& reqNode)
{
    if (!reqNode) {
        return false;
    }

    pugi::xml_node incNode = reqNode.child("tds:IncludeCapability");
    if (!incNode) {
        incNode = reqNode.find_child([](const pugi::xml_node& n) {
            const std::string_view name = n.name();
            const auto pos = name.rfind(':');
            const std::string_view local = (pos == std::string_view::npos) ? name : name.substr(pos + 1);
            return local == "IncludeCapability";
        });
    }

    if (!incNode) {
        return false;
    }

    const std::string_view val = incNode.text().as_string();
    return (val == "true" || val == "1" || val == "True" || val == "TRUE");
}

std::string ServiceRegistry::buildServicesXml(const std::string& host, int port, bool includeCap) const
{
    pugi::xml_document doc;
    auto resp = doc.append_child("tds:GetServicesResponse");

    const std::string safeHost = Xml::escapeXml(host);

    for (const auto& svc : m_services) {
        if (!svc.enabled) {
            continue;
        }

        auto sNode = resp.append_child("tds:Service");

        // 1. tds:Namespace
        auto nsNode = sNode.append_child("tds:Namespace");
        nsNode.text().set(svc.nameSpace.c_str());

        // 2. tds:XAddr (Strictly matching opening and closing tag via DOM)
        auto xNode = sNode.append_child("tds:XAddr");
        const std::string url = "http://" + safeHost + ":" + std::to_string(port) + svc.xAddrPath;
        xNode.text().set(url.c_str());

        // 3. Optional tds:Capabilities (must precede tds:Version per WSDL schema sequence)
        if (includeCap && svc.capabilitiesWriter) {
            auto capNode = sNode.append_child("tds:Capabilities");
            svc.capabilitiesWriter(capNode);
        }

        // 4. tds:Version with standard Major and Minor
        auto verNode = sNode.append_child("tds:Version");
        auto majNode = verNode.append_child("tt:Major");
        majNode.text().set(svc.version.major);
        auto minNode = verNode.append_child("tt:Minor");
        minNode.text().set(svc.version.minor);
    }

    std::ostringstream ss;
    doc.print(ss, "  ", pugi::format_indent | pugi::format_no_declaration);
    return ss.str();
}

ServiceRegistry ServiceRegistry::createDefault(bool thermalEnabled, bool profileGEnabled)
{
    ServiceRegistry reg;

    // 1. Device Management (Core Spec 2.5)
    reg.addService(
        { "http://www.onvif.org/ver10/device/wsdl", "/onvif/device_service", { 2, 5 }, true, [](pugi::xml_node& cap) {
             auto net = cap.append_child("tds:Network");
             net.append_attribute("IPFilter").set_value("false");
             net.append_attribute("ZeroConfiguration").set_value("false");
             net.append_attribute("IPVersion6").set_value("false");
             net.append_attribute("DynDNS").set_value("false");
             auto sys = cap.append_child("tds:System");
             sys.append_attribute("DiscoveryResolve").set_value("false");
             sys.append_attribute("DiscoveryBye").set_value("true");
             sys.append_attribute("RemoteDiscovery").set_value("false");
             sys.append_attribute("SystemBackup").set_value("true");
             sys.append_attribute("SystemLogging").set_value("true");
             sys.append_attribute("FirmwareUpgrade").set_value("false");
             auto sec = cap.append_child("tds:Security");
             sec.append_attribute("TLS1.1").set_value("false");
             sec.append_attribute("TLS1.2").set_value("false");
             sec.append_attribute("OnboardKeyGeneration").set_value("false");
             sec.append_attribute("AccessPolicyConfig").set_value("true");
             sec.append_attribute("DefaultAccessPolicy").set_value("true");
             sec.append_attribute("Dot1X").set_value("false");
             sec.append_attribute("RemoteUserHandling").set_value("false");
         } });

    // 2. Thermal Service (v1.0) - optional
    if (thermalEnabled) {
        reg.addService({ "http://www.onvif.org/ver10/thermal/wsdl", "/onvif/thermal_service", { 1, 0 }, true,
            [](pugi::xml_node& cap) { cap.append_attribute("Radiometry").set_value("true"); } });
    }

    // 3. Media Service (ver10, Spec 2.6)
    reg.addService(
        { "http://www.onvif.org/ver10/media/wsdl", "/onvif/media_service", { 2, 6 }, true, [](pugi::xml_node& cap) {
             auto stream = cap.append_child("trt:StreamingCapabilities");
             stream.append_attribute("RTPMulticast").set_value("false");
             stream.append_attribute("RTP_TCP").set_value("true");
             stream.append_attribute("RTP_RTSP_TCP").set_value("true");
         } });

    // 4. Media2 Service (ver20, Spec 2.0)
    reg.addService(
        { "http://www.onvif.org/ver20/media/wsdl", "/onvif/media2_service", { 2, 0 }, true, [](pugi::xml_node& cap) {
             auto prof = cap.append_child("tr2:ProfileCapabilities");
             prof.append_attribute("MaximumNumberOfProfiles").set_value("10");
         } });

    // 5. PTZ Service (ver20, Spec 2.0)
    reg.addService(
        { "http://www.onvif.org/ver20/ptz/wsdl", "/onvif/ptz_service", { 2, 0 }, true, [](pugi::xml_node& cap) {
             cap.append_attribute("EFlip").set_value("false");
             cap.append_attribute("Reverse").set_value("false");
             cap.append_attribute("MoveStatus").set_value("true");
         } });

    // 6. Imaging Service (ver20, Spec 2.0)
    reg.addService({ "http://www.onvif.org/ver20/imaging/wsdl", "/onvif/imaging_service", { 2, 0 }, true,
        [](pugi::xml_node& cap) { cap.append_attribute("ImageStabilization").set_value("false"); } });

    // 7. DeviceIO Service (ver10, Spec 2.0)
    reg.addService({ "http://www.onvif.org/ver10/deviceIO/wsdl", "/onvif/deviceio_service", { 2, 0 }, true,
        [](pugi::xml_node& cap) {
            cap.append_attribute("VideoSources").set_value("1");
            cap.append_attribute("RelayOutputs").set_value("1");
        } });

    // 8. Events Service (ver10, Spec 2.5)
    reg.addService(
        { "http://www.onvif.org/ver10/events/wsdl", "/onvif/event_service", { 2, 5 }, true, [](pugi::xml_node& cap) {
             cap.append_attribute("WSSubscriptionPolicySupport").set_value("false");
             cap.append_attribute("WSPullPointSupport").set_value("true");
             cap.append_attribute("WSPausableSubscriptionManagerInterfaceSupport").set_value("false");
         } });

    // 9. Analytics Service (ver20, Spec 2.0)
    reg.addService({ "http://www.onvif.org/ver20/analytics/wsdl", "/onvif/analytics_service", { 2, 0 }, true,
        [](pugi::xml_node& cap) {
            cap.append_attribute("RuleSupport").set_value("true");
            cap.append_attribute("AnalyticsModuleSupport").set_value("false");
        } });

    // Profile G Services (optional)
    if (profileGEnabled) {
        // 10. Recording Service (ver10, Spec 2.0)
        reg.addService({ "http://www.onvif.org/ver10/recording/wsdl", "/onvif/recording_service", { 2, 0 }, true,
            [](pugi::xml_node& cap) {
                cap.append_attribute("Receiver").set_value("false");
                cap.append_attribute("MediaProfileSummary").set_value("false");
            } });

        // 11. Search Service (ver10, Spec 2.0)
        reg.addService({ "http://www.onvif.org/ver10/search/wsdl", "/onvif/search_service", { 2, 0 }, true,
            [](pugi::xml_node& cap) { cap.append_attribute("MetadataSearch").set_value("true"); } });

        // 12. Replay Service (ver10, Spec 2.0)
        reg.addService({ "http://www.onvif.org/ver10/replay/wsdl", "/onvif/replay_service", { 2, 0 }, true,
            [](pugi::xml_node& cap) { cap.append_attribute("RTP_RTSP_TCP").set_value("true"); } });
    }

    return reg;
}

} // namespace Onvif
