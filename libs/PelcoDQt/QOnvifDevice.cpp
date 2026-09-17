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

    refreshUsers();
    refreshNetworkInterfaces();
    refreshNetworkGateway();
    refreshDNS();
    refreshNTP();
    refreshRelayOutputs();
    refreshDigitalInputs();
    refreshImagingPresets();
    refreshFocusStatus();

    emit connected(m_endpoint, modelLabel);
    return true;
}

void QOnvifDevice::disconnectFromCamera()
{
    if (!m_connected && !m_client) {
        return;
    }

    stopEventSubscription();
    stopMetadataStreaming();

    m_connected = false;
    m_endpoint.clear();
    m_activeProfileToken.clear();
    m_activeVideoSourceToken.clear();
    m_rtspStreamUri.clear();
    m_snapshotUri.clear();
    m_profiles.clear();
    m_presets.clear();
    m_presetTours.clear();
    m_osds.clear();
    m_users.clear();
    m_networkInterfaces.clear();
    m_networkGateway.clear();
    m_dnsConfig = {};
    m_ntpConfig = {};
    m_deviceInfo = {};
    m_focusStatus = {};
    m_imagingPresets.clear();
    m_relayOutputs.clear();
    m_digitalInputs.clear();
    m_metadataConfigs.clear();
    m_client.reset();

    emit disconnected();
}

bool QOnvifDevice::setActiveProfile(const QString& token)
{
    if (!m_client) {
        return false;
    }

    m_activeProfileToken = token;

    for (const auto& p : m_profiles) {
        if (p.token == token.toStdString()) {
            m_activeVideoSourceToken = QString::fromStdString(p.videoSourceToken);
            break;
        }
    }
    if (m_activeVideoSourceToken.isEmpty() && !m_profiles.empty()) {
        m_activeVideoSourceToken = QString::fromStdString(m_profiles.front().videoSourceToken);
    }

    const auto uriInfo = m_client->getStreamUri(token.toStdString(), true);
    if (uriInfo && !uriInfo->uri.empty()) {
        m_rtspStreamUri = QString::fromStdString(uriInfo->uri);
        emit streamUriResolved(m_rtspStreamUri);
    } else {
        emit errorOccurred(tr("Failed to resolve RTSP stream URI for profile '%1'").arg(token));
    }

    resolveSnapshotUri();
    refreshPresets();
    refreshPresetTours();
    refreshImagingSettings();
    refreshOSDs();
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

QString QOnvifDevice::sendAuxiliaryCommand(const QString& auxiliaryData)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        emit auxiliaryCommandCompleted(false, QString());
        return QString();
    }
    const auto resp = m_client->sendAuxiliaryCommand(m_activeProfileToken.toStdString(), auxiliaryData.toStdString());
    const bool success = resp.has_value();
    const QString result = success ? QString::fromStdString(*resp) : QString();
    emit auxiliaryCommandCompleted(success, result);
    return result;
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
    const auto res
        = m_client->setPreset(m_activeProfileToken.toStdString(), presetName.toStdString(), presetToken.toStdString());
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

void QOnvifDevice::refreshPresetTours()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    m_presetTours = m_client->getPresetTours(m_activeProfileToken.toStdString());
    emit presetToursUpdated(m_presetTours);
}

bool QOnvifDevice::operatePresetTour(const QString& tourToken, PelcoD::Onvif::PresetTourOperation operation)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    const bool ok = m_client->operatePresetTour(m_activeProfileToken.toStdString(), tourToken.toStdString(), operation);
    refreshPresetTours();
    return ok;
}

bool QOnvifDevice::operatePresetTour(const QString& tourToken, const QString& operation)
{
    PelcoD::Onvif::PresetTourOperation op = PelcoD::Onvif::PresetTourOperation::Start;
    if (operation.compare("Stop", ::Qt::CaseInsensitive) == 0) {
        op = PelcoD::Onvif::PresetTourOperation::Stop;
    } else if (operation.compare("Pause", ::Qt::CaseInsensitive) == 0) {
        op = PelcoD::Onvif::PresetTourOperation::Pause;
    }
    return operatePresetTour(tourToken, op);
}

bool QOnvifDevice::modifyPresetTour(const PelcoD::Onvif::PresetTour& tour)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    const bool ok = m_client->modifyPresetTour(m_activeProfileToken.toStdString(), tour);
    if (ok) {
        refreshPresetTours();
    }
    return ok;
}

