/// @file MediaServiceExt.cpp
/// @brief OSD, privacy masks, video modes, and metadata configuration for MediaService.

#include "MediaService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"
#include "builders/MediaXmlBuilder.h"

namespace Onvif {

namespace {

    static OsdConfig parseOsdXml(const pugi::xml_node& osdNode)
    {
        OsdConfig osd;
        if (!osdNode) {
            return osd;
        }
        osd.token = osdNode.attribute("token").as_string();
        const auto vsn = osdNode.select_node(".//*[local-name()='VideoSourceConfigurationToken']").node();
        if (vsn) {
            osd.videoSourceToken = vsn.text().as_string();
        }
        const auto posNode = osdNode.select_node(".//*[local-name()='Position']/*[local-name()='Type']").node();
        const std::string posType = posNode ? posNode.text().as_string() : "UpperLeft";
        if (posType == "UpperRight") {
            osd.position = OsdPositionType::UpperRight;
        } else if (posType == "LowerLeft") {
            osd.position = OsdPositionType::LowerLeft;
        } else if (posType == "LowerRight") {
            osd.position = OsdPositionType::LowerRight;
        } else if (posType == "Custom") {
            osd.position = OsdPositionType::Custom;
        } else {
            osd.position = OsdPositionType::UpperLeft;
        }

        const auto ptNode = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='PlainText']").node();
        if (ptNode) {
            osd.plainText = ptNode.text().as_string();
        }
        const auto fsNode = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='FontSize']").node();
        if (fsNode) {
            osd.fontSize = fsNode.text().as_uint(24U);
        }
        return osd;
    }

} // namespace

SoapHandlerResult MediaService::handleGetOSDs(const SoapContext&, const std::string& prefix)
{
    std::vector<OsdConfig> list;
    const auto handler = m_ctx.getOsdHandler ? m_ctx.getOsdHandler() : nullptr;
    if (handler) {
        list = handler->handleGetOSDs("");
    }
    if (list.empty()) {
        for (const auto& [token, osd] : m_osds) {
            list.push_back(osd);
        }
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetOSDsResponse>\r\n";
    for (const auto& osd : list) {
        ss << MediaXmlBuilder::buildOsdXml(osd, prefix);
    }
    ss << "    </" << prefix << ":GetOSDsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetOSD(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='OSDToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto handler = m_ctx.getOsdHandler ? m_ctx.getOsdHandler() : nullptr;
    std::optional<OsdConfig> found;
    if (handler) {
        found = handler->handleGetOSD(tok);
    }
    if (!found) {
        auto it = m_osds.find(tok);
        if (it != m_osds.end()) {
            found = it->second;
        }
    }
    if (found) {
        std::ostringstream ss;
        ss << "    <" << prefix << ":GetOSDResponse>\r\n"
           << MediaXmlBuilder::buildOsdXml(*found, prefix) << "    </" << prefix << ":GetOSDResponse>\r\n";
        return SoapHandlerResult::ok(ss.str());
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such OSD", 400);
}

SoapHandlerResult MediaService::handleCreateOSD(const SoapContext& ctx, const std::string& prefix)
{
    const auto osdNode = ctx.reqNode.select_node(".//*[local-name()='OSD']").node();
    auto osd = parseOsdXml(osdNode);
    const auto handler = m_ctx.getOsdHandler ? m_ctx.getOsdHandler() : nullptr;
    std::string assignedToken;
    if (handler) {
        assignedToken = handler->handleCreateOSD(osd);
    }
    if (assignedToken.empty()) {
        if (osd.token.empty()) {
            osd.token = "OSD_" + std::to_string(m_nextOsdId++);
        }
        assignedToken = osd.token;
    } else {
        osd.token = assignedToken;
    }
    m_osds[osd.token] = osd;
    std::ostringstream ss;
    ss << "    <" << prefix << ":CreateOSDResponse>\r\n"
       << "      <" << prefix << ":OSDToken>" << Xml::escapeXmlText(assignedToken) << "</" << prefix << ":OSDToken>\r\n"
       << "    </" << prefix << ":CreateOSDResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleDeleteOSD(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='OSDToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto handler = m_ctx.getOsdHandler ? m_ctx.getOsdHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleDeleteOSD(tok));
    }
    m_osds.erase(tok);
    return SoapHandlerResult::ok("    <" + prefix + ":DeleteOSDResponse/>\r\n");
}

SoapHandlerResult MediaService::handleSetOSD(const SoapContext& ctx, const std::string& prefix)
{
    const auto osdNode = ctx.reqNode.select_node(".//*[local-name()='OSD']").node();
    auto osd = parseOsdXml(osdNode);
    if (osd.token.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing OSD token", 400);
    }
    const auto handler = m_ctx.getOsdHandler ? m_ctx.getOsdHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetOSD(osd));
    }
    m_osds[osd.token] = osd;
    return SoapHandlerResult::ok("    <" + prefix + ":SetOSDResponse/>\r\n");
}

