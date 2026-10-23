/// @file MediaServiceConfigs.cpp
/// @brief Configuration and attachment handlers for MediaService.

#include "MediaService.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <sstream>
#include <string>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "XmlUtils.h"
#include "builders/MediaXmlBuilder.h"

namespace Onvif {

SoapHandlerResult MediaService::handleGetVideoSourceConfigs(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoSourceConfigurationsResponse>\r\n";
    for (const auto& vsc : m_videoSources) {
        ss << MediaXmlBuilder::buildVideoSourceXml(vsc, prefix, "Configurations");
    }
    ss << "    </" << prefix << ":GetVideoSourceConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetVideoSourceConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto it = std::find_if(
        m_videoSources.begin(), m_videoSources.end(), [&tok](const VideoSourceConfig& c) { return c.token == tok; });
    if (it == m_videoSources.end() || tok.empty()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested configuration token does not exist.", 400, "ter:NoConfig");
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoSourceConfigurationResponse>\r\n"
       << MediaXmlBuilder::buildVideoSourceXml(*it, prefix, "Configuration") << "    </" << prefix
       << ":GetVideoSourceConfigurationResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetVideoConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto it = std::find_if(
        m_videoEncoders.begin(), m_videoEncoders.end(), [&tok](const VideoEncoderConfig& c) { return c.token == tok; });
    if (it == m_videoEncoders.end() || tok.empty()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested configuration token does not exist.", 400, "ter:NoConfig");
    }
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoEncoderConfigurationResponse>\r\n"
       << MediaXmlBuilder::buildVideoEncoderXml(*it, prefix, "Configuration") << "    </" << prefix
       << ":GetVideoEncoderConfigurationResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleSetVideoEncoderConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto cfgNode = ctx.reqNode.select_node(".//*[local-name()='Configuration']").node();
    if (!cfgNode) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing Configuration node", 400);
    }
    const std::string tok = cfgNode.attribute("token").as_string();
    auto it = std::find_if(
        m_videoEncoders.begin(), m_videoEncoders.end(), [&tok](const VideoEncoderConfig& c) { return c.token == tok; });
    if (it == m_videoEncoders.end() || tok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such configuration", 400, "ter:NoConfig");
    }

    const auto nameNode = cfgNode.select_node(".//*[local-name()='Name']").node();
    if (nameNode) {
        it->name = nameNode.text().as_string();
    }

    const auto resNode = cfgNode.select_node(".//*[local-name()='Resolution']").node();
    if (resNode) {
        const int w = resNode.select_node(".//*[local-name()='Width']").node().text().as_int(0);
        const int h = resNode.select_node(".//*[local-name()='Height']").node().text().as_int(0);
        if (w <= 0 || h <= 0 || w > 3840 || h > 2160) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Resolution out of range", 400);
        }
        it->width = w;
        it->height = h;
    }

    const auto rcNode = cfgNode.select_node(".//*[local-name()='RateControl']").node();
    if (rcNode) {
        const auto fpsNode = rcNode.select_node(".//*[local-name()='FrameRateLimit']").node();
        if (fpsNode) {
            const float fps = fpsNode.text().as_float(0.0f);
            if (std::isnan(fps) || std::isinf(fps) || fps < 1.0f || fps > 60.0f) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "FrameRateLimit out of range [1, 60]", 400);
            }
            it->frameRateLimit = fps;
        }
        const auto brNode = rcNode.select_node(".//*[local-name()='BitrateLimit']").node();
        if (brNode) {
            const int br = brNode.text().as_int(0);
            if (br < 64 || br > 32768) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "BitrateLimit out of range", 400);
            }
            it->bitrateLimitKbps = br;
        }
    }

    const auto qualNode = cfgNode.select_node(".//*[local-name()='Quality']").node();
    if (qualNode) {
        const float q = qualNode.text().as_float(0.0f);
        if (!std::isnan(q) && !std::isinf(q) && q >= 1.0f && q <= 100.0f) {
            it->quality = q;
        }
    }

    const auto h264Node = cfgNode.select_node(".//*[local-name()='H264']").node();
    if (h264Node) {
        const auto govNode = h264Node.select_node(".//*[local-name()='GovLength']").node();
        if (govNode) {
            const int gov = govNode.text().as_int(0);
            if (gov < 1 || gov > 300) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "GovLength out of range [1, 300]", 400);
            }
            it->govLength = gov;
        }
        const auto profSubNode = h264Node.select_node(".//*[local-name()='H264Profile']").node();
        if (profSubNode) {
            it->h264Profile = profSubNode.text().as_string();
        }
    }

    return SoapHandlerResult::ok("    <" + prefix + ":SetVideoEncoderConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleAddVideoEncoder(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    const auto cTokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string cTok = cTokNode ? cTokNode.text().as_string() : "";

    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    auto itC = std::find_if(m_videoEncoders.begin(), m_videoEncoders.end(),
        [&cTok](const VideoEncoderConfig& c) { return c.token == cTok; });
    if (itC == m_videoEncoders.end() || cTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such configuration", 400, "ter:NoConfig");
    }
    itP->vecToken = cTok;
    return SoapHandlerResult::ok("    <" + prefix + ":AddVideoEncoderConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleRemoveVideoEncoder(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    itP->vecToken.clear();
    return SoapHandlerResult::ok("    <" + prefix + ":RemoveVideoEncoderConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleAddVideoSourceConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    const auto cTokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string cTok = cTokNode ? cTokNode.text().as_string() : "";

    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    auto itC = std::find_if(
        m_videoSources.begin(), m_videoSources.end(), [&cTok](const VideoSourceConfig& c) { return c.token == cTok; });
    if (itC == m_videoSources.end() || cTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such configuration", 400, "ter:NoConfig");
    }
    itP->vscToken = cTok;
    return SoapHandlerResult::ok("    <" + prefix + ":AddVideoSourceConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleRemoveVideoSourceConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    itP->vscToken.clear();
    return SoapHandlerResult::ok("    <" + prefix + ":RemoveVideoSourceConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleAddPTZConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    const auto cTokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string cTok = cTokNode ? cTokNode.text().as_string() : "";

    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    if (cTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such configuration", 400, "ter:NoConfig");
    }
    itP->ptzToken = cTok;
    return SoapHandlerResult::ok("    <" + prefix + ":AddPTZConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleRemovePTZConfig(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    itP->ptzToken.clear();
    return SoapHandlerResult::ok("    <" + prefix + ":RemovePTZConfigurationResponse/>\r\n");
}

SoapHandlerResult MediaService::handleGetCompatibleVsc(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    std::ostringstream ss;
    ss << "    <" + prefix + ":GetCompatibleVideoSourceConfigurationsResponse>\r\n";
    for (const auto& vsc : m_videoSources) {
        ss << MediaXmlBuilder::buildVideoSourceXml(vsc, prefix, "Configurations");
    }
    ss << "    </" + prefix + ":GetCompatibleVideoSourceConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetCompatibleVec(const SoapContext& ctx, const std::string& prefix)
{
    const auto pTokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string pTok = pTokNode ? pTokNode.text().as_string() : "";
    auto itP = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&pTok](const ServerMediaProfile& p) { return p.token == pTok; });
    if (itP == m_profiles.end() || pTok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such profile", 400, "ter:NoProfile");
    }
    std::ostringstream ss;
    ss << "    <" + prefix + ":GetCompatibleVideoEncoderConfigurationsResponse>\r\n";
    for (const auto& vec : m_videoEncoders) {
        ss << MediaXmlBuilder::buildVideoEncoderXml(vec, prefix, "Configurations");
    }
    ss << "    </" + prefix + ":GetCompatibleVideoEncoderConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetGuaranteedEncoder(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetGuaranteedNumberOfVideoEncoderInstancesResponse>\r\n"
       << "      <" << prefix << ":TotalNumber>2</" << prefix << ":TotalNumber>\r\n"
       << "      <" << prefix << ":H264>2</" << prefix << ":H264>\r\n"
       << "    </" << prefix << ":GetGuaranteedNumberOfVideoEncoderInstancesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

} // namespace Onvif
