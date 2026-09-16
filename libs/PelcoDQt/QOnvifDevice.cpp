#include "QOnvifDevice.h"

#include <QThread>

namespace PelcoD::Qt {

QOnvifDevice::QOnvifDevice(QObject* parent)
    : QObject(parent)
{
}

QOnvifDevice::~QOnvifDevice()
{
    disconnectFromCamera();
}

bool QOnvifDevice::isConnected() const
{
    return m_connected;
}

QString QOnvifDevice::activeProfileToken() const
{
    return m_activeProfileToken;
}

QString QOnvifDevice::rtspStreamUri() const
{
    return m_rtspStreamUri;
}

QString QOnvifDevice::snapshotUri() const
{
    return m_snapshotUri;
}

std::vector<PelcoD::Onvif::MediaProfile> QOnvifDevice::profiles() const
{
    return m_profiles;
}

std::vector<PelcoD::Onvif::PtzPreset> QOnvifDevice::presets() const
{
    return m_presets;
}

PelcoD::Onvif::DeviceInformation QOnvifDevice::deviceInformation() const
{
    return m_deviceInfo;
}

QList<PelcoD::Onvif::DiscoveredDevice> QOnvifDevice::discoverCameras(int timeoutMs)
{
    const auto stdList = PelcoD::Onvif::OnvifDiscovery::discoverDevices(std::chrono::milliseconds(timeoutMs));

    QList<PelcoD::Onvif::DiscoveredDevice> qList {};
    qList.reserve(static_cast<qsizetype>(stdList.size()));
    for (const auto& dev : stdList) {
        qList.append(dev);
    }
    return qList;
}

void QOnvifDevice::discoverCamerasAsync(int timeoutMs)
{
    QThread* thread = QThread::create([this, timeoutMs]() {
        const auto result = discoverCameras(timeoutMs);
        QMetaObject::invokeMethod(
            this, [this, result]() { emit discoveryFinished(result); }, ::Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool QOnvifDevice::connectToCamera(const QString& endpoint, const QString& username, const QString& password)
{
    disconnectFromCamera();

    PelcoD::Onvif::SecurityCredentials creds {};
    creds.username = username.toStdString();
    creds.password = password.toStdString();

    m_client = std::make_unique<PelcoD::Onvif::OnvifClient>(endpoint.toStdString(), creds);

    // Synchronize camera clock
    m_client->synchronizeSystemTime();

    // Query capabilities
    const auto caps = m_client->getCapabilities();
    if (!caps) {
        emit errorOccurred(tr("Failed to query ONVIF capabilities from %1").arg(endpoint));
        m_client.reset();
        return false;
    }

    const auto devInfo = m_client->getDeviceInformation();
    if (devInfo) {
        m_deviceInfo = *devInfo;
    }

    m_profiles = m_client->getProfiles();
    if (m_profiles.empty()) {
        emit errorOccurred(tr("No media profiles found on camera %1").arg(endpoint));
        m_client.reset();
        return false;
    }

    m_connected = true;
    m_endpoint = endpoint;

    // Set first profile as default active profile
    setActiveProfile(QString::fromStdString(m_profiles.front().token));

    const QString modelLabel = !m_deviceInfo.model.empty() ? QString::fromStdString(m_deviceInfo.model)
                                                           : QString::fromStdString(m_deviceInfo.manufacturer);

    emit connected(m_endpoint, modelLabel);
    return true;
}

void QOnvifDevice::disconnectFromCamera()
{
    if (!m_connected && !m_client) {
        return;
    }

    m_connected = false;
    m_endpoint.clear();
    m_activeProfileToken.clear();
    m_rtspStreamUri.clear();
    m_snapshotUri.clear();
    m_profiles.clear();
    m_presets.clear();
    m_deviceInfo = {};
    m_client.reset();

    emit disconnected();
}

bool QOnvifDevice::setActiveProfile(const QString& token)
{
    if (!m_client) {
        return false;
    }

    m_activeProfileToken = token;

    const auto uriInfo = m_client->getStreamUri(token.toStdString(), true);
    if (uriInfo && !uriInfo->uri.empty()) {
        m_rtspStreamUri = QString::fromStdString(uriInfo->uri);
        emit streamUriResolved(m_rtspStreamUri);
    } else {
        emit errorOccurred(tr("Failed to resolve RTSP stream URI for profile '%1'").arg(token));
    }

    resolveSnapshotUri();
    refreshPresets();
    return !m_rtspStreamUri.isEmpty();
}

void QOnvifDevice::move(double panSpeed, double tiltSpeed)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->continuousMove(m_activeProfileToken.toStdString(), panSpeed, tiltSpeed, 0.0);
}

void QOnvifDevice::zoom(int direction, double speed)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    const double z = (direction > 0) ? speed : ((direction < 0) ? -speed : 0.0);
    m_client->continuousMove(m_activeProfileToken.toStdString(), 0.0, 0.0, z);
}

void QOnvifDevice::stopMotion(bool stopPanTilt, bool stopZoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->stop(m_activeProfileToken.toStdString(), stopPanTilt, stopZoom);
}

void QOnvifDevice::absoluteMove(double pan, double tilt, double zoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->absoluteMove(m_activeProfileToken.toStdString(), pan, tilt, zoom);
}

void QOnvifDevice::relativeMove(double pan, double tilt, double zoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->relativeMove(m_activeProfileToken.toStdString(), pan, tilt, zoom);
}

void QOnvifDevice::gotoHomePosition()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->gotoHomePosition(m_activeProfileToken.toStdString());
}

void QOnvifDevice::setHomePosition()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_client->setHomePosition(m_activeProfileToken.toStdString());
}

void QOnvifDevice::refreshPresets()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_presets = m_client->getPresets(m_activeProfileToken.toStdString());
    emit presetsUpdated(m_presets);
}

bool QOnvifDevice::gotoPreset(const QString& presetToken, double speed)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    return m_client->gotoPreset(m_activeProfileToken.toStdString(), presetToken.toStdString(), speed);
}

bool QOnvifDevice::setPreset(const QString& presetName, const QString& presetToken)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    const auto res = m_client->setPreset(
        m_activeProfileToken.toStdString(), presetName.toStdString(), presetToken.toStdString());
    if (res) {
        refreshPresets();
        return true;
    }
    return false;
}

bool QOnvifDevice::removePreset(const QString& presetToken)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    const bool ok = m_client->removePreset(m_activeProfileToken.toStdString(), presetToken.toStdString());
    if (ok) {
        refreshPresets();
    }
    return ok;
}

QString QOnvifDevice::resolveSnapshotUri()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return {};
    }
    const auto snap = m_client->getSnapshotUri(m_activeProfileToken.toStdString(), true);
    if (snap) {
        m_snapshotUri = QString::fromStdString(*snap);
        emit snapshotUriResolved(m_snapshotUri);
        return m_snapshotUri;
    }
    return {};
}

bool QOnvifDevice::rebootCamera()
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->systemReboot();
    emit rebootCompleted(ok);
    return ok;
}

void QOnvifDevice::refreshStatus()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    const auto status = m_client->getStatus(m_activeProfileToken.toStdString());
    if (status) {
        emit statusUpdated(*status);
    }
}

} // namespace PelcoD::Qt
