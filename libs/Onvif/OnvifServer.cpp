/// @file OnvifServer.cpp
/// @brief Core lifecycle and state management implementation for OnvifServer and OnvifServer::Impl.

#include "OnvifServer.h"
#include "OnvifServerImpl.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <atomic>
#include <chrono>
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

#include "Authenticator.h"
#include "CredentialStore.h"
#include "NotificationDispatcher.h"
#include "ScopeStore.h"
#include "SoapRouter.h"
#include "SubscriptionManager.h"
#include "WsDiscoveryCommon.h"
#include "WsDiscoveryServer.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {

    [[nodiscard]] std::string buildNotificationMsg(const OnvifEvent& ev)
    {
        std::ostringstream ss { };
        const std::string op { ev.propertyOperation.empty() ? "Changed" : ev.propertyOperation };
        ss << "      <wsnt:NotificationMessage>\r\n"
           << "        <wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\" "
           << "xmlns:tns1=\"http://www.onvif.org/ver10/topics\">" << Xml::escapeXmlText(ev.topic) << "</wsnt:Topic>\r\n"
           << "        <wsnt:Message PropertyOperation=\"" << Xml::escapeXmlAttr(op) << "\">\r\n"
           << "          <tt:Message UtcTime=\"" << Xml::escapeXmlAttr(ev.utcTime) << "\" "
           << "PropertyOperation=\"" << Xml::escapeXmlAttr(op) << "\">\r\n";

        if (!ev.sourceName.empty()) {
            ss << "            <tt:Source>\r\n"
               << "              <tt:SimpleItem Name=\"" << Xml::escapeXmlAttr(ev.sourceName) << "\" Value=\""
               << Xml::escapeXmlAttr(ev.sourceValue) << "\"/>\r\n"
               << "            </tt:Source>\r\n";
        }

        if (!ev.dataName.empty()) {
            ss << "            <tt:Data>\r\n"
               << "              <tt:SimpleItem Name=\"" << Xml::escapeXmlAttr(ev.dataName) << "\" Value=\""
               << Xml::escapeXmlAttr(ev.dataValue) << "\"/>\r\n"
               << "            </tt:Data>\r\n";
        }

        ss << "          </tt:Message>\r\n"
           << "        </wsnt:Message>\r\n"
           << "      </wsnt:NotificationMessage>\r\n";
        return ss.str();
    }

    [[nodiscard]] std::string buildNotifyEnvelope(const OnvifEvent& ev, const std::string& toUrl)
    {
        const std::string msgUuid { generateRandomUuid() };
        const std::string notifXml { buildNotificationMsg(ev) };

        std::ostringstream ss { };
        ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
           << "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
           << "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
           << "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
           << "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
           << "xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
           << "  <SOAP-ENV:Header>\r\n"
           << "    <wsa:Action>http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify</wsa:Action>\r\n"
           << "    <wsa:MessageID>urn:uuid:" << Xml::escapeXmlText(msgUuid) << "</wsa:MessageID>\r\n"
           << "    <wsa:To>" << Xml::escapeXmlText(toUrl) << "</wsa:To>\r\n"
           << "  </SOAP-ENV:Header>\r\n"
           << "  <SOAP-ENV:Body>\r\n"
           << "    <wsnt:Notify>\r\n"
           << notifXml << "    </wsnt:Notify>\r\n"
           << "  </SOAP-ENV:Body>\r\n"
           << "</SOAP-ENV:Envelope>\r\n";
        return ss.str();
    }

    [[nodiscard]] bool isLoopbackHost(const std::string& host) noexcept
    {
        return (host.rfind("127.", 0U) == 0U) || (host == "localhost") || (host == "::1") || (host == "[::1]");
    }

    std::string formatIso8601Utc(const std::chrono::system_clock::time_point& tp)
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

} // namespace

// =============================================================================
// OnvifServer (Public Interface Forwarding to Impl)
// =============================================================================

