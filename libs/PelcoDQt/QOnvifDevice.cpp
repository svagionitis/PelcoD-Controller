/// @file QOnvifDevice.cpp
/// @brief Implementation of QOnvifDevice Qt 6 adapter.

#include "QOnvifDevice.h"

#include <cstddef>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <QMetaObject>
#include <QThread>

namespace PelcoD::Qt {

QOnvifDevice::QOnvifDevice(QObject* parent)
    : QObject(parent)
{
}

QOnvifDevice::~QOnvifDevice()
{
    if (m_discoveryThread != nullptr) {
        m_discoveryThread->wait();
        delete m_discoveryThread;
        m_discoveryThread = nullptr;
    }
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

QString QOnvifDevice::activeVideoSourceToken() const
{
    return m_activeVideoSourceToken;
}

std::string QOnvifDevice::resolveVideoSourceToken(const QString& videoSourceToken) const
{
    if (!videoSourceToken.isEmpty()) {
        return videoSourceToken.toStdString();
    }
    if (!m_activeVideoSourceToken.isEmpty()) {
        return m_activeVideoSourceToken.toStdString();
    }
    return "VideoSource_1";
}

QString QOnvifDevice::rtspStreamUri() const
{
    return m_rtspStreamUri;
}

QString QOnvifDevice::snapshotUri() const
{
    return m_snapshotUri;
}

std::vector<Onvif::MediaProfile> QOnvifDevice::profiles() const
{
    return m_profiles;
}

std::vector<Onvif::PtzPreset> QOnvifDevice::presets() const
{
    return m_presets;
}

Onvif::DeviceInformation QOnvifDevice::deviceInformation() const
{
    return m_deviceInfo;
}

QList<Onvif::DiscoveredDevice> QOnvifDevice::discoverCameras(int timeoutMs)
{
    const auto stdList = Onvif::OnvifDiscovery::discoverDevices(std::chrono::milliseconds(timeoutMs));

    QList<Onvif::DiscoveredDevice> qList {};
    qList.reserve(static_cast<qsizetype>(stdList.size()));
    for (const auto& dev : stdList) {
        qList.append(dev);
    }
    return qList;
}

void QOnvifDevice::discoverCamerasAsync(int timeoutMs)
{
    if (m_discoveryThread != nullptr) {
        if (m_discoveryThread->isRunning()) {
            return;
        }
        m_discoveryThread->wait();
        delete m_discoveryThread;
        m_discoveryThread = nullptr;
    }

    m_discoveryThread = QThread::create([this, timeoutMs]() {
        const auto result = discoverCameras(timeoutMs);
        QMetaObject::invokeMethod(
            this, [this, result]() { emit discoveryFinished(result); }, ::Qt::QueuedConnection);
    });

    m_discoveryThread->start();
}

bool QOnvifDevice::connectToCamera(const QString& endpoint, const QString& username, const QString& password)
{
    disconnectFromCamera();

    Onvif::SecurityCredentials creds {};
    creds.username = username.toStdString();
    creds.password = password.toStdString();

    {
        std::scoped_lock lock(m_clientMutex);
        m_client = std::make_unique<Onvif::OnvifClient>(endpoint.toStdString(), creds);
    }

    // Synchronize camera clock
    static_cast<void>(m_client->synchronizeSystemTime());

    // Query capabilities
    const auto caps = m_client->getCapabilities();
    if (!caps) {
        emit errorOccurred(tr("Failed to query ONVIF capabilities from %1").arg(endpoint));
        std::scoped_lock lock(m_clientMutex);
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
        std::scoped_lock lock(m_clientMutex);
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
    refreshMasks();
    refreshMaskOptions();
    refreshVideoSourceModes();

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
    m_certificates.clear();
    m_clientCertMode = Onvif::ClientCertificateMode::Off;
    m_recordings.clear();
    m_recordingJobs.clear();
    m_recordingSummary.reset();
    m_replayConfig.reset();
    m_masks.clear();
    m_maskOptions.reset();
    m_videoSourceModes.clear();
    {
        std::scoped_lock lock(m_clientMutex);
        m_client.reset();
    }

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
    refreshStatus();
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
    static_cast<void>(m_client->continuousMove(m_activeProfileToken.toStdString(), panSpeed, tiltSpeed, 0.0));
}

void QOnvifDevice::zoom(int direction, double speed)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    const double z = (direction > 0) ? speed : ((direction < 0) ? -speed : 0.0);
    static_cast<void>(m_client->continuousMove(m_activeProfileToken.toStdString(), 0.0, 0.0, z));
}

void QOnvifDevice::stopMotion(bool stopPanTilt, bool stopZoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(m_client->stop(m_activeProfileToken.toStdString(), stopPanTilt, stopZoom));
}

void QOnvifDevice::absoluteMove(double pan, double tilt, double zoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(m_client->absoluteMove(m_activeProfileToken.toStdString(), pan, tilt, zoom));
}

void QOnvifDevice::absoluteMoveSpherical(double azimuthDeg, double elevationDeg, double zoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(
        m_client->absoluteMoveSpherical(m_activeProfileToken.toStdString(), azimuthDeg, elevationDeg, zoom));
}

void QOnvifDevice::geoMove(double lat, double lon, double elevation, double speed, double areaWidth, double areaHeight)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        emit geoMoveCompleted(false);
        return;
    }