bool QOnvifDevice::removePresetTour(const QString& tourToken)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return false;
    }
    const bool ok = m_client->removePresetTour(m_activeProfileToken.toStdString(), tourToken.toStdString());
    if (ok) {
        refreshPresetTours();
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

void QOnvifDevice::refreshImagingSettings(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    const auto settings = m_client->getImagingSettings(token.toStdString());
    if (settings) {
        m_imagingSettings = *settings;
        emit imagingSettingsUpdated(m_imagingSettings);
    }
}

bool QOnvifDevice::setImagingSettings(const PelcoD::Onvif::ImagingSettings& settings, const QString& videoSourceToken)
{
    if (!m_client) {
        return false;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return false;
    }

    const bool ok = m_client->setImagingSettings(token.toStdString(), settings, true);
    if (ok) {
        m_imagingSettings = settings;
        emit imagingSettingsUpdated(m_imagingSettings);
    }
    return ok;
}

void QOnvifDevice::focusContinuous(float speed, const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    m_client->moveFocus(token.toStdString(), speed);
}

void QOnvifDevice::focusStop(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    m_client->stopFocus(token.toStdString());
    m_focusStatus.moveStatus = "IDLE";
    emit focusStatusUpdated(m_focusStatus);
}

void QOnvifDevice::refreshFocusStatus(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    const auto st = m_client->getFocusStatus(token.toStdString());
    if (st) {
        m_focusStatus = *st;
        emit focusStatusUpdated(m_focusStatus);
    }
}

void QOnvifDevice::focusAbsolute(float position, float speed, const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    m_client->moveFocusAbsolute(token.toStdString(), position, speed);
    m_focusStatus.position = std::clamp(position, 0.0f, 1.0f);
    m_focusStatus.moveStatus = "MOVING";
    emit focusStatusUpdated(m_focusStatus);
}

void QOnvifDevice::focusRelative(float distance, float speed, const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    m_client->moveFocusRelative(token.toStdString(), distance, speed);
    m_focusStatus.position = std::clamp(m_focusStatus.position + distance, 0.0f, 1.0f);
    m_focusStatus.moveStatus = "MOVING";
    emit focusStatusUpdated(m_focusStatus);
}

void QOnvifDevice::refreshImagingPresets(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return;
    }

    m_imagingPresets = m_client->getImagingPresets(token.toStdString());
    emit imagingPresetsUpdated(m_imagingPresets);
}

bool QOnvifDevice::setCurrentImagingPreset(const QString& presetToken, const QString& videoSourceToken)
{
    if (!m_client) {
        return false;
    }

    const QString token = !videoSourceToken.isEmpty() ? videoSourceToken : m_activeVideoSourceToken;
    if (token.isEmpty()) {
        return false;
    }

    const bool ok = m_client->setCurrentImagingPreset(token.toStdString(), presetToken.toStdString());
    if (ok) {
        refreshImagingSettings(token);
        refreshFocusStatus(token);
    }
    return ok;
}

