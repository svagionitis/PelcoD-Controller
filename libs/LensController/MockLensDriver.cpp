/// @file MockLensDriver.cpp
/// @brief In-memory mock lens driver implementation for deterministic simulation and unit testing.

#include "MockLensDriver.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

MockLensDriver::MockLensDriver(LensRanges ranges) noexcept
    : m_ranges { ranges }
    , m_focusPos { ranges.focusMin }
    , m_zoomPos { ranges.zoomMin }
{
    updateFocusLimitsLocked();
    updateZoomLimitsLocked();
}

bool MockLensDriver::initialize(std::uint8_t port, std::uint32_t baud) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_boundPort = port;
    m_baudRate = baud;
    m_motionStopped = true;
    return true;
}

void MockLensDriver::moveFocus(std::int16_t steps, MotorSpeed speed) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_motionStopped = false;
    m_lastFocusSpeed = speed;

    const std::int32_t cur = static_cast<std::int32_t>(m_focusPos.value());
    const std::int32_t target = cur + static_cast<std::int32_t>(steps);
    const std::int32_t clamped = std::clamp<std::int32_t>(
        target, static_cast<std::int32_t>(m_ranges.focusMin), static_cast<std::int32_t>(m_ranges.focusMax));

    m_focusPos = MotorPosition { static_cast<std::uint16_t>(clamped) };
    updateFocusLimitsLocked();
}

void MockLensDriver::moveZoom(std::int16_t steps, MotorSpeed speed) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_motionStopped = false;
    m_lastZoomSpeed = speed;

    const std::int32_t cur = static_cast<std::int32_t>(m_zoomPos.value());
    const std::int32_t target = cur + static_cast<std::int32_t>(steps);
    const std::int32_t clamped = std::clamp<std::int32_t>(
        target, static_cast<std::int32_t>(m_ranges.zoomMin), static_cast<std::int32_t>(m_ranges.zoomMax));

    m_zoomPos = MotorPosition { static_cast<std::uint16_t>(clamped) };
    updateZoomLimitsLocked();
}

void MockLensDriver::setFocusPosition(MotorPosition pos, MotorSpeed speed) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_motionStopped = false;
    m_lastFocusSpeed = speed;
    m_focusPos = m_ranges.clampFocus(pos);
    updateFocusLimitsLocked();
}

void MockLensDriver::setZoomPosition(MotorPosition pos, MotorSpeed speed) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_motionStopped = false;
    m_lastZoomSpeed = speed;
    m_zoomPos = m_ranges.clampZoom(pos);
    updateZoomLimitsLocked();
}

void MockLensDriver::stopMotion() noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_motionStopped = true;
    m_lastFocusSpeed = MotorSpeed { 0U };
    m_lastZoomSpeed = MotorSpeed { 0U };
}

MotorPosition MockLensDriver::currentFocus() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_focusPos;
}

MotorPosition MockLensDriver::currentZoom() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_zoomPos;
}

std::uint8_t MockLensDriver::limitFlags() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_limitFlags;
}

void MockLensDriver::sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len == 0U) {
        return;
    }
    const std::lock_guard<std::mutex> lock { m_mutex };
    try {
        m_rawBuffer.insert(m_rawBuffer.end(), data, data + len);
    } catch (...) {
        // Zero-heap / noexcept boundary protection (CERT ERR55-CPP)
    }
}

bool MockLensDriver::hasBuiltinAutofocus() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_builtinAfSupported;
}

bool MockLensDriver::triggerBuiltinAutofocus() noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_builtinAfSupported) {
        return false;
    }
    m_builtinAfTriggered = true;
    return true;
}

bool MockLensDriver::hasBuiltinZoomTracking() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_builtinZoomTrackSupported;
}

bool MockLensDriver::setBuiltinZoomTracking(bool enable) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_builtinZoomTrackSupported = enable;
    return true;
}

LensRanges MockLensDriver::lensRanges() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_ranges;
}

void MockLensDriver::setRanges(const LensRanges& ranges) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_ranges = ranges;
    m_focusPos = ranges.clampFocus(m_focusPos);
    m_zoomPos = ranges.clampZoom(m_zoomPos);
    updateFocusLimitsLocked();
    updateZoomLimitsLocked();
}

void MockLensDriver::setBuiltinAfSupported(bool supported) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_builtinAfSupported = supported;
}

void MockLensDriver::setBuiltinZoomSupported(bool supported) noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_builtinZoomTrackSupported = supported;
}

bool MockLensDriver::isMotionStopped() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_motionStopped;
}

std::uint8_t MockLensDriver::boundPort() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_boundPort;
}

std::uint32_t MockLensDriver::baudRate() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_baudRate;
}

MotorSpeed MockLensDriver::lastFocusSpeed() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_lastFocusSpeed;
}

MotorSpeed MockLensDriver::lastZoomSpeed() const noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_lastZoomSpeed;
}

std::vector<std::uint8_t> MockLensDriver::rawCommands() const
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    return m_rawBuffer;
}

void MockLensDriver::clearRawCommands() noexcept
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    m_rawBuffer.clear();
}

void MockLensDriver::updateFocusLimitsLocked() noexcept
{
    const auto nearMask = static_cast<std::uint8_t>(LensLimitFlags::NearLimit);
    const auto farMask = static_cast<std::uint8_t>(LensLimitFlags::FarLimit);

    m_limitFlags &= static_cast<std::uint8_t>(~(nearMask | farMask));

    if (m_focusPos.value() <= m_ranges.focusMin) {
        m_limitFlags |= nearMask;
    }
    if (m_focusPos.value() >= m_ranges.focusMax) {
        m_limitFlags |= farMask;
    }
}

void MockLensDriver::updateZoomLimitsLocked() noexcept
{
    const auto wideMask = static_cast<std::uint8_t>(LensLimitFlags::WideLimit);
    const auto teleMask = static_cast<std::uint8_t>(LensLimitFlags::TeleLimit);

    m_limitFlags &= static_cast<std::uint8_t>(~(wideMask | teleMask));

    if (m_zoomPos.value() <= m_ranges.zoomMin) {
        m_limitFlags |= wideMask;
    }
    if (m_zoomPos.value() >= m_ranges.zoomMax) {
        m_limitFlags |= teleMask;
    }
}

} // namespace LensControl
