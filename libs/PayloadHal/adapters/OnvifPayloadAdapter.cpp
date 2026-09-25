#include "OnvifPayloadAdapter.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

    constexpr double kPi { 3.14159265358979323846 };
    constexpr double kWideHfovDeg { 60.0 };

    inline double calculateHfov(double zoomFactor) noexcept
    {
        const double clampedFactor = std::max(1.0, zoomFactor);
        const double wideHfovRad = kWideHfovDeg * (kPi / 180.0);
        const double currentHfovRad = 2.0 * std::atan(std::tan(wideHfovRad / 2.0) / clampedFactor);
        return currentHfovRad * (180.0 / kPi);
    }

    inline double calculateVfov(double hfovDeg) noexcept
    {
        const double hfovRad = hfovDeg * (kPi / 180.0);
        const double vfovRad = 2.0 * std::atan(std::tan(hfovRad / 2.0) * (9.0 / 16.0));
        return vfovRad * (180.0 / kPi);
    }

} // namespace

// =============================================================================
// OnvifPtuAdapter Implementation
// =============================================================================

OnvifPtuAdapter::OnvifPtuAdapter(std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken)
    : m_client(std::move(client))
    , m_profileToken(std::move(profileToken))
{
}

bool OnvifPtuAdapter::connect()
{
    if (!m_client) {
        return false;
    }

    if (m_profileToken.empty()) {
        const auto profiles = m_client->getProfiles();
        if (!profiles.empty()) {
            m_profileToken = profiles.front().token;
        }
    }

    const auto caps = m_client->getCapabilities();
    if (caps.has_value() && caps->ptzXAddr.empty()) {
        m_ptzSupported = false;
    }

    updateTelemetry();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected = true;
        if (m_stateCallback) {
            m_stateCallback(DeviceState::Ready, "ONVIF PTU Connected");
        }
    }
    return true;
}

void OnvifPtuAdapter::disconnect()
{
    if (m_connected && m_ptzSupported && !m_profileToken.empty()) {
        stopMotion();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected = false;
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "ONVIF PTU Disconnected");
    }
}

bool OnvifPtuAdapter::isConnected() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected;
}

DeviceState OnvifPtuAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_client) {
        return DeviceState::Fault;
    }
    if (!m_connected) {
        return DeviceState::Disconnected;
    }
    return m_ptzSupported ? DeviceState::Ready : DeviceState::Degraded;
}

DeviceInfo OnvifPtuAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "ONVIF";
    d.model = "ONVIF PTZ Subsystem";
    d.firmwareVersion = "1.0.0";
    if (m_client) {
        const auto devInfo = m_client->getDeviceInformation();
        if (devInfo.has_value()) {
            if (!devInfo->manufacturer.empty())
                d.manufacturer = devInfo->manufacturer;
            if (!devInfo->model.empty())
                d.model = devInfo->model;
            if (!devInfo->serialNumber.empty())
                d.serialNumber = devInfo->serialNumber;
            if (!devInfo->firmwareVersion.empty())
                d.firmwareVersion = devInfo->firmwareVersion;
        }
    }
    return d;
}

void OnvifPtuAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

bool OnvifPtuAdapter::setRate(double panDegPerSec, double tiltDegPerSec)
{
    constexpr double kMaxPanRateDegSec { 60.0 };
    constexpr double kMaxTiltRateDegSec { 30.0 };
    const float normPan = static_cast<float>(std::clamp(panDegPerSec / kMaxPanRateDegSec, -1.0, 1.0));
    const float normTilt = static_cast<float>(std::clamp(tiltDegPerSec / kMaxTiltRateDegSec, -1.0, 1.0));
    return setNormalizedVelocity(normPan, normTilt);
}