void QOnvifDevice::startEventSubscription(int pollIntervalMs)
{
    if (!m_client || m_eventSubActive) {
        return;
    }

    const auto subUrl = m_client->createPullPointSubscription();
    if (!subUrl || subUrl->empty()) {
        emit errorOccurred(tr("Failed to establish ONVIF PullPoint event subscription."));
        return;
    }

    m_eventSubscriptionUrl = QString::fromStdString(*subUrl);
    m_eventSubActive = true;

    // Launch background worker thread for event polling loop
    QThread* thread = QThread::create([this, pollIntervalMs]() {
        while (m_eventSubActive && m_client && !m_eventSubscriptionUrl.isEmpty()) {
            const auto events = m_client->pullMessages(m_eventSubscriptionUrl.toStdString(), 3, 10);
            for (const auto& ev : events) {
                QMetaObject::invokeMethod(
                    this, [this, ev]() { emit eventReceived(ev); }, ::Qt::QueuedConnection);
            }
            QThread::msleep(static_cast<unsigned long>(std::max(0, pollIntervalMs)));
        }
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void QOnvifDevice::stopEventSubscription()
{
    if (!m_eventSubActive) {
        return;
    }

    m_eventSubActive = false;
    if (m_client && !m_eventSubscriptionUrl.isEmpty()) {
        m_client->unsubscribe(m_eventSubscriptionUrl.toStdString());
    }
    m_eventSubscriptionUrl.clear();
}

bool QOnvifDevice::isEventSubscriptionActive() const
{
    return m_eventSubActive;
}

void QOnvifDevice::refreshOSDs()
{
    if (!m_client) {
        return;
    }
    const std::string vsToken = m_activeVideoSourceToken.toStdString();
    m_osds = m_client->getOSDs(vsToken);
    emit osdsUpdated(m_osds);
}

QString QOnvifDevice::createOSD(const PelcoD::Onvif::OsdConfig& osd)
{
    if (!m_client) {
        return QString();
    }
    PelcoD::Onvif::OsdConfig cfg = osd;
    if (cfg.videoSourceToken.empty() && !m_activeVideoSourceToken.isEmpty()) {
        cfg.videoSourceToken = m_activeVideoSourceToken.toStdString();
    }
    const std::string token = m_client->createOSD(cfg);
    if (!token.empty()) {
        refreshOSDs();
        return QString::fromStdString(token);
    }
    return QString();
}

bool QOnvifDevice::setOSD(const PelcoD::Onvif::OsdConfig& osd)
{
    if (!m_client) {
        return false;
    }
    PelcoD::Onvif::OsdConfig cfg = osd;
    if (cfg.videoSourceToken.empty() && !m_activeVideoSourceToken.isEmpty()) {
        cfg.videoSourceToken = m_activeVideoSourceToken.toStdString();
    }
    const bool ok = m_client->setOSD(cfg);
    if (ok) {
        refreshOSDs();
    }
    return ok;
}

bool QOnvifDevice::deleteOSD(const QString& osdToken)
{
    if (!m_client || osdToken.isEmpty()) {
        return false;
    }
    const bool ok = m_client->deleteOSD(osdToken.toStdString());
    if (ok) {
        refreshOSDs();
    }
    return ok;
}

void QOnvifDevice::refreshUsers()
{
    if (!m_client) {
        return;
    }
    m_users = m_client->getUsers();
    emit usersUpdated(m_users);
}

bool QOnvifDevice::createUser(const PelcoD::Onvif::OnvifUser& user)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->createUsers({ user });
    if (ok) {
        refreshUsers();
    }
    return ok;
}

bool QOnvifDevice::setUser(const PelcoD::Onvif::OnvifUser& user)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setUser(user);
    if (ok) {
        refreshUsers();
    }
    return ok;
}

bool QOnvifDevice::deleteUser(const QString& username)
{
    if (!m_client || username.isEmpty()) {
        return false;
    }
    const bool ok = m_client->deleteUsers({ username.toStdString() });
    if (ok) {
        refreshUsers();
    }
    return ok;
}

void QOnvifDevice::refreshNetworkInterfaces()
{
    if (!m_client) {
        return;
    }
    m_networkInterfaces = m_client->getNetworkInterfaces();
    emit networkInterfacesUpdated(m_networkInterfaces);
}

bool QOnvifDevice::setNetworkInterface(const PelcoD::Onvif::NetworkInterfaceConfig& config)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setNetworkInterfaces(config);
    if (ok) {
        refreshNetworkInterfaces();
    }
    return ok;
}

void QOnvifDevice::refreshNetworkGateway()
{
    if (!m_client) {
        return;
    }
    m_networkGateway = QString::fromStdString(m_client->getNetworkDefaultGateway());
    emit networkGatewayUpdated(m_networkGateway);
}

bool QOnvifDevice::setNetworkGateway(const QString& gateway)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setNetworkDefaultGateway(gateway.toStdString());
    if (ok) {
        refreshNetworkGateway();
    }
    return ok;
}

void QOnvifDevice::refreshDNS()
{
    if (!m_client) {
        return;
    }
    const auto res = m_client->getDNS();
    if (res) {
        m_dnsConfig = *res;
        emit dnsUpdated(m_dnsConfig);
    }
}

bool QOnvifDevice::setDNS(const PelcoD::Onvif::DnsConfig& dns)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setDNS(dns);
    if (ok) {
        refreshDNS();
    }
    return ok;
}

void QOnvifDevice::refreshNTP()
{
    if (!m_client) {
        return;
    }
    const auto res = m_client->getNTP();
    if (res) {
        m_ntpConfig = *res;
        emit ntpUpdated(m_ntpConfig);
    }
}

bool QOnvifDevice::setNTP(const PelcoD::Onvif::NtpConfig& ntp)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setNTP(ntp);
    if (ok) {
        refreshNTP();
    }
    return ok;
}

bool QOnvifDevice::setSystemDateAndTime(const PelcoD::Onvif::SystemDateTimeConfig& dt)
{
    if (!m_client) {
        return false;
    }
    return m_client->setSystemDateAndTime(dt);
}

