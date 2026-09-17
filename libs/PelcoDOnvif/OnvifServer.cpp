#include "OnvifServer.h"

#include <pugixml.hpp>

#include <algorithm>
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
            << "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
            << "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
            << "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
            << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
            << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
            << "  <SOAP-ENV:Body>\r\n"
            << bodyXml << "  </SOAP-ENV:Body>\r\n"
            << "</SOAP-ENV:Envelope>\r\n";
        return oss.str();
    }

    std::string formatIso8601Utc(const std::chrono::system_clock::time_point& tp)
    {
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tmUtc {};
#ifdef _WIN32
        gmtime_s(&tmUtc, &t);
#else
        gmtime_r(&t, &tmUtc);
#endif
        char buf[32] { 0 };
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
        return std::string(buf);
    }

    int parseTimeoutSeconds(const std::string& isoDuration, int defaultSecs = 5)
    {
        if (isoDuration.empty()) {
            return defaultSecs;
        }

        const auto posT = isoDuration.find('T');
        const auto posS = isoDuration.find('S');
        if (posT != std::string::npos && posS != std::string::npos && posS > posT + 1) {
            try {
                const int val = std::stoi(isoDuration.substr(posT + 1, posS - posT - 1));
                return std::clamp(val, 0, 10);
            } catch (...) {
                // Fallback
            }
        }
        return defaultSecs;
    }

} // namespace

OnvifServer::OnvifServer(
    OnvifServerConfig config, std::shared_ptr<IPtzHandler> ptzHandler, std::shared_ptr<IImagingHandler> imagingHandler)
    : m_config(std::move(config))
    , m_ptzHandler(std::move(ptzHandler))
    , m_imagingHandler(std::move(imagingHandler))
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

    {
        std::lock_guard<std::mutex> lock(m_subMutex);
        for (auto& [id, sub] : m_subscriptions) {
            std::lock_guard<std::mutex> subLock(sub->mutex);
            sub->cv.notify_all();
        }
    }

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

void OnvifServer::setImagingHandler(std::shared_ptr<IImagingHandler> handler)
{
    m_imagingHandler = std::move(handler);
}

void OnvifServer::publishEvent(const OnvifEvent& event)
{
    OnvifEvent ev = event;
    if (ev.utcTime.empty()) {
        ev.utcTime = formatIso8601Utc(std::chrono::system_clock::now());
    }

    std::lock_guard<std::mutex> lock(m_subMutex);
    for (auto& [id, sub] : m_subscriptions) {
        std::lock_guard<std::mutex> subLock(sub->mutex);
        sub->queue.push_back(ev);
        sub->cv.notify_one();
    }
}