bool OnvifPtuAdapter::setNormalizedVelocity(float panVel, float tiltVel)
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    const float clampedPan = std::clamp(panVel, -1.0f, 1.0f);
    const float clampedTilt = std::clamp(tiltVel, -1.0f, 1.0f);

    const bool ok = m_client->continuousMove(m_profileToken, clampedPan, clampedTilt, 0.0);
    if (ok) {
        GimbalTelemetry telem {};
        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.panRateDegPerSec = static_cast<double>(clampedPan) * 60.0;
            m_telemetry.tiltRateDegPerSec = static_cast<double>(clampedTilt) * 30.0;
            m_telemetry.isMoving = (std::abs(clampedPan) > 0.01f || std::abs(clampedTilt) > 0.01f);
            m_telemetry.timestamp = std::chrono::system_clock::now();
            telem = m_telemetry;
            cbCopy = m_telemetryCallback;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }
    return ok;
}

bool OnvifPtuAdapter::setAbsoluteAngles(double panDeg, double tiltDeg)
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    double normPan = std::fmod(panDeg, 360.0);
    if (normPan < 0.0)
        normPan += 360.0;
    const double clampedTilt = std::clamp(tiltDeg, -90.0, 90.0);

    const bool ok = m_client->absoluteMoveSpherical(m_profileToken, normPan, clampedTilt, 0.0);
    if (ok) {
        GimbalTelemetry telem {};
        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.panAngleDeg = normPan;
            m_telemetry.tiltAngleDeg = clampedTilt;
            m_telemetry.timestamp = std::chrono::system_clock::now();
            telem = m_telemetry;
            cbCopy = m_telemetryCallback;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }
    return ok;
}

bool OnvifPtuAdapter::setRelativeNudge(double deltaPanDeg, double deltaTiltDeg)
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    const double normDeltaPan = deltaPanDeg / 180.0;
    const double normDeltaTilt = deltaTiltDeg / 90.0;
    const bool ok = m_client->relativeMove(m_profileToken, normDeltaPan, normDeltaTilt, 0.0);
    if (ok) {
        GimbalTelemetry telem {};
        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.panAngleDeg = std::fmod(m_telemetry.panAngleDeg + deltaPanDeg + 360.0, 360.0);
            m_telemetry.tiltAngleDeg = std::clamp(m_telemetry.tiltAngleDeg + deltaTiltDeg, -90.0, 90.0);
            m_telemetry.timestamp = std::chrono::system_clock::now();
            telem = m_telemetry;
            cbCopy = m_telemetryCallback;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }
    return ok;
}

bool OnvifPtuAdapter::stopMotion()
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    const bool ok = m_client->stop(m_profileToken, true, false);
    if (ok) {
        GimbalTelemetry telem {};
        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.panRateDegPerSec = 0.0;
            m_telemetry.tiltRateDegPerSec = 0.0;
            m_telemetry.isMoving = false;
            m_telemetry.timestamp = std::chrono::system_clock::now();
            telem = m_telemetry;
            cbCopy = m_telemetryCallback;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }
    return ok;
}

bool OnvifPtuAdapter::supportsStabilization() const noexcept
{
    return false;
}

bool OnvifPtuAdapter::setStabilizationMode(StabilizationMode mode)
{
    return mode == StabilizationMode::Disabled;
}

StabilizationMode OnvifPtuAdapter::stabilizationMode() const noexcept
{
    return StabilizationMode::Disabled;
}

bool OnvifPtuAdapter::zeroGyroDrift()
{
    return false;
}

bool OnvifPtuAdapter::getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const
{
    minPan = 0.0;
    maxPan = 360.0;
    minTilt = -90.0;
    maxTilt = 90.0;
    return true;
}

bool OnvifPtuAdapter::savePreset(std::uint8_t presetId, const std::string& name)
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    const std::string presetName = name.empty() ? ("Preset_" + std::to_string(presetId)) : name;
    const auto token = m_client->setPreset(m_profileToken, presetName, std::to_string(presetId));
    return token.has_value();
}

bool OnvifPtuAdapter::recallPreset(std::uint8_t presetId)
{
    if (!m_client || !m_ptzSupported) {
        return false;
    }
    return m_client->gotoPreset(m_profileToken, std::to_string(presetId));
}

void OnvifPtuAdapter::registerTelemetryCallback(TelemetryCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetryCallback = std::move(cb);
}

