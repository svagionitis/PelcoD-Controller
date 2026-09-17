#include "OnvifServer.h"

#include <pugixml.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PelcoD::Onvif {

namespace {

    std::string wrapSoapResponse(const std::string& bodyXml)
    {
        std::ostringstream oss;
        oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
            << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
            << "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
            << "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
            << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
            << "  <SOAP-ENV:Body>\r\n"
            << bodyXml << "  </SOAP-ENV:Body>\r\n"
            << "</SOAP-ENV:Envelope>\r\n";
        return oss.str();
    }

} // namespace

OnvifServer::OnvifServer(OnvifServerConfig config, std::shared_ptr<IPtzHandler> ptzHandler)
    : m_config(std::move(config))
    , m_ptzHandler(std::move(ptzHandler))
{
    m_discoveryServer = std::make_unique<WsDiscoveryServer>(m_config);
    setupRoutes();
}

OnvifServer::~OnvifServer()
{
    stop();
}

bool OnvifServer::start()
{
    if (m_running) {
        return true;
    }

    if (m_discoveryServer) {
        const bool discStarted = m_discoveryServer->start();
        (void)discStarted;
    }

    m_running = true;

    m_httpThread = std::thread([this]() {
        const std::string host = m_config.bindAddress.empty() ? "0.0.0.0" : m_config.bindAddress;
        m_httpServer.listen(host.c_str(), m_config.port);
    });

    // Brief wait for socket binding
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

void OnvifServer::stop()
{
    if (!m_running) {
        return;
    }

    if (m_discoveryServer) {
        m_discoveryServer->stop();
    }

    m_httpServer.stop();
    m_running = false;

    if (m_httpThread.joinable()) {
        m_httpThread.join();
    }
}

bool OnvifServer::isRunning() const noexcept
{
    return m_running;
}

OnvifServerConfig OnvifServer::getConfig() const
{
    return m_config;
}

void OnvifServer::setPtzHandler(std::shared_ptr<IPtzHandler> handler)
{
    m_ptzHandler = std::move(handler);
}

std::string OnvifServer::resolveHost(const httplib::Request& req) const
{
    if (m_config.bindAddress != "0.0.0.0" && !m_config.bindAddress.empty()) {
        return m_config.bindAddress;
    }

    if (req.has_header("Host")) {
        const std::string hostHeader = req.get_header_value("Host");
        const auto colonPos = hostHeader.find(':');
        if (colonPos != std::string::npos) {
            return hostHeader.substr(0, colonPos);
        }
        return hostHeader;
    }

    return "127.0.0.1";
}

void OnvifServer::setupRoutes()
{
    m_httpServer.Post("/onvif/device_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleDeviceService(req, res); });

    m_httpServer.Post("/onvif/media_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleMediaService(req, res); });

    m_httpServer.Post("/onvif/ptz_service",
        [this](const httplib::Request& req, httplib::Response& res) { handlePtzService(req, res); });

    m_httpServer.Get("/onvif/snapshot", [](const httplib::Request&, httplib::Response& res) {
        res.status = 501;
        res.set_content("Snapshot service not implemented", "text/plain");
    });
}

void OnvifServer::handleDeviceService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return;
    }

    const std::string host = resolveHost(req);
    const int port = m_config.port;

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";

    std::ostringstream body;

    if (opName.find("GetSystemDateAndTime") != std::string::npos) {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm utcTm {};
#ifdef _WIN32
        gmtime_s(&utcTm, &nowTime);
#else
        gmtime_r(&nowTime, &utcTm);
#endif
        body << "    <tds:GetSystemDateAndTimeResponse>\r\n"
             << "      <tds:SystemDateAndTime>\r\n"
             << "        <tt:DateTimeType>Manual</tt:DateTimeType>\r\n"
             << "        <tt:DaylightSavings>false</tt:DaylightSavings>\r\n"
             << "        <tt:TimeZone><tt:TZ>UTC</tt:TZ></tt:TimeZone>\r\n"
             << "        <tt:UTCDateTime>\r\n"
             << "          <tt:Time><tt:Hour>" << utcTm.tm_hour << "</tt:Hour><tt:Minute>" << utcTm.tm_min
             << "</tt:Minute><tt:Second>" << utcTm.tm_sec << "</tt:Second></tt:Time>\r\n"
             << "          <tt:Date><tt:Year>" << (utcTm.tm_year + 1900) << "</tt:Year><tt:Month>" << (utcTm.tm_mon + 1)
             << "</tt:Month><tt:Day>" << utcTm.tm_mday << "</tt:Day></tt:Date>\r\n"
             << "        </tt:UTCDateTime>\r\n"
             << "      </tds:SystemDateAndTime>\r\n"
             << "    </tds:GetSystemDateAndTimeResponse>\r\n";
    } else if (opName.find("GetDeviceInformation") != std::string::npos) {
        body << "    <tds:GetDeviceInformationResponse>\r\n"
             << "      <tds:Manufacturer>" << m_config.manufacturer << "</tds:Manufacturer>\r\n"
             << "      <tds:Model>" << m_config.model << "</tds:Model>\r\n"
             << "      <tds:FirmwareVersion>" << m_config.firmwareVersion << "</tds:FirmwareVersion>\r\n"
             << "      <tds:SerialNumber>" << m_config.serialNumber << "</tds:SerialNumber>\r\n"
             << "      <tds:HardwareId>" << m_config.hardwareId << "</tds:HardwareId>\r\n"
             << "    </tds:GetDeviceInformationResponse>\r\n";
    } else if (opName.find("GetCapabilities") != std::string::npos) {
        body << "    <tds:GetCapabilitiesResponse>\r\n"
             << "      <tds:Capabilities>\r\n"
             << "        <tt:Device>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/device_service</tt:XAddr>\r\n"
             << "        </tt:Device>\r\n"
             << "        <tt:Media>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/media_service</tt:XAddr>\r\n"
             << "        </tt:Media>\r\n"
             << "        <tt:PTZ>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/ptz_service</tt:XAddr>\r\n"
             << "        </tt:PTZ>\r\n"
             << "      </tds:Capabilities>\r\n"
             << "    </tds:GetCapabilitiesResponse>\r\n";
    } else if (opName.find("GetServices") != std::string::npos) {
        body << "    <tds:GetServicesResponse>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/device_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/media_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver20/ptz/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/ptz_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>2</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "    </tds:GetServicesResponse>\r\n";
    } else if (opName.find("GetScopes") != std::string::npos) {
        body << "    <tds:GetScopesResponse>\r\n"
             << "      "
                "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/type/video_encoder</"
                "tt:ScopeItem></tds:Scopes>\r\n"
             << "      "
                "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/type/ptz</"
                "tt:ScopeItem></tds:Scopes>\r\n"
             << "      <tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/name/"
             << m_config.deviceName << "</tt:ScopeItem></tds:Scopes>\r\n"
             << "      <tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>onvif://www.onvif.org/hardware/"
             << m_config.model << "</tt:ScopeItem></tds:Scopes>\r\n";
        for (const auto& scope : m_config.scopes) {
            body << "      <tds:Scopes><tt:ScopeDef>Configurable</tt:ScopeDef><tt:ScopeItem>" << scope
                 << "</tt:ScopeItem></tds:Scopes>\r\n";
        }
        body << "    </tds:GetScopesResponse>\r\n";
    } else {
        // Generic success acknowledgement
        body << "    <tds:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handleMediaService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return;
    }

    const std::string host = resolveHost(req);
    const int port = m_config.port;

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";

    std::ostringstream body;

    if (opName.find("GetProfiles") != std::string::npos) {
        body << "    <trt:GetProfilesResponse>\r\n"
             << "      <trt:Profiles token=\"ProfileToken_1\" fixed=\"true\">\r\n"
             << "        <tt:Name>MainProfile</tt:Name>\r\n"
             << "        <tt:VideoSourceConfiguration token=\"VideoSourceConfig_1\">\r\n"
             << "          <tt:Name>VideoSourceConfig</tt:Name>\r\n"
             << "          <tt:UseCount>1</tt:UseCount>\r\n"
             << "          <tt:SourceToken>VideoSource_1</tt:SourceToken>\r\n"
             << "          <tt:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/>\r\n"
             << "        </tt:VideoSourceConfiguration>\r\n"
             << "        <tt:PTZConfiguration token=\"PTZConfig_1\">\r\n"
             << "          <tt:Name>PTZConfig</tt:Name>\r\n"
             << "          <tt:UseCount>1</tt:UseCount>\r\n"
             << "          <tt:NodeToken>PTZNode_1</tt:NodeToken>\r\n"
             << "          "
                "<tt:DefaultContinuousPanTiltVelocitySpace>http://www.onvif.org/ver10/tptz/PanTiltSpaces/"
                "VelocityGenericSpace</tt:DefaultContinuousPanTiltVelocitySpace>\r\n"
             << "          "
                "<tt:DefaultContinuousZoomVelocitySpace>http://www.onvif.org/ver10/tptz/ZoomSpaces/"
                "VelocityGenericSpace</tt:DefaultContinuousZoomVelocitySpace>\r\n"
             << "        </tt:PTZConfiguration>\r\n"
             << "      </trt:Profiles>\r\n"
             << "    </trt:GetProfilesResponse>\r\n";
    } else if (opName.find("GetVideoSources") != std::string::npos) {
        body << "    <trt:GetVideoSourcesResponse>\r\n"
             << "      <trt:VideoSources token=\"VideoSource_1\">\r\n"
             << "        <tt:Framerate>30</tt:Framerate>\r\n"
             << "        <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>\r\n"
             << "      </trt:VideoSources>\r\n"
             << "    </trt:GetVideoSourcesResponse>\r\n";
    } else if (opName.find("GetStreamUri") != std::string::npos) {
        body << "    <trt:GetStreamUriResponse>\r\n"
             << "      <trt:MediaUri>\r\n"
             << "        <tt:Uri>" << m_config.rtspStreamUri << "</tt:Uri>\r\n"
             << "        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>\r\n"
             << "        <tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>\r\n"
             << "        <tt:Timeout>PT30S</tt:Timeout>\r\n"
             << "      </trt:MediaUri>\r\n"
             << "    </trt:GetStreamUriResponse>\r\n";
    } else if (opName.find("GetSnapshotUri") != std::string::npos) {
        body << "    <trt:GetSnapshotUriResponse>\r\n"
             << "      <trt:MediaUri>\r\n"
             << "        <tt:Uri>http://" << host << ":" << port << "/onvif/snapshot</tt:Uri>\r\n"
             << "        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>\r\n"
             << "        <tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>\r\n"
             << "        <tt:Timeout>PT30S</tt:Timeout>\r\n"
             << "      </trt:MediaUri>\r\n"
             << "    </trt:GetSnapshotUriResponse>\r\n";
    } else {
        body << "    <trt:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handlePtzService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return;
    }

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";

    std::ostringstream body;

    if (opName.find("ContinuousMove") != std::string::npos) {
        float pan = 0.0f;
        float tilt = 0.0f;
        float zoom = 0.0f;

        const pugi::xml_node ptNode = doc.select_node("//*[local-name()='PanTilt']").node();
        if (ptNode) {
            pan = ptNode.attribute("x").as_float(0.0f);
            tilt = ptNode.attribute("y").as_float(0.0f);
        }

        const pugi::xml_node zNode = doc.select_node("//*[local-name()='Zoom']").node();
        if (zNode) {
            zoom = zNode.attribute("x").as_float(0.0f);
        }

        if (m_ptzHandler) {
            m_ptzHandler->handleContinuousMove(pan, tilt, zoom);
        }

        body << "    <tptz:ContinuousMoveResponse/>\r\n";
    } else if (opName.find("Stop") != std::string::npos) {
        bool stopPanTilt = true;
        bool stopZoom = true;

        const pugi::xml_node ptNode = doc.select_node("//*[local-name()='PanTilt']").node();
        if (ptNode) {
            stopPanTilt = ptNode.text().as_bool(true);
        }

        const pugi::xml_node zNode = doc.select_node("//*[local-name()='Zoom']").node();
        if (zNode) {
            stopZoom = zNode.text().as_bool(true);
        }

        if (m_ptzHandler) {
            m_ptzHandler->handleStop(stopPanTilt, stopZoom);
        }

        body << "    <tptz:StopResponse/>\r\n";
    } else if (opName.find("AbsoluteMove") != std::string::npos) {
        float pan = -1.0f;
        float tilt = -1.0f;
        float zoom = -1.0f;

        const pugi::xml_node ptNode = doc.select_node("//*[local-name()='PanTilt']").node();
        if (ptNode) {
            pan = ptNode.attribute("x").as_float(-1.0f);
            tilt = ptNode.attribute("y").as_float(-1.0f);
        }

        const pugi::xml_node zNode = doc.select_node("//*[local-name()='Zoom']").node();
        if (zNode) {
            zoom = zNode.attribute("x").as_float(-1.0f);
        }

        if (m_ptzHandler) {
            m_ptzHandler->handleAbsoluteMove(pan, tilt, zoom);
        }

        body << "    <tptz:AbsoluteMoveResponse/>\r\n";
    } else if (opName.find("GetConfigurations") != std::string::npos
        || opName.find("GetConfiguration") != std::string::npos) {
        body << "    <tptz:GetConfigurationsResponse>\r\n"
             << "      <tptz:PTZConfiguration token=\"PTZConfig_1\">\r\n"
             << "        <tt:Name>PTZConfig</tt:Name>\r\n"
             << "        <tt:UseCount>1</tt:UseCount>\r\n"
             << "        <tt:NodeToken>PTZNode_1</tt:NodeToken>\r\n"
             << "        "
                "<tt:DefaultContinuousPanTiltVelocitySpace>http://www.onvif.org/ver10/tptz/PanTiltSpaces/"
                "VelocityGenericSpace</tt:DefaultContinuousPanTiltVelocitySpace>\r\n"
             << "        "
                "<tt:DefaultContinuousZoomVelocitySpace>http://www.onvif.org/ver10/tptz/ZoomSpaces/"
                "VelocityGenericSpace</tt:DefaultContinuousZoomVelocitySpace>\r\n"
             << "      </tptz:PTZConfiguration>\r\n"
             << "    </tptz:GetConfigurationsResponse>\r\n";
    } else if (opName.find("GetNodes") != std::string::npos || opName.find("GetNode") != std::string::npos) {
        body << "    <tptz:GetNodesResponse>\r\n"
             << "      <tptz:PTZNode token=\"PTZNode_1\">\r\n"
             << "        <tt:Name>PTZNode</tt:Name>\r\n"
             << "        <tt:SupportedPTZSpaces>\r\n"
             << "          <tt:ContinuousPanTiltVelocitySpace>\r\n"
             << "            <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>\r\n"
             << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
             << "            <tt:YRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:YRange>\r\n"
             << "          </tt:ContinuousPanTiltVelocitySpace>\r\n"
             << "          <tt:ContinuousZoomVelocitySpace>\r\n"
             << "            <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>\r\n"
             << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
             << "          </tt:ContinuousZoomVelocitySpace>\r\n"
             << "        </tt:SupportedPTZSpaces>\r\n"
             << "        <tt:MaximumNumberOfPresets>255</tt:MaximumNumberOfPresets>\r\n"
             << "        <tt:HomeSupported>false</tt:HomeSupported>\r\n"
             << "      </tptz:PTZNode>\r\n"
             << "    </tptz:GetNodesResponse>\r\n";
    } else if (opName.find("GetPresets") != std::string::npos) {
        body << "    <tptz:GetPresetsResponse>\r\n";
        if (m_ptzHandler) {
            const auto presets = m_ptzHandler->handleGetPresets();
            for (const auto& p : presets) {
                body << "      <tptz:Preset token=\"" << p.token << "\">\r\n"
                     << "        <tt:Name>" << p.name << "</tt:Name>\r\n"
                     << "      </tptz:Preset>\r\n";
            }
        }
        body << "    </tptz:GetPresetsResponse>\r\n";
    } else if (opName.find("SetPreset") != std::string::npos) {
        const pugi::xml_node nameNode = doc.select_node("//*[local-name()='PresetName']").node();
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='PresetToken']").node();

        const std::string presetName = nameNode ? nameNode.text().as_string() : "";
        const std::string presetToken = tokenNode ? tokenNode.text().as_string() : "";

        std::string assignedToken = presetToken;
        if (m_ptzHandler) {
            assignedToken = m_ptzHandler->handleSetPreset(presetName, presetToken);
        }

        body << "    <tptz:SetPresetResponse>\r\n"
             << "      <tptz:PTZPresetToken>" << assignedToken << "</tptz:PTZPresetToken>\r\n"
             << "    </tptz:SetPresetResponse>\r\n";
    } else if (opName.find("GotoPreset") != std::string::npos) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='PresetToken']").node();
        const std::string presetToken = tokenNode ? tokenNode.text().as_string() : "";

        if (m_ptzHandler) {
            m_ptzHandler->handleGotoPreset(presetToken);
        }

        body << "    <tptz:GotoPresetResponse/>\r\n";
    } else if (opName.find("RemovePreset") != std::string::npos) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='PresetToken']").node();
        const std::string presetToken = tokenNode ? tokenNode.text().as_string() : "";

        if (m_ptzHandler) {
            m_ptzHandler->handleRemovePreset(presetToken);
        }

        body << "    <tptz:RemovePresetResponse/>\r\n";
    } else if (opName.find("GetStatus") != std::string::npos) {
        PtzStatus status {};
        if (m_ptzHandler) {
            status = m_ptzHandler->handleGetStatus();
        }

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
             << "      </tptz:PTZStatus>\r\n"
             << "    </tptz:GetStatusResponse>\r\n";
    } else {
        body << "    <tptz:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

} // namespace PelcoD::Onvif