OnvifServer::OnvifServer(OnvifServerConfig config, std::shared_ptr<IPtzHandler> ptzHandler,
    std::shared_ptr<IImagingHandler> imagingHandler, std::shared_ptr<IOsdHandler> osdHandler,
    std::shared_ptr<IDeviceManagementHandler> deviceHandler, std::shared_ptr<IDeviceIoHandler> deviceIoHandler,
    std::shared_ptr<IMetadataHandler> metadataHandler, std::shared_ptr<IAnalyticsHandler> analyticsHandler)
    : m_impl(std::make_unique<Impl>(std::move(config), std::move(ptzHandler), std::move(imagingHandler),
          std::move(osdHandler), std::move(deviceHandler), std::move(deviceIoHandler), std::move(metadataHandler),
          std::move(analyticsHandler)))
{
}

OnvifServer::~OnvifServer() = default;

bool OnvifServer::start()
{
    return m_impl->start();
}
void OnvifServer::stop()
{
    m_impl->stop();
}
bool OnvifServer::isRunning() const noexcept
{
    return m_impl->isRunning();
}
ProvisioningState OnvifServer::provisioningState() const noexcept
{
    return m_impl->provisioningState();
}
DiscoveryMode OnvifServer::getDiscoveryMode() const noexcept
{
    return m_impl->getDiscoveryMode();
}
void OnvifServer::setDiscoveryMode(DiscoveryMode mode)
{
    m_impl->setDiscoveryMode(mode);
}
std::vector<NetworkProtocolConfig> OnvifServer::getNetworkProtocols() const
{
    return m_impl->getNetworkProtocols();
}
bool OnvifServer::setNetworkProtocols(const std::vector<NetworkProtocolConfig>& protocols)
{
    return m_impl->setNetworkProtocols(protocols);
}
OnvifServerConfig OnvifServer::getConfig() const
{
    return m_impl->getConfig();
}
std::shared_ptr<ScopeStore> OnvifServer::getScopeStore() const noexcept
{
    return m_impl->getScopeStore();
}
const SoapRouter& OnvifServer::getSoapRouter() const noexcept
{
    return m_impl->getSoapRouter();
}

