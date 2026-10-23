/// @file DeviceService.cpp
#include "DeviceService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "CredentialStore.h"
#include "PasswordPolicy.h"
#include "ScopeStore.h"
#include "ServiceRegistry.h"
#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"
#include "builders/DeviceXmlBuilder.h"

namespace Onvif {

namespace {
    constexpr std::string_view kDeviceNs { "http://www.onvif.org/ver10/device/wsdl" };

    [[nodiscard]] OnvifUser parseOnvifUser(const pugi::xml_node& uNode)
    {
        OnvifUser u { };
        const auto un = uNode.select_node(".//*[local-name()='Username']").node();
        if (un) {
            u.username = un.text().as_string();
        }
        const auto pw = uNode.select_node(".//*[local-name()='Password']").node();
        if (pw) {
            u.password = pw.text().as_string();
        }
        const auto ul = uNode.select_node(".//*[local-name()='UserLevel']").node();
        if (ul) {
            u.level = userLevelFromString(ul.text().as_string());
        }
        return u;
    }
}

DeviceService::DeviceService(DeviceServiceContext ctx)
    : m_ctx(std::move(ctx))
    , m_hostname(m_ctx.config.hostname.empty() ? "PelcoD-Bridge" : m_ctx.config.hostname)
    , m_gateway(m_ctx.config.defaultGateway.empty() ? "192.168.1.1" : m_ctx.config.defaultGateway)
    , m_dns(m_ctx.config.defaultDns)
    , m_ntp(m_ctx.config.defaultNtp)
    , m_netInterfaces(m_ctx.config.defaultNetworkInterfaces)
    , m_certs(m_ctx.config.defaultCertificates)
{
}

std::string_view DeviceService::serviceName() const noexcept
{
    return "Device";
}

void DeviceService::registerRoutes(SoapRouter& router)
{
    const std::string ns { kDeviceNs };

    router.registerRoute(ns, "GetDeviceInformation", AccessClass::PreAuth,
        [this](const SoapContext& ctx) { return handleGetDeviceInfo(ctx); });
    router.registerRoute(
        ns, "GetCapabilities", AccessClass::PreAuth, [this](const SoapContext& ctx) { return handleGetCaps(ctx); });
    router.registerRoute(
        ns, "GetServices", AccessClass::PreAuth, [this](const SoapContext& ctx) { return handleGetServices(ctx); });
    router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok("    <tds:GetServiceCapabilitiesResponse>\r\n"
                                     "      <tds:Capabilities>\r\n"
                                     "        <tds:Network IPFilter=\"false\" ZeroConfiguration=\"false\" "
                                     "IPVersion6=\"false\" DynDNS=\"false\"/>\r\n"
                                     "        <tds:Security TLS1.2=\"true\" UsernameToken=\"true\" HttpDigest=\"true\" "
                                     "AccessPolicyConfig=\"true\"/>\r\n"
                                     "        <tds:System DiscoveryBye=\"true\" DiscoveryResolve=\"false\" "
                                     "SystemBackup=\"true\" SystemLogging=\"true\"/>\r\n"
                                     "      </tds:Capabilities>\r\n"
                                     "    </tds:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(ns, "GetSystemDateAndTime", AccessClass::PreAuth,
        [this](const SoapContext& ctx) { return handleGetSysDate(ctx); });
    router.registerRoute(ns, "SetSystemDateAndTime", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetSysDate(ctx); });
    router.registerRoute(
        ns, "SystemReboot", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleReboot(ctx); });
    router.registerRoute(ns, "SetSystemFactoryDefault", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleFactoryDefault(ctx); });
    router.registerRoute(ns, "GetUsers", AccessClass::ReadSystemSensitive,
        [this](const SoapContext& ctx) { return handleGetUsers(ctx); });
    router.registerRoute(
        ns, "CreateUsers", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleCreateUsers(ctx); });
    router.registerRoute(
        ns, "DeleteUsers", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleDeleteUsers(ctx); });
    router.registerRoute(
        ns, "SetUser", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleSetUser(ctx); });
    router.registerRoute(
        ns, "GetScopes", AccessClass::PreAuth, [this](const SoapContext& ctx) { return handleGetScopes(ctx); });
    router.registerRoute(
        ns, "AddScopes", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleAddScopes(ctx); });
    router.registerRoute(
        ns, "SetScopes", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleSetScopes(ctx); });
    router.registerRoute(ns, "RemoveScopes", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleRemoveScopes(ctx); });
    router.registerRoute(ns, "GetNetworkInterfaces", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetNetIfaces(ctx); });
    router.registerRoute(ns, "SetNetworkInterfaces", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetNetIfaces(ctx); });
    router.registerRoute(ns, "GetNetworkProtocols", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetNetProtos(ctx); });
    router.registerRoute(ns, "SetNetworkProtocols", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetNetProtos(ctx); });
    router.registerRoute(ns, "GetNetworkDefaultGateway", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetGateway(ctx); });
    router.registerRoute(ns, "SetNetworkDefaultGateway", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetGateway(ctx); });
    router.registerRoute(
        ns, "GetDNS", AccessClass::ReadSystem, [this](const SoapContext& ctx) { return handleGetDns(ctx); });
    router.registerRoute(
        ns, "SetDNS", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleSetDns(ctx); });
    router.registerRoute(
        ns, "GetNTP", AccessClass::ReadSystem, [this](const SoapContext& ctx) { return handleGetNtp(ctx); });
    router.registerRoute(
        ns, "SetNTP", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleSetNtp(ctx); });
    router.registerRoute(
        ns, "GetHostname", AccessClass::PreAuth, [this](const SoapContext& ctx) { return handleGetHostname(ctx); });
    router.registerRoute(
        ns, "SetHostname", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleSetHostname(ctx); });
    router.registerRoute(ns, "GetDiscoveryMode", AccessClass::PreAuth,
        [this](const SoapContext& ctx) { return handleGetDiscMode(ctx); });
    router.registerRoute(ns, "SetDiscoveryMode", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetDiscMode(ctx); });
    router.registerRoute(
        ns, "GetWsdlUrl", AccessClass::PreAuth, [this](const SoapContext& ctx) { return handleGetWsdlUrl(ctx); });
    router.registerRoute(ns, "GetEndpointReference", AccessClass::PreAuth,
        [this](const SoapContext& ctx) { return handleGetEndpointRef(ctx); });
    router.registerRoute(ns, "GetSystemLog", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetSystemLog(ctx); });
    router.registerRoute(ns, "GetSystemSupportInformation", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetSysSupport(ctx); });
    router.registerRoute(ns, "GetSystemBackup", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetSysBackup(ctx); });
    router.registerRoute(ns, "RestoreSystem", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleRestoreSys(ctx); });
    router.registerRoute(
        ns, "GetCertificates", AccessClass::ReadSystem, [this](const SoapContext& ctx) { return handleGetCerts(ctx); });
    router.registerRoute(ns, "GetCertificateInformation", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetCertInfo(ctx); });
    router.registerRoute(ns, "CreateCertificate", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleCreateCert(ctx); });
    router.registerRoute(ns, "GetPkcs10Request", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleGetPkcs10(ctx); });
    router.registerRoute(ns, "LoadCertificates", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleLoadCerts(ctx); });
    router.registerRoute(ns, "DeleteCertificates", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteCerts(ctx); });
    router.registerRoute(ns, "GetClientCertificateMode", AccessClass::ReadSystem,
        [this](const SoapContext& ctx) { return handleGetClientCert(ctx); });
    router.registerRoute(ns, "SetClientCertificateMode", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetClientCert(ctx); });
    router.registerRoute(
        ns, "GetGeoLocation", AccessClass::ReadSystem, [this](const SoapContext& ctx) { return handleGetGeoLoc(ctx); });
    router.registerRoute(ns, "SetGeoLocation", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetGeoLoc(ctx); });
    router.registerRoute(ns, "DeleteGeoLocation", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteGeoLoc(ctx); });
    router.registerRoute(ns, "GetVideoSourceModes", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetVideoModes(ctx); });
    router.registerRoute(ns, "SetVideoSourceMode", AccessClass::Actuate,
        [this](const SoapContext& ctx) { return handleSetVideoMode(ctx); });
}

