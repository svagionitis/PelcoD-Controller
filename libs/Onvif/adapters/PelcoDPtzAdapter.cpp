#include "PelcoDPtzAdapter.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GeodesyUtils.h"
#include "XmlUtils.h"
#include <PelcoDCore/PatrolController.h>

namespace Onvif {

using namespace PelcoD;

PelcoDPtzAdapter::PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device))
{
    if (m_device) {
        m_statusConn = m_device->addStatusCallback(
            [this](const PelcoD::DeviceStatus& status) { onDeviceStatusUpdated(status); });
    }
    loadTours();

    m_timerRunning = true;
    m_timerThread = std::thread(&PelcoDPtzAdapter::timerLoop, this);
}

PelcoDPtzAdapter::~PelcoDPtzAdapter()
{
    cancelAutoStop();
    m_timerRunning = false;
    m_timerCv.notify_all();
    if (m_timerThread.joinable()) {
        m_timerThread.join();
    }
    m_statusConn.disconnect();
}

void PelcoDPtzAdapter::armAutoStop(std::chrono::milliseconds timeout)
{
    std::scoped_lock lock(m_timerMutex);
    m_autoStopDeadline = std::chrono::steady_clock::now() + timeout;
    m_timerCv.notify_all();
}

void PelcoDPtzAdapter::cancelAutoStop()
{
    std::scoped_lock lock(m_timerMutex);
    m_autoStopDeadline = std::nullopt;
    m_timerCv.notify_all();
}

void PelcoDPtzAdapter::timerLoop()
{
    std::unique_lock lock(m_timerMutex);
    while (m_timerRunning.load()) {
        if (!m_autoStopDeadline.has_value()) {
            m_timerCv.wait(lock, [this] { return !m_timerRunning.load() || m_autoStopDeadline.has_value(); });
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= *m_autoStopDeadline) {
            m_autoStopDeadline = std::nullopt;
            lock.unlock();

            if (m_device) {
                static_cast<void>(m_device->stopMotion());
                static_cast<void>(m_device->zoomStop());
            }
            m_isMoving = false;

            lock.lock();
        } else {
            const auto remaining = *m_autoStopDeadline - now;
            m_timerCv.wait_for(lock, remaining, [this] {
                return !m_timerRunning.load() || !m_autoStopDeadline.has_value()
                    || (std::chrono::steady_clock::now() >= *m_autoStopDeadline);
            });
        }
    }
}

void PelcoDPtzAdapter::setEventPublisher(EventCallback publisher)
{
    std::scoped_lock lock(m_mutex);
    m_eventPublisher = std::move(publisher);
}

void PelcoDPtzAdapter::handleContinuousMove(
    float panSpeed, float tiltSpeed, float zoomSpeed, std::optional<std::chrono::milliseconds> timeout)
{
    if (!m_device) {
        return;
    }

    if (!std::isfinite(panSpeed) || !std::isfinite(tiltSpeed) || !std::isfinite(zoomSpeed)) {
        return;
    }

    if (timeout.has_value() && *timeout > std::chrono::milliseconds::zero()) {
        armAutoStop(*timeout);
    } else {
        cancelAutoStop();
    }

    const bool panIdle = std::abs(panSpeed) < 0.02f;
    const bool tiltIdle = std::abs(tiltSpeed) < 0.02f;

    if (panIdle && tiltIdle) {
        static_cast<void>(m_device->stopMotion());
        if (std::abs(zoomSpeed) <= 0.05f) {
            m_isMoving = false;
        }
    } else {
        m_isMoving = true;
        PanDirection panDir = PanDirection::Stop;
        if (panSpeed > 0.02f) {
            panDir = PanDirection::Right;
        } else if (panSpeed < -0.02f) {
            panDir = PanDirection::Left;
        }

        TiltDirection tiltDir = TiltDirection::Stop;
        if (tiltSpeed > 0.02f) {
            tiltDir = TiltDirection::Up;
        } else if (tiltSpeed < -0.02f) {
            tiltDir = TiltDirection::Down;
        }

        const auto pSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(panSpeed) * 63.0f, 0.0f, 63.0f));
        const auto tSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(tiltSpeed) * 63.0f, 0.0f, 63.0f));

        static_cast<void>(m_device->move(panDir, pSpeed, tiltDir, tSpeed));
    }

    if (zoomSpeed > 0.05f) {
        m_isMoving = true;
        static_cast<void>(m_device->zoomTele());
    } else if (zoomSpeed < -0.05f) {
        m_isMoving = true;
        static_cast<void>(m_device->zoomWide());
    } else {
        static_cast<void>(m_device->zoomStop());
    }
}