void OnvifServer::setPtzHandler(std::shared_ptr<IPtzHandler> h)
{
    m_impl->setPtzHandler(std::move(h));
}
std::shared_ptr<IPtzHandler> OnvifServer::getPtzHandler() const
{
    return m_impl->getPtzHandler();
}
void OnvifServer::setImagingHandler(std::shared_ptr<IImagingHandler> h)
{
    m_impl->setImagingHandler(std::move(h));
}
std::shared_ptr<IImagingHandler> OnvifServer::getImagingHandler() const
{
    return m_impl->getImagingHandler();
}
void OnvifServer::setOsdHandler(std::shared_ptr<IOsdHandler> h)
{
    m_impl->setOsdHandler(std::move(h));
}
std::shared_ptr<IOsdHandler> OnvifServer::getOsdHandler() const
{
    return m_impl->getOsdHandler();
}
void OnvifServer::setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> h)
{
    m_impl->setDeviceManagementHandler(std::move(h));
}
std::shared_ptr<IDeviceManagementHandler> OnvifServer::getDeviceManagementHandler() const
{
    return m_impl->getDeviceManagementHandler();
}
void OnvifServer::setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> h)
{
    m_impl->setDeviceIoHandler(std::move(h));
}
std::shared_ptr<IDeviceIoHandler> OnvifServer::getDeviceIoHandler() const
{
    return m_impl->getDeviceIoHandler();
}
void OnvifServer::setMetadataHandler(std::shared_ptr<IMetadataHandler> h)
{
    m_impl->setMetadataHandler(std::move(h));
}
std::shared_ptr<IMetadataHandler> OnvifServer::getMetadataHandler() const
{
    return m_impl->getMetadataHandler();
}
void OnvifServer::setRecordingHandler(std::shared_ptr<IRecordingHandler> h)
{
    m_impl->setRecordingHandler(std::move(h));
}
std::shared_ptr<IRecordingHandler> OnvifServer::getRecordingHandler() const
{
    return m_impl->getRecordingHandler();
}
void OnvifServer::setSearchHandler(std::shared_ptr<ISearchHandler> h)
{
    m_impl->setSearchHandler(std::move(h));
}
std::shared_ptr<ISearchHandler> OnvifServer::getSearchHandler() const
{
    return m_impl->getSearchHandler();
}
void OnvifServer::setReplayHandler(std::shared_ptr<IReplayHandler> h)
{
    m_impl->setReplayHandler(std::move(h));
}
std::shared_ptr<IReplayHandler> OnvifServer::getReplayHandler() const
{
    return m_impl->getReplayHandler();
}
void OnvifServer::setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> h)
{
    m_impl->setAnalyticsHandler(std::move(h));
}
std::shared_ptr<IAnalyticsHandler> OnvifServer::getAnalyticsHandler() const
{
    return m_impl->getAnalyticsHandler();
}
void OnvifServer::setMaskHandler(std::shared_ptr<IMaskHandler> h)
{
    m_impl->setMaskHandler(std::move(h));
}
std::shared_ptr<IMaskHandler> OnvifServer::getMaskHandler() const
{
    return m_impl->getMaskHandler();
}
void OnvifServer::setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> h)
{
    m_impl->setVideoSourceModeHandler(std::move(h));
}
std::shared_ptr<IVideoSourceModeHandler> OnvifServer::getVideoSourceModeHandler() const
{
    return m_impl->getVideoSourceModeHandler();
}
void OnvifServer::setThermalHandler(std::shared_ptr<IThermalHandler> h)
{
    m_impl->setThermalHandler(std::move(h));
}
std::shared_ptr<IThermalHandler> OnvifServer::getThermalHandler() const
{
    return m_impl->getThermalHandler();
}

void OnvifServer::logSystemMessage(const std::string& level, const std::string& msg)
{
    m_impl->logSystemMessage(level, msg);
}
void OnvifServer::publishEvent(const OnvifEvent& event)
{
    m_impl->publishEvent(event);
}
void OnvifServer::setRequestLogCallback(RequestLogCallback callback)
{
    m_impl->setRequestLogCallback(std::move(callback));
}
void OnvifServer::setSnapshotProvider(SnapshotProvider provider)
{
    m_impl->setSnapshotProvider(std::move(provider));
}

// =============================================================================
// OnvifServer::Impl Implementation
// =============================================================================

