/// @file DeviceServicePki.cpp
/// @brief Modular ONVIF Device Management PKI certificate and geolocation operations.

#include "DeviceService.h"

#include <cstddef>
#include <cstdint>

#include <sstream>
#include <string>
#include <vector>

#include "XmlUtils.h"
#include "builders/DeviceXmlBuilder.h"

namespace Onvif {

SoapHandlerResult DeviceService::handleGetCerts(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const auto certs = dev ? dev->handleGetCertificates() : m_certs;
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildCertsXml(certs));
}

SoapHandlerResult DeviceService::handleGetCertInfo(const SoapContext& ctx)
{
    const auto idNode = ctx.reqNode.select_node(".//*[local-name()='CertificateID']").node();
    const std::string certId = idNode ? idNode.text().as_string() : "";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    CertificateInformation info;
    if (dev) {
        const auto opt = dev->handleGetCertInformation(certId);
        if (opt.has_value()) {
            info = *opt;
        }
    }
    return SoapHandlerResult::ok(DeviceXmlBuilder::buildCertInfoXml(certId, info));
}

SoapHandlerResult DeviceService::handleCreateCert(const SoapContext& ctx)
{
    const auto idNode = ctx.reqNode.select_node(".//*[local-name()='CertificateID']").node();
    const auto subjNode = ctx.reqNode.select_node(".//*[local-name()='Subject']").node();
    const std::string certId = idNode ? idNode.text().as_string() : "Cert_1";
    const std::string subj = subjNode ? subjNode.text().as_string() : "CN=PelcoD";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    OnvifCertificate newCert;
    if (dev) {
        newCert = dev->handleCreateCertificate(certId, subj, 365);
    }
    std::ostringstream ss;
    ss << "    <tds:CreateCertificateResponse>\r\n"
       << "      <tds:NvtCertificate>\r\n"
       << "        <tt:CertificateID>" << Xml::escapeXmlText(certId) << "</tt:CertificateID>\r\n"
       << "        <tt:Certificate><tt:Data>" << Xml::escapeXmlText(newCert.x509DerBase64)
       << "</tt:Data></tt:Certificate>\r\n"
       << "      </tds:NvtCertificate>\r\n"
       << "    </tds:CreateCertificateResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleGetPkcs10(const SoapContext& ctx)
{
    const auto idNode = ctx.reqNode.select_node(".//*[local-name()='CertificateID']").node();
    const auto subjNode = ctx.reqNode.select_node(".//*[local-name()='Subject']").node();
    const std::string certId = idNode ? idNode.text().as_string() : "";
    const std::string subj = subjNode ? subjNode.text().as_string() : "";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    Pkcs10Request csr;
    if (dev) {
        csr = dev->handleGetPkcs10Request(certId, subj);
    }
    std::ostringstream ss;
    ss << "    <tds:GetPkcs10RequestResponse>\r\n"
       << "      <tds:Pkcs10Request>" << Xml::escapeXmlText(csr.csrBase64) << "</tds:Pkcs10Request>\r\n"
       << "    </tds:GetPkcs10RequestResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleLoadCerts(const SoapContext&)
{
    return SoapHandlerResult::ok("    <tds:LoadCertificatesResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleDeleteCerts(const SoapContext&)
{
    return SoapHandlerResult::ok("    <tds:DeleteCertificatesResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetClientCert(const SoapContext&)
{
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    const ClientCertificateMode mode = dev ? dev->handleGetClientCertMode() : m_clientCertMode;
    std::ostringstream ss;
    ss << "    <tds:GetClientCertificateModeResponse>\r\n"
       << "      <tds:ClientCertificateMode>" << clientCertificateModeToString(mode)
       << "</tds:ClientCertificateMode>\r\n"
       << "    </tds:GetClientCertificateModeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceService::handleSetClientCert(const SoapContext& ctx)
{
    const auto modeNode = ctx.reqNode.select_node(".//*[local-name()='ClientCertificateMode']").node();
    const std::string modeStr = modeNode ? modeNode.text().as_string() : "Off";
    const ClientCertificateMode mode = clientCertModeFromString(modeStr);
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleSetClientCertMode(mode));
    }
    m_clientCertMode = mode;
    return SoapHandlerResult::ok("    <tds:SetClientCertificateModeResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleGetGeoLoc(const SoapContext& ctx)
{
    const auto entNode = ctx.reqNode.select_node(".//*[local-name()='Entity']").node();
    const std::string ent = entNode ? entNode.text().as_string() : "Device";
    LocationEntity loc;
    loc.entity = ent;
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
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
        static_cast<void>(dev->handleSetGeoLocation(loc));
    }
    return SoapHandlerResult::ok("    <tds:SetGeoLocationResponse/>\r\n");
}

SoapHandlerResult DeviceService::handleDeleteGeoLoc(const SoapContext& ctx)
{
    const auto entNode = ctx.reqNode.select_node(".//*[local-name()='Entity']").node();
    const std::string ent = entNode ? entNode.text().as_string() : "Device";
    const auto dev = m_ctx.getDeviceHandler ? m_ctx.getDeviceHandler() : nullptr;
    if (dev) {
        static_cast<void>(dev->handleDeleteGeoLocation(ent));
    }
    return SoapHandlerResult::ok("    <tds:DeleteGeoLocationResponse/>\r\n");
}

} // namespace Onvif
