#include "QOnvifServer.h"

namespace PelcoD::Qt {

QOnvifServer::QOnvifServer(Onvif::OnvifServerConfig config, QObject* parent)
    : QObject(parent)
    , m_config(std::move(config))
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
    } else {
        m_ptzAdapter.reset();
    }

    if (m_server != nullptr) {
        m_server->setPtzHandler(m_ptzAdapter);
        m_server->setImagingHandler(m_ptzAdapter);
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
    QString host;
    int portNum = 8080;
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
        m_server = std::make_unique<Onvif::OnvifServer>(m_config, m_ptzAdapter, m_ptzAdapter);
        m_server->setRequestLogCallback(
            [this](const std::string& service, const std::string& action, const std::string& clientIp) {
                emit requestLogged(
                    QString::fromStdString(service), QString::fromStdString(action), QString::fromStdString(clientIp));
            });

        if (!m_server->start()) {
            m_server.reset();
            emit errorOccurred(tr("Failed to bind or start ONVIF server on port %1").arg(m_config.port));
            return false;
        }

        const QString url = QStringLiteral("http://%1:%2/onvif/device_service")
                                .arg(QString::fromStdString(m_config.bindAddress))
                                .arg(m_config.port);
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
