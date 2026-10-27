/// @file QOnvifServer.cpp
/// @brief Implementation of QOnvifServer Qt 6 adapter.

#include "QOnvifServer.h"

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace PelcoD::Qt {

QOnvifServer::QOnvifServer(Onvif::OnvifServerConfig config, QObject* parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_deviceSystemAdapter(std::make_shared<Onvif::DeviceSystemAdapter>())
    , m_recordingAdapter(std::make_shared<Onvif::StubRecordingAdapter>())
    , m_analyticsAdapter(std::make_shared<Onvif::StubAnalyticsAdapter>())
    , m_mediaExtAdapter(std::make_shared<Onvif::StubMediaExtensionAdapter>())
    , m_thermalAdapter(std::make_shared<Onvif::StubThermalAdapter>(nullptr))
{
}

QOnvifServer::~QOnvifServer()
{
    stop();
}

bool QOnvifServer::isRunning() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_server != nullptr && m_server->isRunning();
}

Onvif::OnvifServerConfig QOnvifServer::config() const
{
    std::scoped_lock lock(m_mutex);
    return m_config;
}

void QOnvifServer::setConfig(const Onvif::OnvifServerConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_config = config;
}

void QOnvifServer::bindDevice(PelcoDQt::QPelcoDDevice* device)
{
    std::scoped_lock lock(m_mutex);
    if (device != nullptr && device->sharedCoreDevice() != nullptr) {
        m_ptzAdapter = std::make_shared<Onvif::PelcoDPtzAdapter>(device->sharedCoreDevice());
        if (m_patrolController != nullptr) {
            m_ptzAdapter->setPatrolController(m_patrolController);
        }
        m_thermalAdapter = std::make_shared<Onvif::StubThermalAdapter>(device->sharedCoreDevice());
    } else {
        m_ptzAdapter.reset();
        m_thermalAdapter = std::make_shared<Onvif::StubThermalAdapter>(nullptr);
    }

    if (m_server != nullptr) {
        Onvif::OnvifServer* const srvPtr { m_server.get() };
        const auto pub = [srvPtr](const Onvif::OnvifEvent& evt) { srvPtr->publishEvent(evt); };
        if (m_ptzAdapter != nullptr) {
            m_ptzAdapter->setEventPublisher(pub);
        }
        if (m_thermalAdapter != nullptr) {
            m_thermalAdapter->setEventPublisher(pub);
        }
        m_server->setPtzHandler(m_ptzAdapter);
        m_server->setImagingHandler(m_ptzAdapter);
        m_server->setThermalHandler(m_thermalAdapter);
    }
}

void QOnvifServer::bindPatrolController(PelcoD::PatrolController* patrol)
{
    std::scoped_lock lock(m_mutex);
    m_patrolController = patrol;
    if (m_ptzAdapter != nullptr) {
        m_ptzAdapter->setPatrolController(m_patrolController);
    }
}

QString QOnvifServer::endpointUrl() const
{
    QString host {};
    int portNum { 8080 };
    {
        std::scoped_lock lock(m_mutex);
        host = QString::fromStdString(m_config.bindAddress);
        portNum = m_config.port;
    }

    if (host == QStringLiteral("0.0.0.0") || host.isEmpty()) {
        host = QStringLiteral("127.0.0.1");
    }
    return QStringLiteral("http://%1:%2/onvif/device_service").arg(host).arg(portNum);
}

bool QOnvifServer::start()
{
    std::scoped_lock lock(m_mutex);
    if (m_server != nullptr && m_server->isRunning()) {
        return true;
    }

    try {
        m_server = std::make_unique<Onvif::OnvifServer>(m_config, m_ptzAdapter, m_ptzAdapter, nullptr,
            m_deviceSystemAdapter, m_deviceSystemAdapter, m_analyticsAdapter, m_analyticsAdapter);
        m_server->setRecordingHandler(m_recordingAdapter);
        m_server->setSearchHandler(m_recordingAdapter);
        m_server->setReplayHandler(m_recordingAdapter);
        m_server->setMaskHandler(m_mediaExtAdapter);
        m_server->setVideoSourceModeHandler(m_mediaExtAdapter);
        m_server->setThermalHandler(m_thermalAdapter);

        Onvif::OnvifServer* const srvPtr { m_server.get() };
        const auto pub = [srvPtr](const Onvif::OnvifEvent& evt) { srvPtr->publishEvent(evt); };
        if (m_ptzAdapter != nullptr) {
            m_ptzAdapter->setEventPublisher(pub);
        }
        if (m_deviceSystemAdapter != nullptr) {
            m_deviceSystemAdapter->setEventPublisher(pub);
        }
        if (m_analyticsAdapter != nullptr) {
            m_analyticsAdapter->setEventPublisher(pub);
        }
        if (m_thermalAdapter != nullptr) {
            m_thermalAdapter->setEventPublisher(pub);
        }

        m_server->setRequestLogCallback(
            [this](const std::string& service, const std::string& action, const std::string& clientIp) {
                emit requestLogged(
                    QString::fromStdString(service), QString::fromStdString(action), QString::fromStdString(clientIp));
            });

        if (!m_server->start()) {
            if (m_ptzAdapter != nullptr) {
                m_ptzAdapter->setEventPublisher(nullptr);
            }
            if (m_deviceSystemAdapter != nullptr) {
                m_deviceSystemAdapter->setEventPublisher(nullptr);
            }
            if (m_analyticsAdapter != nullptr) {
                m_analyticsAdapter->setEventPublisher(nullptr);
            }
            if (m_thermalAdapter != nullptr) {
                m_thermalAdapter->setEventPublisher(nullptr);
            }
            m_server.reset();
            emit errorOccurred(tr("Failed to bind or start ONVIF server on port %1 (port in use, or non-loopback "
                                  "bind requires strong Admin password >= 8 chars with 3 character classes)")
                                   .arg(m_config.port));
            return false;
        }

        QString host { QString::fromStdString(m_config.bindAddress) };
        if (host == QStringLiteral("0.0.0.0") || host.isEmpty()) {
            host = QStringLiteral("127.0.0.1");
        }
        const QString url { QStringLiteral("http://%1:%2/onvif/device_service").arg(host).arg(m_config.port) };
        emit serverStarted(url);
        return true;
    } catch (const std::exception& ex) {
        m_server.reset();
        emit errorOccurred(QString::fromUtf8(ex.what()));
        return false;
    }
}

void QOnvifServer::stop()
{
    std::unique_ptr<Onvif::OnvifServer> toStop;
    {
        std::scoped_lock lock(m_mutex);
        if (m_server == nullptr) {
            return;
        }
        if (m_ptzAdapter != nullptr) {
            m_ptzAdapter->setEventPublisher(nullptr);
        }
        if (m_deviceSystemAdapter != nullptr) {
            m_deviceSystemAdapter->setEventPublisher(nullptr);
        }
        if (m_analyticsAdapter != nullptr) {
            m_analyticsAdapter->setEventPublisher(nullptr);
        }
        if (m_thermalAdapter != nullptr) {
            m_thermalAdapter->setEventPublisher(nullptr);
        }
        toStop = std::move(m_server);
    }
    toStop->stop();
    emit serverStopped();
}

bool QOnvifServer::restart()
{
    stop();
    return start();
}

} // namespace PelcoD::Qt