SoapHandlerResult MediaService::handleGetOSDOptions(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetOSDOptionsResponse>\r\n"
       << "      <" << prefix << ":OSDOptions>\r\n"
       << "        <tt:MaximumNumberOfOSDs Total=\"10\" Image=\"2\" PlainText=\"8\" DateAndTime=\"4\"/>\r\n"
       << "        <tt:Type>Text</tt:Type>\r\n"
       << "        <tt:PositionOption>UpperLeft</tt:PositionOption>\r\n"
       << "        <tt:PositionOption>UpperRight</tt:PositionOption>\r\n"
       << "        <tt:PositionOption>LowerLeft</tt:PositionOption>\r\n"
       << "        <tt:PositionOption>LowerRight</tt:PositionOption>\r\n"
       << "        <tt:PositionOption>Custom</tt:PositionOption>\r\n"
       << "        <tt:TextOption>\r\n"
       << "          <tt:Type>Plain</tt:Type>\r\n"
       << "          <tt:Type>DateAndTime</tt:Type>\r\n"
       << "          <tt:FontSizeRange Min=\"12\" Max=\"64\"/>\r\n"
       << "          <tt:DateFormat>YYYY/MM/DD</tt:DateFormat>\r\n"
       << "          <tt:TimeFormat>HH:mm:ss</tt:TimeFormat>\r\n"
       << "        </tt:TextOption>\r\n"
       << "      </" << prefix << ":OSDOptions>\r\n"
       << "    </" << prefix << ":GetOSDOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetMasks(const SoapContext&, const std::string& prefix)
{
    std::vector<PrivacyMask> list;
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    if (handler) {
        list = handler->handleGetMasks("");
    }
    if (list.empty()) {
        list = m_masks;
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetMasksResponse>\r\n";
    for (const auto& m : list) {
        ss << MediaXmlBuilder::buildMaskXml(m, prefix);
    }
    ss << "    </" << prefix << ":GetMasksResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetMask(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='Token']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    std::optional<PrivacyMask> found;
    if (handler) {
        found = handler->handleGetMask(tok);
    }
    if (!found) {
        for (const auto& m : m_masks) {
            if (m.token == tok) {
                found = m;
                break;
            }
        }
    }
    if (found) {
        std::ostringstream ss;
        ss << "    <" << prefix << ":GetMaskResponse>\r\n"
           << MediaXmlBuilder::buildMaskXml(*found, prefix) << "    </" << prefix << ":GetMaskResponse>\r\n";
        return SoapHandlerResult::ok(ss.str());
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such mask", 400);
}

static PrivacyMask parseMaskNode(const pugi::xml_node& maskNode)
{
    PrivacyMask mask;
    if (!maskNode) {
        return mask;
    }
    if (maskNode.attribute("token")) {
        mask.token = maskNode.attribute("token").as_string();
    } else if (maskNode.attribute("Token")) {
        mask.token = maskNode.attribute("Token").as_string();
    }
    const auto cfg = maskNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    if (cfg) {
        mask.configurationToken = cfg.text().as_string();
    }
    const auto polyNode = maskNode.select_node(".//*[local-name()='Polygon']").node();
    if (polyNode) {
        mask.polygon = Xml::parsePoint2DList(polyNode);
    }
    const auto typeNode = maskNode.select_node(".//*[local-name()='Type']").node();
    if (typeNode) {
        mask.type = stringToMaskType(typeNode.text().as_string("Color"));
    }
    const auto enabledNode = maskNode.select_node(".//*[local-name()='Enabled']").node();
    if (enabledNode) {
        mask.enabled = enabledNode.text().as_bool(true);
    }
    return mask;
}

SoapHandlerResult MediaService::handleCreateMask(const SoapContext& ctx, const std::string& prefix)
{
    const auto maskNode = ctx.reqNode.select_node(".//*[local-name()='Mask']").node();
    PrivacyMask mask = parseMaskNode(maskNode);
    if (mask.token.empty()) {
        mask.token = "Mask_" + std::to_string(m_nextMaskId++);
    }
    if (mask.configurationToken.empty()) {
        mask.configurationToken = "VideoSource_1";
    }
    std::string assignedToken = mask.token;
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    if (handler) {
        const auto assigned = handler->handleCreateMask(mask);
        if (!assigned.empty()) {
            assignedToken = assigned;
            mask.token = assigned;
        }
    }
    m_masks.push_back(mask);
    std::ostringstream ss;
    ss << "    <" << prefix << ":CreateMaskResponse>\r\n"
       << "      <" << prefix << ":Token>" << assignedToken << "</" << prefix << ":Token>\r\n"
       << "    </" << prefix << ":CreateMaskResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleDeleteMask(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='Token']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleDeleteMask(tok));
    }
    m_masks.erase(
        std::remove_if(m_masks.begin(), m_masks.end(), [&tok](const PrivacyMask& m) { return m.token == tok; }),
        m_masks.end());
    return SoapHandlerResult::ok("    <" + prefix + ":DeleteMaskResponse/>\r\n");
}

SoapHandlerResult MediaService::handleSetMask(const SoapContext& ctx, const std::string& prefix)
{
    const auto maskNode = ctx.reqNode.select_node(".//*[local-name()='Mask']").node();
    PrivacyMask mask = parseMaskNode(maskNode);
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetMask(mask));
    }
    const auto it
        = std::find_if(m_masks.begin(), m_masks.end(), [&](const PrivacyMask& m) { return m.token == mask.token; });
    if (it != m_masks.end()) {
        *it = mask;
    } else {
        m_masks.push_back(mask);
    }
    return SoapHandlerResult::ok("    <" + prefix + ":SetMaskResponse/>\r\n");
}

