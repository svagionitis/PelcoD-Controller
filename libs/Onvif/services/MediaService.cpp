/// @file MediaService.cpp
#include "MediaService.h"

#include <cstdint>

#include <algorithm>
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
    constexpr std::string_view kMedia1Ns { "http://www.onvif.org/ver10/media/wsdl" };
    constexpr std::string_view kMedia2Ns { "http://www.onvif.org/ver20/media/wsdl" };
}

MediaService::MediaService(MediaServiceContext ctx)
    : m_ctx(std::move(ctx))
    , m_profiles(m_ctx.config.defaultMediaProfiles)
    , m_videoSources(m_ctx.config.defaultVideoSourceConfigs)
    , m_videoEncoders(m_ctx.config.defaultVideoEncoderConfigs)
    , m_masks(m_ctx.config.defaultMasks)
{
    if (m_profiles.empty()) {
        ServerMediaProfile p;
        p.token = "ProfileToken_1";
        p.name = "MainProfile";
        p.fixed = true;
        p.vscToken = "VideoSourceConfig_1";
        p.vecToken = "VideoEncoderConfig_1";
        p.ptzToken = "PTZConfig_1";
        p.metadataToken = "MetadataConfig_1";
        m_profiles.push_back(p);
    }

    if (m_videoSources.empty()) {
        VideoSourceConfig vs;
        vs.token = "VideoSourceConfig_1";
        vs.name = "VideoSourceConfig";
        vs.sourceToken = "VideoSource_1";
        vs.width = 1920;
        vs.height = 1080;
        m_videoSources.push_back(vs);
    }

    if (m_videoEncoders.empty()) {
        VideoEncoderConfig vec;
        vec.token = "VideoEncoderConfig_1";
        vec.name = "H264_1080P";
        vec.encoding = "H264";
        vec.width = 1920;
        vec.height = 1080;
        vec.quality = 5.0F;
        vec.frameRateLimit = 30.0F;
        vec.bitrateLimitKbps = 4096;
        vec.govLength = 30;
        vec.h264Profile = "Main";
        m_videoEncoders.push_back(vec);
    }

    OsdConfig defOsd;
    defOsd.token = "OSD_1";
    defOsd.videoSourceToken = "VideoSource_1";
    defOsd.type = OsdType::Text;
    defOsd.position = OsdPositionType::UpperLeft;
    defOsd.plainText = "PelcoD Camera";
    defOsd.fontSize = 24U;
    m_osds[defOsd.token] = defOsd;

    MetadataConfiguration defMeta;
    defMeta.token = "MetadataConfig_1";
    defMeta.name = "DefaultMetadataConfig";
    defMeta.ptzStatusEnabled = true;
    defMeta.analyticsEnabled = true;
    defMeta.eventsEnabled = true;
    defMeta.geoOrientationEnabled = true;
    m_metadataConfigs.push_back(defMeta);
}

std::string_view MediaService::serviceName() const noexcept
{
    return "Media";
}