bool QOnvifDevice::setSystemFactoryDefault(bool hard)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setSystemFactoryDefault(
        hard ? PelcoD::Onvif::FactoryDefaultType::Hard : PelcoD::Onvif::FactoryDefaultType::Soft);
    emit factoryDefaultCompleted(ok);
    return ok;
}

void QOnvifDevice::refreshRelayOutputs()
{
    if (!m_client) {
        return;
    }

    m_relayOutputs = m_client->getRelayOutputs();
    emit relayOutputsUpdated(m_relayOutputs);
}

bool QOnvifDevice::setRelayOutputState(const QString& relayToken, bool active)
{
    if (!m_client) {
        return false;
    }

    const auto state = active ? PelcoD::Onvif::RelayLogicalState::Active : PelcoD::Onvif::RelayLogicalState::Inactive;
    const bool ok = m_client->setRelayOutputState(relayToken.toStdString(), state);
    if (ok) {
        refreshRelayOutputs();
    }
    return ok;
}

bool QOnvifDevice::setRelayOutputSettings(const QString& relayToken, const PelcoD::Onvif::RelayOutputConfig& settings)
{
    if (!m_client) {
        return false;
    }

    const bool ok = m_client->setRelayOutputSettings(relayToken.toStdString(), settings);
    if (ok) {
        refreshRelayOutputs();
    }
    return ok;
}

void QOnvifDevice::refreshDigitalInputs()
{
    if (!m_client) {
        return;
    }

    m_digitalInputs = m_client->getDigitalInputs();
    emit digitalInputsUpdated(m_digitalInputs);
}

void QOnvifDevice::refreshMetadataConfigurations()
{
    if (!m_client) {
        return;
    }
    m_metadataConfigs = m_client->getMetadataConfigurations();
    emit metadataConfigurationsUpdated(m_metadataConfigs);
}

bool QOnvifDevice::setMetadataConfiguration(const PelcoD::Onvif::MetadataConfiguration& config)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setMetadataConfiguration(config);
    if (ok) {
        refreshMetadataConfigurations();
    }
    return ok;
}

void QOnvifDevice::pollCurrentMetadata()
{
    if (!m_client) {
        return;
    }
    const auto payload = m_client->getMetadataStream();
    if (payload) {
        emit metadataReceived(*payload);
    }
}

void QOnvifDevice::startMetadataStreaming(int intervalMs)
{
    if (!m_client || m_metadataStreamingActive) {
        return;
    }
    m_metadataStreamingActive = true;

    QThread* thread = QThread::create([this, intervalMs]() {
        while (m_metadataStreamingActive && m_client) {
            const auto payload = m_client->getMetadataStream();
            if (payload) {
                QMetaObject::invokeMethod(
                    this, [this, payload]() { emit metadataReceived(*payload); }, ::Qt::QueuedConnection);
            }
            QThread::msleep(static_cast<unsigned long>(std::max(100, intervalMs)));
        }
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void QOnvifDevice::stopMetadataStreaming()
{
    m_metadataStreamingActive = false;
}

bool QOnvifDevice::isMetadataStreamingActive() const
{
    return m_metadataStreamingActive;
}

void QOnvifDevice::pollMetadata()
{
    pollCurrentMetadata();
}

void QOnvifDevice::fetchSystemLog(PelcoD::Onvif::SystemLogType logType)
{
    if (!m_client) {
        return;
    }
    const auto logOpt = m_client->getSystemLog(logType);
    if (logOpt) {
        emit systemLogReceived(logType, QString::fromStdString(*logOpt));
    }
}

void QOnvifDevice::fetchSystemSupportInformation()
{
    if (!m_client) {
        return;
    }
    const auto infoOpt = m_client->getSystemSupportInformation();
    if (infoOpt) {
        emit systemSupportInfoReceived(*infoOpt);
    }
}

void QOnvifDevice::downloadSystemBackup()
{
    if (!m_client) {
        return;
    }
    const auto backupOpt = m_client->getSystemBackup();
    if (backupOpt) {
        emit systemBackupReceived(QString::fromStdString(*backupOpt));
    }
}

bool QOnvifDevice::restoreSystem(const QString& backupData)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->restoreSystem(backupData.toStdString());
    emit systemRestoreCompleted(ok);
    return ok;
}

void QOnvifDevice::fetchEndpointReference()
{
    if (!m_client) {
        return;
    }
    const auto epOpt = m_client->getEndpointReference();
    if (epOpt) {
        emit endpointReferenceReceived(QString::fromStdString(*epOpt));
    }
}

} // namespace PelcoD::Qt