SoapHandlerResult DeviceService::handleGetDeviceInfo(const SoapContext&)
{
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildDeviceInfoXml(m_ctx.config));
}

SoapHandlerResult DeviceService::handleGetCaps(const SoapContext& ctx)
{
    const auto catNodes = ctx.reqNode.select_nodes(".//*[local-name()='Category']");
    std::vector<std::string> cats;
    for (const auto& sel : catNodes) {
        cats.push_back(sel.node().text().as_string());
    }
    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    const std::size_t numRelays = m_ctx.getRelayOutputCount ? m_ctx.getRelayOutputCount() : 0U;
    return SoapHandlerResult::ok(
        DeviceXmlBuilder::buildCapsXml(m_ctx.config, host, m_ctx.config.port, numRelays, cats));
}

SoapHandlerResult DeviceService::handleGetServices(const SoapContext& ctx)
{
    const bool includeCap = ServiceRegistry::parseIncludeCap(ctx.reqNode);
    const auto registry = ServiceRegistry::createDefault(true, true);
    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    return SoapHandlerResult::ok(registry.buildServicesXml(host, m_ctx.config.port, includeCap));
}

SoapHandlerResult DeviceService::handleGetSysDate(const SoapContext&)
{
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildSysDateXml(m_dateTime));
}