GimbalTelemetry OnvifPtuAdapter::currentTelemetry() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_telemetry;
}

void OnvifPtuAdapter::updateTelemetry()
{
    if (!m_client || !m_ptzSupported || m_profileToken.empty()) {
        return;
    }
    const auto status = m_client->getStatus(m_profileToken);
    if (!status.has_value()) {
        return;
    }

    GimbalTelemetry telem {};
    TelemetryCallback cbCopy {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (status->pan >= 0.0 && status->pan <= 360.0) {
            m_telemetry.panAngleDeg = status->pan;
        } else {
            m_telemetry.panAngleDeg = std::clamp((status->pan + 1.0) * 180.0, 0.0, 360.0);
        }
        if (status->tilt >= -90.0 && status->tilt <= 90.0) {
            m_telemetry.tiltAngleDeg = status->tilt;
        } else {
            m_telemetry.tiltAngleDeg = std::clamp(status->tilt * 90.0, -90.0, 90.0);
        }
        m_telemetry.isMoving = status->isMoving;
        m_telemetry.timestamp = std::chrono::system_clock::now();
        telem = m_telemetry;
        cbCopy = m_telemetryCallback;
    }
    if (cbCopy) {
        cbCopy(telem);
    }
}

void OnvifPtuAdapter::setProfileToken(std::string profileToken)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profileToken = std::move(profileToken);
}

const std::string& OnvifPtuAdapter::profileToken() const noexcept
{
    return m_profileToken;
}

// =============================================================================
// OnvifCameraAdapter Implementation
// =============================================================================

OnvifCameraAdapter::OnvifCameraAdapter(
    std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken, std::string videoSourceToken)
    : m_client(std::move(client))
    , m_profileToken(std::move(profileToken))
    , m_videoSourceToken(std::move(videoSourceToken))
{
    m_telemetry.opticalZoomFactor = 1.0;
    m_telemetry.horizontalFovDeg = kWideHfovDeg;
    m_telemetry.verticalFovDeg = calculateVfov(kWideHfovDeg);
}

bool OnvifCameraAdapter::connect()
{
    if (!m_client) {
        return false;
    }

    if (m_profileToken.empty() || m_videoSourceToken.empty()) {
        const auto profiles = m_client->getProfiles();
        if (!profiles.empty()) {
            if (m_profileToken.empty()) {
                m_profileToken = profiles.front().token;
            }
            if (m_videoSourceToken.empty()) {
                m_videoSourceToken = profiles.front().videoSourceToken;
            }
        }
    }

    if (!m_profileToken.empty()) {
        const auto streamUri = m_client->getStreamUri(m_profileToken);
        if (streamUri.has_value()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_streamUri = streamUri->uri;
        }
    }

    updateTelemetry();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected = true;
        if (m_stateCallback) {
            m_stateCallback(DeviceState::Ready, "ONVIF Camera Connected");
        }
    }
    return true;
}

void OnvifCameraAdapter::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected = false;
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "ONVIF Camera Disconnected");
    }
}

bool OnvifCameraAdapter::isConnected() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected;
}

DeviceState OnvifCameraAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_client) {
        return DeviceState::Fault;
    }
    return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo OnvifCameraAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "ONVIF";
    d.model = "ONVIF Optical Sensor";
    d.firmwareVersion = "1.0.0";
    if (m_client) {
        const auto devInfo = m_client->getDeviceInformation();
        if (devInfo.has_value()) {
            if (!devInfo->manufacturer.empty())
                d.manufacturer = devInfo->manufacturer;
            if (!devInfo->model.empty())
                d.model = devInfo->model;
            if (!devInfo->serialNumber.empty())
                d.serialNumber = devInfo->serialNumber;
            if (!devInfo->firmwareVersion.empty())
                d.firmwareVersion = devInfo->firmwareVersion;
        }
    }
    return d;
}

void OnvifCameraAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

CameraSpectrum OnvifCameraAdapter::spectrum() const noexcept
{
    return CameraSpectrum::DaylightVisible;
}

