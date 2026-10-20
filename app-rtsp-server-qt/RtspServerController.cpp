#include "RtspServerController.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QString>
#include <QStringList>

#include "Live555RtspServer.h"
#include "RtspServerConfig.h"
#include "RtspStreamFeeder.h"
#include "ServerPresetManager.h"

namespace RtspServerApp {

namespace {

    constexpr int kTelemetryIntervalMs { 500 };

} // namespace

RtspServerController::RtspServerController(QObject* parent)
    : QObject(parent)
{
    // Default server configuration
    m_serverConfig.port = 8554U;
    m_serverConfig.streamPath = "live";
    m_serverConfig.streamName = "Tactical Live Stream";
    m_serverConfig.streamInfo = "PelcoD RTSP Server Feed";
    m_serverConfig.streamType = Video::RtspStreamType::H264ElementaryStream;
    m_serverConfig.transport = Video::RtspTransportProtocol::All;
    m_serverConfig.maxClients = 16U;
    m_serverConfig.queueCapacity = 30U;

    // Default feeder configuration
    m_feederConfig.mode = FeederSourceMode::SyntheticPattern;
    m_feederConfig.targetFps = 30U;
    m_feederConfig.loopPlayback = true;

    m_feeder = std::make_unique<RtspStreamFeeder>();

    connect(&m_telemetryTimer, &QTimer::timeout, this, &RtspServerController::onTelemetryTimer);

    updatePrimaryUrl();
    logEvent(QStringLiteral("INFO"), QStringLiteral("RTSP Controller initialized. Ready."));
}

RtspServerController::~RtspServerController()
{
    stopServer();
}

int RtspServerController::serverState() const noexcept
{
    return static_cast<int>(m_state);
}

QString RtspServerController::serverStateString() const
{
    switch (m_state) {
    case State::Running:
        return QStringLiteral("RUNNING");
    case State::Starting:
        return QStringLiteral("STARTING");
    case State::Error:
        return QStringLiteral("ERROR");
    case State::Stopped:
    default:
        return QStringLiteral("STOPPED");
    }
}

QString RtspServerController::primaryUrl() const
{
    return m_primaryUrl;
}

QString RtspServerController::localUrl() const
{
    return m_localUrl;
}

int RtspServerController::port() const noexcept
{
    return static_cast<int>(m_serverConfig.port);
}

QString RtspServerController::streamPath() const
{
    return QString::fromStdString(m_serverConfig.streamPath);
}

QString RtspServerController::streamName() const
{
    return QString::fromStdString(m_serverConfig.streamName);
}

QString RtspServerController::streamInfo() const
{
    return QString::fromStdString(m_serverConfig.streamInfo);
}

int RtspServerController::streamType() const noexcept
{
    return static_cast<int>(m_serverConfig.streamType);
}

int RtspServerController::transport() const noexcept
{
    return static_cast<int>(m_serverConfig.transport);
}

int RtspServerController::maxClients() const noexcept
{
    return static_cast<int>(m_serverConfig.maxClients);
}

int RtspServerController::queueCapacity() const noexcept
{
    return static_cast<int>(m_serverConfig.queueCapacity);
}

bool RtspServerController::authEnabled() const noexcept
{
    return m_authEnabled;
}

QString RtspServerController::username() const
{
    return m_serverConfig.username.has_value() ? QString::fromStdString(*m_serverConfig.username) : QString {};
}

QString RtspServerController::password() const
{
    return m_serverConfig.password.has_value() ? QString::fromStdString(*m_serverConfig.password) : QString {};
}

QString RtspServerController::realm() const
{
    return QString::fromStdString(m_serverConfig.realm);
}

bool RtspServerController::enableMulticast() const noexcept
{
    return m_serverConfig.enableMulticast;
}

QString RtspServerController::multicastAddress() const
{
    return QString::fromStdString(m_serverConfig.multicastAddress);
}

int RtspServerController::multicastPort() const noexcept
{
    return static_cast<int>(m_serverConfig.multicastPort);
}

int RtspServerController::multicastTtl() const noexcept
{
    return static_cast<int>(m_serverConfig.multicastTtl);
}

int RtspServerController::feederMode() const noexcept
{
    return static_cast<int>(m_feederConfig.mode);
}

QString RtspServerController::feederFilePath() const
{
    return QString::fromStdString(m_feederConfig.filePath);
}

bool RtspServerController::feederLoop() const noexcept
{
    return m_feederConfig.loopPlayback;
}

int RtspServerController::feederFps() const noexcept
{
    return static_cast<int>(m_feederConfig.targetFps);
}

int RtspServerController::activeClients() const noexcept
{
    return static_cast<int>(m_activeClients);
}

int RtspServerController::queueDepth() const noexcept
{
    return static_cast<int>(m_queueDepth);
}

qulonglong RtspServerController::totalPushed() const noexcept
{
    return static_cast<qulonglong>(m_totalPushed);
}

qulonglong RtspServerController::totalDropped() const noexcept
{
    return static_cast<qulonglong>(m_totalDropped);
}

double RtspServerController::egressBitrateKbps() const noexcept
{
    return m_egressBitrateKbps;
}

double RtspServerController::feederFpsActual() const noexcept
{
    return m_feederFpsActual;
}

QString RtspServerController::serverUptime() const
{
    if (m_state != State::Running) {
        return QStringLiteral("00:00:00");
    }

    const qint64 elapsedSec = m_uptimeTimer.elapsed() / 1000;
    const qint64 hours = elapsedSec / 3600;
    const qint64 minutes = (elapsedSec % 3600) / 60;
    const qint64 seconds = elapsedSec % 60;

    return QString::asprintf("%02lld:%02lld:%02lld", hours, minutes, seconds);
}

QString RtspServerController::statusMessage() const
{
    return m_statusMessage;
}

QStringList RtspServerController::presetList() const
{
    return m_presetManager.presetNames();
}

void RtspServerController::setPort(int val)
{
    if (val >= 0 && val <= 65535 && m_serverConfig.port != static_cast<std::uint16_t>(val)) {
        m_serverConfig.port = static_cast<std::uint16_t>(val);
        updatePrimaryUrl();
        emit configChanged();
    }
}

void RtspServerController::setStreamPath(const QString& val)
{
    const std::string s = val.trimmed().toStdString();
    if (m_serverConfig.streamPath != s) {
        m_serverConfig.streamPath = s;
        updatePrimaryUrl();
        emit configChanged();
    }
}

void RtspServerController::setStreamName(const QString& val)
{
    const std::string s = val.toStdString();
    if (m_serverConfig.streamName != s) {
        m_serverConfig.streamName = s;
        emit configChanged();
    }
}

void RtspServerController::setStreamInfo(const QString& val)
{
    const std::string s = val.toStdString();
    if (m_serverConfig.streamInfo != s) {
        m_serverConfig.streamInfo = s;
        emit configChanged();
    }
}

void RtspServerController::setStreamType(int val)
{
    const auto t = static_cast<Video::RtspStreamType>(val);
    if (m_serverConfig.streamType != t) {
        m_serverConfig.streamType = t;
        emit configChanged();
    }
}

void RtspServerController::setTransport(int val)
{
    const auto t = static_cast<Video::RtspTransportProtocol>(val);
    if (m_serverConfig.transport != t) {
        m_serverConfig.transport = t;
        emit configChanged();
    }
}

void RtspServerController::setMaxClients(int val)
{
    if (val > 0 && m_serverConfig.maxClients != static_cast<std::size_t>(val)) {
        m_serverConfig.maxClients = static_cast<std::size_t>(val);
        emit configChanged();
    }
}

void RtspServerController::setQueueCapacity(int val)
{
    if (val > 0 && m_serverConfig.queueCapacity != static_cast<std::size_t>(val)) {
        m_serverConfig.queueCapacity = static_cast<std::size_t>(val);
        emit configChanged();
    }
}

void RtspServerController::setAuthEnabled(bool val)
{
    if (m_authEnabled != val) {
        m_authEnabled = val;
        if (!m_authEnabled) {
            m_serverConfig.username = std::nullopt;
            m_serverConfig.password = std::nullopt;
        }
        emit configChanged();
    }
}

void RtspServerController::setUsername(const QString& val)
{
    const QString trimmed = val.trimmed();
    if (trimmed.isEmpty()) {
        m_serverConfig.username = std::nullopt;
    } else {
        m_serverConfig.username = trimmed.toStdString();
    }
    emit configChanged();
}

void RtspServerController::setPassword(const QString& val)
{
    if (val.isEmpty()) {
        m_serverConfig.password = std::nullopt;
    } else {
        m_serverConfig.password = val.toStdString();
    }
    emit configChanged();
}

void RtspServerController::setRealm(const QString& val)
{
    const std::string s = val.toStdString();
    if (m_serverConfig.realm != s) {
        m_serverConfig.realm = s;
        emit configChanged();
    }
}

void RtspServerController::setEnableMulticast(bool val)
{
    if (m_serverConfig.enableMulticast != val) {
        m_serverConfig.enableMulticast = val;
        emit configChanged();
    }
}

void RtspServerController::setMulticastAddress(const QString& val)
{
    const std::string s = val.trimmed().toStdString();
    if (m_serverConfig.multicastAddress != s) {
        m_serverConfig.multicastAddress = s;
        emit configChanged();
    }
}

void RtspServerController::setMulticastPort(int val)
{
    if (val >= 0 && val <= 65535 && m_serverConfig.multicastPort != static_cast<std::uint16_t>(val)) {
        m_serverConfig.multicastPort = static_cast<std::uint16_t>(val);
        emit configChanged();
    }
}

void RtspServerController::setMulticastTtl(int val)
{
    if (val >= 1 && val <= 255 && m_serverConfig.multicastTtl != static_cast<std::uint8_t>(val)) {
        m_serverConfig.multicastTtl = static_cast<std::uint8_t>(val);
        emit configChanged();
    }
}

void RtspServerController::setFeederMode(int val)
{
    const auto m = static_cast<FeederSourceMode>(val);
    if (m_feederConfig.mode != m) {
        m_feederConfig.mode = m;
        // Synchronize server stream type with feeder mode
        if (m == FeederSourceMode::MpegTsFile) {
            setStreamType(static_cast<int>(Video::RtspStreamType::Mpeg2TransportStream));
        } else {
            setStreamType(static_cast<int>(Video::RtspStreamType::H264ElementaryStream));
        }
        emit configChanged();
    }
}

void RtspServerController::setFeederFilePath(const QString& val)
{
    const std::string s = val.trimmed().toStdString();
    if (m_feederConfig.filePath != s) {
        m_feederConfig.filePath = s;
        emit configChanged();
    }
}

void RtspServerController::setFeederLoop(bool val)
{
    if (m_feederConfig.loopPlayback != val) {
        m_feederConfig.loopPlayback = val;
        emit configChanged();
    }
}

void RtspServerController::setFeederFps(int val)
{
    if (val > 0 && m_feederConfig.targetFps != static_cast<std::uint32_t>(val)) {
        m_feederConfig.targetFps = static_cast<std::uint32_t>(val);
        emit configChanged();
    }
}

bool RtspServerController::startServer()
{
    if (m_state == State::Running) {
        return true;
    }

    m_state = State::Starting;
    emit serverStateChanged();
    logEvent(QStringLiteral("INFO"), QStringLiteral("Validating server configuration..."));

    if (!m_serverConfig.validate()) {
        m_state = State::Error;
        m_statusMessage = QStringLiteral("Configuration validation failed.");
        emit serverStateChanged();
        emit statusMessageChanged();
        logEvent(QStringLiteral("ERROR"), m_statusMessage);
        return false;
    }

    try {
        m_server = std::make_unique<Video::Live555RtspServer>(m_serverConfig);
    } catch (...) {
        m_state = State::Error;
        m_statusMessage = QStringLiteral("Failed to allocate Live555RtspServer instance.");
        emit serverStateChanged();
        emit statusMessageChanged();
        logEvent(QStringLiteral("ERROR"), m_statusMessage);
        return false;
    }

    if (!m_server->start()) {
        m_state = State::Error;
        m_statusMessage = QString::asprintf("Failed to bind RTSP server to port %u", m_serverConfig.port);
        m_server.reset();
        emit serverStateChanged();
        emit statusMessageChanged();
        logEvent(QStringLiteral("ERROR"), m_statusMessage);
        return false;
    }

    // Start background stream feeder
    if (!m_feeder->start(m_feederConfig, m_server.get())) {
        logEvent(QStringLiteral("WARN"), QStringLiteral("Feeder failed to start. Running server with empty feed."));
    } else {
        logEvent(QStringLiteral("INFO"), QStringLiteral("Stream feeder active."));
    }

    m_state = State::Running;
    m_statusMessage = QStringLiteral("Streaming active.");
    m_uptimeTimer.start();
    m_lastBitrateCalcTime = m_uptimeTimer.elapsed();
    m_lastBytesStreamed = 0U;

    updatePrimaryUrl();
    m_telemetryTimer.start(kTelemetryIntervalMs);

    emit serverStateChanged();
    emit statusMessageChanged();
    logEvent(QStringLiteral("INFO"), QStringLiteral("Server online at ") + m_primaryUrl);

    return true;
}

void RtspServerController::stopServer()
{
    if (m_state == State::Stopped && !m_server) {
        return;
    }

    logEvent(QStringLiteral("INFO"), QStringLiteral("Stopping RTSP server and closing client sessions..."));
    m_telemetryTimer.stop();

    if (m_feeder) {
        m_feeder->stop();
    }

    if (m_server) {
        m_server->stop();
        m_server.reset();
    }

    m_state = State::Stopped;
    m_statusMessage = QStringLiteral("Server stopped.");
    m_activeClients = 0U;
    m_queueDepth = 0U;
    m_egressBitrateKbps = 0.0;
    m_feederFpsActual = 0.0;

    emit serverStateChanged();
    emit statusMessageChanged();
    emit telemetryUpdated();
    logEvent(QStringLiteral("INFO"), QStringLiteral("Server stopped cleanly."));
}

bool RtspServerController::toggleServer()
{
    if (m_state == State::Running) {
        stopServer();
        return false;
    }
    return startServer();
}

void RtspServerController::copyUrlToClipboard()
{
    if (!m_primaryUrl.isEmpty()) {
        QClipboard* cb = QGuiApplication::clipboard();
        if (cb != nullptr) {
            cb->setText(m_primaryUrl);
            logEvent(QStringLiteral("INFO"), QStringLiteral("Copied RTSP URL to clipboard: ") + m_primaryUrl);
        }
    }
}

bool RtspServerController::savePreset(const QString& name)
{
    const bool ok = m_presetManager.savePreset(name, m_serverConfig, m_feederConfig);
    if (ok) {
        emit presetListChanged();
        logEvent(QStringLiteral("INFO"), QStringLiteral("Saved preset: ") + name);
    }
    return ok;
}

bool RtspServerController::loadPreset(const QString& name)
{
    const bool wasRunning = (m_state == State::Running);
    if (wasRunning) {
        stopServer();
    }

    const bool ok = m_presetManager.loadPreset(name, m_serverConfig, m_feederConfig);
    if (ok) {
        m_authEnabled = m_serverConfig.username.has_value();
        updatePrimaryUrl();
        emit configChanged();
        logEvent(QStringLiteral("INFO"), QStringLiteral("Loaded preset: ") + name);

        if (wasRunning) {
            startServer();
        }
    }
    return ok;
}

bool RtspServerController::deletePreset(const QString& name)
{
    const bool ok = m_presetManager.deletePreset(name);
    if (ok) {
        emit presetListChanged();
        logEvent(QStringLiteral("INFO"), QStringLiteral("Deleted preset: ") + name);
    }
    return ok;
}

QStringList RtspServerController::availableNetworkInterfaces() const
{
    QStringList ips;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp) || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = iface.addressEntries();
        for (const auto& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                ips.append(entry.ip().toString());
            }
        }
    }
    if (ips.isEmpty()) {
        ips.append(QStringLiteral("127.0.0.1"));
    }
    return ips;
}

