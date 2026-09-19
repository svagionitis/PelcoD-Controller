#include "OnvifServer.h"
#include "OnvifSecurity.h"
#include "XmlUtils.h"

#include <pugixml.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace PelcoD::Onvif {

namespace {

    std::string wrapSoapResponse(const std::string& bodyXml)
    {
        std::ostringstream oss;
        oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
            << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
            << "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
            << "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" "
            << "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
            << "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
            << "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
            << "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
            << "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
            << "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
            << "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" "
            << "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\" "
            << "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
            << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
            << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
            << "  <SOAP-ENV:Body>\r\n"
            << bodyXml << "  </SOAP-ENV:Body>\r\n"
            << "</SOAP-ENV:Envelope>\r\n";
        return oss.str();
    }

    bool isOp(const std::string& opName, const std::string& target)
    {
        if (opName == target) {
            return true;
        }
        if (opName.length() > target.length() && opName.rfind(":" + target) == opName.length() - target.length() - 1) {
            return true;
        }
        return false;
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
            } catch (const std::invalid_argument&) {
                // Fallback
            } catch (const std::out_of_range&) {
                // Fallback
            } catch (const std::exception&) {
                // Fallback
            } catch (...) {
                // Fallback
            }
        }
        return defaultSecs;
    }

    [[nodiscard]] OnvifUser parseOnvifUser(const pugi::xml_node& uNode)
    {
        OnvifUser u {};
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

    [[nodiscard]] PrivacyMask parsePrivacyMask(const pugi::xml_node& maskNode)
    {
        PrivacyMask mask {};
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

        const auto colorNode = maskNode.select_node(".//*[local-name()='Color']").node();
        if (colorNode) {
            if (colorNode.attribute("X")) {
                mask.color.x = colorNode.attribute("X").as_int(0);
            } else if (colorNode.attribute("x")) {
                mask.color.x = colorNode.attribute("x").as_int(0);
            }
            if (colorNode.attribute("Y")) {
                mask.color.y = colorNode.attribute("Y").as_int(0);
            } else if (colorNode.attribute("y")) {
                mask.color.y = colorNode.attribute("y").as_int(0);
            }
            if (colorNode.attribute("Z")) {
                mask.color.z = colorNode.attribute("Z").as_int(0);
            } else if (colorNode.attribute("z")) {
                mask.color.z = colorNode.attribute("z").as_int(0);
            }
            if (colorNode.attribute("Colorspace")) {
                mask.color.colorspace = colorNode.attribute("Colorspace").as_string("RGB");
            } else if (colorNode.attribute("colorspace")) {
                mask.color.colorspace = colorNode.attribute("colorspace").as_string("RGB");
            }
        }

        const auto enNode = maskNode.select_node(".//*[local-name()='Enabled']").node();
        if (enNode) {
            mask.enabled = enNode.text().as_bool(true);
        } else if (maskNode.attribute("Enabled")) {
            mask.enabled = maskNode.attribute("Enabled").as_bool(true);
        }

        return mask;
    }

    void appendPresetTourXml(std::ostringstream& body, const PresetTour& t, const std::string& prefix = "tptz")
    {
        body << "      <" << prefix << ":PresetTour token=\"" << t.token << "\">\r\n";
        if (!t.name.empty()) {
            body << "        <tt:Name>" << t.name << "</tt:Name>\r\n";
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
                 << "            <tt:PresetToken>" << s.presetToken << "</tt:PresetToken>\r\n"
                 << "          </tt:PresetDetail>\r\n"
                 << "          <tt:Speed>\r\n"
                 << "            <tt:PanTilt x=\"" << s.speed << "\" y=\"" << s.speed << "\"/>\r\n"
                 << "          </tt:Speed>\r\n"
                 << "          <tt:StayTime>PT" << s.stayTimeSeconds << "S</tt:StayTime>\r\n"
                 << "        </tt:TourSpot>\r\n";
        }
        body << "      </" << prefix << ":PresetTour>\r\n";
    }

    [[nodiscard]] std::vector<AnalyticsRule> parseAnalyticsRules(const pugi::xml_node& reqNode)
    {
        std::vector<AnalyticsRule> rules;
        for (auto rNode : reqNode.select_nodes(".//*[local-name()='Rule']")) {
            AnalyticsRule rule {};
            rule.name = rNode.node().attribute("Name").as_string();
            rule.type = rNode.node().attribute("Type").as_string();
            for (auto si : rNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
                const std::string sName = si.node().attribute("Name").as_string();
                const std::string sVal = si.node().attribute("Value").as_string();
                if (sName == "Direction") {
                    rule.direction = sVal;
                } else if (sName == "DwellTime") {
                    try {
                        rule.dwellTimeSeconds = std::stod(sVal);
                    } catch (const std::invalid_argument&) {
                    } catch (const std::out_of_range&) {
                    } catch (const std::exception&) {
                    } catch (...) {
                    }
                } else if (sName == "Sensitivity") {
                    try {
                        rule.sensitivity = std::stoi(sVal);
                    } catch (const std::invalid_argument&) {
                    } catch (const std::out_of_range&) {
                    } catch (const std::exception&) {
                    } catch (...) {
                    }
                } else if (sName == "MinConfidence") {
                    try {
                        rule.minConfidence = std::stof(sVal);
                    } catch (const std::invalid_argument&) {
                    } catch (const std::out_of_range&) {
                    } catch (const std::exception&) {
                    } catch (...) {
                    }
                } else if (sName == "Enabled") {
                    rule.enabled = (sVal == "true" || sVal == "1");
                } else if (sName == "Classes") {
                    std::stringstream ss(sVal);
                    std::string c;
                    while (std::getline(ss, c, ',')) {
                        if (!c.empty()) {
                            rule.objectClasses.push_back(c);
                        }
                    }
                }
            }
            const std::vector<Point2D> pts = Xml::parsePoint2DList(rNode.node());
            if (!pts.empty()) {
                if (rule.type.find("Line") != std::string::npos && pts.size() >= 2) {
                    rule.lineStart = pts[0];
                    rule.lineEnd = pts[1];
                } else {
                    rule.polygon = pts;
                }
            }
            rules.push_back(std::move(rule));
        }
        return rules;
    }

} // namespace

OnvifServer::OnvifServer(OnvifServerConfig config, std::shared_ptr<IPtzHandler> ptzHandler,
    std::shared_ptr<IImagingHandler> imagingHandler, std::shared_ptr<IOsdHandler> osdHandler,
    std::shared_ptr<IDeviceManagementHandler> deviceHandler, std::shared_ptr<IDeviceIoHandler> deviceIoHandler,
    std::shared_ptr<IMetadataHandler> metadataHandler, std::shared_ptr<IAnalyticsHandler> analyticsHandler)
    : m_config(std::move(config))
    , m_ptzHandler(std::move(ptzHandler))
    , m_imagingHandler(std::move(imagingHandler))
    , m_osdHandler(std::move(osdHandler))
    , m_deviceHandler(std::move(deviceHandler))
    , m_deviceIoHandler(std::move(deviceIoHandler))
    , m_metadataHandler(std::move(metadataHandler))
    , m_analyticsHandler(std::move(analyticsHandler))
{
    m_internalUsers = m_config.defaultUsers;
    m_internalNetworkInterfaces = m_config.defaultNetworkInterfaces;
    m_internalGateway = m_config.defaultGateway;
    m_internalDns = m_config.defaultDns;
    m_internalNtp = m_config.defaultNtp;
    m_internalHostname = m_config.hostname;
    m_internalRelayOutputs = m_config.defaultRelayOutputs;
    m_internalDigitalInputs = m_config.defaultDigitalInputs;
    m_internalImagingPresets
        = { { "Preset_Clear", "Clear", "Clear Daylight" }, { "Preset_BW", "B/W", "Night Vision B/W" } };
    m_currentImagingPresetToken = "Preset_Clear";

    if (!m_config.defaultMetadataConfigs.empty()) {
        m_internalMetadataConfigs = m_config.defaultMetadataConfigs;
    } else {
        MetadataConfiguration defMeta;
        defMeta.token = "MetadataConfig_1";
        defMeta.name = "DefaultMetadataConfig";
        defMeta.ptzStatusEnabled = true;
        defMeta.analyticsEnabled = true;
        defMeta.eventsEnabled = true;
        defMeta.geoOrientationEnabled = true;
        m_internalMetadataConfigs.push_back(defMeta);
    }

    OsdConfig defaultOsd;
    defaultOsd.token = "OSD_1";
    defaultOsd.videoSourceToken = "VideoSource_1";
    defaultOsd.type = OsdType::Text;
    defaultOsd.position = OsdPositionType::UpperLeft;
    defaultOsd.plainText = "PelcoD Camera";
    defaultOsd.fontSize = 24U;
    m_internalOsds[defaultOsd.token] = defaultOsd;

    if (!m_config.defaultCertificates.empty()) {
        m_internalCertificates = m_config.defaultCertificates;
    } else {
        auto defCert = OnvifSecurity::generateSelfSignedCertificate("Cert_Server", "CN=PelcoD-Camera, O=PelcoD", 365);
        m_internalCertificates.push_back(defCert);
    }

    if (!m_config.defaultRecordings.empty()) {
        m_internalRecordings = m_config.defaultRecordings;
    } else {
        RecordingConfig defRec;
        defRec.recordingToken = "Rec_Main";
        defRec.content = "Continuous Recording";
        defRec.sourceToken = "VideoSource_1";
        defRec.maximumRetentionTime = "P30D";
        RecordingTrack vidTrack;
        vidTrack.trackToken = "Track_Video_1";
        vidTrack.trackType = RecordingTrackType::Video;
        vidTrack.description = "Main H.264 Video Track";
        defRec.tracks.push_back(vidTrack);
        RecordingTrack audTrack;
        audTrack.trackToken = "Track_Audio_1";
        audTrack.trackType = RecordingTrackType::Audio;
        audTrack.description = "AAC Audio Track";
        defRec.tracks.push_back(audTrack);
        m_internalRecordings.push_back(defRec);
    }

    if (!m_config.defaultRecordingJobs.empty()) {
        m_internalRecordingJobs = m_config.defaultRecordingJobs;
    } else {
        RecordingJob defJob;
        defJob.jobToken = "Job_Continuous";
        defJob.recordingToken = "Rec_Main";
        defJob.mode = RecordingJobMode::Active;
        defJob.priority = 5;
        defJob.sourceToken = "Profile_1";
        m_internalRecordingJobs.push_back(defJob);
    }

    m_internalReplayConfig.sessionTimeout = "PT60S";
    if (m_deviceHandler) {
        m_deviceHandler->handleSetGeoLocation(m_config.defaultLocation);
    }

    m_internalMasks = m_config.defaultMasks;
    m_internalMaskOptions = m_config.defaultMaskOptions;
    m_internalVideoSourceModes = m_config.defaultVideoSourceModes;
    if (m_internalVideoSourceModes.empty()) {
        VideoSourceMode mode1;
        mode1.token = "Mode_1080p60";
        mode1.enabled = true;
        mode1.maxFramerate = 60.0f;
        mode1.width = 1920;
        mode1.height = 1080;
        mode1.encodings = { "H264", "H265" };
        mode1.reboot = false;
        mode1.description = "1080p 60fps";

        VideoSourceMode mode2;
        mode2.token = "Mode_4K30";
        mode2.enabled = false;
        mode2.maxFramerate = 30.0f;
        mode2.width = 3840;
        mode2.height = 2160;
        mode2.encodings = { "H264", "H265" };
        mode2.reboot = true;
        mode2.description = "4K 30fps (Reboot required)";

        m_internalVideoSourceModes.push_back(mode1);
        m_internalVideoSourceModes.push_back(mode2);
    }

    m_internalRadiometryConfig = m_config.defaultRadiometryConfig;
    m_internalRadiometrySpots = m_config.defaultRadiometrySpots;
    m_internalRadiometryBoxes = m_config.defaultRadiometryBoxes;
    m_internalColorPalettes = m_config.defaultColorPalettes;
    m_internalThermalCaps = m_config.defaultThermalCapabilities;

    logSystemMessage("INFO", "ONVIF Server initialized successfully");

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
        [[maybe_unused]] const bool discStarted = m_discoveryServer->start();
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

void OnvifServer::setOsdHandler(std::shared_ptr<IOsdHandler> handler)
{
    m_osdHandler = std::move(handler);
}

void OnvifServer::setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> handler)
{
    m_deviceHandler = std::move(handler);
    if (m_deviceHandler) {
        m_deviceHandler->handleSetGeoLocation(m_config.defaultLocation);
    }
}

void OnvifServer::setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> handler)
{
    m_deviceIoHandler = std::move(handler);
}

void OnvifServer::setMetadataHandler(std::shared_ptr<IMetadataHandler> handler)
{
    m_metadataHandler = std::move(handler);
}

void OnvifServer::setRecordingHandler(std::shared_ptr<IRecordingHandler> handler)
{
    m_recordingHandler = std::move(handler);
}

void OnvifServer::setSearchHandler(std::shared_ptr<ISearchHandler> handler)
{
    m_searchHandler = std::move(handler);
}

void OnvifServer::setReplayHandler(std::shared_ptr<IReplayHandler> handler)
{
    m_replayHandler = std::move(handler);
}

void OnvifServer::setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> handler)
{
    m_analyticsHandler = std::move(handler);
}

void OnvifServer::setMaskHandler(std::shared_ptr<IMaskHandler> handler)
{
    m_maskHandler = std::move(handler);
}

void OnvifServer::setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> handler)
{
    m_videoSourceModeHandler = std::move(handler);
}

void OnvifServer::setThermalHandler(std::shared_ptr<IThermalHandler> handler)
{
    m_thermalHandler = std::move(handler);
}

void OnvifServer::logSystemMessage(const std::string& level, const std::string& msg)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    const std::string timestamp = formatIso8601Utc(std::chrono::system_clock::now());
    std::ostringstream ss;
    ss << "[" << timestamp << "] [" << level << "] " << msg;
    m_systemLogs.push_back(ss.str());
    if (m_systemLogs.size() > 500) {
        m_systemLogs.pop_front();
    }
}