OnvifServer::Impl::Impl(OnvifServerConfig configIn, std::shared_ptr<IPtzHandler> ptzIn,
    std::shared_ptr<IImagingHandler> imagingIn, std::shared_ptr<IOsdHandler> osdIn,
    std::shared_ptr<IDeviceManagementHandler> deviceIn, std::shared_ptr<IDeviceIoHandler> deviceIoIn,
    std::shared_ptr<IMetadataHandler> metadataIn, std::shared_ptr<IAnalyticsHandler> analyticsIn)
    : config(std::move(configIn))
    , ptzHandler(std::move(ptzIn))
    , imagingHandler(std::move(imagingIn))
    , osdHandler(std::move(osdIn))
    , deviceHandler(std::move(deviceIn))
    , deviceIoHandler(std::move(deviceIoIn))
    , metadataHandler(std::move(metadataIn))
    , analyticsHandler(std::move(analyticsIn))
    , discoveryMode(config.discoveryMode)
{
    credStore = std::make_shared<CredentialStore>(config.defaultUsers);
    authenticator = std::make_unique<Authenticator>(credStore, config.auth);
    if ((credStore->adminCount() > 0U) && !credStore->hasDefaultPassword()) {
        provState = ProvisioningState::Provisioned;
    } else {
        provState = ProvisioningState::Unprovisioned;
    }
    internalNetworkInterfaces = config.defaultNetworkInterfaces;
    internalNetworkProtocols = config.defaultNetworkProtocols;
    if (internalNetworkProtocols.empty()) {
        internalNetworkProtocols = { { "HTTP", true, config.port }, { "HTTPS", false, 8443 }, { "RTSP", true, 8554 } };
    } else {
        for (auto& proto : internalNetworkProtocols) {
            if (proto.name == "HTTP" && config.port != 8080 && proto.port == 8080) {
                proto.port = config.port;
            }
        }
    }
    internalGateway = config.defaultGateway;
    internalDns = config.defaultDns;
    internalNtp = config.defaultNtp;
    internalHostname = config.hostname;
    internalRelayOutputs = config.defaultRelayOutputs;
    internalDigitalInputs = config.defaultDigitalInputs;

    subManager = std::make_shared<SubscriptionManager>(config.notification);
    dispatcher = std::make_shared<NotificationDispatcher>(config.notification);
    scopeStore = std::make_shared<ScopeStore>(config.scopes);
    discoveryServer = std::make_unique<WsDiscoveryServer>(config, scopeStore);

    setupRoutes();
}

OnvifServer::Impl::~Impl()
{
    stop();
}

bool OnvifServer::Impl::start()
{
    if (running) {
        return true;
    }

    const std::string host { config.bindAddress.empty() ? std::string { "0.0.0.0" } : config.bindAddress };
    const bool loopback { isLoopbackHost(host) };
    if (!config.auth.enabled && !loopback) {
        logSystemMessage("ERROR", "Refusing to start: authentication disabled on non-loopback address " + host);
        return false;
    }
    if (config.auth.enabled && !loopback && !config.auth.allowDefaultPassword && credStore->hasDefaultPassword()) {
        logSystemMessage("ERROR", "Refusing to start: factory-default ONVIF password configured on " + host);
        return false;
    }

    httpServer.set_socket_options([](socket_t sock) {
        const int opt { 1 };
#ifdef _WIN32
        static_cast<void>(::setsockopt(
            sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), static_cast<int>(sizeof(opt))));
#else
        static_cast<void>(::setsockopt(
            sock, SOL_SOCKET, SO_REUSEADDR, static_cast<const void*>(&opt), static_cast<socklen_t>(sizeof(opt))));
#endif
    });
    if (!httpServer.bind_to_port(host.c_str(), config.port)) {
        logSystemMessage("ERROR", "Failed to bind ONVIF HTTP server to " + host + ":" + std::to_string(config.port));
        return false;
    }

    const auto listenExited { std::make_shared<std::atomic<bool>>(false) };
    httpThread = std::thread([this, listenExited]() {
        const bool res { httpServer.listen_after_bind() };
        if (!res) {
            logSystemMessage("ERROR", "ONVIF HTTP server listen_after_bind failed");
        }
        listenExited->store(true, std::memory_order_release);
    });

    constexpr auto maxWait { std::chrono::milliseconds(1000) };
    const auto startWait { std::chrono::steady_clock::now() };
    bool listening { false };
    while (!listenExited->load(std::memory_order_acquire)) {
        if (httpServer.is_running()) {
            listening = true;
            break;
        }
        if ((std::chrono::steady_clock::now() - startWait) > maxWait) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!listening) {
        logSystemMessage("ERROR", "Failed to start listening on " + host + ":" + std::to_string(config.port));
        httpServer.stop();
        if (httpThread.joinable()) {
            httpThread.join();
        }
        running = false;
        return false;
    }

    running = true;

    if (dispatcher) {
        dispatcher->start();
    }

    if (discoveryServer) {
        if (!discoveryServer->start()) {
            logSystemMessage("WARNING", "WS-Discovery server failed to start on UDP port 3702");
        }
    }

    logSystemMessage("INFO", "ONVIF HTTP server started successfully on " + host + ":" + std::to_string(config.port));
    return true;
}

