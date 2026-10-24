/// @file OnvifServerRoutes.cpp
/// @brief HTTP and SOAP endpoint routing and authorization pipeline for OnvifServer::Impl.

#include "OnvifServer.h"
#include "OnvifServerImpl.h"
#include "services/IService.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "Authenticator.h"
#include "CredentialStore.h"
#include "JpegGenerator.h"
#include "NotificationDispatcher.h"
#include "ScopeStore.h"
#include "SoapFault.h"
#include "SoapRouter.h"
#include "SubscriptionManager.h"
#include "UsernameToken.h"
#include "XmlUtils.h"

#include "services/AnalyticsService.h"
#include "services/DeviceIoService.h"
#include "services/DeviceService.h"
#include "services/EventService.h"
#include "services/ImagingService.h"
#include "services/MediaService.h"
#include "services/PtzService.h"
#include "services/RecordingService.h"
#include "services/ThermalService.h"

namespace Onvif {

namespace {

    std::string formatUtc(const std::chrono::system_clock::time_point& tp)
    {
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tmUtc { };
#ifdef _WIN32
        gmtime_s(&tmUtc, &t);
#else
        gmtime_r(&t, &tmUtc);
#endif
        char buf[32] { 0 };
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
        return std::string(buf);
    }

    [[nodiscard]] bool isNsAllowed(std::string_view svc, std::string_view ns) noexcept
    {
        if (svc == "Device") {
            return ns == "http://www.onvif.org/ver10/device/wsdl";
        }
        if (svc == "Media") {
            return ns == "http://www.onvif.org/ver10/media/wsdl";
        }
        if (svc == "Media2") {
            return ns == "http://www.onvif.org/ver20/media/wsdl";
        }
        if (svc == "PTZ") {
            return ns == "http://www.onvif.org/ver20/ptz/wsdl" || ns == "http://www.onvif.org/ver10/ptz/wsdl";
        }
        if (svc == "Imaging") {
            return ns == "http://www.onvif.org/ver20/imaging/wsdl";
        }
        if (svc == "Event") {
            return ns == "http://www.onvif.org/ver10/events/wsdl" || ns == "http://docs.oasis-open.org/wsn/b-2";
        }
        if (svc == "Subscription") {
            return ns == "http://docs.oasis-open.org/wsn/b-2" || ns == "http://www.onvif.org/ver10/events/wsdl";
        }
        if (svc == "Analytics") {
            return ns == "http://www.onvif.org/ver20/analytics/wsdl";
        }
        if (svc == "DeviceIO") {
            return ns == "http://www.onvif.org/ver10/deviceIO/wsdl";
        }
        if (svc == "Recording") {
            return ns == "http://www.onvif.org/ver10/recording/wsdl";
        }
        if (svc == "Search") {
            return ns == "http://www.onvif.org/ver10/search/wsdl";
        }
        if (svc == "Replay") {
            return ns == "http://www.onvif.org/ver10/replay/wsdl";
        }
        if (svc == "Thermal") {
            return ns == "http://www.onvif.org/ver10/thermal/wsdl";
        }
        return true;
    }

} // namespace

void OnvifServer::Impl::setupRoutes()
{
    // 1. DeviceService
    DeviceServiceContext devCtx { };
    devCtx.config = config;
    devCtx.scopeStore = scopeStore;
    devCtx.credStore = credStore;
    devCtx.getDeviceHandler = [this]() { return getDeviceManagementHandler(); };
    devCtx.getDiscoveryMode = [this]() { return getDiscoveryMode(); };
    devCtx.setDiscoveryMode = [this](DiscoveryMode m) { setDiscoveryMode(m); };
    devCtx.setProvisioningState = [this](ProvisioningState s) { provState.store(s); };
    devCtx.resolveHost = [this](const SoapContext& ctx) { return ctx.host.empty() ? config.bindAddress : ctx.host; };
    devCtx.logMessage = [this](const std::string& lvl, const std::string& msg) { logSystemMessage(lvl, msg); };
    devCtx.getNetProtocols = [this]() { return getNetworkProtocols(); };
    devCtx.setNetProtocols = [this](const std::vector<NetworkProtocolConfig>& p) { return setNetworkProtocols(p); };
    devCtx.getSystemLog = [this](SystemLogType type) {
        std::scoped_lock lock(logMutex);
        std::ostringstream ss;
        const auto& logs = (type == SystemLogType::Access) ? accessLogs : systemLogs;
        for (const auto& line : logs) {
            ss << line << "\n";
        }
        return ss.str();
    };
    devCtx.getRelayOutputCount = [this]() {
        const auto devIo = getDeviceIoHandler();
        if (devIo) {
            return devIo->handleGetRelayOutputs().size();
        }
        std::scoped_lock lock(deviceIoMutex);
        return internalRelayOutputs.size();
    };
    devCtx.getVideoSourceModeHandler = [this]() { return getVideoSourceModeHandler(); };
    devCtx.isProfileGSupported = [this]() {
        return getRecordingHandler() != nullptr || getSearchHandler() != nullptr || getReplayHandler() != nullptr;
    };
    devCtx.isThermalSupported = [this]() { return getThermalHandler() != nullptr; };

    services.push_back(std::make_unique<DeviceService>(std::move(devCtx)));

    // 2. MediaService
    MediaServiceContext mediaCtx { };
    mediaCtx.config = config;
    mediaCtx.getOsdHandler = [this]() { return getOsdHandler(); };
    mediaCtx.getMaskHandler = [this]() { return getMaskHandler(); };
    mediaCtx.getVideoSourceModeHandler = [this]() { return getVideoSourceModeHandler(); };
    mediaCtx.getMetadataHandler = [this]() { return getMetadataHandler(); };
    mediaCtx.resolveHost = [this](const SoapContext& ctx) { return ctx.host.empty() ? config.bindAddress : ctx.host; };
    services.push_back(std::make_unique<MediaService>(std::move(mediaCtx)));

    // 3. PtzService
    PtzServiceContext ptzCtx { };
    ptzCtx.getPtzHandler = [this]() { return getPtzHandler(); };
    services.push_back(std::make_unique<PtzService>(std::move(ptzCtx)));

    // 4. ImagingService
    ImagingServiceContext imgCtx { };
    imgCtx.getImagingHandler = [this]() { return getImagingHandler(); };
    services.push_back(std::make_unique<ImagingService>(std::move(imgCtx)));

    // 5. EventService
    EventServiceContext evCtx { };
    evCtx.config = config;
    evCtx.subManager = subManager;
    evCtx.dispatcher = dispatcher;
    evCtx.runningFlag = &running;
    evCtx.resolveHost = [this](const SoapContext& ctx) { return ctx.host.empty() ? config.bindAddress : ctx.host; };
    services.push_back(std::make_unique<EventService>(std::move(evCtx)));

    // 6. AnalyticsService
    AnalyticsServiceContext anCtx { };
    anCtx.getAnalyticsHandler = [this]() { return getAnalyticsHandler(); };
    services.push_back(std::make_unique<AnalyticsService>(std::move(anCtx)));

    // 7. DeviceIoService
    DeviceIoServiceContext ioCtx { };
    ioCtx.getDeviceIoHandler = [this]() { return getDeviceIoHandler(); };
    ioCtx.defaultRelays = internalRelayOutputs;
    ioCtx.defaultInputs = internalDigitalInputs;
    services.push_back(std::make_unique<DeviceIoService>(std::move(ioCtx)));

    // 8. ThermalService
    ThermalServiceContext thCtx { };
    thCtx.getThermalHandler = [this]() { return getThermalHandler(); };
    services.push_back(std::make_unique<ThermalService>(std::move(thCtx)));

    // 9. RecordingService
    RecordingServiceContext recCtx { };
    recCtx.getRecordingHandler = [this]() { return getRecordingHandler(); };
    recCtx.getSearchHandler = [this]() { return getSearchHandler(); };
    recCtx.getReplayHandler = [this]() { return getReplayHandler(); };
    recCtx.resolveHost = [this](const SoapContext& ctx) { return ctx.host.empty() ? config.bindAddress : ctx.host; };
    services.push_back(std::make_unique<RecordingService>(std::move(recCtx)));

    for (const auto& svc : services) {
        svc->registerRoutes(soapRouter);
    }

    auto bindPost = [this](const char* pattern, std::string_view svc, std::string_view ns) {
        httpServer.Post(pattern,
            [this, svc, ns](const httplib::Request& q, httplib::Response& s) { handleSoapEndpoint(q, s, svc, ns); });
    };
    bindPost("/onvif/device_service", "Device", "http://www.onvif.org/ver10/device/wsdl");
    bindPost("/onvif/media_service", "Media", "http://www.onvif.org/ver10/media/wsdl");
    bindPost("/onvif/media2_service", "Media2", "http://www.onvif.org/ver20/media/wsdl");
    bindPost("/onvif/ptz_service", "PTZ", "http://www.onvif.org/ver20/ptz/wsdl");
    bindPost("/onvif/imaging_service", "Imaging", "http://www.onvif.org/ver20/imaging/wsdl");
    bindPost("/onvif/event_service", "Event", "http://www.onvif.org/ver10/events/wsdl");
    bindPost("/onvif/events/subscription", "Subscription", "http://docs.oasis-open.org/wsn/b-2");
    bindPost(R"(/onvif/events/subscription/(.+))", "Subscription", "http://docs.oasis-open.org/wsn/b-2");
    bindPost("/onvif/analytics_service", "Analytics", "http://www.onvif.org/ver20/analytics/wsdl");
    bindPost("/onvif/deviceio_service", "DeviceIO", "http://www.onvif.org/ver10/deviceIO/wsdl");
    bindPost("/onvif/recording_service", "Recording", "http://www.onvif.org/ver10/recording/wsdl");
    bindPost("/onvif/search_service", "Search", "http://www.onvif.org/ver10/search/wsdl");
    bindPost("/onvif/replay_service", "Replay", "http://www.onvif.org/ver10/replay/wsdl");
    bindPost("/onvif/thermal_service", "Thermal", "http://www.onvif.org/ver10/thermal/wsdl");

    httpServer.Get("/onvif/metadata_stream",
        [this](const httplib::Request& req, httplib::Response& res) { handleMetadataStream(req, res); });

    httpServer.Get("/onvif/snapshot", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authorizeHttp(req, res, "Snapshot")) {
            return;
        }

        std::vector<std::uint8_t> jpegData { };
        OnvifServer::SnapshotProvider provider { };
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            provider = snapshotProvider;
        }