void MediaService::registerRoutes(SoapRouter& router)
{
    for (const auto& ns : { std::string(kMedia1Ns), std::string(kMedia2Ns) }) {
        const std::string pfx = (ns == kMedia2Ns) ? "tr2" : "trt";

        router.registerRoute(ns, "GetProfiles", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetProfiles(ctx, pfx); });
        router.registerRoute(ns, "GetProfile", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetProfile(ctx, pfx); });
        router.registerRoute(ns, "CreateProfile", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleCreateProfile(ctx, pfx); });
        router.registerRoute(ns, "DeleteProfile", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleDeleteProfile(ctx, pfx); });
        router.registerRoute(ns, "GetVideoSources", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoSources(ctx, pfx); });
        router.registerRoute(ns, "GetVideoSourceConfigurations", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoSourceConfigs(ctx, pfx); });
        router.registerRoute(ns, "GetVideoSourceConfiguration", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoSourceConfig(ctx, pfx); });
        router.registerRoute(ns, "GetVideoEncoderConfigurations", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoConfigs(ctx, pfx); });
        router.registerRoute(ns, "GetVideoEncoderConfiguration", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoConfig(ctx, pfx); });
        router.registerRoute(ns, "SetVideoEncoderConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleSetVideoEncoderConfig(ctx, pfx); });
        router.registerRoute(ns, "GetVideoEncoderConfigurationOptions", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoOptions(ctx, pfx); });
        router.registerRoute(ns, "GetStreamUri", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetStreamUri(ctx, pfx); });
        router.registerRoute(ns, "GetSnapshotUri", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetSnapshotUri(ctx, pfx); });
        router.registerRoute(ns, "AddVideoEncoderConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleAddVideoEncoder(ctx, pfx); });
        router.registerRoute(ns, "RemoveVideoEncoderConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleRemoveVideoEncoder(ctx, pfx); });
        router.registerRoute(ns, "AddVideoSourceConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleAddVideoSourceConfig(ctx, pfx); });
        router.registerRoute(ns, "RemoveVideoSourceConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleRemoveVideoSourceConfig(ctx, pfx); });
        router.registerRoute(ns, "AddPTZConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleAddPTZConfig(ctx, pfx); });
        router.registerRoute(ns, "RemovePTZConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleRemovePTZConfig(ctx, pfx); });
        router.registerRoute(ns, "GetCompatibleVideoSourceConfigurations", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetCompatibleVsc(ctx, pfx); });
        router.registerRoute(ns, "GetCompatibleVideoEncoderConfigurations", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetCompatibleVec(ctx, pfx); });
        router.registerRoute(ns, "GetGuaranteedNumberOfVideoEncoderInstances", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetGuaranteedEncoder(ctx, pfx); });
        router.registerRoute(ns, "GetOSDs", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetOSDs(ctx, pfx); });
        router.registerRoute(ns, "GetOSD", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetOSD(ctx, pfx); });
        router.registerRoute(ns, "CreateOSD", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleCreateOSD(ctx, pfx); });
        router.registerRoute(ns, "DeleteOSD", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleDeleteOSD(ctx, pfx); });
        router.registerRoute(ns, "SetOSD", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleSetOSD(ctx, pfx); });
        router.registerRoute(ns, "GetOSDOptions", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetOSDOptions(ctx, pfx); });
        router.registerRoute(ns, "GetMasks", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMasks(ctx, pfx); });
        router.registerRoute(ns, "GetMask", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMask(ctx, pfx); });
        router.registerRoute(ns, "CreateMask", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleCreateMask(ctx, pfx); });
        router.registerRoute(ns, "DeleteMask", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleDeleteMask(ctx, pfx); });
        router.registerRoute(ns, "SetMask", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleSetMask(ctx, pfx); });
        router.registerRoute(ns, "GetMaskOptions", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMaskOptions(ctx, pfx); });
        router.registerRoute(ns, "GetVideoSourceModes", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetVideoModes(ctx, pfx); });
        router.registerRoute(ns, "SetVideoSourceMode", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleSetVideoMode(ctx, pfx); });
        router.registerRoute(ns, "GetMetadataConfigurations", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMetaConfigs(ctx, pfx); });
        router.registerRoute(ns, "GetMetadataConfiguration", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMetaConfig(ctx, pfx); });
        router.registerRoute(ns, "SetMetadataConfiguration", AccessClass::WriteSystem,
            [this, pfx](const SoapContext& ctx) { return handleSetMetaConfig(ctx, pfx); });
        router.registerRoute(ns, "GetMetadataConfigurationOptions", AccessClass::ReadMedia,
            [this, pfx](const SoapContext& ctx) { return handleGetMetaOptions(ctx, pfx); });
        router.registerRoute(ns, "SetSynchronizationPoint", AccessClass::Actuate, [this, pfx](const SoapContext& ctx) {
            const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
            const std::string tok = tokNode ? tokNode.text().as_string() : "";
            const auto it = std::find_if(
                m_profiles.begin(), m_profiles.end(), [&tok](const ServerMediaProfile& p) { return p.token == tok; });
            if (it == m_profiles.end() || tok.empty()) {
                return SoapHandlerResult::fault(
                    "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
            }
            return SoapHandlerResult::ok("    <" + pfx + ":SetSynchronizationPointResponse/>\r\n");
        });
        router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [pfx](const SoapContext&) {
            return SoapHandlerResult::ok("    <" + pfx
                + ":GetServiceCapabilitiesResponse>\r\n"
                  "      <"
                + pfx
                + ":Capabilities SnapshotUri=\"true\" Rotation=\"false\" VideoSourceMode=\"true\" OSD=\"true\" "
                  "Mask=\"true\">\r\n"
                  "        <"
                + pfx
                + ":ProfileCapabilities MaximumNumberOfProfiles=\"8\"/>\r\n"
                  "        <"
                + pfx
                + ":StreamingCapabilities RTPMulticast=\"false\" RTP_TCP=\"true\" RTP_RTSP_TCP=\"true\" "
                  "NonAggregateControl=\"false\"/>\r\n"
                  "      </"
                + pfx
                + ":Capabilities>\r\n"
                  "    </"
                + pfx + ":GetServiceCapabilitiesResponse>\r\n");
        });
    }
}

SoapHandlerResult MediaService::handleGetProfiles(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetProfilesResponse>\r\n";
    for (const auto& prof : m_profiles) {
        ss << MediaXmlBuilder::buildProfileXml(prof, prefix, false);
    }
    ss << "    </" << prefix << ":GetProfilesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetProfile(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    for (const auto& prof : m_profiles) {
        if (prof.token == tok) {
            std::ostringstream ss;
            ss << "    <" << prefix << ":GetProfileResponse>\r\n"
               << MediaXmlBuilder::buildProfileXml(prof, prefix, true) << "    </" << prefix
               << ":GetProfileResponse>\r\n";
            return SoapHandlerResult::ok(ss.str());
        }
    }
    return SoapHandlerResult::fault(
        "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
}