void OnvifServer::publishEvent(const OnvifEvent& event)
{
    OnvifEvent ev = event;
    if (ev.utcTime.empty()) {
        ev.utcTime = formatIso8601Utc(std::chrono::system_clock::now());
    }

    std::vector<std::string> pushUrls;
    {
        std::lock_guard<std::mutex> lock(m_subMutex);
        for (auto& [id, sub] : m_subscriptions) {
            std::lock_guard<std::mutex> subLock(sub->mutex);
            sub->queue.push_back(ev);
            sub->cv.notify_one();
        }
        for (const auto& [id, pushSub] : m_pushSubscriptions) {
            if (!pushSub.consumerUrl.empty()) {
                pushUrls.push_back(pushSub.consumerUrl);
            }
        }
    }

    if (!pushUrls.empty()) {
        std::thread([pushUrls = std::move(pushUrls), ev]() {
            for (const auto& url : pushUrls) {
                try {
                    const std::string prefixHttp = "http://";
                    if (url.rfind(prefixHttp, 0) == 0) {
                        const std::string noPrefix = url.substr(prefixHttp.length());
                        const auto slashPos = noPrefix.find('/');
                        const std::string hostPort
                            = (slashPos != std::string::npos) ? noPrefix.substr(0, slashPos) : noPrefix;
                        const std::string path = (slashPos != std::string::npos) ? noPrefix.substr(slashPos) : "/";
                        httplib::Client cli(hostPort);
                        cli.set_connection_timeout(1, 0);
                        cli.set_read_timeout(1, 0);
                        cli.Post(path.c_str(), "<NotificationMessage/>", "application/soap+xml; charset=utf-8");
                    }
                } catch (const std::exception&) {
                    // Ignore client connection error on push notifications
                } catch (...) {
                    // Ignore client connection error on push notifications
                }
            }
        }).detach();
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

    m_httpServer.Post("/onvif/media2_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleMedia2Service(req, res); });

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

    m_httpServer.Post("/onvif/analytics_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleAnalyticsService(req, res); });

    m_httpServer.Post("/onvif/deviceio_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleDeviceIoService(req, res); });

    m_httpServer.Post("/onvif/recording_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleRecordingService(req, res); });

    m_httpServer.Post("/onvif/search_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleSearchService(req, res); });

    m_httpServer.Post("/onvif/replay_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleReplayService(req, res); });

    m_httpServer.Post("/onvif/thermal_service",
        [this](const httplib::Request& req, httplib::Response& res) { handleThermalService(req, res); });

    m_httpServer.Get("/onvif/metadata_stream",
        [this](const httplib::Request& req, httplib::Response& res) { handleMetadataStream(req, res); });

    m_httpServer.Get("/onvif/snapshot", [](const httplib::Request&, httplib::Response& res) {
        res.status = 501;
        res.set_content("Snapshot service not implemented", "text/plain");
    });
}

std::optional<OnvifServer::SoapRequest> OnvifServer::parseSoapRequest(
    const httplib::Request& req, httplib::Response& res, std::string_view serviceName, pugi::xml_document& doc)
{
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return std::nullopt;
    }

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    const pugi::xml_node reqNode = bodyNode ? bodyNode.first_child() : pugi::xml_node {};
    const std::string opName = reqNode ? reqNode.name() : "";
    if (m_logCallback) {
        m_logCallback(std::string(serviceName), opName, req.remote_addr);
    }

    return SoapRequest { bodyNode, reqNode, opName };
}

void OnvifServer::appendAccessLog(
    std::string_view serviceName, const std::string& opName, const std::string& remoteAddr)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    const std::string timestamp = formatIso8601Utc(std::chrono::system_clock::now());
    std::ostringstream entry;
    entry << "[" << timestamp << "] [" << remoteAddr << "] " << serviceName << "Service: " << opName;
    m_accessLogs.push_back(entry.str());
    if (m_accessLogs.size() > 500) {
        m_accessLogs.pop_front();
    }
}

void OnvifServer::sendSoapResponse(httplib::Response& res, const std::string& bodyXml, int status)
{
    res.status = status;
    res.set_content(wrapSoapResponse(bodyXml), "application/soap+xml; charset=utf-8");
}

void OnvifServer::handleDeviceService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Device", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;
    appendAccessLog("Device", opName, req.remote_addr);

    const std::string host = resolveHost(req);
    const int port = m_config.port;

    std::ostringstream body;

    if (isOp(opName, "GetSystemDateAndTime")) {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm utcTm {};
#ifdef _WIN32
        gmtime_s(&utcTm, &nowTime);
#else
        gmtime_r(&nowTime, &utcTm);
#endif
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        const std::string dtType = m_internalDateTime.dateTimeType.empty() ? "Manual" : m_internalDateTime.dateTimeType;
        const std::string tzStr = m_internalDateTime.timeZone.empty() ? "UTC" : m_internalDateTime.timeZone;
        const bool dst = m_internalDateTime.daylightSavings;

        body << "    <tds:GetSystemDateAndTimeResponse>\r\n"
             << "      <tds:SystemDateAndTime>\r\n"
             << "        <tt:DateTimeType>" << dtType << "</tt:DateTimeType>\r\n"
             << "        <tt:DaylightSavings>" << (dst ? "true" : "false") << "</tt:DaylightSavings>\r\n"
             << "        <tt:TimeZone><tt:TZ>" << tzStr << "</tt:TZ></tt:TimeZone>\r\n"
             << "        <tt:UTCDateTime>\r\n"
             << "          <tt:Time><tt:Hour>" << utcTm.tm_hour << "</tt:Hour><tt:Minute>" << utcTm.tm_min
             << "</tt:Minute><tt:Second>" << utcTm.tm_sec << "</tt:Second></tt:Time>\r\n"
             << "          <tt:Date><tt:Year>" << (utcTm.tm_year + 1900) << "</tt:Year><tt:Month>" << (utcTm.tm_mon + 1)
             << "</tt:Month><tt:Day>" << utcTm.tm_mday << "</tt:Day></tt:Date>\r\n"
             << "        </tt:UTCDateTime>\r\n"
             << "      </tds:SystemDateAndTime>\r\n"
             << "    </tds:GetSystemDateAndTimeResponse>\r\n";
    } else if (isOp(opName, "SetSystemDateAndTime")) {
        SystemDateTimeConfig dt;
        const auto dtTypeNode = doc.select_node(".//*[local-name()='DateTimeType']").node();
        if (dtTypeNode) {
            dt.dateTimeType = dtTypeNode.text().as_string();
        }
        const auto dsNode = doc.select_node(".//*[local-name()='DaylightSavings']").node();
        if (dsNode) {
            dt.daylightSavings = dsNode.text().as_bool(false);
        }
        const auto tzNode = doc.select_node(".//*[local-name()='TimeZone']/*[local-name()='TZ']").node();
        if (tzNode) {
            dt.timeZone = tzNode.text().as_string();
        }
        const auto hourNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Time']/*[local-name()='Hour']").node();
        if (hourNode) {
            dt.hour = hourNode.text().as_int();
        }
        const auto minNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Time']/*[local-name()='Minute']")
                  .node();
        if (minNode) {
            dt.minute = minNode.text().as_int();
        }
        const auto secNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Time']/*[local-name()='Second']")
                  .node();
        if (secNode) {
            dt.second = secNode.text().as_int();
        }
        const auto yearNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Date']/*[local-name()='Year']").node();
        if (yearNode) {
            dt.year = yearNode.text().as_int();
        }
        const auto monNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Date']/*[local-name()='Month']").node();
        if (monNode) {
            dt.month = monNode.text().as_int();
        }
        const auto dayNode
            = doc.select_node(".//*[local-name()='UTCDateTime']/*[local-name()='Date']/*[local-name()='Day']").node();
        if (dayNode) {
            dt.day = dayNode.text().as_int();
        }

        if (m_deviceHandler) {
            m_deviceHandler->handleSetSystemDateAndTime(dt);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalDateTime = dt;
        }
        body << "    <tds:SetSystemDateAndTimeResponse/>\r\n";
    } else if (isOp(opName, "GetDeviceInformation")) {
        body << "    <tds:GetDeviceInformationResponse>\r\n"
             << "      <tds:Manufacturer>" << m_config.manufacturer << "</tds:Manufacturer>\r\n"
             << "      <tds:Model>" << m_config.model << "</tds:Model>\r\n"
             << "      <tds:FirmwareVersion>" << m_config.firmwareVersion << "</tds:FirmwareVersion>\r\n"
             << "      <tds:SerialNumber>" << m_config.serialNumber << "</tds:SerialNumber>\r\n"
             << "      <tds:HardwareId>" << m_config.hardwareId << "</tds:HardwareId>\r\n"
             << "    </tds:GetDeviceInformationResponse>\r\n";
    } else if (isOp(opName, "GetCapabilities")) {
        size_t numRelays = 0;
        {
            std::lock_guard<std::mutex> lock(m_deviceIoMutex);
            numRelays = m_internalRelayOutputs.size();
        }

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
             << "        <tt:Analytics>\r\n"
             << "          <tt:XAddr>http://" << host << ":" << port << "/onvif/analytics_service</tt:XAddr>\r\n"
             << "          <tt:RuleSupport>true</tt:RuleSupport>\r\n"
             << "        </tt:Analytics>\r\n"
             << "        <tt:Extension>\r\n"
             << "          <tt:DeviceIO>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/deviceio_service</tt:XAddr>\r\n"
             << "            <tt:VideoSources>1</tt:VideoSources>\r\n"
             << "            <tt:RelayOutputs>" << numRelays << "</tt:RelayOutputs>\r\n"
             << "          </tt:DeviceIO>\r\n"
             << "          <tt:Recording>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/recording_service</tt:XAddr>\r\n"
             << "            <tt:Receiver>false</tt:Receiver>\r\n"
             << "            <tt:MediaProfileSummary>false</tt:MediaProfileSummary>\r\n"
             << "          </tt:Recording>\r\n"
             << "          <tt:Search>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/search_service</tt:XAddr>\r\n"
             << "            <tt:MetadataSearch>true</tt:MetadataSearch>\r\n"
             << "          </tt:Search>\r\n"
             << "          <tt:Replay>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/replay_service</tt:XAddr>\r\n"
             << "          </tt:Replay>\r\n"
             << "          <tt:Media2>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/media2_service</tt:XAddr>\r\n"
             << "          </tt:Media2>\r\n"
             << "          <tt:Thermal>\r\n"
             << "            <tt:XAddr>http://" << host << ":" << port << "/onvif/thermal_service</tt:XAddr>\r\n"
             << "          </tt:Thermal>\r\n"
             << "        </tt:Extension>\r\n"
             << "      </tds:Capabilities>\r\n"
             << "    </tds:GetCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "GetServices")) {
        body << "    <tds:GetServicesResponse>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/device_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/thermal/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/thermal_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>1</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/media_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver20/media/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/media2_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>20</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
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
             << "        <tds:Namespace>http://www.onvif.org/ver10/deviceIO/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/deviceio_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/events/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/event_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver20/analytics/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/analytics_service</tds:XAddr>\r\n"
             << "        <tds:Version><tt:Major>20</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/recording/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/recording_service</tt:XAddr>\r\n"
             << "        <tds:Version><tt:Major>17</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/search/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/search_service</tt:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "      <tds:Service>\r\n"
             << "        <tds:Namespace>http://www.onvif.org/ver10/replay/wsdl</tds:Namespace>\r\n"
             << "        <tds:XAddr>http://" << host << ":" << port << "/onvif/replay_service</tt:XAddr>\r\n"
             << "        <tds:Version><tt:Major>10</tt:Major><tt:Minor>0</tt:Minor></tds:Version>\r\n"
             << "      </tds:Service>\r\n"
             << "    </tds:GetServicesResponse>\r\n";
    } else if (isOp(opName, "GetScopes")) {
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
    } else if (isOp(opName, "AddScopes")) {
        const auto scNodes = doc.select_nodes(".//*[local-name()='ScopeItem']");
        for (const auto& sel : scNodes) {
            m_config.scopes.push_back(sel.node().text().as_string());
        }
        body << "    <tds:AddScopesResponse/>\r\n";
    } else if (isOp(opName, "RemoveScopes")) {
        const auto scNodes = doc.select_nodes(".//*[local-name()='ScopeItem']");
        for (const auto& sel : scNodes) {
            const std::string target = sel.node().text().as_string();
            m_config.scopes.erase(
                std::remove(m_config.scopes.begin(), m_config.scopes.end(), target), m_config.scopes.end());
        }
        body << "    <tds:RemoveScopesResponse/>\r\n";
    } else if (isOp(opName, "SetScopes")) {
        m_config.scopes.clear();
        const auto scNodes = doc.select_nodes(".//*[local-name()='ScopeItem']");
        for (const auto& sel : scNodes) {
            m_config.scopes.push_back(sel.node().text().as_string());
        }
        body << "    <tds:SetScopesResponse/>\r\n";
    } else if (isOp(opName, "GetUsers")) {
        std::vector<OnvifUser> users;
        if (m_deviceHandler) {
            users = m_deviceHandler->handleGetUsers();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            users = m_internalUsers;
        }
        body << "    <tds:GetUsersResponse>\r\n";
        for (const auto& u : users) {
            body << "      <tds:User>\r\n"
                 << "        <tt:Username>" << u.username << "</tt:Username>\r\n"
                 << "        <tt:UserLevel>" << userLevelToString(u.level) << "</tt:UserLevel>\r\n"
                 << "      </tds:User>\r\n";
        }
        body << "    </tds:GetUsersResponse>\r\n";
    } else if (isOp(opName, "CreateUsers")) {
        const auto userNodes = doc.select_nodes(".//*[local-name()='User']");
        std::vector<OnvifUser> newUsers;
        for (const auto& sel : userNodes) {
            const auto u = parseOnvifUser(sel.node());
            if (!u.username.empty()) {
                newUsers.push_back(u);
            }
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleCreateUsers(newUsers);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            for (const auto& nu : newUsers) {
                auto it = std::find_if(m_internalUsers.begin(), m_internalUsers.end(),
                    [&](const OnvifUser& existing) { return existing.username == nu.username; });
                if (it != m_internalUsers.end()) {
                    *it = nu;
                } else {
                    m_internalUsers.push_back(nu);
                }
            }
        }
        body << "    <tds:CreateUsersResponse/>\r\n";
    } else if (isOp(opName, "SetUser")) {
        const auto userNodes = doc.select_nodes(".//*[local-name()='User']");
        for (const auto& sel : userNodes) {
            const auto u = parseOnvifUser(sel.node());
            if (!u.username.empty()) {
                if (m_deviceHandler) {
                    m_deviceHandler->handleSetUser(u);
                }
                std::lock_guard<std::mutex> lock(m_deviceMutex);
                auto it = std::find_if(m_internalUsers.begin(), m_internalUsers.end(),
                    [&](const OnvifUser& existing) { return existing.username == u.username; });
                if (it != m_internalUsers.end()) {
                    *it = u;
                } else {
                    m_internalUsers.push_back(u);
                }
            }
        }
        body << "    <tds:SetUserResponse/>\r\n";
    } else if (isOp(opName, "DeleteUsers")) {
        const auto unNodes = doc.select_nodes(".//*[local-name()='Username']");
        std::vector<std::string> names;
        for (const auto& sel : unNodes) {
            names.push_back(sel.node().text().as_string());
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleDeleteUsers(names);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            for (const auto& name : names) {
                m_internalUsers.erase(std::remove_if(m_internalUsers.begin(), m_internalUsers.end(),
                                          [&](const OnvifUser& u) { return u.username == name; }),
                    m_internalUsers.end());
            }
        }
        body << "    <tds:DeleteUsersResponse/>\r\n";
    } else if (isOp(opName, "GetNetworkInterfaces")) {
        std::vector<NetworkInterfaceConfig> ifaces;
        if (m_deviceHandler) {
            ifaces = m_deviceHandler->handleGetNetworkInterfaces();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            ifaces = m_internalNetworkInterfaces;
        }
        body << "    <tds:GetNetworkInterfacesResponse>\r\n";
        for (const auto& iface : ifaces) {
            body << "      <tds:NetworkInterfaces token=\"" << iface.token << "\">\r\n"
                 << "        <tt:Enabled>" << (iface.enabled ? "true" : "false") << "</tt:Enabled>\r\n"
                 << "        <tt:Info>\r\n"
                 << "          <tt:Name>" << iface.name << "</tt:Name>\r\n"
                 << "          <tt:HwAddress>" << iface.hwAddress << "</tt:HwAddress>\r\n"
                 << "          <tt:MTU>" << iface.mtu << "</tt:MTU>\r\n"
                 << "        </tt:Info>\r\n"
                 << "        <tt:IPv4>\r\n"
                 << "          <tt:Enabled>" << (iface.ipv4.enabled ? "true" : "false") << "</tt:Enabled>\r\n"
                 << "          <tt:Config>\r\n"
                 << "            <tt:Manual>\r\n"
                 << "              <tt:Address>" << iface.ipv4.manualAddress << "</tt:Address>\r\n"
                 << "              <tt:PrefixLength>" << iface.ipv4.prefixLength << "</tt:PrefixLength>\r\n"
                 << "            </tt:Manual>\r\n"
                 << "            <tt:DHCP>" << (iface.ipv4.dhcp ? "true" : "false") << "</tt:DHCP>\r\n"
                 << "          </tt:Config>\r\n"
                 << "        </tt:IPv4>\r\n"
                 << "      </tds:NetworkInterfaces>\r\n";
        }
        body << "    </tds:GetNetworkInterfacesResponse>\r\n";
    } else if (isOp(opName, "SetNetworkInterfaces")) {
        NetworkInterfaceConfig iface;
        const auto tokNode = doc.select_node(".//*[local-name()='InterfaceToken']").node();
        if (tokNode) {
            iface.token = tokNode.text().as_string();
        }
        const auto enNode = doc.select_node(".//*[local-name()='NetworkInterface']/*[local-name()='Enabled']").node();
        if (enNode) {
            iface.enabled = enNode.text().as_bool(true);
        }
        const auto mtuNode = doc.select_node(".//*[local-name()='MTU']").node();
        if (mtuNode) {
            iface.mtu = mtuNode.text().as_int(1500);
        }
        const auto ipv4EnNode = doc.select_node(".//*[local-name()='IPv4']/*[local-name()='Enabled']").node();
        if (ipv4EnNode) {
            iface.ipv4.enabled = ipv4EnNode.text().as_bool(true);
        }
        const auto dhcpNode = doc.select_node(".//*[local-name()='DHCP']").node();
        if (dhcpNode) {
            iface.ipv4.dhcp = dhcpNode.text().as_bool(false);
        }
        const auto addrNode = doc.select_node(".//*[local-name()='Manual']/*[local-name()='Address']").node();
        if (addrNode) {
            iface.ipv4.manualAddress = addrNode.text().as_string();
        }
        const auto pfxNode = doc.select_node(".//*[local-name()='Manual']/*[local-name()='PrefixLength']").node();
        if (pfxNode) {
            iface.ipv4.prefixLength = pfxNode.text().as_int(24);
        }

        if (m_deviceHandler) {
            m_deviceHandler->handleSetNetworkInterfaces(iface);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            auto it = std::find_if(m_internalNetworkInterfaces.begin(), m_internalNetworkInterfaces.end(),
                [&](const NetworkInterfaceConfig& c) { return c.token == iface.token; });
            if (it != m_internalNetworkInterfaces.end()) {
                *it = iface;
            } else {
                m_internalNetworkInterfaces.push_back(iface);
            }
        }
        body << "    <tds:SetNetworkInterfacesResponse>\r\n"
             << "      <tds:RebootNeeded>false</tds:RebootNeeded>\r\n"
             << "    </tds:SetNetworkInterfacesResponse>\r\n";
    } else if (isOp(opName, "GetNetworkDefaultGateway")) {
        std::string gw;
        if (m_deviceHandler) {
            gw = m_deviceHandler->handleGetNetworkDefaultGateway();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            gw = m_internalGateway;
        }
        body << "    <tds:GetNetworkDefaultGatewayResponse>\r\n"
             << "      <tds:NetworkGateway>\r\n"
             << "        <tt:IPv4Address>" << gw << "</tt:IPv4Address>\r\n"
             << "      </tds:NetworkGateway>\r\n"
             << "    </tds:GetNetworkDefaultGatewayResponse>\r\n";
    } else if (isOp(opName, "SetNetworkDefaultGateway")) {
        const auto gwNode = doc.select_node(".//*[local-name()='IPv4Address']").node();
        std::string gw = gwNode ? gwNode.text().as_string() : "";
        if (m_deviceHandler) {
            m_deviceHandler->handleSetNetworkDefaultGateway(gw);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalGateway = gw;
        }
        body << "    <tds:SetNetworkDefaultGatewayResponse/>\r\n";
    } else if (isOp(opName, "GetDNS")) {
        DnsConfig dns;
        if (m_deviceHandler) {
            dns = m_deviceHandler->handleGetDNS();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            dns = m_internalDns;
        }
        body << "    <tds:GetDNSResponse>\r\n"
             << "      <tds:DNSInformation>\r\n"
             << "        <tt:FromDHCP>" << (dns.fromDhcp ? "true" : "false") << "</tt:FromDHCP>\r\n";
        for (const auto& sd : dns.searchDomains) {
            body << "        <tt:SearchDomain>" << sd << "</tt:SearchDomain>\r\n";
        }
        for (const auto& server : dns.dnsServers) {
            body << "        <tt:DNSManual>\r\n"
                 << "          <tt:Type>IPv4</tt:Type>\r\n"
                 << "          <tt:IPv4Address>" << server << "</tt:IPv4Address>\r\n"
                 << "        </tt:DNSManual>\r\n";
        }
        body << "      </tds:DNSInformation>\r\n"
             << "    </tds:GetDNSResponse>\r\n";
    } else if (isOp(opName, "SetDNS")) {
        DnsConfig dns;
        const auto dhcpNode = doc.select_node(".//*[local-name()='FromDHCP']").node();
        if (dhcpNode) {
            dns.fromDhcp = dhcpNode.text().as_bool(false);
        }
        const auto sdNodes = doc.select_nodes(".//*[local-name()='SearchDomain']");
        for (const auto& sel : sdNodes) {
            dns.searchDomains.push_back(sel.node().text().as_string());
        }
        const auto srvNodes = doc.select_nodes(".//*[local-name()='DNSManual']/*[local-name()='IPv4Address']");
        for (const auto& sel : srvNodes) {
            dns.dnsServers.push_back(sel.node().text().as_string());
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleSetDNS(dns);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalDns = dns;
        }
        body << "    <tds:SetDNSResponse/>\r\n";
    } else if (isOp(opName, "GetNTP")) {
        NtpConfig ntp;
        if (m_deviceHandler) {
            ntp = m_deviceHandler->handleGetNTP();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            ntp = m_internalNtp;
        }
        body << "    <tds:GetNTPResponse>\r\n"
             << "      <tds:NTPInformation>\r\n"
             << "        <tt:FromDHCP>" << (ntp.fromDhcp ? "true" : "false") << "</tt:FromDHCP>\r\n";
        for (const auto& srv : ntp.manualServers) {
            body << "        <tt:NTPManual>\r\n"
                 << "          <tt:Type>DNS</tt:Type>\r\n"
                 << "          <tt:DNSname>" << srv << "</tt:DNSname>\r\n"
                 << "        </tt:NTPManual>\r\n";
        }
        body << "      </tds:NTPInformation>\r\n"
             << "    </tds:GetNTPResponse>\r\n";
    } else if (isOp(opName, "SetNTP")) {
        NtpConfig ntp;
        const auto dhcpNode = doc.select_node(".//*[local-name()='FromDHCP']").node();
        if (dhcpNode) {
            ntp.fromDhcp = dhcpNode.text().as_bool(false);
        }
        const auto ntpNodes = doc.select_nodes(".//*[local-name()='NTPManual']/*[local-name()='DNSname']");
        for (const auto& sel : ntpNodes) {
            ntp.manualServers.push_back(sel.node().text().as_string());
        }
        const auto ipNodes = doc.select_nodes(".//*[local-name()='NTPManual']/*[local-name()='IPv4Address']");
        for (const auto& sel : ipNodes) {
            ntp.manualServers.push_back(sel.node().text().as_string());
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleSetNTP(ntp);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalNtp = ntp;
        }
        body << "    <tds:SetNTPResponse/>\r\n";
    } else if (isOp(opName, "GetHostname")) {
        std::string hn;
        if (m_deviceHandler) {
            hn = m_deviceHandler->handleGetHostname();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            hn = m_internalHostname;
        }
        body << "    <tds:GetHostnameResponse>\r\n"
             << "      <tds:HostnameInformation>\r\n"
             << "        <tt:FromDHCP>false</tt:FromDHCP>\r\n"
             << "        <tt:Name>" << hn << "</tt:Name>\r\n"
             << "      </tds:HostnameInformation>\r\n"
             << "    </tds:GetHostnameResponse>\r\n";
    } else if (isOp(opName, "SetHostname")) {
        const auto hnNode = doc.select_node(".//*[local-name()='Name']").node();
        std::string hn = hnNode ? hnNode.text().as_string() : "";
        if (m_deviceHandler) {
            m_deviceHandler->handleSetHostname(hn);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalHostname = hn;
        }
        body << "    <tds:SetHostnameResponse/>\r\n";
    } else if (isOp(opName, "SetSystemFactoryDefault")) {
        const auto defNode = doc.select_node(".//*[local-name()='FactoryDefault']").node();
        const std::string defType = defNode ? defNode.text().as_string() : "Soft";
        const FactoryDefaultType type = (defType == "Hard") ? FactoryDefaultType::Hard : FactoryDefaultType::Soft;
        if (m_deviceHandler) {
            m_deviceHandler->handleSetSystemFactoryDefault(type);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_internalUsers = m_config.defaultUsers;
            m_internalNetworkInterfaces = m_config.defaultNetworkInterfaces;
            m_internalGateway = m_config.defaultGateway;
            m_internalDns = m_config.defaultDns;
            m_internalNtp = m_config.defaultNtp;
            m_internalHostname = m_config.hostname;
        }
        body << "    <tds:SetSystemFactoryDefaultResponse/>\r\n";
    } else if (isOp(opName, "SystemReboot")) {
        std::string msg = "Rebooting";
        if (m_deviceHandler) {
            msg = m_deviceHandler->handleSystemReboot();
        }
        body << "    <tds:SystemRebootResponse>\r\n"
             << "      <tds:Message>" << msg << "</tds:Message>\r\n"
             << "    </tds:SystemRebootResponse>\r\n";
    } else if (isOp(opName, "GetSystemLog")) {
        const pugi::xml_node logTypeNode = reqNode.select_node(".//*[local-name()='LogType']").node();
        const std::string logTypeStr = logTypeNode ? logTypeNode.text().as_string() : "System";
        const SystemLogType logType = (logTypeStr == "Access") ? SystemLogType::Access : SystemLogType::System;
        std::string logData;
        if (m_deviceHandler) {
            logData = m_deviceHandler->handleGetSystemLog(logType);
        }
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            const auto& logs = (logType == SystemLogType::Access) ? m_accessLogs : m_systemLogs;
            std::ostringstream ss;
            if (!logData.empty()) {
                ss << logData << "\n";
            }
            for (const auto& entry : logs) {
                ss << entry << "\n";
            }
            logData = ss.str();
        }
        body << "    <tds:GetSystemLogResponse>\r\n"
             << "      <tds:SystemLog>\r\n"
             << "        <tt:String>" << logData << "</tt:String>\r\n"
             << "      </tds:SystemLog>\r\n"
             << "    </tds:GetSystemLogResponse>\r\n";
    } else if (isOp(opName, "GetSystemSupportInformation")) {
        SystemSupportInfo info;
        if (m_deviceHandler) {
            info = m_deviceHandler->handleGetSystemSupportInformation();
        }
        if (info.rawDiagnostics.empty()) {
            const auto uptime
                = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_startTime)
                      .count();
            info.uptimeSeconds = static_cast<std::uint64_t>(uptime);
            info.storageState = "OK";
            std::ostringstream ss;
            ss << "ONVIF System Diagnostics & Support Information\n"
               << "---------------------------------------------\n"
               << "Model: " << m_config.model << "\n"
               << "Manufacturer: " << m_config.manufacturer << "\n"
               << "Firmware: " << m_config.firmwareVersion << "\n"
               << "HardwareId: " << m_config.hardwareId << "\n"
               << "SerialNumber: " << m_config.serialNumber << "\n"
               << "Hostname: " << m_internalHostname << "\n"
               << "Uptime: " << uptime << " seconds\n";
            info.rawDiagnostics = ss.str();
        }
        body << "    <tds:GetSystemSupportInformationResponse>\r\n"
             << "      <tds:SupportInformation>\r\n"
             << "        <tt:String>" << info.rawDiagnostics << "</tt:String>\r\n"
             << "      </tds:SupportInformation>\r\n"
             << "    </tds:GetSystemSupportInformationResponse>\r\n";
    } else if (isOp(opName, "GetSystemBackup")) {
        std::string backupData;
        if (m_deviceHandler) {
            backupData = m_deviceHandler->handleGetSystemBackup();
        }
        if (backupData.empty()) {
            std::ostringstream ss;
            ss << "{\n"
               << "  \"hostname\": \"" << m_internalHostname << "\",\n"
               << "  \"gateway\": \"" << m_internalGateway << "\",\n"
               << "  \"dns\": \"" << (m_internalDns.dnsServers.empty() ? "" : m_internalDns.dnsServers[0]) << "\",\n"
               << "  \"ntp\": \"" << (m_internalNtp.manualServers.empty() ? "" : m_internalNtp.manualServers[0])
               << "\"\n"
               << "}";
            backupData = ss.str();
        }
        body << "    <tds:GetSystemBackupResponse>\r\n"
             << "      <tds:BackupFiles>\r\n"
             << "        <tt:Data>" << backupData << "</tt:Data>\r\n"
             << "      </tds:BackupFiles>\r\n"
             << "    </tds:GetSystemBackupResponse>\r\n";
    } else if (isOp(opName, "RestoreSystem")) {
        const pugi::xml_node dataNode
            = reqNode.select_node(".//*[local-name()='BackupFiles']/*[local-name()='Data']").node();
        const std::string backupData = dataNode ? dataNode.text().as_string() : "";
        bool ok = true;
        if (m_deviceHandler) {
            ok = m_deviceHandler->handleRestoreSystem(backupData);
        }
        logSystemMessage("INFO", std::string("RestoreSystem executed, status: ") + (ok ? "SUCCESS" : "FAILURE"));
        body << "    <tds:RestoreSystemResponse>\r\n"
             << "      <tds:Message>" << (ok ? "Restored" : "Failed") << "</tds:Message>\r\n"
             << "    </tds:RestoreSystemResponse>\r\n";
    } else if (isOp(opName, "GetEndpointReference")) {
        std::string ep;
        if (m_deviceHandler) {
            ep = m_deviceHandler->handleGetEndpointReference();
        }
        if (ep.empty()) {
            ep = "urn:uuid:" + m_config.serialNumber;
        }
        body << "    <tds:GetEndpointReferenceResponse>\r\n"
             << "      <tds:GUID>" << ep << "</tds:GUID>\r\n"
             << "    </tds:GetEndpointReferenceResponse>\r\n";
    } else if (isOp(opName, "GetCertificates")) {
        std::vector<OnvifCertificate> certs;
        if (m_deviceHandler) {
            certs = m_deviceHandler->handleGetCertificates();
        }
        if (certs.empty()) {
            std::lock_guard<std::mutex> lock(m_certMutex);
            certs = m_internalCertificates;
        }
        body << "    <tds:GetCertificatesResponse>\r\n";
        for (const auto& cert : certs) {
            body << "      <tds:NvtCertificate>\r\n"
                 << "        <tt:CertificateID>" << cert.certificateId << "</tt:CertificateID>\r\n"
                 << "        <tt:Certificate>\r\n"
                 << "          <tt:Data>" << cert.x509DerBase64 << "</tt:Data>\r\n"
                 << "        </tt:Certificate>\r\n"
                 << "      </tds:NvtCertificate>\r\n";
        }
        body << "    </tds:GetCertificatesResponse>\r\n";
    } else if (isOp(opName, "GetCertificateInformation")) {
        const pugi::xml_node idNode = reqNode.select_node(".//*[local-name()='CertificateID']").node();
        const std::string certId = idNode ? idNode.text().as_string() : "";
        CertificateInformation info {};
        bool found = false;
        if (m_deviceHandler) {
            const auto opt = m_deviceHandler->handleGetCertificateInformation(certId);
            if (opt.has_value()) {
                info = *opt;
                found = true;
            }
        }
        if (!found) {
            std::lock_guard<std::mutex> lock(m_certMutex);
            for (const auto& c : m_internalCertificates) {
                if (c.certificateId == certId) {
                    if (!c.info.certificateId.empty()) {
                        info = c.info;
                        found = true;
                    } else {
                        info = OnvifSecurity::parseCertificateInfo(c.certificateId, c.x509DerBase64);
                        found = true;
                    }
                    break;
                }
            }
        }
        body << "    <tds:GetCertificateInformationResponse>\r\n"
             << "      <tds:CertificateInformation>\r\n"
             << "        <tt:CertificateID>" << (info.certificateId.empty() ? certId : info.certificateId)
             << "</tt:CertificateID>\r\n"
             << "        <tt:IssuerDN>" << info.issuer << "</tt:IssuerDN>\r\n"
             << "        <tt:SubjectDN>" << info.subject << "</tt:SubjectDN>\r\n"
             << "        <tt:Validity>\r\n"
             << "          <tt:From>" << info.validNotBefore << "</tt:From>\r\n"
             << "          <tt:Until>" << info.validNotAfter << "</tt:Until>\r\n"
             << "        </tt:Validity>\r\n"
             << "        <tt:Extension>\r\n"
             << "          <tt:KeyUsage>" << info.keyAlgorithm << "</tt:KeyUsage>\r\n"
             << "        </tt:Extension>\r\n"
             << "      </tds:CertificateInformation>\r\n"
             << "    </tds:GetCertificateInformationResponse>\r\n";
    } else if (isOp(opName, "CreateCertificate")) {
        const pugi::xml_node idNode = reqNode.select_node(".//*[local-name()='CertificateID']").node();
        const pugi::xml_node subjNode = reqNode.select_node(".//*[local-name()='Subject']").node();
        const std::string certId = idNode
            ? idNode.text().as_string()
            : "Cert_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        const std::string subj = subjNode ? subjNode.text().as_string() : "CN=" + m_config.deviceName;

        OnvifCertificate newCert {};
        if (m_deviceHandler) {
            newCert = m_deviceHandler->handleCreateCertificate(certId, subj, 365);
        } else {
            newCert = OnvifSecurity::generateSelfSignedCertificate(certId, subj, 365);
        }
        {
            std::lock_guard<std::mutex> lock(m_certMutex);
            auto it = std::find_if(m_internalCertificates.begin(), m_internalCertificates.end(),
                [&certId](const OnvifCertificate& c) { return c.certificateId == certId; });
            if (it != m_internalCertificates.end()) {
                *it = newCert;
            } else {
                m_internalCertificates.push_back(newCert);
            }
            logSystemMessage("INFO", "Created certificate ID: " + certId + " subject: " + subj);
        }

        body << "    <tds:CreateCertificateResponse>\r\n"
             << "      <tds:NvtCertificate>\r\n"
             << "        <tt:CertificateID>" << certId << "</tt:CertificateID>\r\n"
             << "        <tt:Certificate>\r\n"
             << "          <tt:Data>" << newCert.x509DerBase64 << "</tt:Data>\r\n"
             << "        </tt:Certificate>\r\n"
             << "      </tds:NvtCertificate>\r\n"
             << "    </tds:CreateCertificateResponse>\r\n";
    } else if (isOp(opName, "GetPkcs10Request")) {
        const pugi::xml_node idNode = reqNode.select_node(".//*[local-name()='CertificateID']").node();
        const pugi::xml_node subjNode = reqNode.select_node(".//*[local-name()='Subject']").node();
        const std::string certId = idNode ? idNode.text().as_string() : "";
        const std::string subj = subjNode ? subjNode.text().as_string() : "CN=" + m_config.deviceName;

        Pkcs10Request csr {};
        if (m_deviceHandler) {
            csr = m_deviceHandler->handleGetPkcs10Request(certId, subj);
        } else {
            csr = OnvifSecurity::generatePkcs10Csr(certId, subj);
        }
        logSystemMessage("INFO", "Generated PKCS#10 CSR for " + certId + " (" + subj + ")");
        body << "    <tds:GetPkcs10RequestResponse>\r\n"
             << "      <tds:Pkcs10Request>" << csr.csrBase64 << "</tds:Pkcs10Request>\r\n"
             << "    </tds:GetPkcs10RequestResponse>\r\n";
    } else if (isOp(opName, "LoadCertificates")) {
        std::vector<OnvifCertificate> certsToLoad;
        const auto certNodes = reqNode.select_nodes(".//*[local-name()='NVTCertificate']");
        for (const auto& cNode : certNodes) {
            const pugi::xml_node idN = cNode.node().select_node(".//*[local-name()='CertificateID']").node();
            const pugi::xml_node dataN = cNode.node().select_node(".//*[local-name()='Data']").node();
            if (idN && dataN) {
                OnvifCertificate c;
                c.certificateId = idN.text().as_string();
                c.x509DerBase64 = dataN.text().as_string();
                c.info = OnvifSecurity::parseCertificateInfo(c.certificateId, c.x509DerBase64);
                certsToLoad.push_back(c);
            }
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleLoadCertificates(certsToLoad);
        }
        {
            std::lock_guard<std::mutex> lock(m_certMutex);
            for (const auto& c : certsToLoad) {
                auto it = std::find_if(m_internalCertificates.begin(), m_internalCertificates.end(),
                    [&c](const OnvifCertificate& item) { return item.certificateId == c.certificateId; });
                if (it != m_internalCertificates.end()) {
                    *it = c;
                } else {
                    m_internalCertificates.push_back(c);
                }
            }
        }
        logSystemMessage("INFO", "Loaded " + std::to_string(certsToLoad.size()) + " certificates");
        body << "    <tds:LoadCertificatesResponse/>\r\n";
    } else if (isOp(opName, "DeleteCertificates")) {
        std::vector<std::string> ids;
        const auto idNodes = reqNode.select_nodes(".//*[local-name()='CertificateID']");
        for (const auto& idN : idNodes) {
            ids.push_back(idN.node().text().as_string());
        }
        if (m_deviceHandler) {
            for (const auto& id : ids) {
                m_deviceHandler->handleDeleteCertificate(id);
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_certMutex);
            for (const auto& id : ids) {
                m_internalCertificates.erase(
                    std::remove_if(m_internalCertificates.begin(), m_internalCertificates.end(),
                        [&id](const OnvifCertificate& c) { return c.certificateId == id; }),
                    m_internalCertificates.end());
            }
        }
        logSystemMessage("INFO", "Deleted " + std::to_string(ids.size()) + " certificates");
        body << "    <tds:DeleteCertificatesResponse/>\r\n";
    } else if (isOp(opName, "GetClientCertificateMode")) {
        ClientCertificateMode mode = m_internalClientCertMode;
        if (m_deviceHandler) {
            mode = m_deviceHandler->handleGetClientCertificateMode();
        }
        body << "    <tds:GetClientCertificateModeResponse>\r\n"
             << "      <tds:ClientCertificateMode>" << clientCertificateModeToString(mode)
             << "</tds:ClientCertificateMode>\r\n"
             << "    </tds:GetClientCertificateModeResponse>\r\n";
    } else if (isOp(opName, "SetClientCertificateMode")) {
        const pugi::xml_node modeNode = reqNode.select_node(".//*[local-name()='ClientCertificateMode']").node();
        const std::string modeStr = modeNode ? modeNode.text().as_string() : "Off";
        const ClientCertificateMode mode = clientCertificateModeFromString(modeStr);
        if (m_deviceHandler) {
            m_deviceHandler->handleSetClientCertificateMode(mode);
        }
        {
            std::lock_guard<std::mutex> lock(m_certMutex);
            m_internalClientCertMode = mode;
        }
        logSystemMessage("INFO", "Set ClientCertificateMode to " + modeStr);
        body << "    <tds:SetClientCertificateModeResponse/>\r\n";
    } else if (isOp(opName, "GetGeoLocation")) {
        const pugi::xml_node entNode = reqNode.select_node(".//*[local-name()='Entity']").node();
        const std::string entToken = entNode ? entNode.text().as_string() : "Device";

        LocationEntity loc = m_config.defaultLocation;
        if (m_deviceHandler) {
            const auto opt = m_deviceHandler->handleGetGeoLocation(entToken);
            if (opt.has_value()) {
                loc = *opt;
            }
        }
        body << "    <tds:GetGeoLocationResponse>\r\n"
             << "      <tds:Location Entity=\"" << loc.entity << "\" Token=\"" << loc.token << "\" Fixed=\""
             << (loc.fixed ? "true" : "false") << "\">\r\n"
             << "        <tt:GeoLocation lat=\"" << std::fixed << std::setprecision(6) << loc.location.latitude
             << "\" lon=\"" << loc.location.longitude << "\" elevation=\"" << std::setprecision(2)
             << loc.location.elevation << "\"/>\r\n"
             << "        <tt:GeoOrientation yaw=\"" << std::setprecision(2) << loc.orientation.yaw << "\" pitch=\""
             << loc.orientation.pitch << "\" roll=\"" << loc.orientation.roll << "\"/>\r\n"
             << "      </tds:Location>\r\n"
             << "    </tds:GetGeoLocationResponse>\r\n";
    } else if (isOp(opName, "SetGeoLocation")) {
        const pugi::xml_node locNode = reqNode.select_node(".//*[local-name()='Location']").node();
        LocationEntity loc {};
        if (locNode) {
            loc.entity = locNode.attribute("Entity").as_string("Device");
            loc.token = locNode.attribute("Token").as_string("Location_1");
            loc.fixed = locNode.attribute("Fixed").as_bool(true);

            const pugi::xml_node geoNode = locNode.select_node(".//*[local-name()='GeoLocation']").node();
            if (geoNode) {
                if (geoNode.attribute("lat")) {
                    loc.location.latitude = geoNode.attribute("lat").as_double(0.0);
                    loc.location.longitude = geoNode.attribute("lon").as_double(0.0);
                    loc.location.elevation = geoNode.attribute("elevation").as_double(0.0);
                } else {
                    const auto latN = geoNode.child("Latitude");
                    const auto lonN = geoNode.child("Longitude");
                    const auto elN = geoNode.child("Elevation");
                    if (latN)
                        loc.location.latitude = latN.text().as_double(0.0);
                    if (lonN)
                        loc.location.longitude = lonN.text().as_double(0.0);
                    if (elN)
                        loc.location.elevation = elN.text().as_double(0.0);
                }
            }

            const pugi::xml_node oriNode = locNode.select_node(".//*[local-name()='GeoOrientation']").node();
            if (oriNode) {
                if (oriNode.attribute("yaw")) {
                    loc.orientation.yaw = oriNode.attribute("yaw").as_double(0.0);
                    loc.orientation.pitch = oriNode.attribute("pitch").as_double(0.0);
                    loc.orientation.roll = oriNode.attribute("roll").as_double(0.0);
                } else {
                    const auto yN = oriNode.child("Yaw");
                    const auto pN = oriNode.child("Pitch");
                    const auto rN = oriNode.child("Roll");
                    if (yN)
                        loc.orientation.yaw = yN.text().as_double(0.0);
                    if (pN)
                        loc.orientation.pitch = pN.text().as_double(0.0);
                    if (rN)
                        loc.orientation.roll = rN.text().as_double(0.0);
                }
            }
        }
        if (m_deviceHandler) {
            m_deviceHandler->handleSetGeoLocation(loc);
        }
        m_config.defaultLocation = loc;
        body << "    <tds:SetGeoLocationResponse/>\r\n";
    } else if (isOp(opName, "DeleteGeoLocation")) {
        const pugi::xml_node entNode = reqNode.select_node(".//*[local-name()='Entity']").node();
        const std::string entToken = entNode ? entNode.text().as_string() : "Device";
        if (m_deviceHandler) {
            m_deviceHandler->handleDeleteGeoLocation(entToken);
        }
        body << "    <tds:DeleteGeoLocationResponse/>\r\n";
    } else if (isOp(opName, "GetVideoSourceModes") || isOp(opName, "SetVideoSourceMode")) {
        processVideoSourceModeRequest(opName, doc, body, "tds");
    } else {
        body << "    <tds:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleMediaService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Media", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    const std::string host = resolveHost(req);
    const int port = m_config.port;

    std::ostringstream body;

    if (isOp(opName, "GetOSDOptions") || isOp(opName, "GetOSDs") || isOp(opName, "GetOSD") || isOp(opName, "CreateOSD")
        || isOp(opName, "SetOSD") || isOp(opName, "DeleteOSD")) {
        processOsdRequest(opName, doc, body, "trt");
    } else if (isOp(opName, "GetMetadataConfigurations") || isOp(opName, "GetMetadataConfiguration")
        || isOp(opName, "SetMetadataConfiguration") || isOp(opName, "GetMetadataConfigurationOptions")
        || isOp(opName, "GetCompatibleMetadataConfigurations")) {
        processMetadataRequest(opName, doc, body, "trt");
    } else if (opName.find("GetProfiles") != std::string::npos) {
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
             << "        <tt:MetadataConfiguration token=\"MetadataConfig_1\">\r\n"
             << "          <tt:Name>DefaultMetadataConfig</tt:Name>\r\n"
             << "          <tt:UseCount>1</tt:UseCount>\r\n"
             << "        </tt:MetadataConfiguration>\r\n"
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

    sendSoapResponse(res, body.str());
}

void OnvifServer::processOsdRequest(
    const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix)
{
    auto serializeOsd = [&](const OsdConfig& osd) {
        std::string posStr = "UpperLeft";
        if (osd.position == OsdPositionType::UpperRight) {
            posStr = "UpperRight";
        } else if (osd.position == OsdPositionType::LowerLeft) {
            posStr = "LowerLeft";
        } else if (osd.position == OsdPositionType::LowerRight) {
            posStr = "LowerRight";
        } else if (osd.position == OsdPositionType::Custom) {
            posStr = "Custom";
        }

        std::ostringstream s;
        s << "      <" << prefix << ":OSD token=\"" << osd.token << "\">\r\n"
          << "        <tt:VideoSourceConfigurationToken>" << osd.videoSourceToken
          << "</tt:VideoSourceConfigurationToken>\r\n"
          << "        <tt:Type>Text</tt:Type>\r\n"
          << "        <tt:Position>\r\n"
          << "          <tt:Type>" << posStr << "</tt:Type>\r\n";
        if (osd.position == OsdPositionType::Custom) {
            s << "          <tt:Pos x=\"" << osd.customX << "\" y=\"" << osd.customY << "\"/>\r\n";
        }
        s << "        </tt:Position>\r\n"
          << "        <tt:TextString>\r\n"
          << "          <tt:Type>" << (osd.isDateAndTime ? "DateAndTime" : "Plain") << "</tt:Type>\r\n";
        if (!osd.isDateAndTime) {
            s << "          <tt:PlainText>" << osd.plainText << "</tt:PlainText>\r\n";
        } else {
            s << "          <tt:DateFormat>" << osd.dateFormat << "</tt:DateFormat>\r\n"
              << "          <tt:TimeFormat>" << osd.timeFormat << "</tt:TimeFormat>\r\n";
        }
        s << "          <tt:FontSize>" << osd.fontSize << "</tt:FontSize>\r\n"
          << "        </tt:TextString>\r\n"
          << "      </" << prefix << ":OSD>\r\n";
        return s.str();
    };

    auto parseOsdConfig = [](const pugi::xml_node& osdNode) -> OsdConfig {
        OsdConfig osd;
        if (!osdNode) {
            return osd;
        }
        osd.token = osdNode.attribute("token").as_string();
        const pugi::xml_node vsn = osdNode.select_node(".//*[local-name()='VideoSourceConfigurationToken']").node();
        if (vsn) {
            osd.videoSourceToken = vsn.text().as_string();
        }

        const pugi::xml_node posTypeNode
            = osdNode.select_node(".//*[local-name()='Position']/*[local-name()='Type']").node();
        const std::string posType = posTypeNode ? posTypeNode.text().as_string() : "UpperLeft";
        if (posType == "UpperRight") {
            osd.position = OsdPositionType::UpperRight;
        } else if (posType == "LowerLeft") {
            osd.position = OsdPositionType::LowerLeft;
        } else if (posType == "LowerRight") {
            osd.position = OsdPositionType::LowerRight;
        } else if (posType == "Custom") {
            osd.position = OsdPositionType::Custom;
            const pugi::xml_node posNode
                = osdNode.select_node(".//*[local-name()='Position']/*[local-name()='Pos']").node();
            if (posNode) {
                const auto pt = Xml::parsePoint2D(posNode);
                osd.customX = pt.x;
                osd.customY = pt.y;
            }
        } else {
            osd.position = OsdPositionType::UpperLeft;
        }

        const pugi::xml_node textTypeNode
            = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='Type']").node();
        const std::string textType = textTypeNode ? textTypeNode.text().as_string() : "Plain";
        osd.isDateAndTime = (textType == "DateAndTime");

        const pugi::xml_node ptNode
            = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='PlainText']").node();
        if (ptNode) {
            osd.plainText = ptNode.text().as_string();
        }

        const pugi::xml_node fsNode
            = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='FontSize']").node();
        if (fsNode) {
            osd.fontSize = fsNode.text().as_uint(24U);
        }

        const pugi::xml_node dfNode
            = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='DateFormat']").node();
        if (dfNode) {
            osd.dateFormat = dfNode.text().as_string();
        }

        const pugi::xml_node tfNode
            = osdNode.select_node(".//*[local-name()='TextString']/*[local-name()='TimeFormat']").node();
        if (tfNode) {
            osd.timeFormat = tfNode.text().as_string();
        }

        return osd;
    };

    if (isOp(opName, "GetOSDOptions")) {
        body << "    <" << prefix << ":GetOSDOptionsResponse>\r\n"
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
    } else if (isOp(opName, "GetOSDs")) {
        const pugi::xml_node vsNode = doc.select_node("//*[local-name()='ConfigurationToken']").node();
        const std::string vsToken = vsNode ? vsNode.text().as_string() : "";

        std::vector<OsdConfig> osds;
        if (m_osdHandler) {
            osds = m_osdHandler->handleGetOSDs(vsToken);
        }
        if (osds.empty()) {
            std::lock_guard<std::mutex> lock(m_osdMutex);
            for (const auto& [tok, osd] : m_internalOsds) {
                if (vsToken.empty() || osd.videoSourceToken == vsToken) {
                    osds.push_back(osd);
                }
            }
        }

        body << "    <" << prefix << ":GetOSDsResponse>\r\n";
        for (const auto& osd : osds) {
            body << serializeOsd(osd);
        }
        body << "    </" << prefix << ":GetOSDsResponse>\r\n";
    } else if (isOp(opName, "GetOSD")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='OSDToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        std::optional<OsdConfig> found;
        if (m_osdHandler) {
            found = m_osdHandler->handleGetOSD(token);
        }
        if (!found) {
            std::lock_guard<std::mutex> lock(m_osdMutex);
            const auto it = m_internalOsds.find(token);
            if (it != m_internalOsds.end()) {
                found = it->second;
            }
        }

        body << "    <" << prefix << ":GetOSDResponse>\r\n";
        if (found) {
            body << serializeOsd(*found);
        }
        body << "    </" << prefix << ":GetOSDResponse>\r\n";
    } else if (isOp(opName, "CreateOSD")) {
        const pugi::xml_node osdNode = doc.select_node("//*[local-name()='OSD']").node();
        OsdConfig osd = parseOsdConfig(osdNode);

        std::string assignedToken;
        if (m_osdHandler) {
            assignedToken = m_osdHandler->handleCreateOSD(osd);
        }
        if (assignedToken.empty()) {
            std::lock_guard<std::mutex> lock(m_osdMutex);
            if (osd.token.empty()) {
                osd.token = "OSD_" + std::to_string(m_nextOsdId++);
            }
            assignedToken = osd.token;
            m_internalOsds[assignedToken] = osd;
        }

        body << "    <" << prefix << ":CreateOSDResponse>\r\n"
             << "      <" << prefix << ":OSDToken>" << assignedToken << "</" << prefix << ":OSDToken>\r\n"
             << "    </" << prefix << ":CreateOSDResponse>\r\n";
    } else if (isOp(opName, "SetOSD")) {
        const pugi::xml_node osdNode = doc.select_node("//*[local-name()='OSD']").node();
        OsdConfig osd = parseOsdConfig(osdNode);

        bool ok = false;
        if (m_osdHandler) {
            ok = m_osdHandler->handleSetOSD(osd);
        }
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_osdMutex);
            if (!osd.token.empty()) {
                m_internalOsds[osd.token] = osd;
            }
        }

        body << "    <" << prefix << ":SetOSDResponse/>\r\n";
    } else if (isOp(opName, "DeleteOSD")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='OSDToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        bool ok = false;
        if (m_osdHandler) {
            ok = m_osdHandler->handleDeleteOSD(token);
        }
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_osdMutex);
            m_internalOsds.erase(token);
        }

        body << "    <" << prefix << ":DeleteOSDResponse/>\r\n";
    }
}

