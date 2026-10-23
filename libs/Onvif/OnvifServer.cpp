/// @file OnvifServer.cpp
/// @brief Core lifecycle and state management implementation for OnvifServer.

#include "OnvifServer.h"

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
    , m_discoveryMode(m_config.discoveryMode)
{
    m_credStore = std::make_shared<CredentialStore>(m_config.defaultUsers);
    m_authenticator = std::make_unique<Authenticator>(m_credStore, m_config.auth);
    if ((m_credStore->adminCount() > 0U) && !m_credStore->hasDefaultPassword()) {
        m_provisioningState = ProvisioningState::Provisioned;
    } else {
        m_provisioningState = ProvisioningState::Unprovisioned;
    }
    m_internalNetworkInterfaces = m_config.defaultNetworkInterfaces;
    m_internalNetworkProtocols = m_config.defaultNetworkProtocols;
    if (m_internalNetworkProtocols.empty()) {
        m_internalNetworkProtocols
            = { { "HTTP", true, m_config.port }, { "HTTPS", false, 8443 }, { "RTSP", true, 8554 } };
    } else {
        for (auto& proto : m_internalNetworkProtocols) {
            if (proto.name == "HTTP" && m_config.port != 8080 && proto.port == 8080) {
                proto.port = m_config.port;
            }
        }
    }
    m_internalGateway = m_config.defaultGateway;
    m_internalDns = m_config.defaultDns;
    m_internalNtp = m_config.defaultNtp;
    m_internalHostname = m_config.hostname;
    m_internalRelayOutputs = m_config.defaultRelayOutputs;
    m_internalDigitalInputs = m_config.defaultDigitalInputs;

    m_subManager = std::make_shared<SubscriptionManager>(m_config.notification);
    m_dispatcher = std::make_shared<NotificationDispatcher>(m_config.notification);
    m_scopeStore = std::make_shared<ScopeStore>(m_config.scopes);
    m_discoveryServer = std::make_unique<WsDiscoveryServer>(m_config, m_scopeStore);

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

    const std::string host { m_config.bindAddress.empty() ? std::string { "0.0.0.0" } : m_config.bindAddress };
    const bool loopback { isLoopbackHost(host) };
    if (!m_config.auth.enabled && !loopback) {
        logSystemMessage("ERROR", "Refusing to start: authentication disabled on non-loopback address " + host);
        return false;
    }
    if (m_config.auth.enabled && !loopback && !m_config.auth.allowDefaultPassword
        && m_credStore->hasDefaultPassword()) {
        logSystemMessage("ERROR", "Refusing to start: factory-default ONVIF password configured on " + host);
        return false;
    }

    m_httpServer.set_socket_options([](socket_t sock) {
        const int opt { 1 };
#ifdef _WIN32
        static_cast<void>(::setsockopt(
            sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), static_cast<int>(sizeof(opt))));
#else
        static_cast<void>(::setsockopt(
            sock, SOL_SOCKET, SO_REUSEADDR, static_cast<const void*>(&opt), static_cast<socklen_t>(sizeof(opt))));
#endif
    });
    if (!m_httpServer.bind_to_port(host.c_str(), m_config.port)) {
        logSystemMessage("ERROR", "Failed to bind ONVIF HTTP server to " + host + ":" + std::to_string(m_config.port));
        return false;
    }

    const auto listenExited { std::make_shared<std::atomic<bool>>(false) };
    m_httpThread = std::thread([this, listenExited]() {
        const bool res { m_httpServer.listen_after_bind() };
        if (!res) {
            logSystemMessage("ERROR", "ONVIF HTTP server listen_after_bind failed");
        }
        listenExited->store(true, std::memory_order_release);
    });

    constexpr auto maxWait { std::chrono::milliseconds(1000) };
    const auto startWait { std::chrono::steady_clock::now() };
    bool listening { false };
    while (!listenExited->load(std::memory_order_acquire)) {
        if (m_httpServer.is_running()) {
            listening = true;
            break;
        }
        if ((std::chrono::steady_clock::now() - startWait) > maxWait) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!listening) {
        logSystemMessage("ERROR", "Failed to start listening on " + host + ":" + std::to_string(m_config.port));
        m_httpServer.stop();
        if (m_httpThread.joinable()) {
            m_httpThread.join();
        }
        m_running = false;
        return false;
    }

    m_running = true;

    if (m_dispatcher) {
        m_dispatcher->start();
    }

    if (m_discoveryServer) {
        if (!m_discoveryServer->start()) {
            logSystemMessage("WARNING", "WS-Discovery server failed to start on UDP port 3702");
        }
    }

    logSystemMessage("INFO", "ONVIF HTTP server started successfully on " + host + ":" + std::to_string(m_config.port));
    return true;
}

