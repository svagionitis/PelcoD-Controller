/// @file LensPayloadAdapter.cpp
/// @brief Payload HAL adapter implementing ICameraPayload over LensController.

#include "LensPayloadAdapter.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "PayloadHal/ICameraPayload.h"
#include "PayloadHal/IDevice.h"
#include "PayloadHal/PayloadTypes.h"
#include "PayloadHal/VideoStreamTypes.h"

#include "LensController.h"
#include "LensTypes.h"

namespace LensControl {

LensPayloadAdapter::LensPayloadAdapter(
    std::shared_ptr<LensController> controller, CameraIndex channel, OpticalSensorConfig config)
    : m_controller { std::move(controller) }
    , m_channel { channel }
    , m_config { config }
{
    updateTelemetryLocked();
}

bool LensPayloadAdapter::connect()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (m_connected) {
        return true;
    }
    m_connected = true;
    m_deviceState = PayloadHal::DeviceState::Ready;
    updateTelemetryLocked();
    if (m_stateCallback) {
        m_stateCallback(m_deviceState, "Connected");
    }
    return true;
}

void LensPayloadAdapter::disconnect()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected) {
        return;
    }
    if (m_controller) {
        m_controller->stopMotion(m_channel);
    }
    m_connected = false;
    m_deviceState = PayloadHal::DeviceState::Disconnected;
    if (m_stateCallback) {
        m_stateCallback(m_deviceState, "Disconnected");
    }
}

bool LensPayloadAdapter::isConnected() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_connected;
}

PayloadHal::DeviceState LensPayloadAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_deviceState;
}

PayloadHal::DeviceInfo LensPayloadAdapter::info() const noexcept
{
    PayloadHal::DeviceInfo dInfo { };
    dInfo.manufacturer = "LensController";
    dInfo.model = "MotorizedLensPayload";
    dInfo.serialNumber = "SN-LENS-CH" + std::to_string(m_channel.value());
    dInfo.firmwareVersion = "1.0.0";
    return dInfo;
}

void LensPayloadAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_stateCallback = std::move(cb);
}

PayloadHal::CameraSpectrum LensPayloadAdapter::spectrum() const noexcept
{
    return m_config.spectrum;
}

bool LensPayloadAdapter::setZoomNormalized(double zoom01)
{
    const double clamped = std::clamp(zoom01, 0.0, 1.0);
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }

    const LensRanges ranges = m_controller->getLensRanges(m_channel);
    const double span = static_cast<double>(ranges.zoomMax - ranges.zoomMin);
    const auto rawTarget = static_cast<std::uint16_t>(static_cast<double>(ranges.zoomMin) + (clamped * span) + 0.5);

    m_cachedNormalizedZoom = clamped;
    m_controller->setZoomTarget(m_channel, MotorPosition { rawTarget });
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::zoomContinuous(float velocity)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }

    if (std::abs(velocity) < 0.01F) {
        m_controller->stopMotion(m_channel);
        return true;
    }

    const float mag = std::clamp(std::abs(velocity), 0.0F, 1.0F);
    const auto speedCount = static_cast<std::uint8_t>(mag * 255.0F + 0.5F);
    m_controller->setZoomSpeed(m_channel, MotorSpeed { speedCount });

    const std::int16_t stepDelta = (velocity > 0.0F) ? static_cast<std::int16_t>(100) : static_cast<std::int16_t>(-100);
    m_controller->moveZoomStep(m_channel, stepDelta);
    return true;
}

bool LensPayloadAdapter::zoomStop()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }
    m_controller->stopMotion(m_channel);
    return true;
}

bool LensPayloadAdapter::setFocusAuto(bool enable)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }
    if (enable) {
        m_controller->setLensMode(m_channel, LensMode::AutoFocusContinuous);
    } else {
        m_controller->setLensMode(m_channel, LensMode::ManualJog);
    }
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::setFocusNormalized(double focus01)
{
    const double clamped = std::clamp(focus01, 0.0, 1.0);
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }

    const LensRanges ranges = m_controller->getLensRanges(m_channel);
    const double span = static_cast<double>(ranges.focusMax - ranges.focusMin);
    const auto rawTarget = static_cast<std::uint16_t>(static_cast<double>(ranges.focusMin) + (clamped * span) + 0.5);

    m_cachedNormalizedFocus = clamped;
    m_controller->setFocusTarget(m_channel, MotorPosition { rawTarget });
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::focusContinuous(float velocity)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }

    if (std::abs(velocity) < 0.01F) {
        m_controller->stopMotion(m_channel);
        return true;
    }

    const float mag = std::clamp(std::abs(velocity), 0.0F, 1.0F);
    const auto speedCount = static_cast<std::uint8_t>(mag * 255.0F + 0.5F);
    m_controller->setFocusSpeed(m_channel, MotorSpeed { speedCount });

    const std::int16_t stepDelta = (velocity > 0.0F) ? static_cast<std::int16_t>(100) : static_cast<std::int16_t>(-100);
    m_controller->moveFocusStep(m_channel, stepDelta);
    return true;
}

bool LensPayloadAdapter::focusStop()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }
    m_controller->stopMotion(m_channel);
    return true;
}