void OnvifServer::processMetadataRequest(
    const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix)
{
    auto serializeMetaConfig = [&](const MetadataConfiguration& cfg) {
        std::ostringstream s;
        s << "      <" << prefix << ":Configurations token=\"" << cfg.token << "\">\r\n"
          << "        <tt:Name>" << cfg.name << "</tt:Name>\r\n"
          << "        <tt:UseCount>1</tt:UseCount>\r\n";
        if (cfg.ptzStatusEnabled) {
            s << "        <tt:PTZStatus>\r\n"
              << "          <tt:Status>true</tt:Status>\r\n"
              << "          <tt:Position>true</tt:Position>\r\n"
              << "        </tt:PTZStatus>\r\n";
        }
        s << "        <tt:Analytics>" << (cfg.analyticsEnabled ? "true" : "false") << "</tt:Analytics>\r\n"
          << "        <tt:Events>" << (cfg.eventsEnabled ? "true" : "false") << "</tt:Events>\r\n"
          << "        <tt:GeoLocation>" << (cfg.geoOrientationEnabled ? "true" : "false") << "</tt:GeoLocation>\r\n"
          << "      </" << prefix << ":Configurations>\r\n";
        return s.str();
    };

    if (isOp(opName, "GetMetadataConfigurations") || isOp(opName, "GetCompatibleMetadataConfigurations")) {
        std::vector<MetadataConfiguration> configs;
        if (m_metadataHandler) {
            configs = m_metadataHandler->handleGetMetadataConfigurations();
        } else {
            std::lock_guard<std::mutex> lock(m_metadataMutex);
            configs = m_internalMetadataConfigs;
        }

        body << "    <" << prefix << ":GetMetadataConfigurationsResponse>\r\n";
        for (const auto& c : configs) {
            body << serializeMetaConfig(c);
        }
        body << "    </" << prefix << ":GetMetadataConfigurationsResponse>\r\n";
    } else if (isOp(opName, "GetMetadataConfiguration")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='ConfigurationToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        MetadataConfiguration found;
        bool hasFound = false;
        if (m_metadataHandler) {
            const auto list = m_metadataHandler->handleGetMetadataConfigurations();
            const auto it = std::find_if(list.begin(), list.end(), [&](const auto& c) { return c.token == token; });
            if (it != list.end()) {
                found = *it;
                hasFound = true;
            }
        }
        if (!hasFound) {
            std::lock_guard<std::mutex> lock(m_metadataMutex);
            const auto it = std::find_if(m_internalMetadataConfigs.begin(), m_internalMetadataConfigs.end(),
                [&](const auto& c) { return c.token == token; });
            if (it != m_internalMetadataConfigs.end()) {
                found = *it;
                hasFound = true;
            }
        }

        body << "    <" << prefix << ":GetMetadataConfigurationResponse>\r\n"
             << "      <" << prefix << ":Configuration token=\"" << found.token << "\">\r\n"
             << "        <tt:Name>" << found.name << "</tt:Name>\r\n"
             << "        <tt:UseCount>1</tt:UseCount>\r\n";
        if (found.ptzStatusEnabled) {
            body << "        <tt:PTZStatus>\r\n"
                 << "          <tt:Status>true</tt:Status>\r\n"
                 << "          <tt:Position>true</tt:Position>\r\n"
                 << "        </tt:PTZStatus>\r\n";
        }
        body << "        <tt:Analytics>" << (found.analyticsEnabled ? "true" : "false") << "</tt:Analytics>\r\n"
             << "        <tt:Events>" << (found.eventsEnabled ? "true" : "false") << "</tt:Events>\r\n"
             << "        <tt:GeoLocation>" << (found.geoOrientationEnabled ? "true" : "false")
             << "</tt:GeoLocation>\r\n"
             << "      </" << prefix << ":Configuration>\r\n"
             << "    </" << prefix << ":GetMetadataConfigurationResponse>\r\n";
    } else if (isOp(opName, "SetMetadataConfiguration")) {
        const pugi::xml_node cfgNode = doc.select_node("//*[local-name()='Configuration']").node();
        MetadataConfiguration cfg;
        if (cfgNode) {
            cfg.token = cfgNode.attribute("token").as_string();
            const pugi::xml_node nameNode = cfgNode.select_node(".//*[local-name()='Name']").node();
            if (nameNode) {
                cfg.name = nameNode.text().as_string();
            }
            const pugi::xml_node ptzNode = cfgNode.select_node(".//*[local-name()='PTZStatus']").node();
            cfg.ptzStatusEnabled = (ptzNode != nullptr);
            const pugi::xml_node anaNode = cfgNode.select_node(".//*[local-name()='Analytics']").node();
            if (anaNode) {
                cfg.analyticsEnabled = anaNode.text().as_bool(true);
            }
            const pugi::xml_node evNode = cfgNode.select_node(".//*[local-name()='Events']").node();
            if (evNode) {
                cfg.eventsEnabled = evNode.text().as_bool(true);
            }
            const pugi::xml_node geoNode = cfgNode.select_node(".//*[local-name()='GeoLocation']").node();
            if (geoNode) {
                cfg.geoOrientationEnabled = geoNode.text().as_bool(true);
            }
        }

        bool ok = false;
        if (m_metadataHandler) {
            ok = m_metadataHandler->handleSetMetadataConfiguration(cfg);
        }
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_metadataMutex);
            const auto it = std::find_if(m_internalMetadataConfigs.begin(), m_internalMetadataConfigs.end(),
                [&](const auto& c) { return c.token == cfg.token; });
            if (it != m_internalMetadataConfigs.end()) {
                *it = cfg;
            } else {
                m_internalMetadataConfigs.push_back(cfg);
            }
        }
        body << "    <" << prefix << ":SetMetadataConfigurationResponse/>\r\n";
    } else if (isOp(opName, "GetMetadataConfigurationOptions")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='ConfigurationToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        MetadataConfigurationOptions opts;
        if (m_metadataHandler) {
            opts = m_metadataHandler->handleGetMetadataConfigurationOptions(token, "");
        } else {
            opts.ptzStatusSupported = true;
            opts.analyticsSupported = true;
            opts.eventsSupported = true;
        }

        body << "    <" << prefix << ":GetMetadataConfigurationOptionsResponse>\r\n"
             << "      <" << prefix << ":Options>\r\n"
             << "        <tt:PTZStatusFilterOptions>\r\n"
             << "          <tt:PanTiltStatusSupported>" << (opts.ptzStatusSupported ? "true" : "false")
             << "</tt:PanTiltStatusSupported>\r\n"
             << "          <tt:ZoomStatusSupported>" << (opts.ptzStatusSupported ? "true" : "false")
             << "</tt:ZoomStatusSupported>\r\n"
             << "          <tt:PanTiltPositionSupported>" << (opts.ptzStatusSupported ? "true" : "false")
             << "</tt:PanTiltPositionSupported>\r\n"
             << "          <tt:ZoomPositionSupported>" << (opts.ptzStatusSupported ? "true" : "false")
             << "</tt:ZoomPositionSupported>\r\n"
             << "        </tt:PTZStatusFilterOptions>\r\n"
             << "      </" << prefix << ":Options>\r\n"
             << "    </" << prefix << ":GetMetadataConfigurationOptionsResponse>\r\n";
    }
}