    Onvif::GeoLocation target { lat, lon, elevation };
    std::optional<float> sp = (speed > 0.0) ? std::optional<float>(static_cast<float>(speed)) : std::nullopt;
    std::optional<float> w = (areaWidth > 0.0) ? std::optional<float>(static_cast<float>(areaWidth)) : std::nullopt;
    std::optional<float> h = (areaHeight > 0.0) ? std::optional<float>(static_cast<float>(areaHeight)) : std::nullopt;

    const bool ok = m_client->geoMove(m_activeProfileToken.toStdString(), target, sp, w, h);
    emit geoMoveCompleted(ok);
}

void QOnvifDevice::refreshGeoLocation()
{
    if (!m_client) {
        return;
    }
    const auto loc = m_client->getGeoLocation("Device");
    if (loc.has_value()) {
        m_geoLocation = loc;
        emit geoLocationUpdated(*loc);
    }
}

void QOnvifDevice::updateGeoLocation(const Onvif::LocationEntity& location)
{
    if (!m_client) {
        return;
    }
    if (m_client->setGeoLocation(location)) {
        m_geoLocation = location;
        emit geoLocationUpdated(location);
    }
}

void QOnvifDevice::relativeMove(double pan, double tilt, double zoom)
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(m_client->relativeMove(m_activeProfileToken.toStdString(), pan, tilt, zoom));
}

void QOnvifDevice::gotoHomePosition()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(m_client->gotoHomePosition(m_activeProfileToken.toStdString()));
}

void QOnvifDevice::setHomePosition()
{
    if (!m_client || m_activeProfileToken.isEmpty()) {
        return;
    }
    static_cast<void>(m_client->setHomePosition(m_activeProfileToken.toStdString()));
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

bool QOnvifDevice::operatePresetTour(const QString& tourToken, Onvif::PresetTourOperation operation)
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
    Onvif::PresetTourOperation op = Onvif::PresetTourOperation::Start;
    if (operation.compare("Stop", ::Qt::CaseInsensitive) == 0) {
        op = Onvif::PresetTourOperation::Stop;
    } else if (operation.compare("Pause", ::Qt::CaseInsensitive) == 0) {
        op = Onvif::PresetTourOperation::Pause;
    }
    return operatePresetTour(tourToken, op);
}

bool QOnvifDevice::modifyPresetTour(const Onvif::PresetTour& tour)
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