bool LensPayloadAdapter::triggerOnePushFocus()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_connected || !m_controller) {
        return false;
    }
    m_controller->triggerAutoFocus(m_channel, AutoFocusMethod::ScanThreePhase);
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::setIrisAuto(bool enable)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_autoIrisActive = enable;
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::setIrisNormalized(double iris01)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_cachedIrisNormalized = std::clamp(iris01, 0.0, 1.0);
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::irisContinuous(float velocity)
{
    (void)velocity;
    return true;
}

bool LensPayloadAdapter::irisStop()
{
    return true;
}

bool LensPayloadAdapter::setDayNightIcr(bool nightMode)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_dayNightIcrActive = nightMode;
    updateTelemetryLocked();
    return true;
}

bool LensPayloadAdapter::setDefog(bool enable)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_defogActive = enable;
    return true;
}

bool LensPayloadAdapter::setStabilizer(bool enable)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_stabilizerActive = enable;
    return true;
}

void LensPayloadAdapter::registerTelemetryCallback(TelemetryCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_telemetryCallback = std::move(cb);
}

PayloadHal::CameraTelemetry LensPayloadAdapter::currentTelemetry() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_telemetry;
}

std::string LensPayloadAdapter::videoStreamUri(PayloadHal::VideoStreamProfile profile) const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const auto it = m_streamUris.find(profile);
    if (it != m_streamUris.end()) {
        return it->second;
    }
    return { };
}

bool LensPayloadAdapter::setVideoStreamUri(const std::string& uri, PayloadHal::VideoStreamProfile profile)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_streamUris[profile] = uri;
    return true;
}

std::vector<PayloadHal::VideoStreamDescriptor> LensPayloadAdapter::availableStreams() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    std::vector<PayloadHal::VideoStreamDescriptor> streams;
    const auto it = m_streamUris.find(PayloadHal::VideoStreamProfile::Primary);
    if (it != m_streamUris.end() && !it->second.empty()) {
        PayloadHal::VideoStreamDescriptor desc { };
        desc.uri = it->second;
        desc.profile = PayloadHal::VideoStreamProfile::Primary;
        desc.width = 1920;
        desc.height = 1080;
        desc.framerateFps = 30.0;
        desc.encoding = "H264";
        desc.isDefault = true;
        streams.push_back(desc);
    }
    return streams;
}

void LensPayloadAdapter::updateTelemetry()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    updateTelemetryLocked();
}

double LensPayloadAdapter::computeFocalLength(double normZoom) const noexcept
{
    if (m_config.minFocalLengthMm <= 0.0 || m_config.maxFocalLengthMm <= 0.0) {
        return 1.0;
    }
    const double ratio = m_config.maxFocalLengthMm / m_config.minFocalLengthMm;
    return m_config.minFocalLengthMm * std::pow(ratio, normZoom);
}

double LensPayloadAdapter::computeHorizontalFov(double focalLengthMm) const noexcept
{
    if (focalLengthMm <= 0.0) {
        return 0.0;
    }
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    return 2.0 * std::atan(m_config.sensorWidthMm / (2.0 * focalLengthMm)) * kRadToDeg;
}

double LensPayloadAdapter::computeVerticalFov(double focalLengthMm) const noexcept
{
    if (focalLengthMm <= 0.0) {
        return 0.0;
    }
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    return 2.0 * std::atan(m_config.sensorHeightMm / (2.0 * focalLengthMm)) * kRadToDeg;
}

void LensPayloadAdapter::updateTelemetryLocked()
{
    double normZoom = m_cachedNormalizedZoom;
    double normFocus = m_cachedNormalizedFocus;
    bool afActive = false;

    if (m_controller) {
        const LensStatus status = m_controller->getLensStatus(m_channel);
        const LensRanges ranges = m_controller->getLensRanges(m_channel);

        const double zoomSpan = static_cast<double>(ranges.zoomMax - ranges.zoomMin);
        if (zoomSpan > 0.0 && status.zoomPos.value() >= ranges.zoomMin) {
            normZoom = static_cast<double>(status.zoomPos.value() - ranges.zoomMin) / zoomSpan;
        }

        const double focusSpan = static_cast<double>(ranges.focusMax - ranges.focusMin);
        if (focusSpan > 0.0 && status.focusPos.value() >= ranges.focusMin) {
            normFocus = static_cast<double>(status.focusPos.value() - ranges.focusMin) / focusSpan;
        }

        afActive = status.isAfActive;
    }

    normZoom = std::clamp(normZoom, 0.0, 1.0);
    normFocus = std::clamp(normFocus, 0.0, 1.0);

    const double focalLength = computeFocalLength(normZoom);

    m_telemetry.opticalZoomFactor = 1.0 + normZoom * (m_config.maxOpticalZoom - 1.0);
    m_telemetry.normalizedZoom = normZoom;
    m_telemetry.focusDistanceNormalized = normFocus;
    m_telemetry.autoFocusActive = afActive;
    m_telemetry.irisNormalized = m_cachedIrisNormalized;
    m_telemetry.autoIrisActive = m_autoIrisActive;
    m_telemetry.dayNightIcrActive = m_dayNightIcrActive;
    m_telemetry.horizontalFovDeg = computeHorizontalFov(focalLength);
    m_telemetry.verticalFovDeg = computeVerticalFov(focalLength);
    m_telemetry.timestamp = std::chrono::system_clock::now();

    if (m_telemetryCallback) {
        m_telemetryCallback(m_telemetry);
    }
}

} // namespace LensControl