void OnvifServer::processMaskRequest(
    const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix)
{
    auto serializeMask = [&](const PrivacyMask& m) {
        std::ostringstream s;
        s << "      <" << prefix << ":Mask token=\"" << m.token << "\">\r\n"
          << "        <" << prefix << ":ConfigurationToken>" << m.configurationToken << "</" << prefix
          << ":ConfigurationToken>\r\n"
          << "        <" << prefix << ":Polygon>\r\n";
        for (const auto& pt : m.polygon) {
            s << "          <tt:Point x=\"" << std::fixed << std::setprecision(4) << pt.x << "\" y=\"" << pt.y
              << "\"/>\r\n";
        }
        s << "        </" << prefix << ":Polygon>\r\n"
          << "        <" << prefix << ":Type>" << maskTypeToString(m.type) << "</" << prefix << ":Type>\r\n"
          << "        <" << prefix << ":Color X=\"" << m.color.x << "\" Y=\"" << m.color.y << "\" Z=\"" << m.color.z
          << "\" Colorspace=\"" << m.color.colorspace << "\"/>\r\n"
          << "        <" << prefix << ":Enabled>" << (m.enabled ? "true" : "false") << "</" << prefix << ":Enabled>\r\n"
          << "      </" << prefix << ":Mask>\r\n";
        return s.str();
    };

    if (isOp(opName, "GetMaskOptions")) {
        const pugi::xml_node cfgNode = doc.select_node("//*[local-name()='ConfigurationToken']").node();
        const std::string cfgToken = cfgNode ? cfgNode.text().as_string() : "";

        MaskOptions opts;
        if (m_maskHandler) {
            opts = m_maskHandler->handleGetMaskOptions(cfgToken);
        } else {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            opts = m_internalMaskOptions;
        }

        body << "    <" << prefix << ":GetMaskOptionsResponse>\r\n"
             << "      <" << prefix << ":Options MaxMasks=\"" << opts.maxMasks << "\" MaxPoints=\"" << opts.maxPoints
             << "\" Rectangle=\"" << (opts.rectangleSupported ? "true" : "false") << "\" RectangleSupported=\""
             << (opts.rectangleSupported ? "true" : "false") << "\" Polygon=\""
             << (opts.polygonSupported ? "true" : "false") << "\" PolygonSupported=\""
             << (opts.polygonSupported ? "true" : "false") << "\">\r\n"
             << "        <" << prefix << ":MaxMasks>" << opts.maxMasks << "</" << prefix << ":MaxMasks>\r\n"
             << "        <" << prefix << ":MaxPoints>" << opts.maxPoints << "</" << prefix << ":MaxPoints>\r\n";
        for (const auto& t : opts.supportedTypes) {
            body << "        <" << prefix << ":Types>" << maskTypeToString(t) << "</" << prefix << ":Types>\r\n";
            body << "        <" << prefix << ":SupportedTypes>" << maskTypeToString(t) << "</" << prefix
                 << ":SupportedTypes>\r\n";
        }
        for (const auto& cs : opts.supportedColorSpaces) {
            body << "        <" << prefix << ":SupportedColorSpaces>" << cs << "</" << prefix
                 << ":SupportedColorSpaces>\r\n";
        }
        body << "      </" << prefix << ":Options>\r\n"
             << "    </" << prefix << ":GetMaskOptionsResponse>\r\n";

    } else if (isOp(opName, "GetMasks")) {
        const pugi::xml_node cfgNode = doc.select_node("//*[local-name()='ConfigurationToken']").node();
        const std::string cfgToken = cfgNode ? cfgNode.text().as_string() : "";

        std::vector<PrivacyMask> masks;
        if (m_maskHandler) {
            masks = m_maskHandler->handleGetMasks(cfgToken);
        } else {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            if (cfgToken.empty()) {
                masks = m_internalMasks;
            } else {
                for (const auto& m : m_internalMasks) {
                    if (m.configurationToken == cfgToken) {
                        masks.push_back(m);
                    }
                }
            }
        }

        body << "    <" << prefix << ":GetMasksResponse>\r\n";
        for (const auto& m : masks) {
            body << serializeMask(m);
        }
        body << "    </" << prefix << ":GetMasksResponse>\r\n";

    } else if (isOp(opName, "GetMask")) {
        const pugi::xml_node tokNode = doc.select_node("//*[local-name()='Token']").node();
        const std::string token = tokNode ? tokNode.text().as_string() : "";

        std::optional<PrivacyMask> opt;
        if (m_maskHandler) {
            opt = m_maskHandler->handleGetMask(token);
        } else {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            const auto it = std::find_if(m_internalMasks.begin(), m_internalMasks.end(),
                [&token](const PrivacyMask& m) { return m.token == token; });
            if (it != m_internalMasks.end()) {
                opt = *it;
            }
        }

        body << "    <" << prefix << ":GetMaskResponse>\r\n";
        if (opt.has_value()) {
            body << serializeMask(*opt);
        }
        body << "    </" << prefix << ":GetMaskResponse>\r\n";

    } else if (isOp(opName, "CreateMask")) {
        const pugi::xml_node maskNode = doc.select_node("//*[local-name()='Mask']").node();
        PrivacyMask mask = parsePrivacyMask(maskNode);

        std::string assignedToken = mask.token;
        if (assignedToken.empty()) {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            assignedToken = "Mask_" + std::to_string(m_nextMaskId++);
            mask.token = assignedToken;
        }

        if (m_maskHandler) {
            assignedToken = m_maskHandler->handleCreateMask(mask);
            if (assignedToken.empty()) {
                assignedToken = mask.token;
            }
        } else {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            m_internalMasks.push_back(mask);
        }

        body << "    <" << prefix << ":CreateMaskResponse>\r\n"
             << "      <" << prefix << ":Token>" << assignedToken << "</" << prefix << ":Token>\r\n"
             << "    </" << prefix << ":CreateMaskResponse>\r\n";

    } else if (isOp(opName, "SetMask")) {
        const pugi::xml_node maskNode = doc.select_node("//*[local-name()='Mask']").node();
        PrivacyMask mask = parsePrivacyMask(maskNode);

        bool ok = false;
        if (m_maskHandler) {
            ok = m_maskHandler->handleSetMask(mask);
        }
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            const auto it = std::find_if(m_internalMasks.begin(), m_internalMasks.end(),
                [&mask](const PrivacyMask& m) { return m.token == mask.token; });
            if (it != m_internalMasks.end()) {
                *it = mask;
            } else {
                m_internalMasks.push_back(mask);
            }
        }

        body << "    <" << prefix << ":SetMaskResponse/>\r\n";

    } else if (isOp(opName, "DeleteMask")) {
        const pugi::xml_node tokNode = doc.select_node("//*[local-name()='Token']").node();
        const std::string token = tokNode ? tokNode.text().as_string() : "";

        bool ok = false;
        if (m_maskHandler) {
            ok = m_maskHandler->handleDeleteMask(token);
        }
        if (!ok) {
            std::lock_guard<std::mutex> lock(m_maskMutex);
            m_internalMasks.erase(std::remove_if(m_internalMasks.begin(), m_internalMasks.end(),
                                      [&token](const PrivacyMask& m) { return m.token == token; }),
                m_internalMasks.end());
        }

        body << "    <" << prefix << ":DeleteMaskResponse/>\r\n";
    }
}