bool OnvifCameraAdapter::setZoomNormalized(double zoom01)
{
    if (!m_client || m_profileToken.empty()) {
        return false;
    }
    const double clampedZoom = std::clamp(zoom01, 0.0, 1.0);
    const bool ok = m_client->absoluteMove(m_profileToken, 0.0, 0.0, clampedZoom);
    if (ok) {
        CameraTelemetry telem {};
        TelemetryCallback cbCopy {};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.normalizedZoom = clampedZoom;
            m_telemetry.opticalZoomFactor = 1.0 + clampedZoom * 29.0;
            m_telemetry.horizontalFovDeg = calculateHfov(m_telemetry.opticalZoomFactor);
            m_telemetry.verticalFovDeg = calculateVfov(m_telemetry.horizontalFovDeg);
            m_telemetry.timestamp = std::chrono::system_clock::now();
            telem = m_telemetry;
            cbCopy = m_telemetryCallback;
        }
        if (cbCopy) {
            cbCopy(telem);
        }
    }
    return ok;
}

bool OnvifCameraAdapter::zoomContinuous(float velocity)
{
    if (!m_client || m_profileToken.empty()) {
        return false;
    }
    const float clampedVel = std::clamp(velocity, -1.0f, 1.0f);
    return m_client->continuousMove(m_profileToken, 0.0, 0.0, clampedVel);
}

bool OnvifCameraAdapter::zoomStop()
{
    if (!m_client || m_profileToken.empty()) {
        return false;
    }
    return m_client->stop(m_profileToken, false, true);
}

