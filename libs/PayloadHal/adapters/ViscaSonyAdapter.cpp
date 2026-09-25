#include "ViscaSonyAdapter.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

ViscaSonyAdapter::ViscaSonyAdapter(std::shared_ptr<Visca::Sony::SonyFCBDevice> device)
    : m_device(std::move(device))
{
}

bool ViscaSonyAdapter::connect()
{
    if (!m_device) {
        return false;
    }
    const bool ok = m_device->initialize();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected = ok;
        if (m_stateCallback) {
            m_stateCallback(
                ok ? DeviceState::Ready : DeviceState::Fault, ok ? "Sony FCB Connected" : "Sony FCB Init Failed");
        }
    }
    if (ok) {
        updateTelemetry();
    }
    return ok;
}

void ViscaSonyAdapter::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected = false;
    if (m_stateCallback) {
        m_stateCallback(DeviceState::Disconnected, "Sony FCB Disconnected");
    }
}

bool ViscaSonyAdapter::isConnected() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected;
}

DeviceState ViscaSonyAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_device) {
        return DeviceState::Fault;
    }
    return m_connected ? DeviceState::Ready : DeviceState::Disconnected;
}

DeviceInfo ViscaSonyAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "Sony";
    if (m_device) {
        const auto model = m_device->modelType();
        d.model = (model == Visca::Sony::SonyCameraModelType::FCB_EW9500H) ? "FCB-EW9500H" : "FCB-EV9520L";
    } else {
        d.model = "FCB Series";
    }
    d.firmwareVersion = "1.0.0";
    return d;
}

void ViscaSonyAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

CameraSpectrum ViscaSonyAdapter::spectrum() const noexcept
{
    return CameraSpectrum::DaylightVisible;
}

bool ViscaSonyAdapter::setZoomNormalized(double zoom01)
{
    if (!m_device) {
        return false;
    }
    const double clamped = std::clamp(zoom01, 0.0, 1.0);
    // Sony optical zoom: 0x0000 (wide) to 0x4000 (tele)
    const auto pos = static_cast<std::uint16_t>(clamped * 0x4000);
    const bool ok = m_device->setZoomDirect(pos);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.normalizedZoom = clamped;
        m_telemetry.opticalZoomFactor = 1.0 + clamped * 29.0;
        constexpr double kWideHfovRad { 63.7 * 3.14159265358979323846 / 180.0 };
        const double currentHfovRad = 2.0 * std::atan(std::tan(kWideHfovRad / 2.0) / m_telemetry.opticalZoomFactor);
        m_telemetry.horizontalFovDeg = currentHfovRad * (180.0 / 3.14159265358979323846);
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool ViscaSonyAdapter::zoomContinuous(float velocity)
{
    if (!m_device) {
        return false;
    }
    if (velocity > 0.05f) {
        const auto spd = static_cast<std::uint8_t>(std::clamp(velocity * 7.0f, 1.0f, 7.0f));
        return m_device->zoomTele(spd);
    }
    if (velocity < -0.05f) {
        const auto spd = static_cast<std::uint8_t>(std::clamp(-velocity * 7.0f, 1.0f, 7.0f));
        return m_device->zoomWide(spd);
    }
    return m_device->zoomStop();
}

bool ViscaSonyAdapter::zoomStop()
{
    if (!m_device) {
        return false;
    }
    return m_device->zoomStop();
}

bool ViscaSonyAdapter::setFocusAuto(bool enable)
{
    if (!m_device) {
        return false;
    }
    const bool ok = m_device->setFocusAuto(enable);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.autoFocusActive = enable;
    }
    return ok;
}

bool ViscaSonyAdapter::setFocusNormalized(double focus01)
{
    if (!m_device) {
        return false;
    }
    const double clamped = std::clamp(focus01, 0.0, 1.0);
    // Sony focus range: 0x1000 (Near) to 0xF000 (Infinity)
    const auto pos = static_cast<std::uint16_t>(0x1000 + clamped * (0xF000 - 0x1000));
    return m_device->setFocusDirect(pos);
}

bool ViscaSonyAdapter::triggerOnePushFocus()
{
    if (!m_device) {
        return false;
    }
    return m_device->focusOnePush();
}

bool ViscaSonyAdapter::setDayNightIcr(bool nightMode)
{
    if (!m_device) {
        return false;
    }
    const bool ok = m_device->setIcr(nightMode);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.dayNightIcrActive = nightMode;
        m_telemetry.timestamp = std::chrono::system_clock::now();
    }
    return ok;
}

bool ViscaSonyAdapter::setDefog(bool enable)
{
    if (!m_device) {
        return false;
    }
    return m_device->setDefog(enable ? Visca::Sony::SonyDefogMode::Mid : Visca::Sony::SonyDefogMode::Off);
}

bool ViscaSonyAdapter::setStabilizer(bool enable)
{
    if (!m_device) {
        return false;
    }
    return m_device->setStabilizer(
        enable ? Visca::Sony::SonyStabilizerMode::Super : Visca::Sony::SonyStabilizerMode::Off);
}

void ViscaSonyAdapter::registerTelemetryCallback(TelemetryCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetryCallback = std::move(cb);
}

CameraTelemetry ViscaSonyAdapter::currentTelemetry() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_telemetry;
}

void ViscaSonyAdapter::updateTelemetry()
{
    if (!m_device) {
        return;
    }
    m_device->pollStatus();
    const auto st = m_device->status();

    CameraTelemetry telem {};
    // Sony zoom 0x0000 - 0x4000
    const double normZoom = std::clamp(static_cast<double>(st.zoomPosition) / static_cast<double>(0x4000), 0.0, 1.0);
    telem.normalizedZoom = normZoom;
    telem.opticalZoomFactor = 1.0 + normZoom * 29.0; // 30x optical zoom block
    telem.focusDistanceNormalized
        = std::clamp(static_cast<double>(st.focusPosition) / static_cast<double>(0xF000), 0.0, 1.0);
    telem.autoFocusActive = st.focusAuto;
    telem.dayNightIcrActive = st.icrOn;

    // Sony FCB HFOV: ~63.7° at wide, ~2.3° at 30x tele
    constexpr double kWideHfovRad { 63.7 * 3.14159265358979323846 / 180.0 };
    const double currentHfovRad = 2.0 * std::atan(std::tan(kWideHfovRad / 2.0) / telem.opticalZoomFactor);
    telem.horizontalFovDeg = currentHfovRad * (180.0 / 3.14159265358979323846);
    const double currentVfovRad = 2.0 * std::atan(std::tan(currentHfovRad / 2.0) * (9.0 / 16.0));
    telem.verticalFovDeg = currentVfovRad * (180.0 / 3.14159265358979323846);
    telem.timestamp = std::chrono::system_clock::now();

    TelemetryCallback cbCopy {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry = telem;
        cbCopy = m_telemetryCallback;
    }
    if (cbCopy) {
        cbCopy(telem);
    }
}

} // namespace PayloadHal