void OnvifServer::processVideoSourceModeRequest(
    const std::string& opName, const pugi::xml_document& doc, std::ostringstream& body, const std::string& prefix)
{
    if (isOp(opName, "GetVideoSourceModes")) {
        const pugi::xml_node vsNode = doc.select_node("//*[local-name()='VideoSourceToken']").node();
        const std::string vsToken = vsNode ? vsNode.text().as_string() : "VideoSource_1";

        std::vector<VideoSourceMode> modes;
        if (m_videoSourceModeHandler) {
            modes = m_videoSourceModeHandler->handleGetVideoSourceModes(vsToken);
        } else {
            std::lock_guard<std::mutex> lock(m_videoSourceModeMutex);
            modes = m_internalVideoSourceModes;
        }

        body << "    <" << prefix << ":GetVideoSourceModesResponse>\r\n";
        for (const auto& mode : modes) {
            body << "      <" << prefix << ":VideoSourceModes token=\"" << mode.token << "\" Enabled=\""
                 << (mode.enabled ? "true" : "false") << "\">\r\n"
                 << "        <" << prefix << ":MaxFramerate>" << std::fixed << std::setprecision(1) << mode.maxFramerate
                 << "</" << prefix << ":MaxFramerate>\r\n"
                 << "        <" << prefix << ":MaxResolution>\r\n"
                 << "          <tt:Width>" << mode.width << "</tt:Width>\r\n"
                 << "          <tt:Height>" << mode.height << "</tt:Height>\r\n"
                 << "        </" << prefix << ":MaxResolution>\r\n"
                 << "        <" << prefix << ":Encodings>";
            for (size_t i = 0; i < mode.encodings.size(); ++i) {
                if (i > 0)
                    body << " ";
                body << mode.encodings[i];
            }
            body << "</" << prefix << ":Encodings>\r\n"
                 << "        <" << prefix << ":Reboot>" << (mode.reboot ? "true" : "false") << "</" << prefix
                 << ":Reboot>\r\n"
                 << "        <" << prefix << ":Description>" << mode.description << "</" << prefix
                 << ":Description>\r\n"
                 << "      </" << prefix << ":VideoSourceModes>\r\n";
        }
        body << "    </" << prefix << ":GetVideoSourceModesResponse>\r\n";

    } else if (isOp(opName, "SetVideoSourceMode")) {
        const pugi::xml_node vsNode = doc.select_node("//*[local-name()='VideoSourceToken']").node();
        const std::string vsToken = vsNode ? vsNode.text().as_string() : "VideoSource_1";

        const pugi::xml_node modeNode
            = doc.select_node("//*[local-name()='VideoSourceModeToken' or local-name()='ModeToken']").node();
        const std::string modeToken = modeNode ? modeNode.text().as_string() : "";

        bool rebootRequired = false;
        if (m_videoSourceModeHandler) {
            static_cast<void>(m_videoSourceModeHandler->handleSetVideoSourceMode(vsToken, modeToken, rebootRequired));
        } else {
            std::lock_guard<std::mutex> lock(m_videoSourceModeMutex);
            for (auto& m : m_internalVideoSourceModes) {
                if (m.token == modeToken) {
                    m.enabled = true;
                    rebootRequired = m.reboot;
                } else {
                    m.enabled = false;
                }
            }
        }

        body << "    <" << prefix << ":SetVideoSourceModeResponse>\r\n"
             << "      <" << prefix << ":Reboot>" << (rebootRequired ? "true" : "false") << "</" << prefix
             << ":Reboot>\r\n"
             << "    </" << prefix << ":SetVideoSourceModeResponse>\r\n";
    }
}

std::string OnvifServer::generateMetadataStreamXml(const MetadataStreamPayload& payload) const
{
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
       << "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
       << "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n";

    if (payload.ptzStatus.has_value()) {
        const auto& ptz = payload.ptzStatus.value();
        ss << "  <tt:PTZStatus>\r\n"
           << "    <tt:Position>\r\n"
           << "      <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << ptz.pan << "\" "
           << "y=\"" << std::fixed << std::setprecision(4) << ptz.tilt << "\" "
           << "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\"/>\r\n"
           << "      <tt:Zoom x=\"" << std::fixed << std::setprecision(4) << ptz.zoom << "\" "
           << "space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace\"/>\r\n"
           << "    </tt:Position>\r\n"
           << "    <tt:MoveStatus>\r\n"
           << "      <tt:PanTilt>" << (ptz.isMoving ? "MOVING" : "IDLE") << "</tt:PanTilt>\r\n"
           << "      <tt:Zoom>" << (ptz.isMoving ? "MOVING" : "IDLE") << "</tt:Zoom>\r\n"
           << "    </tt:MoveStatus>\r\n";
        if (!ptz.utcTime.empty()) {
            ss << "    <tt:UtcTime>" << ptz.utcTime << "</tt:UtcTime>\r\n";
        }
        ss << "  </tt:PTZStatus>\r\n";
    }

    if (payload.analyticsFrame.has_value() && !payload.analyticsFrame->objects.empty()) {
        const auto& frame = payload.analyticsFrame.value();
        const std::string frameTime
            = frame.utcTime.empty() ? formatIso8601Utc(std::chrono::system_clock::now()) : frame.utcTime;
        ss << "  <tt:VideoAnalytics>\r\n"
           << "    <tt:Frame UtcTime=\"" << frameTime << "\">\r\n"
           << "      <tt:Transformation>\r\n"
           << "        <tt:Translate x=\"0.00\" y=\"0.00\"/>\r\n"
           << "        <tt:Scale x=\"1.00\" y=\"1.00\"/>\r\n"
           << "      </tt:Transformation>\r\n";

        for (const auto& obj : frame.objects) {
            ss << "      <tt:Object ObjectId=\"" << obj.objectId << "\">\r\n"
               << "        <tt:Appearance>\r\n"
               << "          <tt:Shape>\r\n"
               << "            <tt:BoundingBox left=\"" << std::fixed << std::setprecision(4) << obj.boundingBox.left
               << "\" top=\"" << std::fixed << std::setprecision(4) << obj.boundingBox.top << "\" right=\""
               << std::fixed << std::setprecision(4) << obj.boundingBox.right << "\" bottom=\"" << std::fixed
               << std::setprecision(4) << obj.boundingBox.bottom << "\"/>\r\n"
               << "          </tt:Shape>\r\n"
               << "          <tt:Class>\r\n"
               << "            <tt:ClassCandidate>\r\n"
               << "              <tt:Type>" << obj.className << "</tt:Type>\r\n"
               << "              <tt:Likelihood>" << std::fixed << std::setprecision(2) << obj.confidence
               << "</tt:Likelihood>\r\n"
               << "            </tt:ClassCandidate>\r\n"
               << "          </tt:Class>\r\n";
            if (std::abs(obj.geoLocation.latitude) > 1e-7 || std::abs(obj.geoLocation.longitude) > 1e-7) {
                ss << "          <tt:GeoLocation lat=\"" << std::fixed << std::setprecision(6)
                   << obj.geoLocation.latitude << "\" lon=\"" << std::fixed << std::setprecision(6)
                   << obj.geoLocation.longitude << "\" elevation=\"" << std::fixed << std::setprecision(2)
                   << obj.geoLocation.elevation << "\"/>\r\n";
            }
            ss << "        </tt:Appearance>\r\n"
               << "      </tt:Object>\r\n";
        }
        ss << "    </tt:Frame>\r\n"
           << "  </tt:VideoAnalytics>\r\n";
    }

    for (const auto& ev : payload.events) {
        ss << "  <wsnt:NotificationMessage>\r\n"
           << "    <wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">" << ev.topic
           << "</wsnt:Topic>\r\n"
           << "    <wsnt:Message>\r\n"
           << "      <tt:Message UtcTime=\"" << ev.utcTime << "\" PropertyOperation=\"Initialized\">\r\n";
        if (!ev.sourceName.empty()) {
            ss << "        <tt:Source>\r\n"
               << "          <tt:SimpleItem Name=\"" << ev.sourceName << "\" Value=\"" << ev.sourceValue << "\"/>\r\n"
               << "        </tt:Source>\r\n";
        }
        if (!ev.dataName.empty()) {
            ss << "        <tt:Data>\r\n"
               << "          <tt:SimpleItem Name=\"" << ev.dataName << "\" Value=\"" << ev.dataValue << "\"/>\r\n"
               << "        </tt:Data>\r\n";
        }
        ss << "      </tt:Message>\r\n"
           << "    </wsnt:Message>\r\n"
           << "  </wsnt:NotificationMessage>\r\n";
    }

    ss << "</tt:MetadataStream>\r\n";
    return ss.str();
}

void OnvifServer::handleMetadataStream(const httplib::Request& req, httplib::Response& res)
{
    if (m_logCallback) {
        m_logCallback("MetadataStream", "GetStream", req.remote_addr);
    }
    MetadataStreamPayload payload;
    if (m_metadataHandler) {
        payload = m_metadataHandler->handleGetCurrentMetadata("");
    } else if (m_ptzHandler) {
        payload.ptzStatus = m_ptzHandler->handleGetStatus();
    }
    const std::string xml = generateMetadataStreamXml(payload);
    res.status = 200;
    res.set_content(xml, "application/xml; charset=utf-8");
}

