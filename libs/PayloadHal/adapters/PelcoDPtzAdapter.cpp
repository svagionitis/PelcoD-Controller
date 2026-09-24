#include "PelcoDPtzAdapter.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

PelcoDPtzAdapter::PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device)) {
    if (m_device) {
        m_statusConnection = m_device->addStatusCallback([this](const PelcoD::DeviceStatus& status) {
            handleDeviceStatus(status);
        });
    }
}

PelcoDPtzAdapter::~PelcoDPtzAdapter() {
    m_statusConnection.disconnect();
}

bool PelcoDPtzAdapter::connect() {
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

void PelcoDPtzAdapter::disconnect() {
    if (m_device) {
        m_device->stop();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentState = DeviceState::Disconnected;
    if (m_stateCallback) {
        m_stateCallback(m_currentState, "Disconnected");
    }
}

bool PelcoDPtzAdapter::isConnected() const noexcept {
    return m_device && m_device->isConnected();
}

DeviceState PelcoDPtzAdapter::state() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_device) {
        return DeviceState::Fault;
    }
    return m_device->isConnected() ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo PelcoDPtzAdapter::info() const noexcept {
    DeviceInfo dev {};
    dev.manufacturer = "Pelco";
    dev.model = "Pelco-D PTZ Unit";
    dev.firmwareVersion = "1.0.0";
    return dev;
}

void PelcoDPtzAdapter::registerStateCallback(StateCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

bool PelcoDPtzAdapter::setRate(double panDegPerSec, double tiltDegPerSec) {
    // Normalizing assuming max pan speed of 60 deg/sec and max tilt speed of 30 deg/sec
    constexpr double kMaxPanRateDegSec { 60.0 };
    constexpr double kMaxTiltRateDegSec { 30.0 };

    const float normPan = static_cast<float>(std::clamp(panDegPerSec / kMaxPanRateDegSec, -1.0, 1.0));
    const float normTilt = static_cast<float>(std::clamp(tiltDegPerSec / kMaxTiltRateDegSec, -1.0, 1.0));
    return setNormalizedVelocity(normPan, normTilt);
}

bool PelcoDPtzAdapter::setNormalizedVelocity(float panVel, float tiltVel) {
    if (!m_device) {
        return false;
    }

    const float clampedPan = std::clamp(panVel, -1.0f, 1.0f);
    const float clampedTilt = std::clamp(tiltVel, -1.0f, 1.0f);

    const auto panSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(clampedPan) * 63.0f, 0.0f, 63.0f));
    const auto tiltSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(clampedTilt) * 63.0f, 0.0f, 63.0f));

    PelcoD::PanDirection panDir { PelcoD::PanDirection::Stop };
    if (clampedPan > 0.02f) {
        panDir = PelcoD::PanDirection::Right;
    } else if (clampedPan < -0.02f) {
        panDir = PelcoD::PanDirection::Left;
    }

    PelcoD::TiltDirection tiltDir { PelcoD::TiltDirection::Stop };
    if (clampedTilt > 0.02f) {
        tiltDir = PelcoD::TiltDirection::Up;
    } else if (clampedTilt < -0.02f) {
        tiltDir = PelcoD::TiltDirection::Down;
    }

    m_device->move(panDir, panSpeed, tiltDir, tiltSpeed);
    return true;
}

bool PelcoDPtzAdapter::setAbsoluteAngles(double panDeg, double tiltDeg) {
    if (!m_device) {
        return false;
    }

    double normPan = std::fmod(panDeg, 360.0);
    if (normPan < 0.0) {
        normPan += 360.0;
    }

    // In Pelco-D, tilt is commonly represented in centidegrees (0 to 35999, with 0° level and 90° down)
    double pelcoTilt = (tiltDeg < 0.0) ? -tiltDeg : (360.0 - tiltDeg);
    pelcoTilt = std::fmod(pelcoTilt, 360.0);
    if (pelcoTilt < 0.0) {
        pelcoTilt += 360.0;
    }

    const auto panCd = static_cast<std::uint16_t>(normPan * 100.0);
    const auto tiltCd = static_cast<std::uint16_t>(pelcoTilt * 100.0);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastTelemetry.panAngleDeg = normPan;
        m_lastTelemetry.tiltAngleDeg = std::clamp(tiltDeg, -90.0, 90.0);
        m_lastTelemetry.timestamp = std::chrono::system_clock::now();
    }

    m_device->setPanAngle(panCd);
    m_device->setTiltAngle(tiltCd);
    return true;
}

bool PelcoDPtzAdapter::setRelativeNudge(double deltaPanDeg, double deltaTiltDeg) {
    double currentPan { 0.0 };
    double currentTilt { 0.0 };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentPan = m_lastTelemetry.panAngleDeg;
        currentTilt = m_lastTelemetry.tiltAngleDeg;
    }
    return setAbsoluteAngles(currentPan + deltaPanDeg, currentTilt + deltaTiltDeg);
}

bool PelcoDPtzAdapter::stopMotion() {
    if (!m_device) {
        return false;
    }
    m_device->stopMotion();
    return true;
}

bool PelcoDPtzAdapter::supportsStabilization() const noexcept {
    return false; // Pelco-D heads are unstabilized mounts
}

bool PelcoDPtzAdapter::setStabilizationMode(StabilizationMode mode) {
    return mode == StabilizationMode::Disabled;
}

StabilizationMode PelcoDPtzAdapter::stabilizationMode() const noexcept {
    return StabilizationMode::Disabled;
}

bool PelcoDPtzAdapter::zeroGyroDrift() {
    return false; // Unsupported on unstabilized mounts
}

bool PelcoDPtzAdapter::getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const {
    minPan = 0.0;
    maxPan = 360.0;
    minTilt = -90.0;
    maxTilt = 90.0;
    return true;
}

bool PelcoDPtzAdapter::savePreset(uint8_t presetId, const std::string& /*name*/) {
    if (!m_device) {
        return false;
    }
    m_device->setPreset(presetId);
    return true;
}

bool PelcoDPtzAdapter::recallPreset(uint8_t presetId) {
    if (!m_device) {
        return false;
    }
    m_device->goToPreset(presetId);
    return true;
}

void PelcoDPtzAdapter::registerTelemetryCallback(TelemetryCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetryCallback = std::move(cb);
}

void PelcoDPtzAdapter::handleDeviceStatus(const PelcoD::DeviceStatus& status) {
    GimbalTelemetry telem {};
    telem.panAngleDeg = status.panDegrees();
    // Pelco tilt conversion back to elevation: elevation = -tilt if downwards
    const double rawTilt = status.tiltDegrees();
    if (rawTilt <= 180.0) {
        telem.tiltAngleDeg = -rawTilt;
    } else {
        telem.tiltAngleDeg = 360.0 - rawTilt;
    }
    telem.isStabilized = false;
    telem.isMoving = false;
    telem.limitReached = false;
    telem.timestamp = std::chrono::system_clock::now();

    TelemetryCallback cbCopy {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastTelemetry = telem;
        cbCopy = m_telemetryCallback;
    }

    if (cbCopy) {
        cbCopy(telem);
    }
}

} // namespace PayloadHal