        if (provider) {
            jpegData = provider();
        }

        if (jpegData.empty()) {
            jpegData = JpegGenerator::createTestFrame(640, 480);
        }

        res.status = 200;
        res.set_content(reinterpret_cast<const char*>(jpegData.data()), jpegData.size(), "image/jpeg");
    });
}

void OnvifServer::Impl::handleSoapEndpoint(
    const httplib::Request& req, httplib::Response& res, std::string_view serviceName, std::string_view fallbackNs)
{
    pugi::xml_document doc;
    const auto soap = parseSoapRequest(req, res, serviceName, doc);
    if (!soap) {
        return;
    }
    const auto& [bodyNode, reqNode, opName] = *soap;
    appendAccessLog(serviceName, opName, req.remote_addr);

    const auto key = soapRouter.extractOpKey(reqNode, fallbackNs);
    if (!isNsAllowed(serviceName, key.namespaceUri)) {
        sendSoapResponse(res, SoapFault::actionNotSupported("Action Not Supported: " + key.localName), 500);
        return;
    }

    SoapContext ctx { };
    ctx.reqNode = reqNode;
    ctx.principal = lastAuthPrincipal;
    ctx.peerAddress = req.remote_addr;
    ctx.requestUrl = req.path;
    ctx.serviceName = std::string(serviceName);
    ctx.host = resolveHost(req);
    ctx.isUnprovisioned = (provState.load() == ProvisioningState::Unprovisioned);

    const auto result = soapRouter.dispatch(reqNode, ctx, fallbackNs);
    sendSoapResponse(res, result.responseXml, result.httpStatusCode);
}