SoapHandlerResult MediaService::handleGetMaskOptions(const SoapContext& ctx, const std::string& prefix)
{
    const auto cfgNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string cfgTok = cfgNode ? cfgNode.text().as_string() : "";
    MaskOptions opts = m_maskOptions;
    const auto handler = m_ctx.getMaskHandler ? m_ctx.getMaskHandler() : nullptr;
    if (handler) {
        opts = handler->handleGetMaskOptions(cfgTok);
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetMaskOptionsResponse>\r\n"
       << "      <" << prefix << ":Options MaxMasks=\"" << opts.maxMasks << "\" MaxPoints=\"" << opts.maxPoints
       << "\" Rectangle=\"" << (opts.rectangleSupported ? "true" : "false") << "\" RectangleSupported=\""
       << (opts.rectangleSupported ? "true" : "false") << "\" Polygon=\"" << (opts.polygonSupported ? "true" : "false")
       << "\" PolygonSupported=\"" << (opts.polygonSupported ? "true" : "false") << "\">\r\n"
       << "        <" << prefix << ":MaxMasks>" << opts.maxMasks << "</" << prefix << ":MaxMasks>\r\n"
       << "        <" << prefix << ":MaxPoints>" << opts.maxPoints << "</" << prefix << ":MaxPoints>\r\n";
    for (const auto& t : opts.supportedTypes) {
        ss << "        <" << prefix << ":Types>" << maskTypeToString(t) << "</" << prefix << ":Types>\r\n"
           << "        <" << prefix << ":SupportedTypes>" << maskTypeToString(t) << "</" << prefix
           << ":SupportedTypes>\r\n";
    }
    for (const auto& cs : opts.supportedColorSpaces) {
        ss << "        <" << prefix << ":SupportedColorSpaces>" << Xml::escapeXmlText(cs) << "</" << prefix
           << ":SupportedColorSpaces>\r\n";
    }
    ss << "      </" << prefix << ":Options>\r\n"
       << "    </" << prefix << ":GetMaskOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetVideoModes(const SoapContext&, const std::string& prefix)
{
    std::vector<VideoSourceMode> modes;
    const auto handler = m_ctx.getVideoSourceModeHandler ? m_ctx.getVideoSourceModeHandler() : nullptr;
    if (handler) {
        modes = handler->handleGetVideoSourceModes("VideoSource_1");
    }
    if (modes.empty()) {
        VideoSourceMode m1;
        m1.token = "Mode_1080P";
        m1.width = 1920;
        m1.height = 1080;
        m1.maxFramerate = 30.0f;
        modes.push_back(m1);
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoSourceModesResponse>\r\n";
    for (const auto& m : modes) {
        ss << "      <" << prefix << ":VideoSourceModes token=\"" << Xml::escapeXmlAttr(m.token) << "\">\r\n"
           << "        <tt:MaxFramerate>" << m.maxFramerate << "</tt:MaxFramerate>\r\n"
           << "        <tt:MaxResolution><tt:Width>" << m.width << "</tt:Width><tt:Height>" << m.height
           << "</tt:Height></tt:MaxResolution>\r\n"
           << "      </" << prefix << ":VideoSourceModes>\r\n";
    }
    ss << "    </" << prefix << ":GetVideoSourceModesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleSetVideoMode(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceModeToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    bool rebootNeeded = false;
    const auto handler = m_ctx.getVideoSourceModeHandler ? m_ctx.getVideoSourceModeHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetVideoSourceMode("VideoSource_1", tok, rebootNeeded));
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":SetVideoSourceModeResponse><" << prefix << ":Reboot>"
       << (rebootNeeded ? "true" : "false") << "</" << prefix << ":Reboot></" << prefix
       << ":SetVideoSourceModeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetMetaConfigs(const SoapContext&, const std::string& prefix)
{
    std::vector<MetadataConfiguration> list;
    const auto handler = m_ctx.getMetadataHandler ? m_ctx.getMetadataHandler() : nullptr;
    if (handler) {
        list = handler->handleGetMetadataConfigs();
    }
    if (list.empty()) {
        list = m_metadataConfigs;
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetMetadataConfigurationsResponse>\r\n";
    for (const auto& c : list) {
        ss << MediaXmlBuilder::buildMetaConfigXml(c, prefix);
    }
    ss << "    </" << prefix << ":GetMetadataConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetMetaConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto handler = m_ctx.getMetadataHandler ? m_ctx.getMetadataHandler() : nullptr;
    std::optional<MetadataConfiguration> found;
    if (handler) {
        const auto list = handler->handleGetMetadataConfigs();
        for (const auto& c : list) {
            if (c.token == tok) {
                found = c;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& c : m_metadataConfigs) {
            if (c.token == tok) {
                found = c;
                break;
            }
        }
    }
    if (found) {
        std::ostringstream ss;
        ss << "    <" << prefix << ":GetMetadataConfigurationResponse>\r\n"
           << MediaXmlBuilder::buildMetaConfigXml(*found, prefix) << "    </" << prefix
           << ":GetMetadataConfigurationResponse>\r\n";
        return SoapHandlerResult::ok(ss.str());
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such metadata configuration", 400);
}

SoapHandlerResult MediaService::handleSetMetaConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto cfgNode = ctx.reqNode.select_node(".//*[local-name()='Configuration']").node();
    if (!cfgNode) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing Configuration", 400);
    }
    MetadataConfiguration cfg;
    cfg.token = cfgNode.attribute("token").as_string();
    const auto nameNode = cfgNode.select_node(".//*[local-name()='Name']").node();
    if (nameNode) {
        cfg.name = nameNode.text().as_string();
    }
    const auto ptzNode = cfgNode.select_node(".//*[local-name()='PTZStatus']").node();
    cfg.ptzStatusEnabled = (ptzNode != nullptr);
    const auto anaNode = cfgNode.select_node(".//*[local-name()='Analytics']").node();
    if (anaNode) {
        cfg.analyticsEnabled = anaNode.text().as_bool(true);
    }
    const auto evNode = cfgNode.select_node(".//*[local-name()='Events']").node();
    if (evNode) {
        cfg.eventsEnabled = evNode.text().as_bool(true);
    }
    const auto geoNode = cfgNode.select_node(".//*[local-name()='GeoLocation']").node();
    if (geoNode) {
        cfg.geoOrientationEnabled = geoNode.text().as_bool(true);
    }

    const auto handler = m_ctx.getMetadataHandler ? m_ctx.getMetadataHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetMetadataConfig(cfg));
    }
    const auto it = std::find_if(
        m_metadataConfigs.begin(), m_metadataConfigs.end(), [&](const auto& c) { return c.token == cfg.token; });
    if (it != m_metadataConfigs.end()) {
        *it = cfg;
    } else {
        m_metadataConfigs.push_back(cfg);
    }
    return SoapHandlerResult::ok("    <" + prefix + ":SetMetadataConfigurationResponse/>\r\n");
}

} // namespace Onvif