bool QOnvifDevice::setImagingSettings(const Onvif::ImagingSettings& settings, const QString& videoSourceToken)
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

    static_cast<void>(m_client->moveFocus(token.toStdString(), speed));
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

    static_cast<void>(m_client->stopFocus(token.toStdString()));
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

    static_cast<void>(m_client->moveFocusAbsolute(token.toStdString(), position, speed));
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

    static_cast<void>(m_client->moveFocusRelative(token.toStdString(), distance, speed));
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
    if (m_eventSubActive.load()) {
        return;
    }

    std::optional<std::string> subUrl {};
    {
        std::scoped_lock lock(m_clientMutex);
        if (!m_client) {
            return;
        }
        subUrl = m_client->createPullPointSubscription();
    }
    if (!subUrl || subUrl->empty()) {
        emit errorOccurred(tr("Failed to establish ONVIF PullPoint event subscription."));
        return;
    }

    if (m_eventThread != nullptr) {
        m_eventThread->wait();
        delete m_eventThread;
        m_eventThread = nullptr;
    }

    const std::string subUrlStr { *subUrl };
    m_eventSubscriptionUrl = QString::fromStdString(subUrlStr);
    m_eventSubActive.store(true);

    // Launch managed background worker thread for event polling loop
    m_eventThread = QThread::create([this, pollIntervalMs, subUrlStr]() {
        const int targetSleepMs { std::max(50, pollIntervalMs) };
        while (m_eventSubActive.load()) {
            std::vector<Onvif::OnvifEvent> events {};
            {
                std::scoped_lock lock(m_clientMutex);
                if (!m_client) {
                    break;
                }
                events = m_client->pullMessages(subUrlStr, 1, 10);
            }
            for (const auto& ev : events) {
                if (!m_eventSubActive.load()) {
                    break;
                }
                QMetaObject::invokeMethod(
                    this, [this, ev]() { emit eventReceived(ev); }, ::Qt::QueuedConnection);
            }
            for (int elapsed = 0; elapsed < targetSleepMs && m_eventSubActive.load(); elapsed += 50) {
                QThread::msleep(50UL);
            }
        }
    });

    m_eventThread->start();
}

void QOnvifDevice::stopEventSubscription()
{
    m_eventSubActive.store(false);
    if (m_eventThread != nullptr) {
        m_eventThread->wait();
        delete m_eventThread;
        m_eventThread = nullptr;
    }

    if (!m_eventSubscriptionUrl.isEmpty()) {
        const std::string subUrlStr { m_eventSubscriptionUrl.toStdString() };
        m_eventSubscriptionUrl.clear();
        std::scoped_lock lock(m_clientMutex);
        if (m_client) {
            static_cast<void>(m_client->unsubscribe(subUrlStr));
        }
    }
}

bool QOnvifDevice::isEventSubscriptionActive() const
{
    return m_eventSubActive.load();
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

QString QOnvifDevice::createOSD(const Onvif::OsdConfig& osd)
{
    if (!m_client) {
        return QString();
    }
    Onvif::OsdConfig cfg = osd;
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

bool QOnvifDevice::setOSD(const Onvif::OsdConfig& osd)
{
    if (!m_client) {
        return false;
    }
    Onvif::OsdConfig cfg = osd;
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

bool QOnvifDevice::createUser(const Onvif::OnvifUser& user)
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

bool QOnvifDevice::setUser(const Onvif::OnvifUser& user)
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

bool QOnvifDevice::setNetworkInterface(const Onvif::NetworkInterfaceConfig& config)
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

bool QOnvifDevice::setDNS(const Onvif::DnsConfig& dns)
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

bool QOnvifDevice::setNTP(const Onvif::NtpConfig& ntp)
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

bool QOnvifDevice::setSystemDateAndTime(const Onvif::SystemDateTimeConfig& dt)
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
    const bool ok
        = m_client->setSystemFactoryDefault(hard ? Onvif::FactoryDefaultType::Hard : Onvif::FactoryDefaultType::Soft);
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

    const auto state = active ? Onvif::RelayLogicalState::Active : Onvif::RelayLogicalState::Inactive;
    const bool ok = m_client->setRelayOutputState(relayToken.toStdString(), state);
    if (ok) {
        refreshRelayOutputs();
    }
    return ok;
}

bool QOnvifDevice::setRelayOutputSettings(const QString& relayToken, const Onvif::RelayOutputConfig& settings)
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

bool QOnvifDevice::setMetadataConfiguration(const Onvif::MetadataConfiguration& config)
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
    if (m_metadataStreamingActive.load()) {
        return;
    }
    {
        std::scoped_lock lock(m_clientMutex);
        if (!m_client) {
            return;
        }
    }

    if (m_metadataThread != nullptr) {
        m_metadataThread->wait();
        delete m_metadataThread;
        m_metadataThread = nullptr;
    }

    m_metadataStreamingActive.store(true);

    m_metadataThread = QThread::create([this, intervalMs]() {
        const int targetSleepMs { std::max(100, intervalMs) };
        while (m_metadataStreamingActive.load()) {
            std::optional<Onvif::MetadataStreamPayload> payload {};
            {
                std::scoped_lock lock(m_clientMutex);
                if (!m_client) {
                    break;
                }
                payload = m_client->getMetadataStream();
            }
            if (payload && m_metadataStreamingActive.load()) {
                QMetaObject::invokeMethod(
                    this, [this, payload]() { emit metadataReceived(*payload); }, ::Qt::QueuedConnection);
            }
            for (int elapsed = 0; elapsed < targetSleepMs && m_metadataStreamingActive.load(); elapsed += 50) {
                QThread::msleep(50UL);
            }
        }
    });

    m_metadataThread->start();
}

void QOnvifDevice::stopMetadataStreaming()
{
    m_metadataStreamingActive.store(false);
    if (m_metadataThread != nullptr) {
        m_metadataThread->wait();
        delete m_metadataThread;
        m_metadataThread = nullptr;
    }
}

bool QOnvifDevice::isMetadataStreamingActive() const
{
    return m_metadataStreamingActive.load();
}

void QOnvifDevice::pollMetadata()
{
    pollCurrentMetadata();
}

void QOnvifDevice::fetchSystemLog(Onvif::SystemLogType logType)
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

// =========================================================================
// PKI Certificates & HTTPS/TLS Security Service
// =========================================================================

void QOnvifDevice::refreshCertificates()
{
    if (!m_client) {
        return;
    }
    m_certificates = m_client->getCertificates();
    emit certificatesUpdated(m_certificates);
}

void QOnvifDevice::fetchCertificateInformation(const QString& certificateId)
{
    if (!m_client) {
        return;
    }
    const auto infoOpt = m_client->getCertificateInformation(certificateId.toStdString());
    if (infoOpt) {
        emit certificateInfoReceived(*infoOpt);
    }
}

bool QOnvifDevice::createCertificate(const QString& certificateId, const QString& subject, int daysValid)
{
    if (!m_client) {
        return false;
    }
    const auto certOpt = m_client->createCertificate(certificateId.toStdString(), subject.toStdString(), daysValid);
    if (certOpt) {
        refreshCertificates();
        return true;
    }
    return false;
}

bool QOnvifDevice::createPkcs10Csr(const QString& certificateId, const QString& subject)
{
    if (!m_client) {
        return false;
    }
    const auto csrOpt = m_client->getPkcs10Request(certificateId.toStdString(), subject.toStdString());
    if (csrOpt) {
        emit pkcs10CsrReceived(*csrOpt);
        return true;
    }
    return false;
}

bool QOnvifDevice::loadCertificates(const std::vector<Onvif::OnvifCertificate>& certificates)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->loadCertificates(certificates);
    if (ok) {
        refreshCertificates();
    }
    return ok;
}