void PelcoDPtzAdapter::handleAbsoluteMove(
    std::optional<float> pan, std::optional<float> tilt, std::optional<float> zoom, const std::string& spaceUri)
{
    if (!m_device) {
        return;
    }

    const bool isSpherical = (spaceUri.find("Spherical") != std::string::npos);

    if (pan.has_value()) {
        if (!std::isfinite(*pan)) {
            return;
        }
        float deg = 0.0f;
        if (isSpherical) {
            deg = std::fmod(*pan + 360.0f, 360.0f);
            if (deg < 0.0f) {
                deg += 360.0f;
            }
        } else {
            const float clampedNorm = std::clamp(*pan, -1.0f, 1.0f);
            deg = (clampedNorm + 1.0f) * 180.0f;
            if (deg >= 360.0f) {
                deg = 359.99f;
            }
        }
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(deg * 100.0f, 0.0f, 35999.0f));
        static_cast<void>(m_device->setPanAngle(cDeg));
    }

    if (tilt.has_value()) {
        if (!std::isfinite(*tilt)) {
            return;
        }
        float deg = 0.0f;
        if (isSpherical) {
            deg = std::clamp(*tilt, -90.0f, 90.0f);
        } else {
            const float clampedNorm = std::clamp(*tilt, -1.0f, 1.0f);
            deg = clampedNorm * 90.0f;
        }
        float posDeg = deg;
        if (posDeg < 0.0f) {
            posDeg += 360.0f;
        }
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(posDeg * 100.0f, 0.0f, 35999.0f));
        static_cast<void>(m_device->setTiltAngle(cDeg));
    }

    if (zoom.has_value()) {
        if (!std::isfinite(*zoom)) {
            return;
        }
        const float clampedZoom = std::clamp(*zoom, 0.0f, 1.0f);
        const auto zoomVal = static_cast<std::uint16_t>(clampedZoom * 65535.0f);
        static_cast<void>(m_device->setZoomPosition(zoomVal));
    }
}

void PelcoDPtzAdapter::handleAbsoluteMoveSpherical(float azimuthDeg, float elevationDeg, float zoom)
{
    handleAbsoluteMove(
        azimuthDeg, elevationDeg, zoom, "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace");
}

bool PelcoDPtzAdapter::handleGeoMove(const std::string& /*profileToken*/, const GeoMoveTarget& target)
{
    if (!m_device) {
        return false;
    }

    const double camLat = 37.7749;
    const double camLon = -122.4194;
    const double camAlt = 10.0;

    const auto azEl = GeodesyUtils::calculateAzimuthElevation(
        camLat, camLon, camAlt, target.targetLat, target.targetLon, target.targetElevation);

    handleAbsoluteMoveSpherical(static_cast<float>(azEl.azimuthDeg), static_cast<float>(azEl.elevationDeg), 1.0f);
    return true;
}

void PelcoDPtzAdapter::handleStop(bool stopPanTilt, bool stopZoom)
{
    if (!m_device) {
        return;
    }

    cancelAutoStop();

    if (stopPanTilt) {
        static_cast<void>(m_device->stopMotion());
    }
    if (stopZoom) {
        static_cast<void>(m_device->zoomStop());
    }

    if (stopPanTilt && stopZoom) {
        m_isMoving = false;
    }
}

bool PelcoDPtzAdapter::handleRelativeMove(std::optional<float> pan, std::optional<float> tilt,
    std::optional<float> zoom, float speed, const std::string& /*spaceUri*/)
{
    if (!m_device) {
        return false;
    }

    if (pan.has_value() && !std::isfinite(*pan)) {
        return false;
    }
    if (tilt.has_value() && !std::isfinite(*tilt)) {
        return false;
    }
    if (zoom.has_value() && !std::isfinite(*zoom)) {
        return false;
    }
    if (!std::isfinite(speed)) {
        return false;
    }

    const float spd = std::clamp(speed, 0.0f, 1.0f);
    const auto panSpeed = static_cast<std::uint8_t>(spd * 63.0f);
    const auto tiltSpeed = static_cast<std::uint8_t>(spd * 63.0f);

    if (pan.has_value() && std::abs(*pan) > 0.01f) {
        const auto dir = (*pan > 0.0f) ? PanDirection::Right : PanDirection::Left;
        static_cast<void>(m_device->move(dir, panSpeed, TiltDirection::Stop, 0U));
    }

    if (tilt.has_value() && std::abs(*tilt) > 0.01f) {
        const auto dir = (*tilt > 0.0f) ? TiltDirection::Up : TiltDirection::Down;
        static_cast<void>(m_device->move(PanDirection::Stop, 0U, dir, tiltSpeed));
    }

    if (zoom.has_value() && std::abs(*zoom) > 0.01f) {
        if (*zoom > 0.0f) {
            static_cast<void>(m_device->zoomTele());
        } else {
            static_cast<void>(m_device->zoomWide());
        }
    }

    return true;
}