bool OnvifCameraAdapter::setFocusAuto(bool enable)
{
    if (!m_client) {
        return false;
    }
    Onvif::ImagingSettings settings {};
    settings.autoFocusMode = enable ? "AUTO" : "MANUAL";
    const bool ok = m_client->setImagingSettings(m_videoSourceToken, settings);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.autoFocusActive = enable;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool OnvifCameraAdapter::setFocusNormalized(double focus01)
{
    if (!m_client) {
        return false;
    }
    const float pos = static_cast<float>(std::clamp(focus01, 0.0, 1.0));
    const bool ok = m_client->moveFocusAbsolute(m_videoSourceToken, pos);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.focusDistanceNormalized = pos;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool OnvifCameraAdapter::focusContinuous(float velocity)
{
    if (!m_client || m_videoSourceToken.empty()) {
        return false;
    }
    const float clampedVel = std::clamp(velocity, -1.0f, 1.0f);
    return m_client->moveFocusContinuous(m_videoSourceToken, clampedVel);
}

bool OnvifCameraAdapter::focusStop()
{
    if (!m_client || m_videoSourceToken.empty()) {
        return false;
    }
    return m_client->stopFocus(m_videoSourceToken);
}

bool OnvifCameraAdapter::triggerOnePushFocus()
{
    if (!m_client) {
        return false;
    }
    return m_client->moveFocus(m_videoSourceToken, 0.0f);
}

bool OnvifCameraAdapter::setIrisAuto(bool enable)
{
    if (!m_client || m_videoSourceToken.empty()) {
        return false;
    }
    Onvif::ImagingSettings settings {};
    settings.exposure.mode = enable ? "AUTO" : "MANUAL";
    const bool ok = m_client->setImagingSettings(m_videoSourceToken, settings);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.autoIrisActive = enable;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool OnvifCameraAdapter::setIrisNormalized(double iris01)
{
    if (!m_client || m_videoSourceToken.empty()) {
        return false;
    }
    const double clamped = std::clamp(iris01, 0.0, 1.0);
    Onvif::ImagingSettings settings {};
    settings.exposure.mode = "MANUAL";
    settings.exposure.iris = static_cast<float>(clamped * 100.0);
    const bool ok = m_client->setImagingSettings(m_videoSourceToken, settings);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.irisNormalized = clamped;
        m_telemetry.autoIrisActive = false;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool OnvifCameraAdapter::irisContinuous(float velocity)
{
    if (!m_client || m_videoSourceToken.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::abs(velocity) <= 0.05f) {
        return true;
    }
    const double stepDelta = (velocity > 0.0f ? 0.05 : -0.05);
    const double newIris = std::clamp(m_telemetry.irisNormalized + stepDelta, 0.0, 1.0);
    m_telemetry.irisNormalized = newIris;
    m_telemetry.autoIrisActive = false;
    m_telemetry.timestamp = std::chrono::system_clock::now();
    Onvif::ImagingSettings settings {};
    settings.exposure.mode = "MANUAL";
    settings.exposure.iris = static_cast<float>(newIris * 100.0);
    return m_client->setImagingSettings(m_videoSourceToken, settings);
}

bool OnvifCameraAdapter::irisStop()
{
    return m_client != nullptr;
}

bool OnvifCameraAdapter::setDayNightIcr(bool nightMode)
{
    if (!m_client) {
        return false;
    }
    Onvif::ImagingSettings settings {};
    settings.irCutFilter = nightMode ? "OFF" : "ON";
    const bool ok = m_client->setImagingSettings(m_videoSourceToken, settings);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.dayNightIcrActive = nightMode;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool OnvifCameraAdapter::setDefog(bool enable)
{
    if (!m_client) {
        return false;
    }
    Onvif::ImagingSettings settings {};
    settings.wideDynamicRange = enable;
    return m_client->setImagingSettings(m_videoSourceToken, settings);
}

bool OnvifCameraAdapter::setStabilizer(bool /*enable*/)
{
    return false;
}

void OnvifCameraAdapter::registerTelemetryCallback(TelemetryCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetryCallback = std::move(cb);
}

CameraTelemetry OnvifCameraAdapter::currentTelemetry() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_telemetry;
}

std::string OnvifCameraAdapter::videoStreamUri() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_streamUri.empty()) {
        return m_streamUri;
    }
    if (m_client && !m_profileToken.empty()) {
        const auto uri = m_client->getStreamUri(m_profileToken);
        if (uri.has_value()) {
            m_streamUri = uri->uri;
            return m_streamUri;
        }
    }
    return {};
}

void OnvifCameraAdapter::updateTelemetry()
{
    if (!m_client) {
        return;
    }
    if (!m_profileToken.empty()) {
        const auto ptzSt = m_client->getStatus(m_profileToken);
        if (ptzSt.has_value()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.normalizedZoom = std::clamp(ptzSt->zoom, 0.0, 1.0);
            m_telemetry.opticalZoomFactor = 1.0 + m_telemetry.normalizedZoom * 29.0;
            m_telemetry.horizontalFovDeg = calculateHfov(m_telemetry.opticalZoomFactor);
            m_telemetry.verticalFovDeg = calculateVfov(m_telemetry.horizontalFovDeg);
            m_telemetry.timestamp = std::chrono::system_clock::now();
        }
    }
    if (!m_videoSourceToken.empty()) {
        const auto imgSt = m_client->getImagingSettings(m_videoSourceToken);
        if (imgSt.has_value()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.autoFocusActive = (imgSt->autoFocusMode == "AUTO");
            m_telemetry.autoIrisActive = (imgSt->exposure.mode == "AUTO");
            m_telemetry.irisNormalized = std::clamp(static_cast<double>(imgSt->exposure.iris) / 100.0, 0.0, 1.0);
            m_telemetry.dayNightIcrActive = (imgSt->irCutFilter == "OFF");
            m_telemetry.timestamp = std::chrono::system_clock::now();
        }
    }
}

void OnvifCameraAdapter::setProfileToken(std::string profileToken)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profileToken = std::move(profileToken);
    m_streamUri.clear();
}

const std::string& OnvifCameraAdapter::profileToken() const noexcept
{
    return m_profileToken;
}

void OnvifCameraAdapter::setVideoSourceToken(std::string videoSourceToken)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_videoSourceToken = std::move(videoSourceToken);
}

const std::string& OnvifCameraAdapter::videoSourceToken() const noexcept
{
    return m_videoSourceToken;
}