bool QOnvifDevice::deleteCertificates(const QStringList& certificateIds)
{
    if (!m_client) {
        return false;
    }
    std::vector<std::string> ids;
    ids.reserve(static_cast<std::size_t>(certificateIds.size()));
    for (const auto& id : certificateIds) {
        ids.push_back(id.toStdString());
    }
    const bool ok = m_client->deleteCertificates(ids);
    if (ok) {
        refreshCertificates();
    }
    return ok;
}

void QOnvifDevice::refreshClientCertificateMode()
{
    if (!m_client) {
        return;
    }
    const auto modeOpt = m_client->getClientCertificateMode();
    if (modeOpt) {
        m_clientCertMode = *modeOpt;
        emit clientCertificateModeUpdated(m_clientCertMode);
    }
}

bool QOnvifDevice::setClientCertificateMode(Onvif::ClientCertificateMode mode)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setClientCertificateMode(mode);
    if (ok) {
        m_clientCertMode = mode;
        emit clientCertificateModeUpdated(m_clientCertMode);
    }
    return ok;
}

// =========================================================================
// Profile G: Recording Service
// =========================================================================

void QOnvifDevice::refreshRecordings()
{
    if (!m_client) {
        return;
    }
    m_recordings = m_client->getRecordings();
    emit recordingsUpdated(m_recordings);
}

QString QOnvifDevice::createRecording(const Onvif::RecordingConfig& config)
{
    if (!m_client) {
        return {};
    }
    const auto tokOpt = m_client->createRecording(config);
    if (tokOpt) {
        refreshRecordings();
        return QString::fromStdString(*tokOpt);
    }
    return {};
}

bool QOnvifDevice::setRecordingConfiguration(const Onvif::RecordingConfig& config)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setRecordingConfiguration(config);
    if (ok) {
        refreshRecordings();
    }
    return ok;
}

bool QOnvifDevice::deleteRecording(const QString& recordingToken)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->deleteRecording(recordingToken.toStdString());
    if (ok) {
        refreshRecordings();
    }
    return ok;
}