void OnvifServer::handleMedia2Service(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Media2", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    const std::string host = resolveHost(req);
    const int port = m_config.port;

    std::ostringstream body;

    if (isOp(opName, "GetOSDOptions") || isOp(opName, "GetOSDs") || isOp(opName, "GetOSD") || isOp(opName, "CreateOSD")
        || isOp(opName, "SetOSD") || isOp(opName, "DeleteOSD")) {
        processOsdRequest(opName, doc, body, "tr2");
    } else if (isOp(opName, "GetMetadataConfigurations") || isOp(opName, "GetMetadataConfiguration")
        || isOp(opName, "SetMetadataConfiguration") || isOp(opName, "GetMetadataConfigurationOptions")
        || isOp(opName, "GetCompatibleMetadataConfigurations")) {
        processMetadataRequest(opName, doc, body, "tr2");
    } else if (isOp(opName, "GetProfiles")) {
        body << "    <tr2:GetProfilesResponse>\r\n"
             << "      <tr2:Profiles token=\"ProfileToken_1\" fixed=\"true\">\r\n"
             << "        <tr2:Name>MainProfile2</tr2:Name>\r\n"
             << "        <tr2:Configurations>\r\n"
             << "          <tr2:VideoSource token=\"VideoSourceConfig_1\">\r\n"
             << "            <tr2:Name>VideoSourceConfig</tr2:Name>\r\n"
             << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
             << "            <tr2:SourceToken>VideoSource_1</tr2:SourceToken>\r\n"
             << "            <tr2:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/>\r\n"
             << "          </tr2:VideoSource>\r\n"
             << "          <tr2:VideoEncoder token=\"VideoEncoderConfig_1\">\r\n"
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
             << "          </tr2:VideoEncoder>\r\n"
             << "          <tr2:PTZ token=\"PTZConfig_1\">\r\n"
             << "            <tr2:Name>PTZConfig</tr2:Name>\r\n"
             << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
             << "            <tr2:NodeToken>PTZNode_1</tr2:NodeToken>\r\n"
             << "          </tr2:PTZ>\r\n"
             << "          <tr2:Metadata token=\"MetadataConfig_1\">\r\n"
             << "            <tr2:Name>DefaultMetadataConfig</tr2:Name>\r\n"
             << "            <tr2:UseCount>1</tr2:UseCount>\r\n"
             << "          </tr2:Metadata>\r\n"
             << "        </tr2:Configurations>\r\n"
             << "      </tr2:Profiles>\r\n"
             << "    </tr2:GetProfilesResponse>\r\n";
    } else if (isOp(opName, "GetStreamUri")) {
        body << "    <tr2:GetStreamUriResponse>\r\n"
             << "      <tr2:Uri>" << m_config.rtspStreamUri << "</tr2:Uri>\r\n"
             << "    </tr2:GetStreamUriResponse>\r\n";
    } else if (isOp(opName, "GetSnapshotUri")) {
        body << "    <tr2:GetSnapshotUriResponse>\r\n"
             << "      <tr2:Uri>http://" << host << ":" << port << "/onvif/snapshot</tr2:Uri>\r\n"
             << "    </tr2:GetSnapshotUriResponse>\r\n";
    } else if (isOp(opName, "GetVideoEncoderConfigurations")) {
        body << "    <tr2:GetVideoEncoderConfigurationsResponse>\r\n"
             << "      <tr2:Configurations token=\"VideoEncoderConfig_1\">\r\n"
             << "        <tr2:Name>VideoEncoderConfig</tr2:Name>\r\n"
             << "        <tr2:UseCount>1</tr2:UseCount>\r\n"
             << "        <tr2:Encoding>H264</tr2:Encoding>\r\n"
             << "        <tr2:Resolution>\r\n"
             << "          <tr2:Width>1920</tr2:Width>\r\n"
             << "          <tr2:Height>1080</tr2:Height>\r\n"
             << "        </tr2:Resolution>\r\n"
             << "        <tr2:RateControl>\r\n"
             << "          <tr2:FrameRateLimit>30</tr2:FrameRateLimit>\r\n"
             << "          <tr2:BitrateLimit>4096</tr2:BitrateLimit>\r\n"
             << "        </tr2:RateControl>\r\n"
             << "        <tr2:GovLength>30</tr2:GovLength>\r\n"
             << "      </tr2:Configurations>\r\n"
             << "    </tr2:GetVideoEncoderConfigurationsResponse>\r\n";
    } else if (isOp(opName, "GetVideoEncoderConfigurationOptions")) {
        body << "    <tr2:GetVideoEncoderConfigurationOptionsResponse>\r\n"
             << "      <tr2:Options>\r\n"
             << "        <tr2:GovLengthRange Min=\"1\" Max=\"120\"/>\r\n"
             << "        <tr2:FrameRatesSupported>30</tr2:FrameRatesSupported>\r\n"
             << "        <tr2:ResolutionsAvailable>\r\n"
             << "          <tr2:Width>1920</tr2:Width>\r\n"
             << "          <tr2:Height>1080</tr2:Height>\r\n"
             << "        </tr2:ResolutionsAvailable>\r\n"
             << "      </tr2:Options>\r\n"
             << "    </tr2:GetVideoEncoderConfigurationOptionsResponse>\r\n";
    } else if (isOp(opName, "GetMaskOptions") || isOp(opName, "GetMasks") || isOp(opName, "GetMask")
        || isOp(opName, "CreateMask") || isOp(opName, "SetMask") || isOp(opName, "DeleteMask")) {
        processMaskRequest(opName, doc, body, "tr2");
    } else if (isOp(opName, "GetVideoSourceModes") || isOp(opName, "SetVideoSourceMode")) {
        processVideoSourceModeRequest(opName, doc, body, "tr2");
    } else if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <tr2:GetServiceCapabilitiesResponse>\r\n"
             << "      <tr2:Capabilities SnapshotUri=\"true\" Rotation=\"false\" OSD=\"true\" Mask=\"true\" "
                "SourceConfigurations=\"true\"/>\r\n"
             << "    </tr2:GetServiceCapabilitiesResponse>\r\n";
    } else {
        body << "    <tr2:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handlePtzService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "PTZ", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

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
        std::string spaceUri {};

        const pugi::xml_node ptNode = doc.select_node("//*[local-name()='PanTilt']").node();
        if (ptNode) {
            pan = ptNode.attribute("x").as_float(-1.0f);
            tilt = ptNode.attribute("y").as_float(-1.0f);
            spaceUri = ptNode.attribute("space").as_string();
        }

        const pugi::xml_node zNode = doc.select_node("//*[local-name()='Zoom']").node();
        if (zNode) {
            zoom = zNode.attribute("x").as_float(-1.0f);
        }

        if (m_ptzHandler) {
            if (spaceUri.find("Spherical") != std::string::npos) {
                m_ptzHandler->handleAbsoluteMoveSpherical(pan, tilt, zoom);
            } else {
                m_ptzHandler->handleAbsoluteMove(pan, tilt, zoom);
            }
        }

        body << "    <tptz:AbsoluteMoveResponse/>\r\n";
    } else if (opName.find("GeoMove") != std::string::npos) {
        const pugi::xml_node profNode = doc.select_node("//*[local-name()='ProfileToken']").node();
        const std::string profToken = profNode ? profNode.text().as_string() : "Profile_1";

        GeoMoveTarget target {};
        pugi::xml_node targetNode = doc.select_node("//*[local-name()='Target']").node();
        pugi::xml_node geoNode
            = doc.select_node(
                     "//*[local-name()='Target']/*[local-name()='GeoLocation'] | //*[local-name()='GeoLocation']")
                  .node();
        if (!geoNode && targetNode) {
            geoNode = targetNode;
        }
        if (geoNode) {
            if (geoNode.attribute("lat")) {
                target.targetGeo.latitude = geoNode.attribute("lat").as_double(0.0);
                target.targetGeo.longitude = geoNode.attribute("lon").as_double(0.0);
                target.targetGeo.elevation = geoNode.attribute("elevation").as_double(0.0);
            } else {
                const auto latNode = geoNode.select_node(".//*[local-name()='Latitude' or local-name()='lat']").node();
                const auto lonNode = geoNode.select_node(".//*[local-name()='Longitude' or local-name()='lon']").node();
                const auto elevNode
                    = geoNode.select_node(".//*[local-name()='Elevation' or local-name()='elevation']").node();
                if (latNode)
                    target.targetGeo.latitude = latNode.text().as_double(0.0);
                if (lonNode)
                    target.targetGeo.longitude = lonNode.text().as_double(0.0);
                if (elevNode)
                    target.targetGeo.elevation = elevNode.text().as_double(0.0);
            }
        }

        const pugi::xml_node spNode = doc.select_node("//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
        if (spNode) {
            target.speed = spNode.attribute("x").as_float(1.0f);
        }

        const pugi::xml_node wNode = doc.select_node("//*[local-name()='AreaWidth']").node();
        if (wNode) {
            target.areaWidth = wNode.text().as_float(0.0f);
        }

        const pugi::xml_node hNode = doc.select_node("//*[local-name()='AreaHeight']").node();
        if (hNode) {
            target.areaHeight = hNode.text().as_float(0.0f);
        }

        if (m_ptzHandler) {
            m_ptzHandler->handleGeoMove(profToken, target);
        }

        body << "    <tptz:GeoMoveResponse/>\r\n";
    } else if (opName.find("RelativeMove") != std::string::npos) {
        float pan = 0.0f;
        float tilt = 0.0f;
        float zoom = 0.0f;
        float speed = 1.0f;

        const pugi::xml_node ptNode
            = doc.select_node("//*[local-name()='Translation']/*[local-name()='PanTilt']").node();
        if (ptNode) {
            pan = ptNode.attribute("x").as_float(0.0f);
            tilt = ptNode.attribute("y").as_float(0.0f);
        }

        const pugi::xml_node zNode = doc.select_node("//*[local-name()='Translation']/*[local-name()='Zoom']").node();
        if (zNode) {
            zoom = zNode.attribute("x").as_float(0.0f);
        }

        const pugi::xml_node spNode = doc.select_node("//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
        if (spNode) {
            speed = spNode.attribute("x").as_float(1.0f);
        }

        if (m_ptzHandler) {
            m_ptzHandler->handleRelativeMove(pan, tilt, zoom, speed);
        }

        body << "    <tptz:RelativeMoveResponse/>\r\n";
    } else if (opName.find("GotoHomePosition") != std::string::npos) {
        float speed = 1.0f;
        const pugi::xml_node spNode = doc.select_node("//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
        if (spNode) {
            speed = spNode.attribute("x").as_float(1.0f);
        }
        if (m_ptzHandler) {
            m_ptzHandler->handleGotoHomePosition(speed);
        }
        body << "    <tptz:GotoHomePositionResponse/>\r\n";
    } else if (opName.find("SetHomePosition") != std::string::npos) {
        if (m_ptzHandler) {
            m_ptzHandler->handleSetHomePosition();
        }
        body << "    <tptz:SetHomePositionResponse/>\r\n";
    } else if (opName.find("SendAuxiliaryCommand") != std::string::npos) {
        const pugi::xml_node auxNode = doc.select_node("//*[local-name()='AuxiliaryData']").node();
        const std::string auxData = auxNode ? auxNode.text().as_string() : "";
        std::string auxResp = auxData;
        if (m_ptzHandler) {
            auxResp = m_ptzHandler->handleSendAuxiliaryCommand(auxData);
        }
        body << "    <tptz:SendAuxiliaryCommandResponse>\r\n"
             << "      <tptz:AuxiliaryResponse>" << auxResp << "</tptz:AuxiliaryResponse>\r\n"
             << "    </tptz:SendAuxiliaryCommandResponse>\r\n";
    } else if (opName.find("GetConfigurationOptions") != std::string::npos
        || opName.find("GetConfigurationOption") != std::string::npos) {
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
             << "            <tt:XRange><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:XRange>\r\n"
             << "          </tt:ContinuousZoomVelocitySpace>\r\n"
             << "        </tt:Spaces>\r\n"
             << "        <tt:PTZTimeout>\r\n"
             << "          <tt:Min>PT0S</tt:Min>\r\n"
             << "          <tt:Max>PT300S</tt:Max>\r\n"
             << "        </tt:PTZTimeout>\r\n"
             << "      </tptz:PTZConfigurationOptions>\r\n"
             << "    </tptz:GetConfigurationOptionsResponse>\r\n";
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
                appendPresetTourXml(body, t);
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
                appendPresetTourXml(body, *tourOpt);
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

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleImagingService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Imaging", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

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
    } else if (isOp(opName, "GetStatus")) {
        FocusStatus20 status;
        if (m_imagingHandler) {
            status = m_imagingHandler->handleGetFocusStatus(videoSourceToken);
        } else {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            status = m_internalFocusStatus;
        }

        body << "    <timg:GetStatusResponse>\r\n"
             << "      <timg:Status>\r\n"
             << "        <tt:FocusStatus20>\r\n"
             << "          <tt:Position>" << std::fixed << std::setprecision(2) << status.position
             << "</tt:Position>\r\n"
             << "          <tt:MoveStatus>" << status.moveStatus << "</tt:MoveStatus>\r\n"
             << "        </tt:FocusStatus20>\r\n"
             << "      </timg:Status>\r\n"
             << "    </timg:GetStatusResponse>\r\n";
    } else if (isOp(opName, "Move")) {
        FocusMove move;
        const pugi::xml_node absNode = doc.select_node("//*[local-name()='Absolute']").node();
        const pugi::xml_node relNode = doc.select_node("//*[local-name()='Relative']").node();

        if (absNode) {
            move.mode = FocusMoveMode::Absolute;
            const pugi::xml_node posNode = absNode.select_node(".//*[local-name()='Position']").node();
            move.absolutePosition = posNode ? posNode.text().as_float(0.0f) : absNode.text().as_float(0.0f);
            const pugi::xml_node spdNode = absNode.select_node(".//*[local-name()='Speed']").node();
            if (spdNode) {
                move.relativeSpeed = spdNode.text().as_float(1.0f);
            }
        } else if (relNode) {
            move.mode = FocusMoveMode::Relative;
            const pugi::xml_node distNode = relNode.select_node(".//*[local-name()='Distance']").node();
            move.relativeDistance = distNode ? distNode.text().as_float(0.0f) : relNode.text().as_float(0.0f);
            const pugi::xml_node spdNode = relNode.select_node(".//*[local-name()='Speed']").node();
            if (spdNode) {
                move.relativeSpeed = spdNode.text().as_float(1.0f);
            }
        } else {
            move.mode = FocusMoveMode::Continuous;
            const pugi::xml_node spdNode = doc.select_node("//*[local-name()='Speed']").node();
            move.continuousSpeed = spdNode ? spdNode.text().as_float(0.0f) : 0.0f;
        }

        if (m_imagingHandler) {
            m_imagingHandler->handleMoveFocusAdvanced(videoSourceToken, move);
        } else {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            m_internalFocusStatus.moveStatus = "MOVING";
            if (move.mode == FocusMoveMode::Absolute) {
                m_internalFocusStatus.position = std::clamp(move.absolutePosition, 0.0f, 1.0f);
            } else if (move.mode == FocusMoveMode::Relative) {
                m_internalFocusStatus.position
                    = std::clamp(m_internalFocusStatus.position + move.relativeDistance, 0.0f, 1.0f);
            }
        }

        body << "    <timg:MoveResponse/>\r\n";
    } else if (isOp(opName, "Stop")) {
        if (m_imagingHandler) {
            m_imagingHandler->handleStopFocus(videoSourceToken);
        } else {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            m_internalFocusStatus.moveStatus = "IDLE";
        }

        body << "    <timg:StopResponse/>\r\n";
    } else if (isOp(opName, "GetOptions")) {
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
    } else if (isOp(opName, "GetMoveOptions")) {
        body << "    <timg:GetMoveOptionsResponse>\r\n"
             << "      <timg:MoveOptions>\r\n"
             << "        <tt:Absolute>\r\n"
             << "          <tt:Position><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Position>\r\n"
             << "          <tt:Speed><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed>\r\n"
             << "        </tt:Absolute>\r\n"
             << "        <tt:Relative>\r\n"
             << "          <tt:Distance><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:Distance>\r\n"
             << "          <tt:Speed><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed>\r\n"
             << "        </tt:Relative>\r\n"
             << "        <tt:Continuous>\r\n"
             << "          <tt:Speed><tt:Min>-1.0</tt:Min><tt:Max>1.0</tt:Max></tt:Speed>\r\n"
             << "        </tt:Continuous>\r\n"
             << "      </timg:MoveOptions>\r\n"
             << "    </timg:GetMoveOptionsResponse>\r\n";
    } else if (isOp(opName, "GetPresets")) {
        std::vector<ImagingPreset> presets;
        if (m_imagingHandler) {
            presets = m_imagingHandler->handleGetImagingPresets(videoSourceToken);
        } else {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            presets = m_internalImagingPresets;
        }

        body << "    <timg:GetPresetsResponse>\r\n";
        for (const auto& p : presets) {
            body << "      <timg:Preset token=\"" << p.token << "\" type=\"" << p.type << "\">\r\n"
                 << "        <timg:Name>" << p.name << "</timg:Name>\r\n"
                 << "      </timg:Preset>\r\n";
        }
        body << "    </timg:GetPresetsResponse>\r\n";
    } else if (isOp(opName, "GetCurrentPreset")) {
        std::string currentToken;
        {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            currentToken = m_currentImagingPresetToken;
        }
        body << "    <timg:GetCurrentPresetResponse>\r\n"
             << "      <timg:CurrentPreset>\r\n"
             << "        <timg:Token>" << currentToken << "</timg:Token>\r\n"
             << "      </timg:CurrentPreset>\r\n"
             << "    </timg:GetCurrentPresetResponse>\r\n";
    } else if (isOp(opName, "SetCurrentPreset")) {
        const pugi::xml_node presetNode = doc.select_node("//*[local-name()='PresetToken']").node();
        const std::string token = presetNode ? presetNode.text().as_string() : "";
        if (m_imagingHandler) {
            m_imagingHandler->handleSetCurrentImagingPreset(videoSourceToken, token);
        }
        {
            std::lock_guard<std::mutex> lock(m_imagingMutex);
            m_currentImagingPresetToken = token;
        }
        body << "    <timg:SetCurrentPresetResponse/>\r\n";
    } else {
        body << "    <timg:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleDeviceIoService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "DeviceIO", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    std::ostringstream body;

    if (isOp(opName, "GetRelayOutputs")) {
        std::vector<RelayOutputConfig> relays;
        if (m_deviceIoHandler) {
            relays = m_deviceIoHandler->handleGetRelayOutputs();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceIoMutex);
            relays = m_internalRelayOutputs;
        }

        body << "    <tmd:GetRelayOutputsResponse>\r\n";
        for (const auto& r : relays) {
            body << "      <tmd:RelayOutputs token=\"" << r.token << "\">\r\n"
                 << "        <tt:Properties>\r\n"
                 << "          <tt:Mode>" << relayModeToString(r.mode) << "</tt:Mode>\r\n"
                 << "          <tt:DelayTime>PT" << static_cast<int>(r.delayTimeSeconds) << "S</tt:DelayTime>\r\n"
                 << "          <tt:IdleState>" << relayIdleStateToString(r.idleState) << "</tt:IdleState>\r\n"
                 << "        </tt:Properties>\r\n"
                 << "        <tt:LogicalState>" << relayLogicalStateToString(r.logicalState) << "</tt:LogicalState>\r\n"
                 << "      </tmd:RelayOutputs>\r\n";
        }
        body << "    </tmd:GetRelayOutputsResponse>\r\n";
    } else if (isOp(opName, "GetRelayOutputOptions")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='RelayOutputToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "Relay_1";

        body << "    <tmd:GetRelayOutputOptionsResponse>\r\n"
             << "      <tmd:RelayOutputOptions token=\"" << token << "\">\r\n"
             << "        <tt:Mode>Bistable</tt:Mode>\r\n"
             << "        <tt:Mode>Monostable</tt:Mode>\r\n"
             << "        <tt:DelayTimes>\r\n"
             << "          <tt:Min>PT0S</tt:Min>\r\n"
             << "          <tt:Max>PT300S</tt:Max>\r\n"
             << "        </tt:DelayTimes>\r\n"
             << "        <tt:Discrete>false</tt:Discrete>\r\n"
             << "      </tmd:RelayOutputOptions>\r\n"
             << "    </tmd:GetRelayOutputOptionsResponse>\r\n";
    } else if (isOp(opName, "SetRelayOutputSettings")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='RelayOutputToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        RelayOutputConfig updated;
        updated.token = token;

        const pugi::xml_node modeNode = doc.select_node("//*[local-name()='Mode']").node();
        if (modeNode) {
            updated.mode = relayModeFromString(modeNode.text().as_string());
        }

        const pugi::xml_node delayNode = doc.select_node("//*[local-name()='DelayTime']").node();
        if (delayNode) {
            updated.delayTimeSeconds = static_cast<float>(parseTimeoutSeconds(delayNode.text().as_string(), 0));
        }

        const pugi::xml_node idleNode = doc.select_node("//*[local-name()='IdleState']").node();
        if (idleNode) {
            updated.idleState = relayIdleStateFromString(idleNode.text().as_string());
        }

        if (m_deviceIoHandler) {
            m_deviceIoHandler->handleSetRelayOutputSettings(token, updated);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceIoMutex);
            for (auto& r : m_internalRelayOutputs) {
                if (r.token == token) {
                    r.mode = updated.mode;
                    r.delayTimeSeconds = updated.delayTimeSeconds;
                    r.idleState = updated.idleState;
                    break;
                }
            }
        }

        body << "    <tmd:SetRelayOutputSettingsResponse/>\r\n";
    } else if (isOp(opName, "SetRelayOutputState")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='RelayOutputToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";

        const pugi::xml_node stateNode = doc.select_node("//*[local-name()='LogicalState']").node();
        const std::string stateStr = stateNode ? stateNode.text().as_string() : "active";
        const RelayLogicalState state = relayLogicalStateFromString(stateStr);

        if (m_deviceIoHandler) {
            m_deviceIoHandler->handleSetRelayOutputState(token, state);
        }
        {
            std::lock_guard<std::mutex> lock(m_deviceIoMutex);
            for (auto& r : m_internalRelayOutputs) {
                if (r.token == token) {
                    r.logicalState = state;
                    break;
                }
            }
        }

        body << "    <tmd:SetRelayOutputStateResponse/>\r\n";
    } else if (isOp(opName, "GetDigitalInputs")) {
        std::vector<DigitalInputConfig> inputs;
        if (m_deviceIoHandler) {
            inputs = m_deviceIoHandler->handleGetDigitalInputs();
        } else {
            std::lock_guard<std::mutex> lock(m_deviceIoMutex);
            inputs = m_internalDigitalInputs;
        }

        body << "    <tmd:GetDigitalInputsResponse>\r\n";
        for (const auto& in : inputs) {
            body << "      <tmd:DigitalInputs token=\"" << in.token << "\">\r\n"
                 << "        <tt:IdleState>" << relayIdleStateToString(in.idleState) << "</tt:IdleState>\r\n"
                 << "      </tmd:DigitalInputs>\r\n";
        }
        body << "    </tmd:GetDigitalInputsResponse>\r\n";
    } else if (isOp(opName, "GetDigitalInputConfigurationOptions")) {
        const pugi::xml_node tokenNode = doc.select_node("//*[local-name()='Token']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "Input_1";

        body << "    <tmd:GetDigitalInputConfigurationOptionsResponse>\r\n"
             << "      <tmd:DigitalInputOptions token=\"" << token << "\">\r\n"
             << "        <tt:IdleState>open</tt:IdleState>\r\n"
             << "        <tt:IdleState>closed</tt:IdleState>\r\n"
             << "      </tmd:DigitalInputOptions>\r\n"
             << "    </tmd:GetDigitalInputConfigurationOptionsResponse>\r\n";
    } else if (isOp(opName, "GetVideoSources")) {
        body << "    <tmd:GetVideoSourcesResponse>\r\n"
             << "      <tmd:VideoSources token=\"VideoSource_1\">\r\n"
             << "        <tt:Framerate>30.0</tt:Framerate>\r\n"
             << "        <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>\r\n"
             << "      </tmd:VideoSources>\r\n"
             << "    </tmd:GetVideoSourcesResponse>\r\n";
    } else if (isOp(opName, "GetVideoOutputs")) {
        body << "    <tmd:GetVideoOutputsResponse/>\r\n";
    } else if (isOp(opName, "GetAudioSources")) {
        body << "    <tmd:GetAudioSourcesResponse/>\r\n";
    } else if (isOp(opName, "GetAudioOutputs")) {
        body << "    <tmd:GetAudioOutputsResponse/>\r\n";
    } else {
        body << "    <tmd:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleEventService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Events", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    const std::string host = resolveHost(req);
    const int port = m_config.port;

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
    } else if (isOp(opName, "Subscribe")) {
        const pugi::xml_node consumerNode
            = doc.select_node("//*[local-name()='ConsumerReference']/*[local-name()='Address']").node();
        const std::string consumerUrl = consumerNode ? consumerNode.text().as_string() : "";

        std::string subId;
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            subId = std::to_string(m_nextSubId++);
            PushSubscription pushSub;
            pushSub.id = subId;
            pushSub.consumerUrl = consumerUrl;
            pushSub.terminationTime = std::chrono::steady_clock::now() + std::chrono::minutes(10);
            m_pushSubscriptions[subId] = pushSub;
        }

        const auto now = std::chrono::system_clock::now();
        const std::string curTime = formatIso8601Utc(now);
        const std::string termTime = formatIso8601Utc(now + std::chrono::minutes(10));

        body << "    <wsnt:SubscribeResponse>\r\n"
             << "      <wsnt:SubscriptionReference>\r\n"
             << "        <wsa:Address>http://" << host << ":" << port << "/onvif/events/subscription/" << subId
             << "</wsa:Address>\r\n"
             << "      </wsnt:SubscriptionReference>\r\n"
             << "      <wsnt:CurrentTime>" << curTime << "</wsnt:CurrentTime>\r\n"
             << "      <wsnt:TerminationTime>" << termTime << "</wsnt:TerminationTime>\r\n"
             << "    </wsnt:SubscribeResponse>\r\n";
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

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleSubscriptionService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "PullPoint", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

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
                m_pushSubscriptions.erase(subId);
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

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleAnalyticsService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Analytics", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    std::ostringstream body;

    const auto serializeRuleToXml = [](std::ostringstream& ss, const AnalyticsRule& rule) {
        ss << "      <tan:Rule Name=\"" << rule.name << "\" Type=\"" << rule.type << "\">\r\n"
           << "        <tan:Parameters>\r\n";
        if (rule.type.find("Line") != std::string::npos) {
            ss << "          <tt:SimpleItem Name=\"Direction\" Value=\"" << rule.direction << "\"/>\r\n"
               << "          <tt:ElementItem Name=\"Segment\">\r\n"
               << "            <tt:Point x=\"" << std::fixed << std::setprecision(4) << rule.lineStart.x << "\" y=\""
               << std::fixed << std::setprecision(4) << rule.lineStart.y << "\"/>\r\n"
               << "            <tt:Point x=\"" << std::fixed << std::setprecision(4) << rule.lineEnd.x << "\" y=\""
               << std::fixed << std::setprecision(4) << rule.lineEnd.y << "\"/>\r\n"
               << "          </tt:ElementItem>\r\n";
        } else if (rule.type.find("Field") != std::string::npos || rule.type.find("Loitering") != std::string::npos) {
            if (rule.type.find("Loitering") != std::string::npos) {
                ss << "          <tt:SimpleItem Name=\"DwellTime\" Value=\"" << std::fixed << std::setprecision(2)
                   << rule.dwellTimeSeconds << "\"/>\r\n";
            }
            if (!rule.polygon.empty()) {
                ss << "          <tt:ElementItem Name=\"Field\">\r\n"
                   << "            <tt:Polygon>\r\n";
                for (const auto& pt : rule.polygon) {
                    ss << "              <tt:Point x=\"" << std::fixed << std::setprecision(4) << pt.x << "\" y=\""
                       << std::fixed << std::setprecision(4) << pt.y << "\"/>\r\n";
                }
                ss << "            </tt:Polygon>\r\n"
                   << "          </tt:ElementItem>\r\n";
            }
        } else if (rule.type.find("CellMotion") != std::string::npos) {
            ss << "          <tt:SimpleItem Name=\"Sensitivity\" Value=\"" << rule.sensitivity << "\"/>\r\n";
        }

        if (!rule.objectClasses.empty()) {
            std::string classesJoined;
            for (size_t i = 0; i < rule.objectClasses.size(); ++i) {
                if (i > 0) {
                    classesJoined += ",";
                }
                classesJoined += rule.objectClasses[i];
            }
            ss << "          <tt:SimpleItem Name=\"Classes\" Value=\"" << classesJoined << "\"/>\r\n"
               << "          <tt:SimpleItem Name=\"MinConfidence\" Value=\"" << std::fixed << std::setprecision(2)
               << rule.minConfidence << "\"/>\r\n";
        }

        ss << "          <tt:SimpleItem Name=\"Enabled\" Value=\"" << (rule.enabled ? "true" : "false") << "\"/>\r\n"
           << "        </tan:Parameters>\r\n"
           << "      </tan:Rule>\r\n";
    };

    const auto serializeModuleToXml = [](std::ostringstream& ss, const AnalyticsModule& mod) {
        ss << "      <tan:AnalyticsModule Name=\"" << mod.name << "\" Type=\"" << mod.type << "\">\r\n"
           << "        <tan:Parameters>\r\n";
        for (const auto& [k, v] : mod.parameters) {
            ss << "          <tt:SimpleItem Name=\"" << k << "\" Value=\"" << v << "\"/>\r\n";
        }
        ss << "        </tan:Parameters>\r\n"
           << "      </tan:AnalyticsModule>\r\n";
    };

    if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <tan:GetServiceCapabilitiesResponse>\r\n"
             << "      <tan:Capabilities RuleSupport=\"true\" AnalyticsModuleSupport=\"true\" "
                "CellBasedSceneDescriptionSupported=\"false\"/>\r\n"
             << "    </tan:GetServiceCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "GetSupportedRules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsRuleDescription> descs;
        if (m_analyticsHandler) {
            descs = m_analyticsHandler->handleGetSupportedRules(configToken);
        }
        if (descs.empty()) {
            descs = {
                { "tt:LineDetector", { "Segment", "Direction", "Classes", "MinConfidence", "Enabled" } },
                { "tt:FieldDetector", { "Field", "Classes", "MinConfidence", "Enabled" } },
                { "tt:LoiteringDetector", { "Field", "DwellTime", "Classes", "MinConfidence", "Enabled" } },
                { "tt:CellMotionDetector", { "Sensitivity", "Enabled" } },
            };
        }
        body << "    <tan:GetSupportedRulesResponse>\r\n"
             << "      <tan:SupportedRules>\r\n";
        for (const auto& desc : descs) {
            body << "        <tan:RuleDescription Name=\"" << desc.ruleType << "\">\r\n"
                 << "          <tan:Messages IsProperty=\"true\">\r\n"
                 << "            <tt:Source>\r\n"
                 << "              <tt:SimpleItemDescription Name=\"VideoAnalyticsConfigurationToken\" "
                    "Type=\"tt:ReferenceToken\"/>\r\n"
                 << "            </tt:Source>\r\n"
                 << "            <tt:Data>\r\n"
                 << "              <tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>\r\n"
                 << "            </tt:Data>\r\n"
                 << "          </tan:Messages>\r\n";
            for (const auto& param : desc.supportedParameters) {
                body << "          <tan:Parameters>\r\n"
                     << "            <tt:SimpleItemDescription Name=\"" << param << "\" Type=\"xs:string\"/>\r\n"
                     << "          </tan:Parameters>\r\n";
            }
            body << "        </tan:RuleDescription>\r\n";
        }
        body << "      </tan:SupportedRules>\r\n"
             << "    </tan:GetSupportedRulesResponse>\r\n";
    } else if (isOp(opName, "GetRules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsRule> rules;
        if (m_analyticsHandler) {
            rules = m_analyticsHandler->handleGetRules(configToken);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            rules = m_internalRules;
        }
        body << "    <tan:GetRulesResponse>\r\n";
        for (const auto& r : rules) {
            serializeRuleToXml(body, r);
        }
        body << "    </tan:GetRulesResponse>\r\n";
    } else if (isOp(opName, "CreateRules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsRule> newRules = parseAnalyticsRules(reqNode);
        if (m_analyticsHandler) {
            m_analyticsHandler->handleCreateRules(configToken, newRules);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            for (auto& r : newRules) {
                m_internalRules.push_back(r);
            }
        }
        body << "    <tan:CreateRulesResponse/>\r\n";
    } else if (isOp(opName, "ModifyRules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsRule> modRules = parseAnalyticsRules(reqNode);
        if (m_analyticsHandler) {
            m_analyticsHandler->handleModifyRules(configToken, modRules);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            for (const auto& mr : modRules) {
                for (auto& ir : m_internalRules) {
                    if (ir.name == mr.name) {
                        ir = mr;
                        break;
                    }
                }
            }
        }
        body << "    <tan:ModifyRulesResponse/>\r\n";
    } else if (isOp(opName, "DeleteRules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<std::string> delNames;
        for (auto rn : reqNode.select_nodes(".//*[local-name()='RuleName']")) {
            delNames.push_back(rn.node().text().as_string());
        }
        if (m_analyticsHandler) {
            m_analyticsHandler->handleDeleteRules(configToken, delNames);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            m_internalRules.erase(std::remove_if(m_internalRules.begin(), m_internalRules.end(),
                                      [&](const AnalyticsRule& r) {
                                          return std::find(delNames.begin(), delNames.end(), r.name) != delNames.end();
                                      }),
                m_internalRules.end());
        }
        body << "    <tan:DeleteRulesResponse/>\r\n";
    } else if (isOp(opName, "GetSupportedAnalyticsModules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsModuleDescription> descs;
        if (m_analyticsHandler) {
            descs = m_analyticsHandler->handleGetSupportedAnalyticsModules(configToken);
        }
        if (descs.empty()) {
            descs = {
                { "tt:ObjectClassificationModule", { "Classes", "MinConfidence" } },
                { "tt:MotionDetectionModule", { "Sensitivity" } },
            };
        }
        body << "    <tan:GetSupportedAnalyticsModulesResponse>\r\n"
             << "      <tan:SupportedAnalyticsModules>\r\n";
        for (const auto& desc : descs) {
            body << "        <tan:AnalyticsModuleDescription Name=\"" << desc.moduleType << "\">\r\n";
            for (const auto& p : desc.supportedParameters) {
                body << "          <tan:Parameters>\r\n"
                     << "            <tt:SimpleItemDescription Name=\"" << p << "\" Type=\"xs:string\"/>\r\n"
                     << "          </tan:Parameters>\r\n";
            }
            body << "        </tan:AnalyticsModuleDescription>\r\n";
        }
        body << "      </tan:SupportedAnalyticsModules>\r\n"
             << "    </tan:GetSupportedAnalyticsModulesResponse>\r\n";
    } else if (isOp(opName, "GetAnalyticsModules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsModule> mods;
        if (m_analyticsHandler) {
            mods = m_analyticsHandler->handleGetAnalyticsModules(configToken);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            mods = m_internalModules;
        }
        body << "    <tan:GetAnalyticsModulesResponse>\r\n";
        for (const auto& m : mods) {
            serializeModuleToXml(body, m);
        }
        body << "    </tan:GetAnalyticsModulesResponse>\r\n";
    } else if (isOp(opName, "CreateAnalyticsModules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsModule> newMods;
        for (auto mNode : reqNode.select_nodes(".//*[local-name()='AnalyticsModule']")) {
            AnalyticsModule mod;
            mod.name = mNode.node().attribute("Name").as_string();
            mod.type = mNode.node().attribute("Type").as_string();
            for (auto si : mNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
                mod.parameters[si.node().attribute("Name").as_string()] = si.node().attribute("Value").as_string();
            }
            newMods.push_back(std::move(mod));
        }
        if (m_analyticsHandler) {
            m_analyticsHandler->handleCreateAnalyticsModules(configToken, newMods);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            for (auto& m : newMods) {
                m_internalModules.push_back(m);
            }
        }
        body << "    <tan:CreateAnalyticsModulesResponse/>\r\n";
    } else if (isOp(opName, "ModifyAnalyticsModules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<AnalyticsModule> modMods;
        for (auto mNode : reqNode.select_nodes(".//*[local-name()='AnalyticsModule']")) {
            AnalyticsModule mod;
            mod.name = mNode.node().attribute("Name").as_string();
            mod.type = mNode.node().attribute("Type").as_string();
            for (auto si : mNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
                mod.parameters[si.node().attribute("Name").as_string()] = si.node().attribute("Value").as_string();
            }
            modMods.push_back(std::move(mod));
        }
        if (m_analyticsHandler) {
            m_analyticsHandler->handleModifyAnalyticsModules(configToken, modMods);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            for (const auto& mm : modMods) {
                for (auto& im : m_internalModules) {
                    if (im.name == mm.name) {
                        im = mm;
                        break;
                    }
                }
            }
        }
        body << "    <tan:ModifyAnalyticsModulesResponse/>\r\n";
    } else if (isOp(opName, "DeleteAnalyticsModules")) {
        const std::string configToken
            = reqNode.select_node(".//*[local-name()='ConfigurationToken']").node().text().as_string();
        std::vector<std::string> delNames;
        for (auto mn : reqNode.select_nodes(".//*[local-name()='AnalyticsModuleName']")) {
            delNames.push_back(mn.node().text().as_string());
        }
        if (m_analyticsHandler) {
            m_analyticsHandler->handleDeleteAnalyticsModules(configToken, delNames);
        } else {
            std::lock_guard<std::mutex> lock(m_analyticsMutex);
            m_internalModules.erase(std::remove_if(m_internalModules.begin(), m_internalModules.end(),
                                        [&](const AnalyticsModule& m) {
                                            return std::find(delNames.begin(), delNames.end(), m.name)
                                                != delNames.end();
                                        }),
                m_internalModules.end());
        }
        body << "    <tan:DeleteAnalyticsModulesResponse/>\r\n";
    } else {
        body << "    <tan:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleRecordingService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Recording", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    std::ostringstream body;

    if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <trc:GetServiceCapabilitiesResponse>\r\n"
             << "      <trc:Capabilities DynamicRecordings=\"true\" DynamicTracks=\"true\" "
                "Encoding=\"H264,H265\" MaxRecordings=\"10\" MaxRecordingJobs=\"10\" "
                "TotalRecordingJobLicenses=\"10\"/>\r\n"
             << "    </trc:GetServiceCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "CreateRecording")) {
        RecordingConfig config;
        const pugi::xml_node cfgNode = reqNode.select_node(".//*[local-name()='RecordingConfiguration']").node();
        if (cfgNode) {
            const pugi::xml_node srcNode = cfgNode.select_node(".//*[local-name()='Source']").node();
            if (srcNode) {
                const pugi::xml_node srcId = srcNode.select_node(".//*[local-name()='SourceId']").node();
                if (srcId)
                    config.sourceToken = srcId.text().as_string();
            }
            const pugi::xml_node contentNode = cfgNode.select_node(".//*[local-name()='Content']").node();
            if (contentNode)
                config.content = contentNode.text().as_string();
            const pugi::xml_node maxRetNode = cfgNode.select_node(".//*[local-name()='MaximumRetentionTime']").node();
            if (maxRetNode)
                config.maximumRetentionTime = maxRetNode.text().as_string();
        }

        std::string recToken;
        if (m_recordingHandler) {
            recToken = m_recordingHandler->handleCreateRecording(config);
        }
        if (recToken.empty()) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            recToken = "Rec_" + std::to_string(m_internalRecordings.size() + 1);
            config.recordingToken = recToken;
            m_internalRecordings.push_back(config);
        }
        logSystemMessage("INFO", "CreateRecording token=" + recToken);
        body << "    <trc:CreateRecordingResponse>\r\n"
             << "      <trc:RecordingToken>" << recToken << "</trc:RecordingToken>\r\n"
             << "    </trc:CreateRecordingResponse>\r\n";
    } else if (isOp(opName, "GetRecordings")) {
        std::vector<RecordingConfig> recs;
        if (m_recordingHandler) {
            recs = m_recordingHandler->handleGetRecordings();
        }
        if (recs.empty()) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            recs = m_internalRecordings;
        }
        body << "    <trc:GetRecordingsResponse>\r\n";
        for (const auto& rec : recs) {
            body << "      <trc:RecordingItem>\r\n"
                 << "        <trc:RecordingToken>" << rec.recordingToken << "</trc:RecordingToken>\r\n"
                 << "        <trc:Configuration>\r\n"
                 << "          <tt:Source>\r\n"
                 << "            <tt:SourceId>" << rec.sourceToken << "</tt:SourceId>\r\n"
                 << "            <tt:Name>" << rec.sourceToken << "</tt:Name>\r\n"
                 << "          </tt:Source>\r\n"
                 << "          <tt:Content>" << rec.content << "</tt:Content>\r\n"
                 << "          <tt:MaximumRetentionTime>" << rec.maximumRetentionTime
                 << "</tt:MaximumRetentionTime>\r\n"
                 << "        </trc:Configuration>\r\n"
                 << "        <trc:Tracks>\r\n";
            for (const auto& trk : rec.tracks) {
                body << "          <trc:Track>\r\n"
                     << "            <trc:TrackToken>" << trk.trackToken << "</trc:TrackToken>\r\n"
                     << "            <trc:Configuration>\r\n"
                     << "              <tt:TrackType>" << recordingTrackTypeToString(trk.trackType)
                     << "</tt:TrackType>\r\n"
                     << "              <tt:Description>" << trk.description << "</tt:Description>\r\n"
                     << "            </trc:Configuration>\r\n"
                     << "          </trc:Track>\r\n";
            }
            body << "        </trc:Tracks>\r\n"
                 << "      </trc:RecordingItem>\r\n";
        }
        body << "    </trc:GetRecordingsResponse>\r\n";
    } else if (isOp(opName, "GetRecordingConfiguration")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";
        RecordingConfig rec;
        bool found = false;
        if (m_recordingHandler) {
            const auto opt = m_recordingHandler->handleGetRecordingConfiguration(token);
            if (opt.has_value()) {
                rec = *opt;
                found = true;
            }
        }
        if (!found) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (const auto& r : m_internalRecordings) {
                if (r.recordingToken == token) {
                    rec = r;
                    found = true;
                    break;
                }
            }
        }
        body << "    <trc:GetRecordingConfigurationResponse>\r\n"
             << "      <trc:RecordingConfiguration>\r\n"
             << "        <tt:Source>\r\n"
             << "          <tt:SourceId>" << rec.sourceToken << "</tt:SourceId>\r\n"
             << "          <tt:Name>" << rec.sourceToken << "</tt:Name>\r\n"
             << "        </tt:Source>\r\n"
             << "        <tt:Content>" << rec.content << "</tt:Content>\r\n"
             << "        <tt:MaximumRetentionTime>" << rec.maximumRetentionTime << "</tt:MaximumRetentionTime>\r\n"
             << "      </trc:RecordingConfiguration>\r\n"
             << "    </trc:GetRecordingConfigurationResponse>\r\n";
    } else if (isOp(opName, "SetRecordingConfiguration")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";
        const pugi::xml_node cfgNode = reqNode.select_node(".//*[local-name()='RecordingConfiguration']").node();
        RecordingConfig rec;
        rec.recordingToken = token;
        if (cfgNode) {
            const pugi::xml_node srcNode = cfgNode.select_node(".//*[local-name()='Source']").node();
            if (srcNode) {
                const pugi::xml_node srcId = srcNode.select_node(".//*[local-name()='SourceId']").node();
                if (srcId)
                    rec.sourceToken = srcId.text().as_string();
            }
            const pugi::xml_node contentNode = cfgNode.select_node(".//*[local-name()='Content']").node();
            if (contentNode)
                rec.content = contentNode.text().as_string();
            const pugi::xml_node maxRetNode = cfgNode.select_node(".//*[local-name()='MaximumRetentionTime']").node();
            if (maxRetNode)
                rec.maximumRetentionTime = maxRetNode.text().as_string();
        }
        if (m_recordingHandler) {
            m_recordingHandler->handleSetRecordingConfiguration(token, rec);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (auto& r : m_internalRecordings) {
                if (r.recordingToken == token) {
                    r.sourceToken = rec.sourceToken;
                    r.content = rec.content;
                    r.maximumRetentionTime = rec.maximumRetentionTime;
                    break;
                }
            }
        }
        body << "    <trc:SetRecordingConfigurationResponse/>\r\n";
    } else if (isOp(opName, "DeleteRecording")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const std::string token = tokenNode ? tokenNode.text().as_string() : "";
        if (m_recordingHandler) {
            m_recordingHandler->handleDeleteRecording(token);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            m_internalRecordings.erase(std::remove_if(m_internalRecordings.begin(), m_internalRecordings.end(),
                                           [&token](const RecordingConfig& r) { return r.recordingToken == token; }),
                m_internalRecordings.end());
        }
        logSystemMessage("INFO", "Deleted recording " + token);
        body << "    <trc:DeleteRecordingResponse/>\r\n";
    } else if (isOp(opName, "CreateTrack")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const std::string recToken = tokenNode ? tokenNode.text().as_string() : "";
        const pugi::xml_node cfgNode = reqNode.select_node(".//*[local-name()='TrackConfiguration']").node();
        RecordingTrack trk;
        if (cfgNode) {
            const pugi::xml_node typeNode = cfgNode.select_node(".//*[local-name()='TrackType']").node();
            const pugi::xml_node descNode = cfgNode.select_node(".//*[local-name()='Description']").node();
            if (typeNode)
                trk.trackType = recordingTrackTypeFromString(typeNode.text().as_string());
            if (descNode)
                trk.description = descNode.text().as_string();
        }
        std::string trkToken;
        if (m_recordingHandler) {
            trkToken = m_recordingHandler->handleCreateTrack(recToken, trk);
        }
        if (trkToken.empty()) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (auto& r : m_internalRecordings) {
                if (r.recordingToken == recToken) {
                    trkToken = "Track_" + std::to_string(r.tracks.size() + 1);
                    trk.trackToken = trkToken;
                    r.tracks.push_back(trk);
                    break;
                }
            }
        }
        body << "    <trc:CreateTrackResponse>\r\n"
             << "      <trc:TrackToken>" << trkToken << "</trc:TrackToken>\r\n"
             << "    </trc:CreateTrackResponse>\r\n";
    } else if (isOp(opName, "GetTrackConfiguration")) {
        const pugi::xml_node recNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const pugi::xml_node trkNode = reqNode.select_node(".//*[local-name()='TrackToken']").node();
        const std::string recToken = recNode ? recNode.text().as_string() : "";
        const std::string trkToken = trkNode ? trkNode.text().as_string() : "";
        RecordingTrack trk;
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (const auto& r : m_internalRecordings) {
                if (r.recordingToken == recToken) {
                    for (const auto& t : r.tracks) {
                        if (t.trackToken == trkToken) {
                            trk = t;
                            break;
                        }
                    }
                }
            }
        }
        body << "    <trc:GetTrackConfigurationResponse>\r\n"
             << "      <trc:TrackConfiguration>\r\n"
             << "        <tt:TrackType>" << recordingTrackTypeToString(trk.trackType) << "</tt:TrackType>\r\n"
             << "        <tt:Description>" << trk.description << "</tt:Description>\r\n"
             << "      </trc:TrackConfiguration>\r\n"
             << "    </trc:GetTrackConfigurationResponse>\r\n";
    } else if (isOp(opName, "DeleteTrack")) {
        const pugi::xml_node recNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const pugi::xml_node trkNode = reqNode.select_node(".//*[local-name()='TrackToken']").node();
        const std::string recToken = recNode ? recNode.text().as_string() : "";
        const std::string trkToken = trkNode ? trkNode.text().as_string() : "";
        if (m_recordingHandler) {
            m_recordingHandler->handleDeleteTrack(recToken, trkToken);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (auto& r : m_internalRecordings) {
                if (r.recordingToken == recToken) {
                    r.tracks.erase(std::remove_if(r.tracks.begin(), r.tracks.end(),
                                       [&trkToken](const RecordingTrack& t) { return t.trackToken == trkToken; }),
                        r.tracks.end());
                    break;
                }
            }
        }
        body << "    <trc:DeleteTrackResponse/>\r\n";
    } else if (isOp(opName, "GetRecordingJobs")) {
        std::vector<RecordingJob> jobs;
        if (m_recordingHandler) {
            jobs = m_recordingHandler->handleGetRecordingJobs();
        }
        if (jobs.empty()) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            jobs = m_internalRecordingJobs;
        }
        body << "    <trc:GetRecordingJobsResponse>\r\n";
        for (const auto& job : jobs) {
            body << "      <trc:JobItem>\r\n"
                 << "        <trc:JobToken>" << job.jobToken << "</trc:JobToken>\r\n"
                 << "        <trc:JobConfiguration>\r\n"
                 << "          <tt:RecordingToken>" << job.recordingToken << "</tt:RecordingToken>\r\n"
                 << "          <tt:Mode>" << recordingJobModeToString(job.mode) << "</tt:Mode>\r\n"
                 << "          <tt:Priority>" << job.priority << "</tt:Priority>\r\n"
                 << "          <tt:Source>\r\n"
                 << "            <tt:SourceToken>" << job.sourceToken << "</tt:SourceToken>\r\n"
                 << "          </tt:Source>\r\n"
                 << "        </trc:JobConfiguration>\r\n"
                 << "      </trc:JobItem>\r\n";
        }
        body << "    </trc:GetRecordingJobsResponse>\r\n";
    } else if (isOp(opName, "CreateRecordingJob")) {
        const pugi::xml_node cfgNode = reqNode.select_node(".//*[local-name()='JobConfiguration']").node();
        RecordingJob job;
        if (cfgNode) {
            const pugi::xml_node recNode = cfgNode.select_node(".//*[local-name()='RecordingToken']").node();
            const pugi::xml_node modeNode = cfgNode.select_node(".//*[local-name()='Mode']").node();
            const pugi::xml_node prioNode = cfgNode.select_node(".//*[local-name()='Priority']").node();
            const pugi::xml_node srcNode = cfgNode.select_node(".//*[local-name()='SourceToken']").node();
            if (recNode)
                job.recordingToken = recNode.text().as_string();
            if (modeNode)
                job.mode = recordingJobModeFromString(modeNode.text().as_string());
            if (prioNode)
                job.priority = prioNode.text().as_int(5);
            if (srcNode)
                job.sourceToken = srcNode.text().as_string();
        }
        std::string jobToken;
        if (m_recordingHandler) {
            jobToken = m_recordingHandler->handleCreateRecordingJob(job);
        }
        if (jobToken.empty()) {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            jobToken = "Job_" + std::to_string(m_internalRecordingJobs.size() + 1);
            job.jobToken = jobToken;
            m_internalRecordingJobs.push_back(job);
        }
        body << "    <trc:CreateRecordingJobResponse>\r\n"
             << "      <trc:JobToken>" << jobToken << "</trc:JobToken>\r\n"
             << "    </trc:CreateRecordingJobResponse>\r\n";
    } else if (isOp(opName, "SetRecordingJobMode")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='JobToken']").node();
        const pugi::xml_node modeNode = reqNode.select_node(".//*[local-name()='Mode']").node();
        const std::string jobToken = tokenNode ? tokenNode.text().as_string() : "";
        const std::string modeStr = modeNode ? modeNode.text().as_string() : "Active";
        const RecordingJobMode mode = recordingJobModeFromString(modeStr);
        if (m_recordingHandler) {
            m_recordingHandler->handleSetRecordingJobMode(jobToken, mode);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            for (auto& j : m_internalRecordingJobs) {
                if (j.jobToken == jobToken) {
                    j.mode = mode;
                    break;
                }
            }
        }
        logSystemMessage("INFO", "RecordingJob " + jobToken + " mode set to " + modeStr);
        body << "    <trc:SetRecordingJobModeResponse/>\r\n";
    } else if (isOp(opName, "DeleteRecordingJob")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='JobToken']").node();
        const std::string jobToken = tokenNode ? tokenNode.text().as_string() : "";
        if (m_recordingHandler) {
            m_recordingHandler->handleDeleteRecordingJob(jobToken);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            m_internalRecordingJobs.erase(std::remove_if(m_internalRecordingJobs.begin(), m_internalRecordingJobs.end(),
                                              [&jobToken](const RecordingJob& j) { return j.jobToken == jobToken; }),
                m_internalRecordingJobs.end());
        }
        body << "    <trc:DeleteRecordingJobResponse/>\r\n";
    } else if (isOp(opName, "GetRecordingSummary")) {
        RecordingSummary sum;
        if (m_recordingHandler) {
            sum = m_recordingHandler->handleGetRecordingSummary();
        } else {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            sum.numberRecordings = static_cast<int>(m_internalRecordings.size());
            sum.dataFrom = "2026-01-01T00:00:00Z";
            sum.dataUntil = formatIso8601Utc(std::chrono::system_clock::now());
            sum.totalStorageBytes = 1073741824ULL;
        }
        body << "    <trc:GetRecordingSummaryResponse>\r\n"
             << "      <trc:Summary>\r\n"
             << "        <tt:DataFrom>" << sum.dataFrom << "</tt:DataFrom>\r\n"
             << "        <tt:DataUntil>" << sum.dataUntil << "</tt:DataUntil>\r\n"
             << "        <tt:NumberRecordings>" << sum.numberRecordings << "</tt:NumberRecordings>\r\n"
             << "      </trc:Summary>\r\n"
             << "    </trc:GetRecordingSummaryResponse>\r\n";
    } else {
        body << "    <trc:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleSearchService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Search", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    std::ostringstream body;

    if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <tse:GetServiceCapabilitiesResponse>\r\n"
             << "      <tse:Capabilities MetadataSearch=\"true\" GeneralStartEvents=\"true\"/>\r\n"
             << "    </tse:GetServiceCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "FindRecordings")) {
        std::string searchToken;
        std::vector<RecordingSearchResult> results;
        if (m_searchHandler) {
            searchToken = m_searchHandler->handleFindRecordings("", 10, "PT60S");
            results = m_searchHandler->handleGetRecordingSearchResults(searchToken);
        }
        if (searchToken.empty()) {
            std::lock_guard<std::mutex> lock(m_searchMutex);
            searchToken = "SearchSession_" + std::to_string(m_nextSearchId++);
            std::lock_guard<std::mutex> recLock(m_recordingMutex);
            for (const auto& r : m_internalRecordings) {
                for (const auto& trk : r.tracks) {
                    RecordingSearchResult resItem;
                    resItem.recordingToken = r.recordingToken;
                    resItem.trackToken = trk.trackToken;
                    resItem.earliestTime = "2026-01-01T00:00:00Z";
                    resItem.latestTime = formatIso8601Utc(std::chrono::system_clock::now());
                    resItem.searchState = "Completed";
                    results.push_back(resItem);
                }
            }
            m_recordingSearches[searchToken] = results;
        }
        logSystemMessage("INFO", "FindRecordings searchToken=" + searchToken);
        body << "    <tse:FindRecordingsResponse>\r\n"
             << "      <tse:SearchToken>" << searchToken << "</tse:SearchToken>\r\n"
             << "    </tse:FindRecordingsResponse>\r\n";
    } else if (isOp(opName, "GetRecordingSearchResults")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='SearchToken']").node();
        const std::string searchToken = tokenNode ? tokenNode.text().as_string() : "";
        std::vector<RecordingSearchResult> results;
        if (m_searchHandler) {
            results = m_searchHandler->handleGetRecordingSearchResults(searchToken);
        }
        if (results.empty()) {
            std::lock_guard<std::mutex> lock(m_searchMutex);
            auto it = m_recordingSearches.find(searchToken);
            if (it != m_recordingSearches.end()) {
                results = it->second;
            }
        }
        body << "    <tse:GetRecordingSearchResultsResponse>\r\n"
             << "      <tse:ResultList SearchState=\"Completed\">\r\n";
        for (const auto& resItem : results) {
            body << "        <tse:RecordingInformation>\r\n"
                 << "          <tt:RecordingToken>" << resItem.recordingToken << "</tt:RecordingToken>\r\n"
                 << "          <tt:TrackToken>" << resItem.trackToken << "</tt:TrackToken>\r\n"
                 << "          <tt:EarliestRecording>" << resItem.earliestTime << "</tt:EarliestRecording>\r\n"
                 << "          <tt:LatestRecording>" << resItem.latestTime << "</tt:LatestRecording>\r\n"
                 << "          <tt:SearchState>" << resItem.searchState << "</tt:SearchState>\r\n"
                 << "        </tse:RecordingInformation>\r\n";
        }
        body << "      </tse:ResultList>\r\n"
             << "    </tse:GetRecordingSearchResultsResponse>\r\n";
    } else if (isOp(opName, "FindEvents")) {
        const pugi::xml_node startNode = reqNode.select_node(".//*[local-name()='StartPoint']").node();
        const pugi::xml_node endNode = reqNode.select_node(".//*[local-name()='EndPoint']").node();
        const std::string startPt = startNode ? startNode.text().as_string() : "";
        const std::string endPt = endNode ? endNode.text().as_string() : "";
        std::string searchToken;
        if (m_searchHandler) {
            searchToken = m_searchHandler->handleFindEvents(startPt, endPt, 10);
        }
        if (searchToken.empty()) {
            std::lock_guard<std::mutex> lock(m_searchMutex);
            searchToken = "EventSearch_" + std::to_string(m_nextSearchId++);
            std::vector<RecordedEventResult> evResults;
            RecordedEventResult ev1;
            ev1.recordingToken = "Rec_Main";
            ev1.eventTime = startPt.empty() ? formatIso8601Utc(std::chrono::system_clock::now()) : startPt;
            ev1.topic = "tns1:VideoAnalytics/Motion";
            ev1.source = "VideoSource_1";
            ev1.data = "State=true";
            evResults.push_back(ev1);
            m_eventSearches[searchToken] = evResults;
        }
        body << "    <tse:FindEventsResponse>\r\n"
             << "      <tse:SearchToken>" << searchToken << "</tse:SearchToken>\r\n"
             << "    </tse:FindEventsResponse>\r\n";
    } else if (isOp(opName, "GetEventSearchResults")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='SearchToken']").node();
        const std::string searchToken = tokenNode ? tokenNode.text().as_string() : "";
        std::vector<RecordedEventResult> events;
        if (m_searchHandler) {
            events = m_searchHandler->handleGetEventSearchResults(searchToken);
        }
        if (events.empty()) {
            std::lock_guard<std::mutex> lock(m_searchMutex);
            auto it = m_eventSearches.find(searchToken);
            if (it != m_eventSearches.end()) {
                events = it->second;
            }
        }
        body << "    <tse:GetEventSearchResultsResponse>\r\n"
             << "      <tse:ResultList SearchState=\"Completed\">\r\n";
        for (const auto& ev : events) {
            body << "        <tse:EventInformation>\r\n"
                 << "          <tt:RecordingToken>" << ev.recordingToken << "</tt:RecordingToken>\r\n"
                 << "          <tt:UtcTime>" << ev.eventTime << "</tt:UtcTime>\r\n"
                 << "          <tt:Topic>" << ev.topic << "</tt:Topic>\r\n"
                 << "          <tt:Source>" << ev.source << "</tt:Source>\r\n"
                 << "          <tt:Data>" << ev.data << "</tt:Data>\r\n"
                 << "        </tse:EventInformation>\r\n";
        }
        body << "      </tse:ResultList>\r\n"
             << "    </tse:GetEventSearchResultsResponse>\r\n";
    } else if (isOp(opName, "EndSearch")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='SearchToken']").node();
        const std::string searchToken = tokenNode ? tokenNode.text().as_string() : "";
        if (m_searchHandler) {
            m_searchHandler->handleEndSearch(searchToken);
        }
        {
            std::lock_guard<std::mutex> lock(m_searchMutex);
            m_recordingSearches.erase(searchToken);
            m_eventSearches.erase(searchToken);
        }
        body << "    <tse:EndSearchResponse>\r\n"
             << "      <tse:Endpoint>" << searchToken << "</tse:Endpoint>\r\n"
             << "    </tse:EndSearchResponse>\r\n";
    } else {
        body << "    <tse:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleReplayService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Replay", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;

    std::ostringstream body;

    if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <trp:GetServiceCapabilitiesResponse>\r\n"
             << "      <trp:Capabilities ReversePlayback=\"true\" SessionTimeoutRange=\"PT10S PT300S\" "
                "RTSPWebSocketUri=\"false\"/>\r\n"
             << "    </trp:GetServiceCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "GetReplayUri")) {
        const pugi::xml_node tokenNode = reqNode.select_node(".//*[local-name()='RecordingToken']").node();
        const std::string recToken = tokenNode ? tokenNode.text().as_string() : "Rec_Main";
        std::string uri;
        if (m_replayHandler) {
            uri = m_replayHandler->handleGetReplayUri(recToken, "Track_Video_1");
        }
        if (uri.empty()) {
            const std::string baseReplay = m_config.replayStreamUri.empty()
                ? "rtsp://" + resolveHost(req) + ":8554/replay"
                : m_config.replayStreamUri;
            uri = baseReplay + "?recording=" + recToken;
        }
        body << "    <trp:GetReplayUriResponse>\r\n"
             << "      <trp:Uri>" << uri << "</trp:Uri>\r\n"
             << "    </trp:GetReplayUriResponse>\r\n";
    } else if (isOp(opName, "GetReplayConfiguration")) {
        ReplayConfiguration cfg;
        if (m_replayHandler) {
            cfg = m_replayHandler->handleGetReplayConfiguration();
        } else {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            cfg = m_internalReplayConfig;
        }
        body << "    <trp:GetReplayConfigurationResponse>\r\n"
             << "      <trp:Configuration>\r\n"
             << "        <tt:SessionTimeout>" << cfg.sessionTimeout << "</tt:SessionTimeout>\r\n"
             << "      </trp:Configuration>\r\n"
             << "    </trp:GetReplayConfigurationResponse>\r\n";
    } else if (isOp(opName, "SetReplayConfiguration")) {
        const pugi::xml_node timeoutNode = reqNode.select_node(".//*[local-name()='SessionTimeout']").node();
        std::string timeoutStr = timeoutNode ? timeoutNode.text().as_string() : "PT60S";
        ReplayConfiguration cfg;
        cfg.sessionTimeout = timeoutStr;
        if (m_replayHandler) {
            m_replayHandler->handleSetReplayConfiguration(cfg);
        }
        {
            std::lock_guard<std::mutex> lock(m_recordingMutex);
            m_internalReplayConfig.sessionTimeout = timeoutStr;
        }
        body << "    <trp:SetReplayConfigurationResponse/>\r\n";
    } else {
        body << "    <trp:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

void OnvifServer::handleThermalService(const httplib::Request& req, httplib::Response& res)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, "Thermal", doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;
    appendAccessLog("Thermal", opName, req.remote_addr);

    std::ostringstream body;

    if (isOp(opName, "GetServiceCapabilities")) {
        body << "    <tth:GetServiceCapabilitiesResponse>\r\n"
             << "      <tth:Capabilities Radiometry=\"true\" ColorPalette=\"true\" NUC=\"true\" Cooler=\"false\"/>\r\n"
             << "    </tth:GetServiceCapabilitiesResponse>\r\n";
    } else if (isOp(opName, "GetRadiometryConfigurationOptions")) {
        body << "    <tth:GetRadiometryConfigurationOptionsResponse>\r\n"
             << "      <tth:Options>\r\n"
             << "        <tth:EmissivityRange Min=\"0.01\" Max=\"1.00\"/>\r\n"
             << "        <tth:DistanceRange Min=\"0.1\" Max=\"1000.0\"/>\r\n"
             << "        <tth:ReflectedTemperatureRange Min=\"-50.0\" Max=\"1500.0\"/>\r\n"
             << "        <tth:AtmosphericTemperatureRange Min=\"-50.0\" Max=\"100.0\"/>\r\n"
             << "        <tth:RelativeHumidityRange Min=\"0.0\" Max=\"100.0\"/>\r\n"
             << "        <tth:WindowTransmissionRange Min=\"0.01\" Max=\"1.00\"/>\r\n"
             << "      </tth:Options>\r\n"
             << "    </tth:GetRadiometryConfigurationOptionsResponse>\r\n";
    } else if (isOp(opName, "GetRadiometryConfiguration")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        RadiometryConfig cfg {};
        if (m_thermalHandler) {
            cfg = m_thermalHandler->handleGetRadiometryConfiguration(tok);
        } else {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            cfg = m_internalRadiometryConfig;
        }
        body << "    <tth:GetRadiometryConfigurationResponse>\r\n"
             << "      <tth:Configuration>\r\n"
             << "        <tth:VideoSourceToken>" << tok << "</tth:VideoSourceToken>\r\n"
             << "        <tth:Emissivity>" << std::fixed << std::setprecision(2) << cfg.emissivity
             << "</tth:Emissivity>\r\n"
             << "        <tth:Distance>" << std::fixed << std::setprecision(1) << cfg.distance << "</tth:Distance>\r\n"
             << "        <tth:ReflectedTemperature>" << std::fixed << std::setprecision(1) << cfg.reflectedTemperature
             << "</tth:ReflectedTemperature>\r\n"
             << "        <tth:AtmosphericTemperature>" << std::fixed << std::setprecision(1)
             << cfg.atmosphericTemperature << "</tth:AtmosphericTemperature>\r\n"
             << "        <tth:RelativeHumidity>" << std::fixed << std::setprecision(1) << cfg.relativeHumidity
             << "</tth:RelativeHumidity>\r\n"
             << "        <tth:WindowTransmission>" << std::fixed << std::setprecision(2) << cfg.windowTransmission
             << "</tth:WindowTransmission>\r\n"
             << "      </tth:Configuration>\r\n"
             << "    </tth:GetRadiometryConfigurationResponse>\r\n";
    } else if (isOp(opName, "SetRadiometryConfiguration")) {
        const auto cfgNode = reqNode.select_node(".//*[local-name()='Configuration']").node();
        const auto tokNode
            = cfgNode ? cfgNode.select_node(".//*[local-name()='VideoSourceToken']").node() : pugi::xml_node {};
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";

        RadiometryConfig cfg {};
        if (cfgNode) {
            const auto emNode = cfgNode.select_node(".//*[local-name()='Emissivity']").node();
            if (emNode)
                cfg.emissivity = emNode.text().as_float(0.95f);
            const auto distNode = cfgNode.select_node(".//*[local-name()='Distance']").node();
            if (distNode)
                cfg.distance = distNode.text().as_float(5.0f);
            const auto refNode = cfgNode.select_node(".//*[local-name()='ReflectedTemperature']").node();
            if (refNode)
                cfg.reflectedTemperature = refNode.text().as_float(20.0f);
            const auto atmNode = cfgNode.select_node(".//*[local-name()='AtmosphericTemperature']").node();
            if (atmNode)
                cfg.atmosphericTemperature = atmNode.text().as_float(20.0f);
            const auto humNode = cfgNode.select_node(".//*[local-name()='RelativeHumidity']").node();
            if (humNode)
                cfg.relativeHumidity = humNode.text().as_float(50.0f);
            const auto winNode = cfgNode.select_node(".//*[local-name()='WindowTransmission']").node();
            if (winNode)
                cfg.windowTransmission = winNode.text().as_float(1.0f);
        }

        if (m_thermalHandler) {
            m_thermalHandler->handleSetRadiometryConfiguration(tok, cfg);
        }
        {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            m_internalRadiometryConfig = cfg;
        }
        body << "    <tth:SetRadiometryConfigurationResponse/>\r\n";
    } else if (isOp(opName, "GetRadiometrySpots")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        std::vector<RadiometrySpot> spots {};
        if (m_thermalHandler) {
            spots = m_thermalHandler->handleGetRadiometrySpots(tok);
        } else {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            spots = m_internalRadiometrySpots;
        }
        body << "    <tth:GetRadiometrySpotsResponse>\r\n";
        for (const auto& s : spots) {
            body << "      <tth:Spot token=\"" << s.token << "\">\r\n"
                 << "        <tth:Position x=\"" << std::fixed << std::setprecision(4) << s.position.x << "\" y=\""
                 << s.position.y << "\"/>\r\n"
                 << "        <tth:Label>" << s.label << "</tth:Label>\r\n"
                 << "        <tth:Temperature>" << std::fixed << std::setprecision(1) << s.temperature
                 << "</tth:Temperature>\r\n"
                 << "      </tth:Spot>\r\n";
        }
        body << "    </tth:GetRadiometrySpotsResponse>\r\n";
    } else if (isOp(opName, "SetRadiometrySpots")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        std::vector<RadiometrySpot> spots {};
        for (const auto& spotNode : reqNode.select_nodes(".//*[local-name()='Spot']")) {
            RadiometrySpot s {};
            s.token = spotNode.node().attribute("token").as_string();
            const auto pos = spotNode.node().select_node(".//*[local-name()='Position']").node();
            if (pos) {
                s.position = Xml::parsePoint2D(pos, 0.5f, 0.5f);
            }
            const auto lbl = spotNode.node().select_node(".//*[local-name()='Label']").node();
            if (lbl)
                s.label = lbl.text().as_string();
            const auto temp = spotNode.node().select_node(".//*[local-name()='Temperature']").node();
            if (temp)
                s.temperature = temp.text().as_float(0.0f);
            if (!s.token.empty())
                spots.push_back(std::move(s));
        }
        if (m_thermalHandler) {
            m_thermalHandler->handleSetRadiometrySpots(tok, spots);
        }
        {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            m_internalRadiometrySpots = spots;
        }
        body << "    <tth:SetRadiometrySpotsResponse/>\r\n";
    } else if (isOp(opName, "GetRadiometryBoxes")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        std::vector<RadiometryBox> boxes {};
        if (m_thermalHandler) {
            boxes = m_thermalHandler->handleGetRadiometryBoxes(tok);
        } else {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            boxes = m_internalRadiometryBoxes;
        }
        body << "    <tth:GetRadiometryBoxesResponse>\r\n";
        for (const auto& b : boxes) {
            body << "      <tth:Box token=\"" << b.token << "\">\r\n"
                 << "        <tth:TopLeft x=\"" << std::fixed << std::setprecision(4) << b.topLeft.x << "\" y=\""
                 << b.topLeft.y << "\"/>\r\n"
                 << "        <tth:BottomRight x=\"" << b.bottomRight.x << "\" y=\"" << b.bottomRight.y << "\"/>\r\n"
                 << "        <tth:Label>" << b.label << "</tth:Label>\r\n"
                 << "        <tth:MinTemperature>" << std::fixed << std::setprecision(1) << b.minTemperature
                 << "</tth:MinTemperature>\r\n"
                 << "        <tth:MaxTemperature>" << b.maxTemperature << "</tth:MaxTemperature>\r\n"
                 << "        <tth:AvgTemperature>" << b.avgTemperature << "</tth:AvgTemperature>\r\n"
                 << "      </tth:Box>\r\n";
        }
        body << "    </tth:GetRadiometryBoxesResponse>\r\n";
    } else if (isOp(opName, "SetRadiometryBoxes")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        std::vector<RadiometryBox> boxes {};
        for (const auto& boxNode : reqNode.select_nodes(".//*[local-name()='Box']")) {
            RadiometryBox b {};
            b.token = boxNode.node().attribute("token").as_string();
            const auto tl = boxNode.node().select_node(".//*[local-name()='TopLeft']").node();
            if (tl) {
                b.topLeft = Xml::parsePoint2D(tl, 0.0f, 0.0f);
            }
            const auto br = boxNode.node().select_node(".//*[local-name()='BottomRight']").node();
            if (br) {
                b.bottomRight = Xml::parsePoint2D(br, 1.0f, 1.0f);
            }
            const auto lbl = boxNode.node().select_node(".//*[local-name()='Label']").node();
            if (lbl)
                b.label = lbl.text().as_string();
            const auto minT = boxNode.node().select_node(".//*[local-name()='MinTemperature']").node();
            if (minT)
                b.minTemperature = minT.text().as_float(0.0f);
            const auto maxT = boxNode.node().select_node(".//*[local-name()='MaxTemperature']").node();
            if (maxT)
                b.maxTemperature = maxT.text().as_float(0.0f);
            const auto avgT = boxNode.node().select_node(".//*[local-name()='AvgTemperature']").node();
            if (avgT)
                b.avgTemperature = avgT.text().as_float(0.0f);
            if (!b.token.empty())
                boxes.push_back(std::move(b));
        }
        if (m_thermalHandler) {
            m_thermalHandler->handleSetRadiometryBoxes(tok, boxes);
        }
        {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            m_internalRadiometryBoxes = boxes;
        }
        body << "    <tth:SetRadiometryBoxesResponse/>\r\n";
    } else if (isOp(opName, "GetColorPalettes")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        std::vector<ColorPalette> palettes {};
        if (m_thermalHandler) {
            palettes = m_thermalHandler->handleGetColorPalettes(tok);
        } else {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            palettes = m_internalColorPalettes;
        }
        body << "    <tth:GetColorPalettesResponse>\r\n";
        for (const auto& p : palettes) {
            body << "      <tth:Palette token=\"" << p.token << "\" IsDefault=\"" << (p.isDefault ? "true" : "false")
                 << "\">\r\n"
                 << "        <tth:Name>" << p.name << "</tth:Name>\r\n"
                 << "      </tth:Palette>\r\n";
        }
        body << "    </tth:GetColorPalettesResponse>\r\n";
    } else if (isOp(opName, "SetColorPalette")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        const auto palNode = reqNode.select_node(".//*[local-name()='PaletteToken']").node();
        const std::string pal = palNode ? palNode.text().as_string() : "WhiteHot";
        if (m_thermalHandler) {
            m_thermalHandler->handleSetColorPalette(tok, pal);
        }
        {
            std::lock_guard<std::mutex> lock(m_thermalMutex);
            m_internalActiveColorPalette = pal;
        }
        body << "    <tth:SetColorPaletteResponse/>\r\n";
    } else if (isOp(opName, "TriggerNUC") || isOp(opName, "ManualNUC")) {
        const auto tokNode = reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
        const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
        if (m_thermalHandler) {
            m_thermalHandler->handleTriggerNuc(tok);
        }
        body << "    <tth:TriggerNUCResponse/>\r\n";
    } else {
        body << "    <tth:" << opName << "Response/>\r\n";
    }

    sendSoapResponse(res, body.str());
}

} // namespace PelcoD::Onvif