SoapHandlerResult MediaService::handleCreateProfile(const SoapContext& ctx, const std::string& prefix)
{
    const auto nameNode = ctx.reqNode.select_node(".//*[local-name()='Name']").node();
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='Token']").node();

    ServerMediaProfile p;
    p.token = tokNode ? tokNode.text().as_string() : "Profile_" + std::to_string(m_nextProfileId++);
    p.name = nameNode ? nameNode.text().as_string() : "Profile";
    p.fixed = false;
    m_profiles.push_back(p);

    std::ostringstream ss;
    ss << "    <" << prefix << ":CreateProfileResponse>\r\n"
       << MediaXmlBuilder::buildProfileXml(p, prefix, true) << "    </" << prefix << ":CreateProfileResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleDeleteProfile(const SoapContext& ctx, const std::string& prefix)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    auto it = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&tok](const ServerMediaProfile& p) { return p.token == tok; });
    if (it == m_profiles.end() || tok.empty()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    if (it->fixed) {
        return SoapHandlerResult::fault("ter:Action", "Cannot delete fixed profile", 400);
    }
    m_profiles.erase(it);
    return SoapHandlerResult::ok("    <" + prefix + ":DeleteProfileResponse/>\r\n");
}

SoapHandlerResult MediaService::handleGetVideoSources(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoSourcesResponse>\r\n";
    for (const auto& vsc : m_videoSources) {
        ss << MediaXmlBuilder::buildVideoSourceXml(vsc, prefix, "VideoSources");
    }
    ss << "    </" << prefix << ":GetVideoSourcesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetVideoConfigs(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoEncoderConfigurationsResponse>\r\n";
    for (const auto& vec : m_videoEncoders) {
        ss << MediaXmlBuilder::buildVideoEncoderXml(vec, prefix, "Configurations");
    }
    ss << "    </" << prefix << ":GetVideoEncoderConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetVideoOptions(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetVideoEncoderConfigurationOptionsResponse>\r\n"
       << "      <" << prefix << ":Options>\r\n"
       << "        <tt:QualityRange><tt:Min>1</tt:Min><tt:Max>10</tt:Max></tt:QualityRange>\r\n"
       << "        <tt:H264>\r\n"
       << "          "
          "<tt:ResolutionsAvailable><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:ResolutionsAvailable>\r\n"
       << "          "
          "<tt:ResolutionsAvailable><tt:Width>1280</tt:Width><tt:Height>720</tt:Height></tt:ResolutionsAvailable>\r\n"
       << "          "
          "<tt:ResolutionsAvailable><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:ResolutionsAvailable>\r\n"
       << "          <tt:GovLengthRange Min=\"1\" "
          "Max=\"120\"><tt:Min>1</tt:Min><tt:Max>120</tt:Max></tt:GovLengthRange>\r\n"
       << "          <tt:FrameRateRange Min=\"1\" "
          "Max=\"30\"><tt:Min>1</tt:Min><tt:Max>30</tt:Max></tt:FrameRateRange>\r\n"
       << "          <tt:EncodingIntervalRange Min=\"1\" "
          "Max=\"1\"><tt:Min>1</tt:Min><tt:Max>1</tt:Max></tt:EncodingIntervalRange>\r\n"
       << "          <tt:H264ProfilesSupported>Baseline</tt:H264ProfilesSupported>\r\n"
       << "          <tt:H264ProfilesSupported>Main</tt:H264ProfilesSupported>\r\n"
       << "          <tt:H264ProfilesSupported>High</tt:H264ProfilesSupported>\r\n"
       << "        </tt:H264>\r\n"
       << "      </" << prefix << ":Options>\r\n"
       << "    </" << prefix << ":GetVideoEncoderConfigurationOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult MediaService::handleGetStreamUri(const SoapContext& ctx, const std::string& prefix)
{
    std::string uri = m_ctx.config.rtspStreamUri;
    if (uri.empty()) {
        const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
        uri = "rtsp://" + host + ":8554/live/ch0";
    }
    return SoapHandlerResult::ok(MediaXmlBuilder::buildStreamUriXml(uri, prefix));
}

SoapHandlerResult MediaService::handleGetSnapshotUri(const SoapContext& ctx, const std::string& prefix)
{
    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    const std::string uri = "http://" + host + ":" + std::to_string(m_ctx.config.port) + "/onvif/snapshot";
    return SoapHandlerResult::ok(MediaXmlBuilder::buildSnapshotUriXml(uri, prefix));
}

SoapHandlerResult MediaService::handleGetMetaOptions(const SoapContext&, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "    <" << prefix << ":GetMetadataConfigurationOptionsResponse>\r\n"
       << "      <" << prefix << ":Options>\r\n"
       << "        <tt:PTZStatusSupported>true</tt:PTZStatusSupported>\r\n"
       << "        <tt:AnalyticsSupported>true</tt:AnalyticsSupported>\r\n"
       << "        <tt:EventsSupported>true</tt:EventsSupported>\r\n"
       << "      </" << prefix << ":Options>\r\n"
       << "    </" << prefix << ":GetMetadataConfigurationOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

} // namespace Onvif