QString QOnvifDevice::createTrack(const QString& recordingToken, const Onvif::RecordingTrack& track)
{
    if (!m_client) {
        return {};
    }
    const auto trkOpt = m_client->createTrack(recordingToken.toStdString(), track);
    if (trkOpt) {
        refreshRecordings();
        return QString::fromStdString(*trkOpt);
    }
    return {};
}

bool QOnvifDevice::deleteTrack(const QString& recordingToken, const QString& trackToken)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->deleteTrack(recordingToken.toStdString(), trackToken.toStdString());
    if (ok) {
        refreshRecordings();
    }
    return ok;
}

void QOnvifDevice::refreshRecordingJobs()
{
    if (!m_client) {
        return;
    }
    m_recordingJobs = m_client->getRecordingJobs();
    emit recordingJobsUpdated(m_recordingJobs);
}

QString QOnvifDevice::createRecordingJob(const Onvif::RecordingJob& job)
{
    if (!m_client) {
        return {};
    }
    const auto jobOpt = m_client->createRecordingJob(job);
    if (jobOpt) {
        refreshRecordingJobs();
        return QString::fromStdString(*jobOpt);
    }
    return {};
}

bool QOnvifDevice::setRecordingJobMode(const QString& jobToken, Onvif::RecordingJobMode mode)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setRecordingJobMode(jobToken.toStdString(), mode);
    if (ok) {
        refreshRecordingJobs();
    }
    return ok;
}

bool QOnvifDevice::deleteRecordingJob(const QString& jobToken)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->deleteRecordingJob(jobToken.toStdString());
    if (ok) {
        refreshRecordingJobs();
    }
    return ok;
}

void QOnvifDevice::refreshRecordingSummary()
{
    if (!m_client) {
        return;
    }
    m_recordingSummary = m_client->getRecordingSummary();
    if (m_recordingSummary) {
        emit recordingSummaryUpdated(*m_recordingSummary);
    }
}

// =========================================================================
// Profile G: Search Service
// =========================================================================

QString QOnvifDevice::findRecordings(const QString& scope, int maxMatches)
{
    if (!m_client) {
        return {};
    }
    const auto tokOpt = m_client->findRecordings(scope.toStdString(), maxMatches);
    if (tokOpt) {
        return QString::fromStdString(*tokOpt);
    }
    return {};
}

void QOnvifDevice::refreshRecordingSearchResults(const QString& searchToken)
{
    if (!m_client || searchToken.isEmpty()) {
        return;
    }
    const auto results = m_client->getRecordingSearchResults(searchToken.toStdString());
    emit recordingSearchResultsReceived(searchToken, results);
}

QString QOnvifDevice::findEvents(const QString& startUtc, const QString& endUtc, int maxMatches)
{
    if (!m_client) {
        return {};
    }
    const auto tokOpt = m_client->findEvents(startUtc.toStdString(), endUtc.toStdString(), maxMatches);
    if (tokOpt) {
        return QString::fromStdString(*tokOpt);
    }
    return {};
}

void QOnvifDevice::refreshEventSearchResults(const QString& searchToken)
{
    if (!m_client || searchToken.isEmpty()) {
        return;
    }
    const auto results = m_client->getEventSearchResults(searchToken.toStdString());
    emit eventSearchResultsReceived(searchToken, results);
}

bool QOnvifDevice::endSearch(const QString& searchToken)
{
    if (!m_client || searchToken.isEmpty()) {
        return false;
    }
    return m_client->endSearch(searchToken.toStdString());
}

// =========================================================================
// Profile G: Replay Service
// =========================================================================

void QOnvifDevice::resolveReplayUri(const QString& recordingToken, const QString& streamType)
{
    if (!m_client) {
        return;
    }
    const auto uriOpt = m_client->getReplayUri(recordingToken.toStdString(), streamType.toStdString());
    if (uriOpt) {
        emit replayUriResolved(recordingToken, QString::fromStdString(*uriOpt));
    }
}

void QOnvifDevice::refreshReplayConfiguration()
{
    if (!m_client) {
        return;
    }
    m_replayConfig = m_client->getReplayConfiguration();
    if (m_replayConfig) {
        emit replayConfigurationUpdated(*m_replayConfig);
    }
}