SoapHandlerResult DeviceService::handleSetSysDate(const SoapContext& ctx)
{
    SystemDateTimeConfig dt;
    const auto dtTypeNode = ctx.reqNode.select_node(".//*[local-name()='DateTimeType']").node();
    if (dtTypeNode) {
        dt.dateTimeType = dtTypeNode.text().as_string();
    }
    const auto dsNode = ctx.reqNode.select_node(".//*[local-name()='DaylightSavings']").node();
    if (dsNode) {
        dt.daylightSavings = dsNode.text().as_bool(false);
    }
    const auto tzNode = ctx.reqNode.select_node(".//*[local-name()='TimeZone']/*[local-name()='TZ']").node();
    if (tzNode) {
        dt.timeZone = tzNode.text().as_string();
    }
    const auto hNode = ctx.reqNode.select_node(".//*[local-name()='Hour']").node();
    if (hNode) {
        dt.hour = hNode.text().as_int();
    }
    const auto mNode = ctx.reqNode.select_node(".//*[local-name()='Minute']").node();
    if (mNode) {
        dt.minute = mNode.text().as_int();
    }
    const auto sNode = ctx.reqNode.select_node(".//*[local-name()='Second']").node();
    if (sNode) {
        dt.second = sNode.text().as_int();
    }
    const auto yNode = ctx.reqNode.select_node(".//*[local-name()='Year']").node();
    if (yNode) {
        dt.year = yNode.text().as_int();
    }
    const auto moNode = ctx.reqNode.select_node(".//*[local-name()='Month']").node();
    if (moNode) {
        dt.month = moNode.text().as_int();
    }
    const auto dNode = ctx.reqNode.select_node(".//*[local-name()='Day']").node();
    if (dNode) {
        dt.day = dNode.text().as_int();
    }

    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        dev->handleSetSystemDateAndTime(dt);
    }
    m_dateTime = dt;
    return SoapHandlerResult::ok("    <tds:SetSystemDateAndTimeResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleReboot(const SoapContext&)
{
    std::string msg = "Rebooting";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        msg = dev->handleSystemReboot();
    }
    std::ostringstream ss;
    ss << "    <tds:SystemRebootResponse>\r\n"
       << "      <tds:Message>" << Xml::escapeXmlText(msg) << "</tds:Message>\r\n"
       << "    </tds:SystemRebootResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleFactoryDefault(const SoapContext& ctx)
{
    const auto defNode = ctx.reqNode.select_node(".//*[local-name()='FactoryDefault']").node();
    const std::string defType = defNode ? defNode.text().as_string() : "Soft";
    const FactoryDefaultType type = (defType == "Hard") ? FactoryDefaultType::Hard : FactoryDefaultType::Soft;

    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        dev->handleSetSystemFactoryDefault(type);
    }
    if (m_ctx.credStore) {
        m_ctx.credStore->reset(m_ctx.config.defaultUsers);
        if (m_ctx.setProvisioningState) {
            const bool prov = (m_ctx.credStore->adminCount() > 0U) && !m_ctx.credStore->hasDefaultPassword();
            m_ctx.setProvisioningState(prov ? ProvisioningState::Provisioned : ProvisioningState::Unprovisioned);
        }
    }
    return SoapHandlerResult::ok("    <tds:SetSystemFactoryDefaultResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetUsers(const SoapContext&)
{
    if (!m_ctx.credStore) {
        return SoapHandlerResult::ok("    <tds:GetUsersResponse/>\r\n");
    }
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildUsersXml(m_ctx.credStore->list()));
}

SoapHandlerResult DeviceService::handleCreateUsers(const SoapContext& ctx)
{
    if (!m_ctx.credStore) {
        return SoapHandlerResult::fault("ter:ActionNotSupported", "No credential store", 500);
    }
    const auto userNodes = ctx.reqNode.select_nodes(".//*[local-name()='User']");
    std::vector<OnvifUser> newUsers;
    for (const auto& sel : userNodes) {
        newUsers.push_back(parseOnvifUser(sel.node()));
    }
    for (const auto& nu : newUsers) {
        if (!CredentialStore::isValidName(nu.username) || (nu.level == OnvifUserLevel::Anonymous)) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid user name or level", 400);
        }
        if (m_ctx.credStore->find(nu.username).has_value()) {
            return SoapHandlerResult::fault("ter:UsernameClash", "User exists", 400);
        }
        const auto checkRes = PasswordPolicy::check(nu.password, nu.username, m_ctx.config.passwordPolicy);
        if (checkRes != PasswordCheckResult::Valid) {
            return SoapHandlerResult::fault(
                "ter:InvalidArgVal", PasswordPolicy::describe(checkRes), 400, "ter:PasswordTooWeak");
        }
    }
    for (const auto& nu : newUsers) {
        static_cast<void>(m_ctx.credStore->upsert(nu));
    }
    if (m_ctx.setProvisioningState) {
        const bool prov = (m_ctx.credStore->adminCount() > 0U) && !m_ctx.credStore->hasDefaultPassword();
        m_ctx.setProvisioningState(prov ? ProvisioningState::Provisioned : ProvisioningState::Unprovisioned);
    }
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev && !newUsers.empty()) {
        static_cast<void>(dev->handleCreateUsers(newUsers));
    }
    return SoapHandlerResult::ok("    <tds:CreateUsersResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleDeleteUsers(const SoapContext& ctx)
{
    if (!m_ctx.credStore) {
        return SoapHandlerResult::fault("ter:ActionNotSupported", "No credential store", 500);
    }
    const auto unNodes = ctx.reqNode.select_nodes(".//*[local-name()='Username']");
    std::vector<std::string> names;
    std::size_t adminsTargeted { 0U };
    for (const auto& sel : unNodes) {
        const std::string name { sel.node().text().as_string() };
        const auto existing = m_ctx.credStore->find(name);
        if (!existing.has_value()) {
            return SoapHandlerResult::fault("ter:UsernameMissing", "Unknown user", 400);
        }
        if (existing->level == OnvifUserLevel::Administrator) {
            ++adminsTargeted;
        }
        names.push_back(name);
    }
    if ((adminsTargeted > 0U) && (adminsTargeted >= m_ctx.credStore->adminCount())) {
        return SoapHandlerResult::fault("ter:FixedUser", "Last administrator", 400);
    }
    std::vector<std::string> deleted;
    for (const auto& name : names) {
        if (m_ctx.credStore->erase(name)) {
            deleted.push_back(name);
        }
    }
    if (m_ctx.setProvisioningState) {
        const bool prov = (m_ctx.credStore->adminCount() > 0U) && !m_ctx.credStore->hasDefaultPassword();
        m_ctx.setProvisioningState(prov ? ProvisioningState::Provisioned : ProvisioningState::Unprovisioned);
    }
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev && !deleted.empty()) {
        static_cast<void>(dev->handleDeleteUsers(deleted));
    }
    return SoapHandlerResult::ok("    <tds:DeleteUsersResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleSetUser(const SoapContext& ctx)
{
    if (!m_ctx.credStore) {
        return SoapHandlerResult::fault("ter:ActionNotSupported", "No credential store", 500);
    }
    const auto userNodes = ctx.reqNode.select_nodes(".//*[local-name()='User']");
    std::vector<OnvifUser> updates;
    for (const auto& sel : userNodes) {
        OnvifUser u = parseOnvifUser(sel.node());
        const auto existing = m_ctx.credStore->find(u.username);
        if (!existing.has_value()) {
            return SoapHandlerResult::fault("ter:UsernameMissing", "Unknown user", 400);
        }
        if (u.password.empty()) {
            u.password = existing->password;
        }
        updates.push_back(std::move(u));
    }
    for (const auto& u : updates) {
        const auto checkRes = PasswordPolicy::check(u.password, u.username, m_ctx.config.passwordPolicy);
        if (checkRes != PasswordCheckResult::Valid) {
            return SoapHandlerResult::fault(
                "ter:InvalidArgVal", PasswordPolicy::describe(checkRes), 400, "ter:PasswordTooWeak");
        }
    }
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    for (const auto& u : updates) {
        if (!m_ctx.credStore->upsert(u)) {
            return SoapHandlerResult::fault("ter:FixedUser", "Change not permitted", 400);
        }
        if (dev) {
            static_cast<void>(dev->handleSetUser(u));
        }
    }
    return SoapHandlerResult::ok("    <tds:SetUserResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetScopes(const SoapContext&)
{
    const auto scopes = m_ctx.scopeStore ? m_ctx.scopeStore->getScopes() : std::vector<std::string> { };
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildScopesXml(m_ctx.config.deviceName, m_ctx.config.model, scopes));
}

SoapHandlerResult DeviceService::handleAddScopes(const SoapContext& ctx)
{
    const auto scNodes = ctx.reqNode.select_nodes(".//*[local-name()='ScopeItem']");
    std::vector<std::string> added;
    for (const auto& sel : scNodes) {
        added.push_back(sel.node().text().as_string());
    }
    if (m_ctx.scopeStore) {
        m_ctx.scopeStore->addScopes(added);
    }
    return SoapHandlerResult::ok("    <tds:AddScopesResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleSetScopes(const SoapContext& ctx)
{
    const auto scNodes = ctx.reqNode.select_nodes(".//*[local-name()='ScopeItem']");
    std::vector<std::string> newScopes;
    for (const auto& sel : scNodes) {
        newScopes.push_back(sel.node().text().as_string());
    }
    if (m_ctx.scopeStore) {
        m_ctx.scopeStore->setScopes(newScopes);
    }
    return SoapHandlerResult::ok("    <tds:SetScopesResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleRemoveScopes(const SoapContext& ctx)
{
    const auto scNodes = ctx.reqNode.select_nodes(".//*[local-name()='ScopeItem']");
    std::vector<std::string> removed;
    for (const auto& sel : scNodes) {
        removed.push_back(sel.node().text().as_string());
    }
    if (m_ctx.scopeStore) {
        m_ctx.scopeStore->removeScopes(removed);
    }
    return SoapHandlerResult::ok("    <tds:RemoveScopesResponse/>\r\n");
}
SoapHandlerResult DeviceService::handleGetGeoLoc(const SoapContext& ctx)
{
    const auto entNode = ctx.reqNode.select_node(".//*[local-name()='Entity']").node();
    const std::string ent = entNode ? entNode.text().as_string() : "Device";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    LocationEntity loc = m_ctx.config.defaultLocation;
    if (dev) {
        const auto opt = dev->handleGetGeoLocation(ent);
        if (opt.has_value()) {
            loc = *opt;
        }
    }
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildGeoLocationXml(loc));
}

SoapHandlerResult DeviceService::handleSetGeoLoc(const SoapContext& ctx)
{
    const auto locNode = ctx.reqNode.select_node(".//*[local-name()='Location']").node();
    LocationEntity loc;
    if (locNode) {
        loc.entity = locNode.attribute("Entity").as_string("Device");
        loc.token = locNode.attribute("Token").as_string("Location_1");
        loc.fixed = locNode.attribute("Fixed").as_bool(true);
        const auto geoNode = locNode.select_node(".//*[local-name()='GeoLocation']").node();
        if (geoNode && geoNode.attribute("lat")) {
            loc.location.latitude = geoNode.attribute("lat").as_double(0.0);
            loc.location.longitude = geoNode.attribute("lon").as_double(0.0);
            loc.location.elevation = geoNode.attribute("elevation").as_double(0.0);
        }
        const auto oriNode = locNode.select_node(".//*[local-name()='GeoOrientation']").node();
        if (oriNode) {
            loc.orientation.yaw = oriNode.attribute("yaw").as_double(0.0);
            loc.orientation.pitch = oriNode.attribute("pitch").as_double(0.0);
            loc.orientation.roll = oriNode.attribute("roll").as_double(0.0);
        }
    }
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        dev->handleSetGeoLocation(loc);
    }
    return SoapHandlerResult::ok("    <tds:SetGeoLocationResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleDeleteGeoLoc(const SoapContext& ctx)
{
    const auto entNode = ctx.reqNode.select_node(".//*[local-name()='Entity']").node();
    const std::string ent = entNode ? entNode.text().as_string() : "Device";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        dev->handleDeleteGeoLocation(ent);
    }
    return SoapHandlerResult::ok("    <tds:DeleteGeoLocationResponse/>\r\n");
}

} // namespace Onvif
