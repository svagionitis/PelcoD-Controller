#include "FlirPfecPtzAdapter.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

FlirPfecPtzAdapter::FlirPfecPtzAdapter(std::shared_ptr<Nmea::FlirPfecDevice> device)
    : m_device(std::move(device))
{
    if (m_device) {
        m_posSubscriptionId
            = m_device->addPositionCallback([this](const Nmea::PfecGimbalPosition& pos) { handlePositionReport(pos); });
    }
}

FlirPfecPtzAdapter::~FlirPfecPtzAdapter()
{
    if (m_device && m_posSubscriptionId != 0U) {
        m_device->removePositionCallback(m_posSubscriptionId);
    }
}

bool FlirPfecPtzAdapter::connect()
{
    if (!m_device) {
        return false;
    }
    const bool ok = m_device->start();
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentState = DeviceState::Ready;
        if (m_stateCallback) {
            m_stateCallback(m_currentState, "Connected successfully");
        }
    }
    return ok;
}

void FlirPfecPtzAdapter::disconnect()
{
    if (m_device) {
        m_device->stop();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentState = DeviceState::Disconnected;
    if (m_stateCallback) {
        m_stateCallback(m_currentState, "Disconnected");
    }
}

bool FlirPfecPtzAdapter::isConnected() const noexcept
{
    return m_device && m_device->isConnected();
}

DeviceState FlirPfecPtzAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_device) {
        return DeviceState::Fault;
    }
    return m_device->isConnected() ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo FlirPfecPtzAdapter::info() const noexcept
{
    DeviceInfo dev {};
    dev.manufacturer = "FLIR";
    dev.model = "FLIR M-Series Marine PTZ";
    dev.firmwareVersion = "1.0.0";
    dev.serialNumber = "FLIR-PFEC-01";
    return dev;
}

void FlirPfecPtzAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

bool FlirPfecPtzAdapter::setRate(double panDegPerSec, double tiltDegPerSec)
{
    constexpr double kMaxPanRateDegSec { 60.0 };
    constexpr double kMaxTiltRateDegSec { 30.0 };

    const float normPan = static_cast<float>(std::clamp(panDegPerSec / kMaxPanRateDegSec, -1.0, 1.0));
    const float normTilt = static_cast<float>(std::clamp(tiltDegPerSec / kMaxTiltRateDegSec, -1.0, 1.0));
    return setNormalizedVelocity(normPan, normTilt);
}

bool FlirPfecPtzAdapter::setNormalizedVelocity(float panVel, float tiltVel)
{
    if (!m_device) {
        return false;
    }
    return m_device->setVelocity(panVel, tiltVel);
}

bool FlirPfecPtzAdapter::setAbsoluteAngles(double panDeg, double tiltDeg)
{
    if (!m_device) {
        return false;
    }
    return m_device->setAbsoluteAngles(panDeg, tiltDeg);
}

bool FlirPfecPtzAdapter::setRelativeNudge(double deltaPanDeg, double deltaTiltDeg)
{
    double targetPan { 0.0 };
    double targetTilt { 0.0 };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        targetPan = m_lastTelemetry.panAngleDeg + deltaPanDeg;
        targetTilt = m_lastTelemetry.tiltAngleDeg + deltaTiltDeg;
    }
    return setAbsoluteAngles(targetPan, targetTilt);
}

bool FlirPfecPtzAdapter::stopMotion()
{
    if (!m_device) {
        return false;
    }
    return m_device->stopMotion();
}

bool FlirPfecPtzAdapter::supportsStabilization() const noexcept
{
    return false;
}

bool FlirPfecPtzAdapter::setStabilizationMode(StabilizationMode mode)
{
    return mode == StabilizationMode::Disabled;
}

StabilizationMode FlirPfecPtzAdapter::stabilizationMode() const noexcept
{
    return StabilizationMode::Disabled;
}

bool FlirPfecPtzAdapter::zeroGyroDrift()
{
    return false;
}

bool FlirPfecPtzAdapter::getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const
{
    minPan = -180.0;
    maxPan = 180.0;
    minTilt = -90.0;
    maxTilt = 90.0;
    return true;
}

bool FlirPfecPtzAdapter::savePreset(uint8_t presetId, const std::string& /*name*/)
{
    if (!m_device) {
        return false;
    }
    return m_device->savePreset(presetId);
}

bool FlirPfecPtzAdapter::recallPreset(uint8_t presetId)
{
    if (!m_device) {
        return false;
    }
    return m_device->recallPreset(presetId);
}

void FlirPfecPtzAdapter::registerTelemetryCallback(TelemetryCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetryCallback = std::move(cb);
}

GimbalTelemetry FlirPfecPtzAdapter::currentTelemetry() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastTelemetry;
}

void FlirPfecPtzAdapter::handlePositionReport(const Nmea::PfecGimbalPosition& pos)
{
    TelemetryCallback cb {};
    GimbalTelemetry telem {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastTelemetry.panAngleDeg = pos.panDegrees;
        m_lastTelemetry.tiltAngleDeg = pos.tiltDegrees;
        m_lastTelemetry.timestamp = std::chrono::system_clock::now();
        telem = m_lastTelemetry;
        cb = m_telemetryCallback;
    }

    if (cb) {
        cb(telem);
    }
}

} // namespace PayloadHal