bool QOnvifDevice::setReplayConfiguration(const Onvif::ReplayConfiguration& config)
{
    if (!m_client) {
        return false;
    }
    const bool ok = m_client->setReplayConfiguration(config);
    if (ok) {
        m_replayConfig = config;
        emit replayConfigurationUpdated(config);
    }
    return ok;
}

// =========================================================================
// Profile M & Profile T: Video Analytics Rule Engine & Modules
// =========================================================================

void QOnvifDevice::refreshSupportedRules()
{
    if (!m_client) {
        return;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    m_supportedRules = m_client->getSupportedRules(cfgToken);
    emit supportedRulesUpdated(m_supportedRules);
}

void QOnvifDevice::refreshRules()
{
    if (!m_client) {
        return;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    m_rules = m_client->getRules(cfgToken);
    emit rulesUpdated(m_rules);
}

bool QOnvifDevice::createRules(const std::vector<Onvif::AnalyticsRule>& rules)
{
    if (!m_client) {
        return false;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->createRules(cfgToken, rules) };
    if (ok) {
        refreshRules();
    }
    return ok;
}

bool QOnvifDevice::modifyRules(const std::vector<Onvif::AnalyticsRule>& rules)
{
    if (!m_client) {
        return false;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->modifyRules(cfgToken, rules) };
    if (ok) {
        refreshRules();
    }
    return ok;
}

bool QOnvifDevice::deleteRules(const QStringList& ruleNames)
{
    if (!m_client) {
        return false;
    }
    std::vector<std::string> names {};
    names.reserve(static_cast<std::size_t>(ruleNames.size()));
    for (const auto& n : ruleNames) {
        names.push_back(n.toStdString());
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->deleteRules(cfgToken, names) };
    if (ok) {
        refreshRules();
    }
    return ok;
}

void QOnvifDevice::refreshSupportedModules()
{
    if (!m_client) {
        return;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    m_supportedModules = m_client->getSupportedAnalyticsModules(cfgToken);
    emit supportedModulesUpdated(m_supportedModules);
}

void QOnvifDevice::refreshAnalyticsModules()
{
    if (!m_client) {
        return;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    m_analyticsModules = m_client->getAnalyticsModules(cfgToken);
    emit analyticsModulesUpdated(m_analyticsModules);
}

bool QOnvifDevice::createAnalyticsModules(const std::vector<Onvif::AnalyticsModule>& modules)
{
    if (!m_client) {
        return false;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->createAnalyticsModules(cfgToken, modules) };
    if (ok) {
        refreshAnalyticsModules();
    }
    return ok;
}

bool QOnvifDevice::modifyAnalyticsModules(const std::vector<Onvif::AnalyticsModule>& modules)
{
    if (!m_client) {
        return false;
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->modifyAnalyticsModules(cfgToken, modules) };
    if (ok) {
        refreshAnalyticsModules();
    }
    return ok;
}

bool QOnvifDevice::deleteAnalyticsModules(const QStringList& moduleNames)
{
    if (!m_client) {
        return false;
    }
    std::vector<std::string> names {};
    names.reserve(static_cast<std::size_t>(moduleNames.size()));
    for (const auto& n : moduleNames) {
        names.push_back(n.toStdString());
    }
    const std::string cfgToken { m_activeAnalyticsToken.toStdString() };
    const bool ok { m_client->deleteAnalyticsModules(cfgToken, names) };
    if (ok) {
        refreshAnalyticsModules();
    }
    return ok;
}

// =========================================================================
// Profile T: Privacy Masks & Video Source Modes
// =========================================================================

void QOnvifDevice::refreshMasks(const QString& configToken)
{
    if (!m_client) {
        return;
    }
    const std::string token { resolveVideoSourceToken(configToken) };
    m_masks = m_client->getMasks(token);
    emit masksUpdated(m_masks);
}

void QOnvifDevice::refreshMaskOptions(const QString& configToken)
{
    if (!m_client) {
        return;
    }
    const std::string token { resolveVideoSourceToken(configToken) };
    m_maskOptions = m_client->getMaskOptions(token);
    if (m_maskOptions.has_value()) {
        emit maskOptionsUpdated(*m_maskOptions);
    }
}

QString QOnvifDevice::createMask(const Onvif::PrivacyMask& mask)
{
    if (!m_client) {
        return QString();
    }
    Onvif::PrivacyMask reqMask { mask };
    if (reqMask.configurationToken.empty()) {
        reqMask.configurationToken = resolveVideoSourceToken(QString());
    }
    const std::string token { m_client->createMask(reqMask) };
    if (!token.empty()) {
        refreshMasks(QString::fromStdString(reqMask.configurationToken));
    }
    return QString::fromStdString(token);
}

bool QOnvifDevice::setMask(const Onvif::PrivacyMask& mask)
{
    if (!m_client) {
        return false;
    }
    Onvif::PrivacyMask reqMask { mask };
    if (reqMask.configurationToken.empty()) {
        reqMask.configurationToken = resolveVideoSourceToken(QString());
    }
    const bool ok { m_client->setMask(reqMask) };
    if (ok) {
        refreshMasks(QString::fromStdString(reqMask.configurationToken));
    }
    return ok;
}

bool QOnvifDevice::deleteMask(const QString& maskToken)
{
    if (!m_client) {
        return false;
    }
    const bool ok { m_client->deleteMask(maskToken.toStdString()) };
    if (ok) {
        refreshMasks();
    }
    return ok;
}

void QOnvifDevice::refreshVideoSourceModes(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }
    const std::string token { resolveVideoSourceToken(videoSourceToken) };
    m_videoSourceModes = m_client->getVideoSourceModes(token);
    emit videoSourceModesUpdated(m_videoSourceModes);
}

bool QOnvifDevice::setVideoSourceMode(const QString& videoSourceToken, const QString& modeToken)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->setVideoSourceMode(vs, modeToken.toStdString()) };
    if (ok) {
        bool reboot { false };
        for (const auto& m : m_videoSourceModes) {
            if (m.token == modeToken.toStdString()) {
                reboot = m.reboot;
                break;
            }
        }
        emit videoSourceModeChanged(modeToken, reboot);
        refreshVideoSourceModes(QString::fromStdString(vs));
    }
    return ok;
}

void QOnvifDevice::refreshRadiometryConfig(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const auto cfg { m_client->getRadiometryConfiguration(vs) };
    if (cfg) {
        m_radiometryConfig = *cfg;
        emit radiometryConfigUpdated(m_radiometryConfig);
    }
}

bool QOnvifDevice::setRadiometryConfiguration(const QString& videoSourceToken, const Onvif::RadiometryConfig& config)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->setRadiometryConfiguration(vs, config) };
    if (ok) {
        m_radiometryConfig = config;
        emit radiometryConfigUpdated(m_radiometryConfig);
    }
    return ok;
}