bool PelcoDPtzAdapter::handleGotoHomePosition(float /*speed*/)
{
    if (!m_device) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    if (m_hasHomeCoordinates) {
        handleAbsoluteMove(m_homePan, m_homeTilt, m_homeZoom);
        return true;
    }

    const auto it = m_presets.find(m_homePresetToken);
    if (it != m_presets.end()) {
        const int presetNum = std::atoi(it->first.c_str());
        if (presetNum > 0 && presetNum <= 32) {
            static_cast<void>(m_device->goToPreset(static_cast<std::uint8_t>(presetNum)));
            return true;
        }
    }

    handleAbsoluteMove(0.0f, 0.0f, 0.0f);
    return true;
}

bool PelcoDPtzAdapter::handleSetHomePosition()
{
    if (!m_device) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    m_hasHomeCoordinates = true;
    m_homePan = 0.0f;
    m_homeTilt = 0.0f;
    m_homeZoom = 0.0f;
    return true;
}

void PelcoDPtzAdapter::handleAbsoluteMoveSpherical(float azimuthDeg, float elevationDeg, float zoom)
{
    if (!m_device) {
        return;
    }

    std::uint16_t panCdeg = 0;
    std::uint16_t tiltCdeg = 0;
    Geodesy::anglesToPelcoCentidegrees(
        static_cast<double>(azimuthDeg), static_cast<double>(elevationDeg), panCdeg, tiltCdeg);

    static_cast<void>(m_device->setPanAngle(panCdeg));
    static_cast<void>(m_device->setTiltAngle(tiltCdeg));

    if (zoom >= 0.0f) {
        const auto pos = static_cast<std::uint16_t>(std::clamp(zoom * 65535.0f, 0.0f, 65535.0f));
        static_cast<void>(m_device->setZoomPosition(pos));
    }
}

bool PelcoDPtzAdapter::handleGeoMove(const std::string& /*profileToken*/, const GeoMoveTarget& target)
{
    if (!m_device) {
        return false;
    }

    double panDeg = 0.0;
    double tiltDeg = 0.0;
    double slantRangeMeters = 0.0;

    LocationEntity currentLoc;
    {
        std::scoped_lock lock(m_mutex);
        currentLoc = m_cameraLocation;
    }

    const bool ok = Geodesy::computeTargetAzimuthElevation(
        currentLoc.location, currentLoc.orientation, target.targetGeo, panDeg, tiltDeg, slantRangeMeters);

    if (!ok) {
        return false;
    }

    std::uint16_t panCdeg = 0;
    std::uint16_t tiltCdeg = 0;
    Geodesy::anglesToPelcoCentidegrees(panDeg, tiltDeg, panCdeg, tiltCdeg);

    static_cast<void>(m_device->setPanAngle(panCdeg));
    static_cast<void>(m_device->setTiltAngle(tiltCdeg));

    if (target.areaWidth.has_value() || target.areaHeight.has_value()) {
        const double span = static_cast<double>(target.areaWidth.value_or(target.areaHeight.value_or(10.0f)));
        const double normZoom = Geodesy::computeZoomFromTargetArea(span, slantRangeMeters);
        const auto zoomPos = static_cast<std::uint16_t>(std::clamp(normZoom * 65535.0, 0.0, 65535.0));
        static_cast<void>(m_device->setZoomPosition(zoomPos));
    }

    return true;
}

LocationEntity PelcoDPtzAdapter::cameraLocation() const
{
    std::scoped_lock lock(m_mutex);
    return m_cameraLocation;
}

void PelcoDPtzAdapter::setCameraLocation(const LocationEntity& location)
{
    std::scoped_lock lock(m_mutex);
    m_cameraLocation = location;
}

} // namespace Onvif