// =============================================================================
// OnvifPayloadAdapter Implementation
// =============================================================================

OnvifPayloadAdapter::OnvifPayloadAdapter(std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken)
    : m_client(std::move(client))
    , m_ptu(std::make_shared<OnvifPtuAdapter>(m_client, profileToken))
    , m_camera(std::make_shared<OnvifCameraAdapter>(m_client, profileToken))
{
}

OnvifPayloadAdapter::OnvifPayloadAdapter(
    const std::string& deviceEndpoint, const Onvif::SecurityCredentials& credentials)
    : OnvifPayloadAdapter(std::make_shared<Onvif::OnvifClient>(deviceEndpoint, credentials))
{
}

OnvifPayloadAdapter::~OnvifPayloadAdapter()
{
    disconnect();
}

bool OnvifPayloadAdapter::connect()
{
    if (!m_client) {
        return false;
    }
    const bool ptuOk = m_ptu ? m_ptu->connect() : false;
    const bool camOk = m_camera ? m_camera->connect() : false;
    const bool ok = ptuOk || camOk;
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stateCallback) {
            m_stateCallback(DeviceState::Ready, "ONVIF Payload Online");
        }
    }
    return ok;
}

void OnvifPayloadAdapter::disconnect()
{
    if (m_ptu)
        m_ptu->disconnect();
    if (m_camera)
        m_camera->disconnect();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "ONVIF Payload Offline");
    }
}

bool OnvifPayloadAdapter::isConnected() const noexcept
{
    return (m_ptu && m_ptu->isConnected()) || (m_camera && m_camera->isConnected());
}

DeviceState OnvifPayloadAdapter::state() const noexcept
{
    if (!m_client)
        return DeviceState::Fault;
    if (isConnected())
        return DeviceState::Ready;
    return DeviceState::Disconnected;
}

DeviceInfo OnvifPayloadAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "ONVIF";
    d.model = "ONVIF Composite Station";
    d.firmwareVersion = "1.0.0";
    if (m_client) {
        const auto devInfo = m_client->getDeviceInformation();
        if (devInfo.has_value()) {
            if (!devInfo->manufacturer.empty())
                d.manufacturer = devInfo->manufacturer;
            if (!devInfo->model.empty())
                d.model = devInfo->model;
            if (!devInfo->serialNumber.empty())
                d.serialNumber = devInfo->serialNumber;
            if (!devInfo->firmwareVersion.empty())
                d.firmwareVersion = devInfo->firmwareVersion;
        }
    }
    return d;
}

void OnvifPayloadAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

std::shared_ptr<IPanTiltUnit> OnvifPayloadAdapter::panTilt() const noexcept
{
    return m_ptu;
}

std::shared_ptr<ICameraPayload> OnvifPayloadAdapter::primaryCamera() const noexcept
{
    return m_camera;
}

std::shared_ptr<ICameraPayload> OnvifPayloadAdapter::secondaryCamera() const noexcept
{
    return nullptr;
}

std::shared_ptr<ILaserRangeFinder> OnvifPayloadAdapter::lrf() const noexcept
{
    return nullptr;
}

std::optional<Klv::GeoPoint2D> OnvifPayloadAdapter::calculateTargetCoordinates(
    const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const
{
    if (!m_ptu) {
        return std::nullopt;
    }
    const GimbalTelemetry telem = m_ptu->currentTelemetry();
    const Klv::GeoPoint3D platform3D { platformGps.latitudeDeg, platformGps.longitudeDeg, platformAltMeters };
    auto target3D = GeoreferenceUtils::computeTargetFromGroundIntersection(
        platform3D, platformHeadingDeg, telem.panAngleDeg, telem.tiltAngleDeg, 0.0);
    if (target3D.has_value()) {
        return Klv::GeoPoint2D { target3D->latitudeDeg, target3D->longitudeDeg };
    }
    return std::nullopt;
}

std::string OnvifPayloadAdapter::videoStreamUri() const
{
    return m_camera ? m_camera->videoStreamUri() : std::string {};
}

} // namespace PayloadHal
