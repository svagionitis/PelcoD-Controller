/// @file PtzXmlBuilder.cpp
#include "PtzXmlBuilder.h"

#include <sstream>
#include <string>

#include "XmlUtils.h"

namespace Onvif {

std::string PtzXmlBuilder::buildPtzNodeXml()
{
    std::ostringstream body;
    body << "      <tptz:PTZNode token=\"PTZNode_1\">\r\n"
         << "        <tt:Name>PTZNode</tt:Name>\r\n"
         << "        <tt:SupportedPTZSpaces>\r\n"
         << "          <tt:AbsolutePanTiltPositionSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:AbsolutePanTiltPositionSpace>\r\n"
         << "          <tt:AbsolutePanTiltPositionSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>360.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-90.0</tt:Min><tt:Max>90.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:AbsolutePanTiltPositionSpace>\r\n"
         << "          <tt:ContinuousPanTiltVelocitySpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:ContinuousPanTiltVelocitySpace>\r\n"
         << "          <tt:ContinuousZoomVelocitySpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "          </tt:ContinuousZoomVelocitySpace>\r\n"
         << "        </tt:SupportedPTZSpaces>\r\n"
         << "        <tt:MaximumNumberOfPresets>255</tt:MaximumNumberOfPresets>\r\n"
         << "        <tt:HomeSupported>true</tt:HomeSupported>\r\n"
         << "        <tt:GeoMove>true</tt:GeoMove>\r\n"
         << "        <tt:AuxiliaryCommands>tt:Wiper|On</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>tt:Wiper|Off</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>tt:Washer|On</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>tt:Washer|Off</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>tt:IR|On</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>tt:IR|Off</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>Aux1On</tt:AuxiliaryCommands>\r\n"
         << "        <tt:AuxiliaryCommands>Aux1Off</tt:AuxiliaryCommands>\r\n"
         << "      </tptz:PTZNode>\r\n";
    return body.str();
}

std::string PtzXmlBuilder::buildPtzConfigXml(
    const PTZConfiguration& cfg, const std::string& prefix, const std::string& elem)
{
    std::ostringstream body;
    body << "      <" << prefix << ":" << elem << " token=\"" << Xml::escapeXmlAttr(cfg.token) << "\">\r\n"
         << "        <tt:Name>" << Xml::escapeXmlText(cfg.name) << "</tt:Name>\r\n"
         << "        <tt:UseCount>" << cfg.useCount << "</tt:UseCount>\r\n"
         << "        <tt:NodeToken>" << Xml::escapeXmlText(cfg.nodeToken) << "</tt:NodeToken>\r\n";
    if (!cfg.defaultAbsolutePanTiltPositionSpace.empty()) {
        body << "        <tt:DefaultAbsolutePanTiltPositionSpace>"
             << Xml::escapeXmlText(cfg.defaultAbsolutePanTiltPositionSpace)
             << "</tt:DefaultAbsolutePanTiltPositionSpace>\r\n";
    }
    if (!cfg.defaultRelativePanTiltTranslationSpace.empty()) {
        body << "        <tt:DefaultRelativePanTiltTranslationSpace>"
             << Xml::escapeXmlText(cfg.defaultRelativePanTiltTranslationSpace)
             << "</tt:DefaultRelativePanTiltTranslationSpace>\r\n";
    }
    if (!cfg.defaultContinuousPanTiltVelocitySpace.empty()) {
        body << "        <tt:DefaultContinuousPanTiltVelocitySpace>"
             << Xml::escapeXmlText(cfg.defaultContinuousPanTiltVelocitySpace)
             << "</tt:DefaultContinuousPanTiltVelocitySpace>\r\n";
    }
    if (!cfg.defaultContinuousZoomVelocitySpace.empty()) {
        body << "        <tt:DefaultContinuousZoomVelocitySpace>"
             << Xml::escapeXmlText(cfg.defaultContinuousZoomVelocitySpace)
             << "</tt:DefaultContinuousZoomVelocitySpace>\r\n";
    }
    if (!cfg.defaultPtzTimeout.empty()) {
        body << "        <tt:DefaultPTZTimeout>" << Xml::escapeXmlText(cfg.defaultPtzTimeout)
             << "</tt:DefaultPTZTimeout>\r\n";
    }
    if (!cfg.panTiltLimits.uri.empty()) {
        body << "        <tt:PanTiltLimits><tt:Range><tt:URI>" << Xml::escapeXmlText(cfg.panTiltLimits.uri)
             << "</tt:URI>"
             << "<tt:XRange><tt:Min>" << cfg.panTiltLimits.minX << "</tt:Min><tt:Max>" << cfg.panTiltLimits.maxX
             << "</tt:Max></tt:XRange>"
             << "<tt:YRange><tt:Min>" << cfg.panTiltLimits.minY << "</tt:Min><tt:Max>" << cfg.panTiltLimits.maxY
             << "</tt:Max></tt:YRange>"
             << "</tt:Range></tt:PanTiltLimits>\r\n";
    }
    if (!cfg.zoomLimits.uri.empty()) {
        body << "        <tt:ZoomLimits><tt:Range><tt:URI>" << Xml::escapeXmlText(cfg.zoomLimits.uri) << "</tt:URI>"
             << "<tt:XRange><tt:Min>" << cfg.zoomLimits.minX << "</tt:Min><tt:Max>" << cfg.zoomLimits.maxX
             << "</tt:Max></tt:XRange>"
             << "</tt:Range></tt:ZoomLimits>\r\n";
    }
    body << "      </" << prefix << ":" << elem << ">\r\n";
    return body.str();
}

std::string PtzXmlBuilder::buildPtzOptionsXml()
{
    std::ostringstream body;
    body << "    <tptz:GetConfigurationOptionsResponse>\r\n"
         << "      <tptz:PTZConfigurationOptions>\r\n"
         << "        <tt:Spaces>\r\n"
         << "          <tt:AbsolutePanTiltPositionSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:AbsolutePanTiltPositionSpace>\r\n"
         << "          <tt:AbsolutePanTiltPositionSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>360.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-90.0</tt:Min><tt:Max>90.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:AbsolutePanTiltPositionSpace>\r\n"
         << "          <tt:AbsoluteZoomPositionSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "          </tt:AbsoluteZoomPositionSpace>\r\n"
         << "          <tt:RelativePanTiltTranslationSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:RelativePanTiltTranslationSpace>\r\n"
         << "          <tt:RelativeZoomTranslationSpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "          </tt:RelativeZoomTranslationSpace>\r\n"
         << "          <tt:ContinuousPanTiltVelocitySpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
         << "          </tt:ContinuousPanTiltVelocitySpace>\r\n"
         << "          <tt:ContinuousZoomVelocitySpace>\r\n"
         << "            <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>\r\n"
         << "            <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
         << "          </tt:ContinuousZoomVelocitySpace>\r\n"
         << "        </tt:Spaces>\r\n"
         << "        <tt:PTZTimeout><tt:Min>PT0S</tt:Min><tt:Max>PT10S</tt:Max></tt:PTZTimeout>\r\n"
         << "      </tptz:PTZConfigurationOptions>\r\n"
         << "    </tptz:GetConfigurationOptionsResponse>\r\n";
    return body.str();
}

std::string PtzXmlBuilder::buildPresetTourXml(const PresetTour& t, const std::string& prefix)
{
    std::ostringstream body;
    body << "      <" << prefix << ":PresetTour token=\"" << Xml::escapeXmlAttr(t.token) << "\">\r\n";
    if (!t.name.empty()) {
        body << "        <tt:Name>" << Xml::escapeXmlText(t.name) << "</tt:Name>\r\n";
    }
    body << "        <tt:Status>\r\n";
    std::string st = "Idle";
    if (t.status == PresetTourState::Touring) {
        st = "Touring";
    } else if (t.status == PresetTourState::Paused) {
        st = "Paused";
    } else if (t.status == PresetTourState::Extended) {
        st = "Extended";
    }
    body << "          <tt:State>" << st << "</tt:State>\r\n"
         << "        </tt:Status>\r\n"
         << "        <tt:AutoStart>" << (t.autoStart ? "true" : "false") << "</tt:AutoStart>\r\n";
    for (const auto& s : t.spots) {
        body << "        <tt:TourSpot>\r\n"
             << "          <tt:PresetDetail>\r\n"
             << "            <tt:PresetToken>" << Xml::escapeXmlText(s.presetToken) << "</tt:PresetToken>\r\n"
             << "          </tt:PresetDetail>\r\n"
             << "          <tt:Speed>\r\n"
             << "            <tt:PanTilt x=\"" << s.speed << "\" y=\"" << s.speed << "\"/>\r\n"
             << "          </tt:Speed>\r\n"
             << "          <tt:StayTime>PT" << s.stayTimeSeconds << "S</tt:StayTime>\r\n"
             << "        </tt:TourSpot>\r\n";
    }
    body << "      </" << prefix << ":PresetTour>\r\n";
    return body.str();
}

std::string PtzXmlBuilder::buildPtzStatusXml(const PtzStatus& status)
{
    std::ostringstream body;
    body << "    <tptz:GetStatusResponse>\r\n"
         << "      <tptz:PTZStatus>\r\n"
         << "        <tt:Position>\r\n"
         << "          <tt:PanTilt x=\"" << status.pan << "\" y=\"" << status.tilt << "\"/>\r\n"
         << "          <tt:Zoom x=\"" << status.zoom << "\"/>\r\n"
         << "        </tt:Position>\r\n"
         << "        <tt:MoveStatus>\r\n"
         << "          <tt:PanTilt>" << (status.isMoving ? "MOVING" : "IDLE") << "</tt:PanTilt>\r\n"
         << "          <tt:Zoom>" << (status.isMoving ? "MOVING" : "IDLE") << "</tt:Zoom>\r\n"
         << "        </tt:MoveStatus>\r\n"
         << "        <tt:UtcTime>" << Xml::escapeXmlText(status.utcTime) << "</tt:UtcTime>\r\n"
         << "      </tptz:PTZStatus>\r\n"
         << "    </tptz:GetStatusResponse>\r\n";
    return body.str();
}

} // namespace Onvif
