/// @file MediaXmlBuilder.cpp
#include "MediaXmlBuilder.h"

#include <cstdint>

#include <sstream>
#include <string>

#include "XmlUtils.h"

namespace Onvif {

namespace {

    [[nodiscard]] const char* osdPositionToString(OsdPositionType pos) noexcept
    {
        switch (pos) {
        case OsdPositionType::UpperRight:
            return "UpperRight";
        case OsdPositionType::LowerLeft:
            return "LowerLeft";
        case OsdPositionType::LowerRight:
            return "LowerRight";
        case OsdPositionType::Custom:
            return "Custom";
        case OsdPositionType::UpperLeft:
        default:
            return "UpperLeft";
        }
    }

} // namespace

std::string MediaXmlBuilder::buildVideoSourceXml(
    const VideoSourceConfig& cfg, const std::string& prefix, const std::string& elem)
{
    std::ostringstream ss;
    ss << "      <" << prefix << ":" << elem << " token=\"" << Xml::escapeXmlAttr(cfg.token) << "\">\r\n"
       << "        <tt:Name>" << Xml::escapeXmlText(cfg.name) << "</tt:Name>\r\n"
       << "        <tt:UseCount>" << cfg.useCount << "</tt:UseCount>\r\n"
       << "        <tt:SourceToken>" << Xml::escapeXmlText(cfg.sourceToken) << "</tt:SourceToken>\r\n"
       << "        <tt:Bounds x=\"" << cfg.x << "\" y=\"" << cfg.y << "\" width=\"" << cfg.width << "\" height=\""
       << cfg.height << "\"/>\r\n"
       << "      </" << prefix << ":" << elem << ">\r\n";
    return ss.str();
}

std::string MediaXmlBuilder::buildVideoEncoderXml(
    const VideoEncoderConfig& cfg, const std::string& prefix, const std::string& elem)
{
    std::ostringstream ss;
    ss << "      <" << prefix << ":" << elem << " token=\"" << Xml::escapeXmlAttr(cfg.token) << "\">\r\n"
       << "        <tt:Name>" << Xml::escapeXmlText(cfg.name) << "</tt:Name>\r\n"
       << "        <tt:UseCount>" << cfg.useCount << "</tt:UseCount>\r\n"
       << "        <tt:Encoding>" << Xml::escapeXmlText(cfg.encoding) << "</tt:Encoding>\r\n"
       << "        <tt:Resolution>\r\n"
       << "          <tt:Width>" << cfg.width << "</tt:Width>\r\n"
       << "          <tt:Height>" << cfg.height << "</tt:Height>\r\n"
       << "        </tt:Resolution>\r\n"
       << "        <tt:Quality>" << cfg.quality << "</tt:Quality>\r\n"
       << "        <tt:RateControl>\r\n"
       << "          <tt:FrameRateLimit>" << static_cast<int>(cfg.frameRateLimit) << "</tt:FrameRateLimit>\r\n"
       << "          <tt:EncodingInterval>1</tt:EncodingInterval>\r\n"
       << "          <tt:BitrateLimit>" << cfg.bitrateLimitKbps << "</tt:BitrateLimit>\r\n"
       << "        </tt:RateControl>\r\n";
    if (cfg.encoding == "H264") {
        ss << "        <tt:H264>\r\n"
           << "          <tt:GovLength>" << cfg.govLength << "</tt:GovLength>\r\n"
           << "          <tt:H264Profile>" << Xml::escapeXmlText(cfg.h264Profile.empty() ? "Main" : cfg.h264Profile)
           << "</tt:H264Profile>\r\n"
           << "        </tt:H264>\r\n";
    }
    ss << "        <tt:Multicast>\r\n"
       << "          <tt:Address>\r\n"
       << "            <tt:Type>IPv4</tt:Type>\r\n"
       << "            <tt:IPv4Address>0.0.0.0</tt:IPv4Address>\r\n"
       << "          </tt:Address>\r\n"
       << "          <tt:Port>0</tt:Port>\r\n"
       << "          <tt:TTL>5</tt:TTL>\r\n"
       << "          <tt:AutoStart>false</tt:AutoStart>\r\n"
       << "        </tt:Multicast>\r\n"
       << "          <tt:SessionTimeout>"
       << Xml::escapeXmlText(cfg.sessionTimeout.empty() ? "PT30S" : cfg.sessionTimeout) << "</tt:SessionTimeout>\r\n"
       << "      </" << prefix << ":" << elem << ">\r\n";
    return ss.str();
}

std::string MediaXmlBuilder::buildProfileXml(const ServerMediaProfile& prof, const std::string& prefix, bool isSingle)
{
    const std::string tag = isSingle ? "Profile" : "Profiles";
    std::ostringstream ss;
    if (prefix == "tr2") {
        ss << "      <tr2:" << tag << " token=\"" << Xml::escapeXmlAttr(prof.token) << "\" fixed=\""
           << (prof.fixed ? "true" : "false") << "\">\r\n"
           << "        <tr2:Name>" << Xml::escapeXmlText(prof.name) << "</tr2:Name>\r\n"
           << "        <tr2:Configurations>\r\n"
           << "          <tr2:VideoSource token=\"" << Xml::escapeXmlAttr(prof.vscToken) << "\">\r\n"
           << "            <tr2:Name>VideoSourceConfig</tr2:Name>\r\n"
           << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
           << "            <tr2:SourceToken>VideoSource_1</tr2:SourceToken>\r\n"
           << "            <tr2:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/>\r\n"
           << "          </tr2:VideoSource>\r\n"
           << "          <tr2:VideoEncoder token=\"" << Xml::escapeXmlAttr(prof.vecToken) << "\">\r\n"
           << "            <tr2:Name>VideoEncoderConfig</tr2:Name>\r\n"
           << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
           << "            <tr2:Encoding>H264</tr2:Encoding>\r\n"
           << "            <tr2:Resolution>\r\n"
           << "              <tr2:Width>1920</tr2:Width>\r\n"
           << "              <tr2:Height>1080</tr2:Height>\r\n"
           << "            </tr2:Resolution>\r\n"
           << "            <tr2:RateControl>\r\n"
           << "              <tr2:FrameRateLimit>30</tr2:FrameRateLimit>\r\n"
           << "              <tr2:BitrateLimit>4096</tr2:BitrateLimit>\r\n"
           << "            </tr2:RateControl>\r\n"
           << "          </tr2:VideoEncoder>\r\n";
        if (!prof.ptzToken.empty()) {
            ss << "          <tr2:PTZ token=\"" << Xml::escapeXmlAttr(prof.ptzToken) << "\">\r\n"
               << "            <tr2:Name>PTZConfig</tr2:Name>\r\n"
               << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
               << "            <tr2:NodeToken>PTZNode_1</tr2:NodeToken>\r\n"
               << "          </tr2:PTZ>\r\n";
        }
        if (!prof.metadataToken.empty()) {
            ss << "          <tr2:Metadata token=\"" << Xml::escapeXmlAttr(prof.metadataToken) << "\">\r\n"
               << "            <tr2:Name>DefaultMetadataConfig</tr2:Name>\r\n"
               << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
               << "          </tr2:Metadata>\r\n";
        }
        ss << "        </tr2:Configurations>\r\n"
           << "      </tr2:" << tag << ">\r\n";
        return ss.str();
    }

    ss << "      <" << prefix << ":" << tag << " token=\"" << Xml::escapeXmlAttr(prof.token) << "\" fixed=\""
       << (prof.fixed ? "true" : "false") << "\">\r\n"
       << "        <tt:Name>" << Xml::escapeXmlText(prof.name) << "</tt:Name>\r\n";

    if (!prof.vscToken.empty()) {
        ss << "        <tt:VideoSourceConfiguration token=\"" << Xml::escapeXmlAttr(prof.vscToken) << "\">\r\n"
           << "          <tt:Name>VideoSourceConfig_1</tt:Name>\r\n"
           << "          <tt:UseCount>1</tt:UseCount>\r\n"
           << "          <tt:SourceToken>VideoSource_1</tt:SourceToken>\r\n"
           << "          <tt:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/>\r\n"
           << "        </tt:VideoSourceConfiguration>\r\n";
    }

    if (!prof.vecToken.empty()) {
        ss << "        <tt:VideoEncoderConfiguration token=\"" << Xml::escapeXmlAttr(prof.vecToken) << "\">\r\n"
           << "          <tt:Name>VideoEncoderConfig_1</tt:Name>\r\n"
           << "          <tt:UseCount>1</tt:UseCount>\r\n"
           << "          <tt:Encoding>H264</tt:Encoding>\r\n"
           << "          <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>\r\n"
           << "          <tt:Quality>5.0</tt:Quality>\r\n"
           << "          "
              "<tt:RateControl><tt:FrameRateLimit>30</tt:FrameRateLimit><tt:EncodingInterval>1</"
              "tt:EncodingInterval><tt:BitrateLimit>4096</tt:BitrateLimit></tt:RateControl>\r\n"
           << "          <tt:H264><tt:GovLength>30</tt:GovLength><tt:H264Profile>Main</tt:H264Profile></tt:H264>\r\n"
           << "          <tt:Multicast>\r\n"
           << "            <tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address>\r\n"
           << "            <tt:Port>0</tt:Port><tt:TTL>5</tt:TTL><tt:AutoStart>false</tt:AutoStart>\r\n"
           << "          </tt:Multicast>\r\n"
           << "          <tt:SessionTimeout>PT30S</tt:SessionTimeout>\r\n"
           << "        </tt:VideoEncoderConfiguration>\r\n";
    }

    if (!prof.ptzToken.empty()) {
        ss << "        <tt:PTZConfiguration token=\"" << Xml::escapeXmlAttr(prof.ptzToken) << "\">\r\n"
           << "          <tt:Name>PTZConfig_1</tt:Name>\r\n"
           << "          <tt:UseCount>1</tt:UseCount>\r\n"
           << "          <tt:NodeToken>PTZNode_1</tt:NodeToken>\r\n"
           << "          "
              "<tt:DefaultAbsolutePanTiltPositionSpace>http://www.onvif.org/ver10/tptz/PanTiltSpaces/"
              "PositionGenericSpace</tt:DefaultAbsolutePanTiltPositionSpace>\r\n"
           << "        </tt:PTZConfiguration>\r\n";
    }

    if (!prof.metadataToken.empty()) {
        ss << "        <tt:MetadataConfiguration token=\"" << Xml::escapeXmlAttr(prof.metadataToken) << "\">\r\n"
           << "          <tt:Name>DefaultMetadataConfig</tt:Name>\r\n"
           << "          <tt:UseCount>1</tt:UseCount>\r\n"
           << "        </tt:MetadataConfiguration>\r\n";
    }

    ss << "      </" << prefix << ":" << tag << ">\r\n";
    return ss.str();
}

std::string MediaXmlBuilder::buildStreamUriXml(const std::string& uri, const std::string& prefix)
{
    std::ostringstream ss;
    if (prefix == "tr2") {
        ss << "    <tr2:GetStreamUriResponse>\r\n"
           << "      <tr2:Uri>" << Xml::escapeXmlText(uri) << "</tr2:Uri>\r\n"
           << "    </tr2:GetStreamUriResponse>\r\n";
    } else {
        ss << "    <trt:GetStreamUriResponse>\r\n"
           << "      <trt:MediaUri>\r\n"
           << "        <tt:Uri>" << Xml::escapeXmlText(uri) << "</tt:Uri>\r\n"
           << "        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>\r\n"
           << "        <tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>\r\n"
           << "        <tt:Timeout>PT30S</tt:Timeout>\r\n"
           << "      </trt:MediaUri>\r\n"
           << "    </trt:GetStreamUriResponse>\r\n";
    }
    return ss.str();
}

std::string MediaXmlBuilder::buildSnapshotUriXml(const std::string& uri, const std::string& prefix)
{
    std::ostringstream ss;
    if (prefix == "tr2") {
        ss << "    <tr2:GetSnapshotUriResponse>\r\n"
           << "      <tr2:Uri>" << Xml::escapeXmlText(uri) << "</tr2:Uri>\r\n"
           << "    </tr2:GetSnapshotUriResponse>\r\n";
    } else {
        ss << "    <trt:GetSnapshotUriResponse>\r\n"
           << "      <trt:MediaUri>\r\n"
           << "        <tt:Uri>" << Xml::escapeXmlText(uri) << "</tt:Uri>\r\n"
           << "        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>\r\n"
           << "        <tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>\r\n"
           << "        <tt:Timeout>PT30S</tt:Timeout>\r\n"
           << "      </trt:MediaUri>\r\n"
           << "    </trt:GetSnapshotUriResponse>\r\n";
    }
    return ss.str();
}

std::string MediaXmlBuilder::buildOsdXml(const OsdConfig& osd, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "      <" << prefix << ":OSD token=\"" << Xml::escapeXmlAttr(osd.token) << "\">\r\n"
       << "        <tt:VideoSourceConfigurationToken>" << Xml::escapeXmlText(osd.videoSourceToken)
       << "</tt:VideoSourceConfigurationToken>\r\n"
       << "        <tt:Type>" << (osd.type == OsdType::Image ? "Image" : "Text") << "</tt:Type>\r\n"
       << "        <tt:Position><tt:Type>" << osdPositionToString(osd.position) << "</tt:Type></tt:Position>\r\n"
       << "        <tt:TextString><tt:Type>Plain</tt:Type><tt:PlainText>" << Xml::escapeXmlText(osd.plainText)
       << "</tt:PlainText><tt:FontSize>" << osd.fontSize << "</tt:FontSize></tt:TextString>\r\n"
       << "      </" << prefix << ":OSD>\r\n";
    return ss.str();
}

std::string MediaXmlBuilder::buildMaskXml(const PrivacyMask& mask, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "      <" << prefix << ":Mask token=\"" << Xml::escapeXmlAttr(mask.token) << "\">\r\n"
       << "        <" << prefix << ":ConfigurationToken>" << Xml::escapeXmlText(mask.configurationToken) << "</"
       << prefix << ":ConfigurationToken>\r\n"
       << "        <" << prefix << ":Polygon>\r\n";
    for (const auto& pt : mask.polygon) {
        ss << "          <tt:Point x=\"" << pt.x << "\" y=\"" << pt.y << "\"/>\r\n";
    }
    ss << "        </" << prefix << ":Polygon>\r\n"
       << "        <" << prefix << ":Type>" << maskTypeToString(mask.type) << "</" << prefix << ":Type>\r\n"
       << "        <" << prefix << ":Color X=\"" << mask.color.x << "\" Y=\"" << mask.color.y << "\" Z=\""
       << mask.color.z << "\" Colorspace=\"" << Xml::escapeXmlAttr(mask.color.colorspace) << "\"/>\r\n"
       << "        <" << prefix << ":Enabled>" << (mask.enabled ? "true" : "false") << "</" << prefix << ":Enabled>\r\n"
       << "      </" << prefix << ":Mask>\r\n";
    return ss.str();
}

std::string MediaXmlBuilder::buildMetaConfigXml(const MetadataConfiguration& cfg, const std::string& prefix)
{
    std::ostringstream ss;
    ss << "      <" << prefix << ":Configuration token=\"" << Xml::escapeXmlAttr(cfg.token) << "\">\r\n"
       << "        <tt:Name>" << Xml::escapeXmlText(cfg.name) << "</tt:Name>\r\n"
       << "        <tt:UseCount>1</tt:UseCount>\r\n"
       << "        <tt:PTZStatus>\r\n"
       << "          <tt:Status>" << (cfg.ptzStatusEnabled ? "true" : "false") << "</tt:Status>\r\n"
       << "          <tt:Position>" << (cfg.ptzStatusEnabled ? "true" : "false") << "</tt:Position>\r\n"
       << "        </tt:PTZStatus>\r\n"
       << "        <tt:Analytics>" << (cfg.analyticsEnabled ? "true" : "false") << "</tt:Analytics>\r\n"
       << "        <tt:Events>\r\n"
       << "          <tt:Filter>" << (cfg.eventsEnabled ? "true" : "false") << "</tt:Filter>\r\n"
       << "        </tt:Events>\r\n"
       << "      </" << prefix << ":Configuration>\r\n";
    return ss.str();
}

} // namespace Onvif