void OnvifServer::Impl::handleMetadataStream(const httplib::Request& req, httplib::Response& res)
{
    if (logCallback) {
        logCallback("MetadataStream", "GetStream", req.remote_addr);
    }
    if (!authorizeHttp(req, res, "MetadataStream")) {
        return;
    }
    MetadataStreamPayload payload;
    const auto metadataHdlr = getMetadataHandler();
    const auto ptzHdlr = getPtzHandler();
    if (metadataHdlr) {
        payload = metadataHdlr->handleGetCurrentMetadata("");
    }
    if (!payload.ptzStatus.has_value() && ptzHdlr) {
        payload.ptzStatus = ptzHdlr->handleGetStatus();
    }
    const std::string xml = generateMetadataStreamXml(payload);
    res.status = 200;
    res.set_content(xml, "application/xml; charset=utf-8");
}

std::string OnvifServer::Impl::generateMetadataStreamXml(const MetadataStreamPayload& payload) const
{
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<tt:MetadataStream "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\" xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n";
    if (payload.ptzStatus.has_value()) {
        const auto& ptz = payload.ptzStatus.value();
        ss << "  <tt:PTZStatus>\r\n    <tt:Position>\r\n"
           << "      <tt:PanTilt x=\"" << std::fixed << std::setprecision(4) << ptz.pan << "\" y=\"" << ptz.tilt
           << "\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\"/>\r\n"
           << "      <tt:Zoom x=\"" << std::fixed << std::setprecision(4) << ptz.zoom
           << "\" space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace\"/>\r\n"
           << "    </tt:Position>\r\n    <tt:MoveStatus>\r\n"
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
            = frame.utcTime.empty() ? formatUtc(std::chrono::system_clock::now()) : frame.utcTime;
        ss << "  <tt:VideoAnalytics>\r\n    <tt:Frame UtcTime=\"" << frameTime << "\">\r\n"
           << "      <tt:Transformation><tt:Translate x=\"0.00\" y=\"0.00\"/><tt:Scale x=\"1.00\" "
              "y=\"1.00\"/></tt:Transformation>\r\n";
        for (const auto& obj : frame.objects) {
            ss << "      <tt:Object ObjectId=\"" << obj.objectId << "\">\r\n        <tt:Appearance>\r\n"
               << "          <tt:Shape><tt:BoundingBox left=\"" << std::fixed << std::setprecision(4)
               << obj.boundingBox.left << "\" top=\"" << obj.boundingBox.top << "\" right=\"" << obj.boundingBox.right
               << "\" bottom=\"" << obj.boundingBox.bottom << "\"/></tt:Shape>\r\n"
               << "          <tt:Class><tt:ClassCandidate><tt:Type>" << obj.className << "</tt:Type><tt:Likelihood>"
               << std::fixed << std::setprecision(2) << obj.confidence
               << "</tt:Likelihood></tt:ClassCandidate></tt:Class>\r\n";
            if (std::abs(obj.geoLocation.latitude) > 1e-7 || std::abs(obj.geoLocation.longitude) > 1e-7) {
                ss << "          <tt:GeoLocation lat=\"" << std::fixed << std::setprecision(6)
                   << obj.geoLocation.latitude << "\" lon=\"" << obj.geoLocation.longitude << "\" elevation=\""
                   << std::fixed << std::setprecision(2) << obj.geoLocation.elevation << "\"/>\r\n";
            }
            ss << "        </tt:Appearance>\r\n      </tt:Object>\r\n";
        }
        ss << "    </tt:Frame>\r\n  </tt:VideoAnalytics>\r\n";
    }

    for (const auto& ev : payload.events) {
        ss << "  <wsnt:NotificationMessage>\r\n    <wsnt:Topic "
              "Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
           << ev.topic << "</wsnt:Topic>\r\n"
           << "    <wsnt:Message>\r\n      <tt:Message UtcTime=\"" << ev.utcTime
           << "\" PropertyOperation=\"Initialized\">\r\n";
        if (!ev.sourceName.empty()) {
            ss << "        <tt:Source><tt:SimpleItem Name=\"" << ev.sourceName << "\" Value=\"" << ev.sourceValue
               << "\"/></tt:Source>\r\n";
        }
        if (!ev.dataName.empty()) {
            ss << "        <tt:Data><tt:SimpleItem Name=\"" << ev.dataName << "\" Value=\"" << ev.dataValue
               << "\"/></tt:Data>\r\n";
        }
        ss << "      </tt:Message>\r\n    </wsnt:Message>\r\n  </wsnt:NotificationMessage>\r\n";
    }

    ss << "</tt:MetadataStream>\r\n";
    return ss.str();
}