void OnvifServer::Impl::stop()
{
    if (!running) {
        if (httpThread.joinable()) {
            httpServer.stop();
            httpThread.join();
        }
        return;
    }

    if (discoveryServer) {
        discoveryServer->stop();
    }

    httpServer.stop();
    running = false;

    if (dispatcher) {
        dispatcher->stop();
    }

    if (subManager) {
        subManager->stop();
    }

    if (httpThread.joinable()) {
        httpThread.join();
    }
}

bool OnvifServer::Impl::isRunning() const noexcept
{
    return running;
}

OnvifServerConfig OnvifServer::Impl::getConfig() const
{
    std::scoped_lock lock(deviceMutex);
    auto cfg = config;
    if (scopeStore) {
        cfg.scopes = scopeStore->getScopes();
    }
    return cfg;
}

std::shared_ptr<ScopeStore> OnvifServer::Impl::getScopeStore() const noexcept
{
    return scopeStore;
}

const SoapRouter& OnvifServer::Impl::getSoapRouter() const noexcept
{
    return soapRouter;
}

ProvisioningState OnvifServer::Impl::provisioningState() const noexcept
{
    return provState.load();
}

DiscoveryMode OnvifServer::Impl::getDiscoveryMode() const noexcept
{
    return discoveryMode.load();
}

void OnvifServer::Impl::setDiscoveryMode(DiscoveryMode mode)
{
    discoveryMode.store(mode);
    if (discoveryServer) {
        discoveryServer->setDiscoveryMode(mode);
    }
}

std::vector<NetworkProtocolConfig> OnvifServer::Impl::getNetworkProtocols() const
{
    const auto devHandler = getDeviceManagementHandler();
    if (devHandler) {
        auto protocols = devHandler->handleGetNetworkProtocols();
        if (!protocols.empty()) {
            return protocols;
        }
    }
    std::scoped_lock lock(deviceMutex);
    return internalNetworkProtocols;
}

bool OnvifServer::Impl::setNetworkProtocols(const std::vector<NetworkProtocolConfig>& protocols)
{
    for (const auto& proto : protocols) {
        if (proto.port < 1 || proto.port > 65535) {
            return false;
        }
    }
    const auto devHandler = getDeviceManagementHandler();
    if (devHandler && !devHandler->handleSetNetworkProtocols(protocols)) {
        return false;
    }
    std::scoped_lock lock(deviceMutex);
    for (const auto& u : protocols) {
        auto it = std::find_if(internalNetworkProtocols.begin(), internalNetworkProtocols.end(),
            [&u](const NetworkProtocolConfig& existing) { return existing.name == u.name; });
        if (it != internalNetworkProtocols.end()) {
            it->enabled = u.enabled;
            it->port = u.port;
        } else {
            internalNetworkProtocols.push_back(u);
        }
    }
    return true;
}