void OnvifServer::stop()
{
    if (!m_running) {
        if (m_httpThread.joinable()) {
            m_httpServer.stop();
            m_httpThread.join();
        }
        return;
    }

    if (m_discoveryServer) {
        m_discoveryServer->stop();
    }

    m_httpServer.stop();
    m_running = false;

    if (m_dispatcher) {
        m_dispatcher->stop();
    }

    if (m_subManager) {
        m_subManager->stop();
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
    std::scoped_lock lock(m_deviceMutex);
    auto cfg = m_config;
    if (m_scopeStore) {
        cfg.scopes = m_scopeStore->getScopes();
    }
    return cfg;
}

std::shared_ptr<ScopeStore> OnvifServer::getScopeStore() const noexcept
{
    return m_scopeStore;
}

const SoapRouter& OnvifServer::getSoapRouter() const noexcept
{
    return m_soapRouter;
}

ProvisioningState OnvifServer::provisioningState() const noexcept
{
    return m_provisioningState.load();
}

DiscoveryMode OnvifServer::getDiscoveryMode() const noexcept
{
    return m_discoveryMode.load();
}

void OnvifServer::setDiscoveryMode(DiscoveryMode mode)
{
    m_discoveryMode.store(mode);
    if (m_discoveryServer) {
        m_discoveryServer->setDiscoveryMode(mode);
    }
}

std::vector<NetworkProtocolConfig> OnvifServer::getNetworkProtocols() const
{
    const auto deviceHandler = getDeviceManagementHandler();
    if (deviceHandler) {
        auto protocols = deviceHandler->handleGetNetworkProtocols();
        if (!protocols.empty()) {
            return protocols;
        }
    }
    std::scoped_lock lock(m_deviceMutex);
    return m_internalNetworkProtocols;
}

bool OnvifServer::setNetworkProtocols(const std::vector<NetworkProtocolConfig>& protocols)
{
    for (const auto& proto : protocols) {
        if (proto.port < 1 || proto.port > 65535) {
            return false;
        }
    }
    const auto deviceHandler = getDeviceManagementHandler();
    if (deviceHandler) {
        deviceHandler->handleSetNetworkProtocols(protocols);
    }
    std::scoped_lock lock(m_deviceMutex);
    for (const auto& u : protocols) {
        auto it = std::find_if(m_internalNetworkProtocols.begin(), m_internalNetworkProtocols.end(),
            [&u](const NetworkProtocolConfig& existing) { return existing.name == u.name; });
        if (it != m_internalNetworkProtocols.end()) {
            it->enabled = u.enabled;
            it->port = u.port;
        } else {
            m_internalNetworkProtocols.push_back(u);
        }
    }
    return true;
}

void OnvifServer::setPtzHandler(std::shared_ptr<IPtzHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_ptzHandler = std::move(h);
}
std::shared_ptr<IPtzHandler> OnvifServer::getPtzHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_ptzHandler;
}
void OnvifServer::setImagingHandler(std::shared_ptr<IImagingHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_imagingHandler = std::move(h);
}
std::shared_ptr<IImagingHandler> OnvifServer::getImagingHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_imagingHandler;
}
void OnvifServer::setOsdHandler(std::shared_ptr<IOsdHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_osdHandler = std::move(h);
}
std::shared_ptr<IOsdHandler> OnvifServer::getOsdHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_osdHandler;
}
void OnvifServer::setDeviceManagementHandler(std::shared_ptr<IDeviceManagementHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_deviceHandler = std::move(h);
}
std::shared_ptr<IDeviceManagementHandler> OnvifServer::getDeviceManagementHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_deviceHandler;
}
void OnvifServer::setDeviceIoHandler(std::shared_ptr<IDeviceIoHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_deviceIoHandler = std::move(h);
}
std::shared_ptr<IDeviceIoHandler> OnvifServer::getDeviceIoHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_deviceIoHandler;
}
void OnvifServer::setMetadataHandler(std::shared_ptr<IMetadataHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_metadataHandler = std::move(h);
}
std::shared_ptr<IMetadataHandler> OnvifServer::getMetadataHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_metadataHandler;
}
void OnvifServer::setRecordingHandler(std::shared_ptr<IRecordingHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_recordingHandler = std::move(h);
}
std::shared_ptr<IRecordingHandler> OnvifServer::getRecordingHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_recordingHandler;
}
void OnvifServer::setSearchHandler(std::shared_ptr<ISearchHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_searchHandler = std::move(h);
}
std::shared_ptr<ISearchHandler> OnvifServer::getSearchHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_searchHandler;
}
void OnvifServer::setReplayHandler(std::shared_ptr<IReplayHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_replayHandler = std::move(h);
}
std::shared_ptr<IReplayHandler> OnvifServer::getReplayHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_replayHandler;
}
void OnvifServer::setAnalyticsHandler(std::shared_ptr<IAnalyticsHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_analyticsHandler = std::move(h);
}
std::shared_ptr<IAnalyticsHandler> OnvifServer::getAnalyticsHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_analyticsHandler;
}
void OnvifServer::setMaskHandler(std::shared_ptr<IMaskHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_maskHandler = std::move(h);
}
std::shared_ptr<IMaskHandler> OnvifServer::getMaskHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_maskHandler;
}
void OnvifServer::setVideoSourceModeHandler(std::shared_ptr<IVideoSourceModeHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_videoSourceModeHandler = std::move(h);
}
std::shared_ptr<IVideoSourceModeHandler> OnvifServer::getVideoSourceModeHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_videoSourceModeHandler;
}
void OnvifServer::setThermalHandler(std::shared_ptr<IThermalHandler> h)
{
    std::scoped_lock lock(m_handlerMutex);
    m_thermalHandler = std::move(h);
}
std::shared_ptr<IThermalHandler> OnvifServer::getThermalHandler() const
{
    std::scoped_lock lock(m_handlerMutex);
    return m_thermalHandler;
}

void OnvifServer::logSystemMessage(const std::string& level, const std::string& msg)
{
    std::scoped_lock lock(m_logMutex);
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
    if (ev.propertyOperation.empty()) {
        ev.propertyOperation = "Changed";
    }

    std::vector<std::string> pushUrls { };
    if (m_subManager) {
        m_subManager->publishEvent(ev, pushUrls);
    }

    if (m_dispatcher && !pushUrls.empty()) {
        for (auto& url : pushUrls) {
            std::string payload { buildNotifyEnvelope(ev, url) };
            m_dispatcher->enqueue(std::move(url), std::move(payload));
        }
    }
}

void OnvifServer::setRequestLogCallback(RequestLogCallback callback)
{
    m_logCallback = std::move(callback);
}

void OnvifServer::setSnapshotProvider(SnapshotProvider provider)
{
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_snapshotProvider = std::move(provider);
}

} // namespace Onvif