void QOnvifDevice::refreshRadiometryMeasurements(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    m_radiometrySpots = m_client->getRadiometrySpots(vs);
    m_radiometryBoxes = m_client->getRadiometryBoxes(vs);
    emit radiometrySpotsUpdated(m_radiometrySpots);
    emit radiometryBoxesUpdated(m_radiometryBoxes);
}

bool QOnvifDevice::setRadiometrySpots(const QString& videoSourceToken, const std::vector<Onvif::RadiometrySpot>& spots)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->setRadiometrySpots(vs, spots) };
    if (ok) {
        m_radiometrySpots = spots;
        emit radiometrySpotsUpdated(m_radiometrySpots);
    }
    return ok;
}

bool QOnvifDevice::setRadiometryBoxes(const QString& videoSourceToken, const std::vector<Onvif::RadiometryBox>& boxes)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->setRadiometryBoxes(vs, boxes) };
    if (ok) {
        m_radiometryBoxes = boxes;
        emit radiometryBoxesUpdated(m_radiometryBoxes);
    }
    return ok;
}

void QOnvifDevice::refreshColorPalettes(const QString& videoSourceToken)
{
    if (!m_client) {
        return;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    m_colorPalettes = m_client->getColorPalettes(vs);
    emit colorPalettesUpdated(m_colorPalettes);
}

bool QOnvifDevice::setColorPalette(const QString& videoSourceToken, const QString& paletteToken)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->setColorPalette(vs, paletteToken.toStdString()) };
    if (ok) {
        refreshColorPalettes(QString::fromStdString(vs));
    }
    return ok;
}

bool QOnvifDevice::triggerNuc(const QString& videoSourceToken)
{
    if (!m_client) {
        return false;
    }
    const std::string vs { resolveVideoSourceToken(videoSourceToken) };
    const bool ok { m_client->triggerNuc(vs) };
    emit nucTriggered(ok);
    return ok;
}

} // namespace PelcoD::Qt