void OnvifServer::Impl::setPtzHandler(std::shared_ptr<IPtzHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    ptzHandler = std::move(h);
}
std::shared_ptr<IPtzHandler> OnvifServer::Impl::getPtzHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return ptzHandler;
}
void OnvifServer::Impl::setImagingHandler(std::shared_ptr<IImagingHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    imagingHandler = std::move(h);
}
std::shared_ptr<IImagingHandler> OnvifServer::Impl::getImagingHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return imagingHandler;
}
void OnvifServer::Impl::setOsdHandler(std::shared_ptr<IOsdHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    osdHandler = std::move(h);
}
std::shared_ptr<IOsdHandler> OnvifServer::Impl::getOsdHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return osdHandler;
}
void OnvifServer::Impl::setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    deviceHandler = std::move(h);
}
std::shared_ptr<IDeviceManagementHandler> OnvifServer::Impl::getDeviceManagementHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return deviceHandler;
}
void OnvifServer::Impl::setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    deviceIoHandler = std::move(h);
}
std::shared_ptr<IDeviceIoHandler> OnvifServer::Impl::getDeviceIoHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return deviceIoHandler;
}
void OnvifServer::Impl::setMetadataHandler(std::shared_ptr<IMetadataHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    metadataHandler = std::move(h);
}
std::shared_ptr<IMetadataHandler> OnvifServer::Impl::getMetadataHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return metadataHandler;
}
void OnvifServer::Impl::setRecordingHandler(std::shared_ptr<IRecordingHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    recordingHandler = std::move(h);
}
std::shared_ptr<IRecordingHandler> OnvifServer::Impl::getRecordingHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return recordingHandler;
}
void OnvifServer::Impl::setSearchHandler(std::shared_ptr<ISearchHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    searchHandler = std::move(h);
}
std::shared_ptr<ISearchHandler> OnvifServer::Impl::getSearchHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return searchHandler;
}
void OnvifServer::Impl::setReplayHandler(std::shared_ptr<IReplayHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    replayHandler = std::move(h);
}
std::shared_ptr<IReplayHandler> OnvifServer::Impl::getReplayHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return replayHandler;
}
void OnvifServer::Impl::setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    analyticsHandler = std::move(h);
}
std::shared_ptr<IAnalyticsHandler> OnvifServer::Impl::getAnalyticsHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return analyticsHandler;
}
void OnvifServer::Impl::setMaskHandler(std::shared_ptr<IMaskHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    maskHandler = std::move(h);
}
std::shared_ptr<IMaskHandler> OnvifServer::Impl::getMaskHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return maskHandler;
}
void OnvifServer::Impl::setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    videoSourceModeHandler = std::move(h);
}
std::shared_ptr<IVideoSourceModeHandler> OnvifServer::Impl::getVideoSourceModeHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return videoSourceModeHandler;
}
void OnvifServer::Impl::setThermalHandler(std::shared_ptr<IThermalHandler> h)
{
    std::scoped_lock lock(handlerMutex);
    thermalHandler = std::move(h);
}
std::shared_ptr<IThermalHandler> OnvifServer::Impl::getThermalHandler() const
{
    std::scoped_lock lock(handlerMutex);
    return thermalHandler;
}

void OnvifServer::Impl::logSystemMessage(const std::string& level, const std::string& msg)
{
    std::scoped_lock lock(logMutex);
    const std::string timestamp = formatIso8601Utc(std::chrono::system_clock::now());
    std::ostringstream ss;
    ss << "[" << timestamp << "] [" << level << "] " << msg;
    systemLogs.push_back(ss.str());
    if (systemLogs.size() > 500) {
        systemLogs.pop_front();
    }
}

void OnvifServer::Impl::publishEvent(const OnvifEvent& event)
{
    OnvifEvent ev = event;
    if (ev.utcTime.empty()) {
        ev.utcTime = formatIso8601Utc(std::chrono::system_clock::now());
    }
    if (ev.propertyOperation.empty()) {
        ev.propertyOperation = "Changed";
    }

    std::vector<std::string> pushUrls { };
    if (subManager) {
        subManager->publishEvent(ev, pushUrls);
    }

    if (dispatcher && !pushUrls.empty()) {
        for (auto& url : pushUrls) {
            std::string payload { buildNotifyEnvelope(ev, url) };
            dispatcher->enqueue(std::move(url), std::move(payload));
        }
    }
}

void OnvifServer::Impl::setRequestLogCallback(RequestLogCallback callback)
{
    logCallback = std::move(callback);
}

void OnvifServer::Impl::setSnapshotProvider(OnvifServer::SnapshotProvider provider)
{
    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshotProvider = std::move(provider);
}

} // namespace Onvif