std::optional<OnvifServer::Impl::SoapRequest> OnvifServer::Impl::parseSoapRequest(
    const httplib::Request& req, httplib::Response& res, std::string_view serviceName, pugi::xml_document& doc)
{
    if (!doc.load_string(req.body.c_str())) {
        res.status = 400;
        return std::nullopt;
    }

    const pugi::xml_node bodyNode = doc.select_node("//*[local-name()='Body']").node();
    if (!bodyNode) {
        sendSoapResponse(res, SoapFault::sender("ter:InvalidArgVal", "", "Missing SOAP Body element"), 400);
        return std::nullopt;
    }

    pugi::xml_node reqNode;
    for (pugi::xml_node child = bodyNode.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element) {
            reqNode = child;
            break;
        }
    }
    if (!reqNode) {
        sendSoapResponse(res, SoapFault::sender("ter:InvalidArgVal", "", "Empty SOAP Body element"), 400);
        return std::nullopt;
    }

    const std::string opName = reqNode.name();
    if (logCallback) {
        logCallback(std::string(serviceName), opName, req.remote_addr);
    }

    if (!authorize(req, res, serviceName, opName, doc)) {
        return std::nullopt;
    }

    return SoapRequest { bodyNode, reqNode, opName };
}

std::string OnvifServer::Impl::resolveHost(const httplib::Request& req) const
{
    if (config.bindAddress != "0.0.0.0" && config.bindAddress != "::" && !config.bindAddress.empty()) {
        return config.bindAddress;
    }

    if (req.has_header("Host")) {
        const std::string hostHeader = req.get_header_value("Host");
        std::string hostCandidate;

        if (!hostHeader.empty() && hostHeader.front() == '[') {
            const auto closeBracket = hostHeader.find(']');
            if (closeBracket != std::string::npos) {
                hostCandidate = hostHeader.substr(0, closeBracket + 1);
            }
        } else {
            const auto colonPos = hostHeader.rfind(':');
            if (colonPos != std::string::npos) {
                hostCandidate = hostHeader.substr(0, colonPos);
            } else {
                hostCandidate = hostHeader;
            }
        }

        const bool hasInvalidChars
            = hostCandidate.empty() || (hostCandidate.find_first_of("<>\"'& \r\n\t") != std::string::npos);
        if (!hasInvalidChars) {
            return hostCandidate;
        }
    }

    return "127.0.0.1";
}

} // namespace Onvif