void OnvifServer::setRequestLogCallback(RequestLogCallback callback)
{
    m_logCallback = std::move(callback);
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

    m_httpServer.Post("/onvif/imaging_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleImagingService(req, res); });

    m_httpServer.Post("/onvif/event_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleEventService(req, res); });

    m_httpServer.Post("/onvif/events/subscription",
        [this](const httplib::Request& req, httplib::Response& res) { handleSubscriptionService(req, res); });

    m_httpServer.Post(R"(/onvif/events/subscription/(.+))",
        [this](const httplib::Request& req, httplib::Response& res) { handleSubscriptionService(req, res); });

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
    if (m_logCallback) {
        m_logCallback("Device", opName, req.remote_addr);
    }

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
             << "        <tt:Imaging>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/imaging_service</tt:XAddr>\r\n"
             << "        </tt:Imaging>\r\n"
             << "        <tt:Events>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/event_service</tt:XAddr>\r\n"
             << "        </tt:Events>\r\n"
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
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver20/imaging/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/imaging_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>20</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/events/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/event_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
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
    if (m_logCallback) {
        m_logCallback("Media", opName, req.remote_addr);
    }

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
    if (m_logCallback) {
        m_logCallback("PTZ", opName, req.remote_addr);
    }

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
    } else if (opName.find("RemovePresetTour") != std::string::npos) {
        const pugi::xml_node tokNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        const std::string tourTok = tokNode ? tokNode.text().as_string() : "";
        if (m_ptzHandler) {
            m_ptzHandler->handleRemovePresetTour(tourTok);
        }
        body << "    <tptz:RemovePresetTourResponse/>\r\n";
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
    } else if (opName.find("GetPresetTours") != std::string::npos) {
        body << "    <tptz:GetPresetToursResponse>\r\n";
        if (m_ptzHandler) {
            const auto tours = m_ptzHandler->handleGetPresetTours();
            for (const auto& t : tours) {
                body << "      <tptz:PresetTour token=\"" << t.token << "\">\r\n";
                if (!t.name.empty()) {
                    body << "        <tt:Name>" << t.name << "</tt:Name>\r\n";
                }
                body << "        <tt:Status>\r\n";
                std::string st = "Idle";
                if (t.status == PresetTourState::Touring)
                    st = "Touring";
                else if (t.status == PresetTourState::Paused)
                    st = "Paused";
                else if (t.status == PresetTourState::Extended)
                    st = "Extended";
                body << "          <tt:State>" << st << "</tt:State>\r\n"
                     << "        </tt:Status>\r\n"
                     << "        <tt:AutoStart>" << (t.autoStart ? "true" : "false") << "</tt:AutoStart>\r\n";
                for (const auto& s : t.spots) {
                    body << "        <tt:TourSpot>\r\n"
                         << "          <tt:PresetDetail>\r\n"
                         << "            <tt:PresetToken>" << s.presetToken << "</tt:PresetToken>\r\n"
                         << "          </tt:PresetDetail>\r\n"
                         << "          <tt:Speed>\r\n"
                         << "            <tt:PanTilt x=\"" << s.speed << "\" y=\"" << s.speed << "\"/>\r\n"
                         << "          </tt:Speed>\r\n"
                         << "          <tt:StayTime>PT" << s.stayTimeSeconds << "S</tt:StayTime>\r\n"
                         << "        </tt:TourSpot>\r\n";
                }
                body << "      </tptz:PresetTour>\r\n";
            }
        }
        body << "    </tptz:GetPresetToursResponse>\r\n";
    } else if (opName.find("GetPresetTourOptions") != std::string::npos) {
        body << "    <tptz:GetPresetTourOptionsResponse>\r\n"
             << "      <tptz:Options>\r\n"
             << "        <tt:AutoStart>true</tt:AutoStart>\r\n"
             << "        <tt:StartingCondition>\r\n"
             << "          <tt:RecurringTime><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:RecurringTime>\r\n"
             << "          <tt:RecurringDuration><tt:Min>PT0S</tt:Min><tt:Max>PT24H</tt:Max></tt:RecurringDuration>\r\n"
             << "        </tt:StartingCondition>\r\n"
             << "        <tt:TourSpot>\r\n"
             << "          <tt:PresetDetail><tt:PresetToken>1</tt:PresetToken></tt:PresetDetail>\r\n"
             << "          <tt:StayTime><tt:Min>PT1S</tt:Min><tt:Max>PT3600S</tt:Max></tt:StayTime>\r\n"
             << "          "
                "<tt:PanTiltSpeedSpace><tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</"
                "tt:URI><tt:XRange><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange></tt:PanTiltSpeedSpace>\r\n"
             << "        </tt:TourSpot>\r\n"
             << "      </tptz:Options>\r\n"
             << "    </tptz:GetPresetTourOptionsResponse>\r\n";
    } else if (opName.find("GetPresetTour") != std::string::npos) {
        const pugi::xml_node tokNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        const std::string tourTok = tokNode ? tokNode.text().as_string() : "";
        body << "    <tptz:GetPresetTourResponse>\r\n";
        if (m_ptzHandler) {
            const auto tourOpt = m_ptzHandler->handleGetPresetTour(tourTok);
            if (tourOpt) {
                const auto& t = *tourOpt;
                body << "      <tptz:PresetTour token=\"" << t.token << "\">\r\n";
                if (!t.name.empty()) {
                    body << "        <tt:Name>" << t.name << "</tt:Name>\r\n";
                }
                body << "        <tt:Status>\r\n";
                std::string st = "Idle";
                if (t.status == PresetTourState::Touring)
                    st = "Touring";
                else if (t.status == PresetTourState::Paused)
                    st = "Paused";
                body << "          <tt:State>" << st << "</tt:State>\r\n"
                     << "        </tt:Status>\r\n"
                     << "        <tt:AutoStart>" << (t.autoStart ? "true" : "false") << "</tt:AutoStart>\r\n";
                for (const auto& s : t.spots) {
                    body << "        <tt:TourSpot>\r\n"
                         << "          <tt:PresetDetail>\r\n"
                         << "            <tt:PresetToken>" << s.presetToken << "</tt:PresetToken>\r\n"
                         << "          </tt:PresetDetail>\r\n"
                         << "          <tt:Speed>\r\n"
                         << "            <tt:PanTilt x=\"" << s.speed << "\" y=\"" << s.speed << "\"/>\r\n"
                         << "          </tt:Speed>\r\n"
                         << "          <tt:StayTime>PT" << s.stayTimeSeconds << "S</tt:StayTime>\r\n"
                         << "        </tt:TourSpot>\r\n";
                }
                body << "      </tptz:PresetTour>\r\n";
            }
        }
        body << "    </tptz:GetPresetTourResponse>\r\n";
    } else if (opName.find("CreatePresetTour") != std::string::npos) {
        std::string assignedToken = "Tour_1";
        if (m_ptzHandler) {
            assignedToken = m_ptzHandler->handleCreatePresetTour();
        }
        body << "    <tptz:CreatePresetTourResponse>\r\n"
             << "      <tptz:PresetTourToken>" << assignedToken << "</tptz:PresetTourToken>\r\n"
             << "    </tptz:CreatePresetTourResponse>\r\n";
    } else if (opName.find("ModifyPresetTour") != std::string::npos) {
        const pugi::xml_node tourNode = doc.select_node("//*[local-name()='PresetTour']").node();
        if (tourNode && m_ptzHandler) {
            PresetTour tour {};
            tour.token = tourNode.attribute("token").as_string();
            const pugi::xml_node nameNode = tourNode.select_node("./*[local-name()='Name']").node();
            if (nameNode)
                tour.name = nameNode.text().as_string();
            const pugi::xml_node autoStartNode = tourNode.select_node("./*[local-name()='AutoStart']").node();
            if (autoStartNode)
                tour.autoStart = autoStartNode.text().as_bool(false);

            const auto spotNodes = tourNode.select_nodes("./*[local-name()='TourSpot']");
            for (const auto& it : spotNodes) {
                PresetTourSpot spot {};
                const pugi::xml_node pTok = it.node().select_node(".//*[local-name()='PresetToken']").node();
                if (pTok)
                    spot.presetToken = pTok.text().as_string();
                const pugi::xml_node speedNode
                    = it.node().select_node(".//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
                if (speedNode) {
                    spot.speed = speedNode.attribute("x").as_float(1.0f);
                }
                const pugi::xml_node stayNode = it.node().select_node(".//*[local-name()='StayTime']").node();
                if (stayNode) {
                    std::string st = stayNode.text().as_string();
                    std::size_t pos = (st.rfind("PT", 0) == 0) ? 2 : 0;
                    std::uint32_t val = 0;
                    while (pos < st.size() && std::isdigit(static_cast<unsigned char>(st[pos]))) {
                        val = val * 10 + static_cast<std::uint32_t>(st[pos] - '0');
                        ++pos;
                    }
                    spot.stayTimeSeconds = (val > 0) ? val : 5U;
                }
                if (!spot.presetToken.empty()) {
                    tour.spots.push_back(std::move(spot));
                }
            }
            m_ptzHandler->handleModifyPresetTour(tour);
        }
        body << "    <tptz:ModifyPresetTourResponse/>\r\n";
    } else if (opName.find("OperatePresetTour") != std::string::npos) {
        const pugi::xml_node tokNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        const pugi::xml_node opNode = doc.select_node("//*[local-name()='Operation']").node();
        const std::string tourTok = tokNode ? tokNode.text().as_string() : "";
        const std::string opVal = opNode ? opNode.text().as_string() : "Start";

        PresetTourOperation tourOp = PresetTourOperation::Start;
        if (opVal == "Stop")
            tourOp = PresetTourOperation::Stop;
        else if (opVal == "Pause")
            tourOp = PresetTourOperation::Pause;

        if (m_ptzHandler) {
            m_ptzHandler->handleOperatePresetTour(tourTok, tourOp);
        }
    } else {
        body << "    <tptz:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handleImagingService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return;
    }

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";
    if (m_logCallback) {
        m_logCallback("Imaging", opName, req.remote_addr);
    }

    const pugi::xml_node vsNode = doc.select_node("//*[local-name()='VideoSourceToken']").node();
    const std::string videoSourceToken = vsNode ? vsNode.text().as_string() : "VideoSource_1";

    std::ostringstream body;

    if (opName.find("GetImagingSettings") != std::string::npos) {
        ImagingSettings settings = m_config.defaultImagingSettings;
        if (m_imagingHandler) {
            settings = m_imagingHandler->handleGetImagingSettings(videoSourceToken);
        }

        body << "    <timg:GetImagingSettingsResponse>\r\n"
             << "      <timg:ImagingSettings>\r\n"
             << "        <tt:Brightness>" << std::fixed << std::setprecision(1) << settings.brightness
             << "</tt:Brightness>\r\n"
             << "        <tt:ColorSaturation>" << settings.colorSaturation << "</tt:ColorSaturation>\r\n"
             << "        <tt:Contrast>" << settings.contrast << "</tt:Contrast>\r\n"
             << "        <tt:Sharpness>" << settings.sharpness << "</tt:Sharpness>\r\n"
             << "        <tt:IrCutFilter>" << settings.irCutFilter << "</tt:IrCutFilter>\r\n"
             << "        <tt:BacklightCompensation>\r\n"
             << "          <tt:Mode>" << (settings.backlightCompensation ? "ON" : "OFF") << "</tt:Mode>\r\n"
             << "          <tt:Level>" << settings.backlightLevel << "</tt:Level>\r\n"
             << "        </tt:BacklightCompensation>\r\n"
             << "        <tt:WideDynamicRange>\r\n"
             << "          <tt:Mode>" << (settings.wideDynamicRange ? "ON" : "OFF") << "</tt:Mode>\r\n"
             << "          <tt:Level>" << settings.wdrLevel << "</tt:Level>\r\n"
             << "        </tt:WideDynamicRange>\r\n"
             << "        <tt:Focus>\r\n"
             << "          <tt:AutoFocusMode>" << settings.autoFocusMode << "</tt:AutoFocusMode>\r\n"
             << "        </tt:Focus>\r\n"
             << "      </timg:ImagingSettings>\r\n"
             << "    </timg:GetImagingSettingsResponse>\r\n";
    } else if (opName.find("SetImagingSettings") != std::string::npos) {
        ImagingSettings settings = m_config.defaultImagingSettings;
        if (m_imagingHandler) {
            settings = m_imagingHandler->handleGetImagingSettings(videoSourceToken);
        }

        const pugi::xml_node brightNode = doc.select_node("//*[local-name()='Brightness']").node();
        if (brightNode) {
            settings.brightness = brightNode.text().as_float(settings.brightness);
        }

        const pugi::xml_node satNode = doc.select_node("//*[local-name()='ColorSaturation']").node();
        if (satNode) {
            settings.colorSaturation = satNode.text().as_float(settings.colorSaturation);
        }

        const pugi::xml_node contrastNode = doc.select_node("//*[local-name()='Contrast']").node();
        if (contrastNode) {
            settings.contrast = contrastNode.text().as_float(settings.contrast);
        }

        const pugi::xml_node sharpNode = doc.select_node("//*[local-name()='Sharpness']").node();
        if (sharpNode) {
            settings.sharpness = sharpNode.text().as_float(settings.sharpness);
        }

        const pugi::xml_node irNode = doc.select_node("//*[local-name()='IrCutFilter']").node();
        if (irNode) {
            settings.irCutFilter = irNode.text().as_string(settings.irCutFilter.c_str());
        }

        const pugi::xml_node blcNode = doc.select_node("//*[local-name()='BacklightCompensation']").node();
        if (blcNode) {
            const pugi::xml_node mode = blcNode.child("tt:Mode") ? blcNode.child("tt:Mode") : blcNode.first_child();
            if (mode) {
                settings.backlightCompensation = (std::string(mode.text().as_string()) == "ON");
            }
        }

        if (m_imagingHandler) {
            m_imagingHandler->handleSetImagingSettings(videoSourceToken, settings);
        }

        body << "    <timg:SetImagingSettingsResponse/>\r\n";
    } else if (opName.find("Move") != std::string::npos) {
        float speed = 0.0f;
        const pugi::xml_node speedNode = doc.select_node("//*[local-name()='Speed']").node();
        if (speedNode) {
            speed = speedNode.text().as_float(0.0f);
        }

        if (m_imagingHandler) {
            m_imagingHandler->handleMoveFocus(videoSourceToken, speed);
        }

        body << "    <timg:MoveResponse/>\r\n";
    } else if (opName.find("Stop") != std::string::npos) {
        if (m_imagingHandler) {
            m_imagingHandler->handleStopFocus(videoSourceToken);
        }

        body << "    <timg:StopResponse/>\r\n";
    } else if (opName.find("GetOptions") != std::string::npos) {
        body << "    <timg:GetOptionsResponse>\r\n"
             << "      <timg:ImagingOptions>\r\n"
             << "        <tt:BacklightCompensation>\r\n"
             << "          <tt:Mode>ON</tt:Mode>\r\n"
             << "          <tt:Mode>OFF</tt:Mode>\r\n"
             << "          <tt:LevelRange><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:LevelRange>\r\n"
             << "        </tt:BacklightCompensation>\r\n"
             << "        <tt:Brightness><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Brightness>\r\n"
             << "        <tt:ColorSaturation><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:ColorSaturation>\r\n"
             << "        <tt:Contrast><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Contrast>\r\n"
             << "        <tt:Sharpness><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Sharpness>\r\n"
             << "        <tt:Focus>\r\n"
             << "          <tt:AutoFocusModes>AUTO</tt:AutoFocusModes>\r\n"
             << "          <tt:AutoFocusModes>MANUAL</tt:AutoFocusModes>\r\n"
             << "          "
                "<tt:Continuous><tt:Speed><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed></tt:Continuous>\r\n"
             << "        </tt:Focus>\r\n"
             << "      </timg:ImagingOptions>\r\n"
             << "    </timg:GetOptionsResponse>\r\n";
    } else {
        body << "    <timg:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handleEventService(const httplib::Request& req, httplib::Response& res)
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
    if (m_logCallback) {
        m_logCallback("Events", opName, req.remote_addr);
    }

    std::ostringstream body;

    if (opName.find("CreatePullPointSubscription") != std::string::npos) {
        std::string subId;
        std::shared_ptr<PullPointSubscription> sub;
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            subId = std::to_string(m_nextSubId++);
            sub = std::make_shared<PullPointSubscription>();
            sub->id = subId;
            sub->terminationTime = std::chrono::steady_clock::now() + std::chrono::minutes(10);
            m_subscriptions[subId] = sub;
        }

        const auto now = std::chrono::system_clock::now();
        const std::string curTime = formatIso8601Utc(now);
        const std::string termTime = formatIso8601Utc(now + std::chrono::minutes(10));

        body << "    <tev:CreatePullPointSubscriptionResponse>\r\n"
             << "      <tev:SubscriptionReference>\r\n"
             << "        <wsa:Address>http://" << host << ":" << port << "/onvif/events/subscription/" << subId
             << "</wsa:Address>\r\n"
             << "      </tev:SubscriptionReference>\r\n"
             << "      <wsnt:CurrentTime>" << curTime << "</wsnt:CurrentTime>\r\n"
             << "      <wsnt:TerminationTime>" << termTime << "</wsnt:TerminationTime>\r\n"
             << "    </tev:CreatePullPointSubscriptionResponse>\r\n";
    } else if (opName.find("GetEventProperties") != std::string::npos) {
        body << "    <tev:GetEventPropertiesResponse>\r\n"
             << "      "
                "<tev:TopicNamespaceLocation>http://www.onvif.org/onvif/ver10/topics/topicns.xml</"
                "tev:TopicNamespaceLocation>\r\n"
             << "      <wsnt:FixedTopicSet>true</wsnt:FixedTopicSet>\r\n"
             << "      <wstop:TopicSet xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\">\r\n"
             << "        <tns1:RuleEngine xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
             << "          <tns1:CellMotionDetector><tns1:Motion/></tns1:CellMotionDetector>\r\n"
             << "        </tns1:RuleEngine>\r\n"
             << "        <tns1:PTZController xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
             << "          <tns1:PTZPresets><tns1:Reached/></tns1:PTZPresets>\r\n"
             << "        </tns1:PTZController>\r\n"
             << "        <tns1:Device xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
             << "          <tns1:Trigger><tns1:DigitalInput/></tns1:Trigger>\r\n"
             << "        </tns1:Device>\r\n"
             << "      </wstop:TopicSet>\r\n"
             << "    </tev:GetEventPropertiesResponse>\r\n";
    } else {
        body << "    <tev:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handleSubscriptionService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return;
    }

    // Extract subscription ID from URI if present
    std::string subId;
    const std::string prefix = "/onvif/events/subscription/";
    if (req.path.rfind(prefix, 0) == 0 && req.path.length() > prefix.length()) {
        subId = req.path.substr(prefix.length());
    }

    std::shared_ptr<PullPointSubscription> sub;
    {
        std::lock_guard<std::mutex> lock(m_subMutex);
        if (!subId.empty()) {
            const auto it = m_subscriptions.find(subId);
            if (it != m_subscriptions.end()) {
                sub = it->second;
            }
        }
        if (!sub && !m_subscriptions.empty()) {
            sub = m_subscriptions.begin()->second;
        }
    }

    if (!sub) {
        // Auto-create implicit subscription if none exists
        std::lock_guard<std::mutex> lock(m_subMutex);
        subId = std::to_string(m_nextSubId++);
        sub = std::make_shared<PullPointSubscription>();
        sub->id = subId;
        sub->terminationTime = std::chrono::steady_clock::now() + std::chrono::minutes(10);
        m_subscriptions[subId] = sub;
    }

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";
    if (m_logCallback) {
        m_logCallback("PullPoint", opName, req.remote_addr);
    }

    std::ostringstream body;

    if (opName.find("PullMessages") != std::string::npos) {
        const pugi::xml_node timeoutNode = doc.select_node("//*[local-name()='Timeout']").node();
        const std::string timeoutStr = timeoutNode ? timeoutNode.text().as_string() : "PT5S";
        const int timeoutSecs = parseTimeoutSeconds(timeoutStr, 5);

        const pugi::xml_node limitNode = doc.select_node("//*[local-name()='MessageLimit']").node();
        const int limit = limitNode ? std::clamp(limitNode.text().as_int(10), 1, 100) : 10;

        std::vector<OnvifEvent> pulledEvents;
        {
            std::unique_lock<std::mutex> lock(sub->mutex);
            if (sub->queue.empty() && timeoutSecs > 0) {
                sub->cv.wait_for(
                    lock, std::chrono::seconds(timeoutSecs), [&]() { return !sub->queue.empty() || !m_running; });
            }

            while (!sub->queue.empty() && static_cast<int>(pulledEvents.size()) < limit) {
                pulledEvents.push_back(std::move(sub->queue.front()));
                sub->queue.pop_front();
            }
        }

        const auto now = std::chrono::system_clock::now();
        const std::string curTime = formatIso8601Utc(now);
        const std::string termTime = formatIso8601Utc(now + std::chrono::minutes(10));

        body << "    <tev:PullMessagesResponse>\r\n"
             << "      <wsnt:CurrentTime>" << curTime << "</wsnt:CurrentTime>\r\n"
             << "      <wsnt:TerminationTime>" << termTime << "</wsnt:TerminationTime>\r\n";

        for (const auto& ev : pulledEvents) {
            body << "      <wsnt:NotificationMessage>\r\n"
                 << "        <wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
                 << ev.topic << "</wsnt:Topic>\r\n"
                 << "        <wsnt:Message UtcTime=\"" << ev.utcTime << "\">\r\n";

            if (!ev.sourceName.empty()) {
                body << "          <tt:Source>\r\n"
                     << "            <tt:SimpleItem Name=\"" << ev.sourceName << "\" Value=\"" << ev.sourceValue
                     << "\"/>\r\n"
                     << "          </tt:Source>\r\n";
            }

            if (!ev.dataName.empty()) {
                body << "          <tt:Data>\r\n"
                     << "            <tt:SimpleItem Name=\"" << ev.dataName << "\" Value=\"" << ev.dataValue
                     << "\"/>\r\n"
                     << "          </tt:Data>\r\n";
            }

            body << "        </wsnt:Message>\r\n"
                 << "      </wsnt:NotificationMessage>\r\n";
        }

        body << "    </tev:PullMessagesResponse>\r\n";
    } else if (opName.find("Unsubscribe") != std::string::npos) {
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            if (!subId.empty()) {
                m_subscriptions.erase(subId);
            }
        }
        body << "    <wsnt:UnsubscribeResponse/>\r\n";
    } else if (opName.find("Renew") != std::string::npos) {
        const auto now = std::chrono::system_clock::now();
        const std::string curTime = formatIso8601Utc(now);
        const std::string termTime = formatIso8601Utc(now + std::chrono::minutes(10));

        body << "    <wsnt:RenewResponse>\r\n"
             << "      <wsnt:TerminationTime>" << termTime << "</wsnt:TerminationTime>\r\n"
             << "      <wsnt:CurrentTime>" << curTime << "</wsnt:CurrentTime>\r\n"
             << "    </wsnt:RenewResponse>\r\n";
    } else {
        body << "    <wsnt:" << opName << "Response/>\r\n";
    }

    res.status = 200;
    res.set_content(wrapSoapResponse(body.str()), "application/soap+xml; charset=utf-8");
}

} // namespace PelcoD::Onvif