void RtspServerController::onTelemetryTimer()
{
    if (m_server && m_state == State::Running) {
        m_activeClients = m_server->activeClientCount();
        const auto stats = m_server->queueStats();
        m_queueDepth = stats.currentDepth;
        m_totalPushed = stats.totalPushed;
        m_totalDropped = stats.totalDropped;

        if (m_feeder) {
            const auto fm = m_feeder->metrics();
            m_feederFpsActual = fm.currentFps;

            const qint64 now = m_uptimeTimer.elapsed();
            const qint64 dtMs = now - m_lastBitrateCalcTime;
            if (dtMs >= 1000) {
                const std::uint64_t bytesDiff
                    = (fm.bytesStreamed >= m_lastBytesStreamed) ? (fm.bytesStreamed - m_lastBytesStreamed) : 0U;
                m_egressBitrateKbps = (static_cast<double>(bytesDiff) * 8.0) / static_cast<double>(dtMs);
                m_lastBytesStreamed = fm.bytesStreamed;
                m_lastBitrateCalcTime = now;
            }
        }

        emit telemetryUpdated();
    }
}

void RtspServerController::logEvent(const QString& level, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    emit logMessageEmitted(timestamp, level, message);
}

void RtspServerController::updatePrimaryUrl()
{
    const QString hostIp = detectHostIp();
    const std::uint16_t portNum
        = (m_server != nullptr && m_server->isRunning()) ? m_server->listeningPort() : m_serverConfig.port;

    const QString path = QString::fromStdString(m_serverConfig.streamPath);
    m_primaryUrl
        = QString::asprintf("rtsp://%s:%u/%s", hostIp.toUtf8().constData(), portNum, path.toUtf8().constData());
    m_localUrl = QString::asprintf("rtsp://127.0.0.1:%u/%s", portNum, path.toUtf8().constData());
}

QString RtspServerController::detectHostIp() const
{
    const QStringList ips = availableNetworkInterfaces();
    return ips.isEmpty() ? QStringLiteral("127.0.0.1") : ips.first();
}

} // namespace RtspServerApp
