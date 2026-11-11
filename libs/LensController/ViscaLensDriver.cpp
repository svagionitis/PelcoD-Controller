/// @file ViscaLensDriver.cpp
/// @brief Implementation of Sony VISCA optical camera block driver adapter.

#include "ViscaLensDriver.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

ViscaLensDriver::ViscaLensDriver(std::uint8_t cameraAddress, LensRanges ranges) noexcept
    : m_cameraAddress { std::clamp(cameraAddress, static_cast<std::uint8_t>(1U), static_cast<std::uint8_t>(7U)) }
    , m_ranges { ranges }
{
}

bool ViscaLensDriver::initialize(std::uint8_t port, std::uint32_t baud) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_port = port;
    m_baud = baud;
    return true;
}

void ViscaLensDriver::moveFocus(std::int16_t steps, MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const std::uint8_t hdr = makeHeader();
    const std::uint8_t vSpeed = toViscaSpeed(speed);

    if (steps == 0) {
        // Focus Stop: 8x 01 04 08 00 FF
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x08U, 0x00U, 0xFFU };
        transmit(frame.data(), frame.size());
    } else if (steps > 0) {
        // Focus Far (Variable): 8x 01 04 08 2p FF
        const std::uint8_t mode = static_cast<std::uint8_t>(0x20U | (vSpeed & 0x07U));
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x08U, mode, 0xFFU };
        transmit(frame.data(), frame.size());
    } else {
        // Focus Near (Variable): 8x 01 04 08 3p FF
        const std::uint8_t mode = static_cast<std::uint8_t>(0x30U | (vSpeed & 0x07U));
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x08U, mode, 0xFFU };
        transmit(frame.data(), frame.size());
    }
}

void ViscaLensDriver::moveZoom(std::int16_t steps, MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const std::uint8_t hdr = makeHeader();
    const std::uint8_t vSpeed = toViscaSpeed(speed);

    if (steps == 0) {
        // Zoom Stop: 8x 01 04 07 00 FF
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x07U, 0x00U, 0xFFU };
        transmit(frame.data(), frame.size());
    } else if (steps > 0) {
        // Zoom Tele (Variable): 8x 01 04 07 2p FF
        const std::uint8_t mode = static_cast<std::uint8_t>(0x20U | (vSpeed & 0x07U));
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x07U, mode, 0xFFU };
        transmit(frame.data(), frame.size());
    } else {
        // Zoom Wide (Variable): 8x 01 04 07 3p FF
        const std::uint8_t mode = static_cast<std::uint8_t>(0x30U | (vSpeed & 0x07U));
        const std::array<std::uint8_t, 6> frame { hdr, 0x01U, 0x04U, 0x07U, mode, 0xFFU };
        transmit(frame.data(), frame.size());
    }
}

void ViscaLensDriver::setFocusPosition(MotorPosition pos, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const MotorPosition clamped = m_ranges.clampFocus(pos);
    const std::uint16_t val = clamped.value();

    const std::uint8_t p = static_cast<std::uint8_t>((val >> 12U) & 0x0FU);
    const std::uint8_t q = static_cast<std::uint8_t>((val >> 8U) & 0x0FU);
    const std::uint8_t r = static_cast<std::uint8_t>((val >> 4U) & 0x0FU);
    const std::uint8_t s = static_cast<std::uint8_t>(val & 0x0FU);

    // Focus Direct: 8x 01 04 48 0p 0q 0r 0s FF
    const std::array<std::uint8_t, 9> frame { makeHeader(), 0x01U, 0x04U, 0x48U, p, q, r, s, 0xFFU };
    transmit(frame.data(), frame.size());

    m_currentFocus = clamped;
    updateLimitFlagsLocked();
}

void ViscaLensDriver::setZoomPosition(MotorPosition pos, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const MotorPosition clamped = m_ranges.clampZoom(pos);
    const std::uint16_t val = clamped.value();

    const std::uint8_t p = static_cast<std::uint8_t>((val >> 12U) & 0x0FU);
    const std::uint8_t q = static_cast<std::uint8_t>((val >> 8U) & 0x0FU);
    const std::uint8_t r = static_cast<std::uint8_t>((val >> 4U) & 0x0FU);
    const std::uint8_t s = static_cast<std::uint8_t>(val & 0x0FU);

    // Zoom Direct: 8x 01 04 47 0p 0q 0r 0s FF
    const std::array<std::uint8_t, 9> frame { makeHeader(), 0x01U, 0x04U, 0x47U, p, q, r, s, 0xFFU };
    transmit(frame.data(), frame.size());

    m_currentZoom = clamped;
    updateLimitFlagsLocked();
}

void ViscaLensDriver::stopMotion() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const std::uint8_t hdr = makeHeader();

    // Zoom Stop: 8x 01 04 07 00 FF
    const std::array<std::uint8_t, 6> zoomStop { hdr, 0x01U, 0x04U, 0x07U, 0x00U, 0xFFU };
    transmit(zoomStop.data(), zoomStop.size());

    // Focus Stop: 8x 01 04 08 00 FF
    const std::array<std::uint8_t, 6> focusStop { hdr, 0x01U, 0x04U, 0x08U, 0x00U, 0xFFU };
    transmit(focusStop.data(), focusStop.size());
}

MotorPosition ViscaLensDriver::currentFocus() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_currentFocus;
}

MotorPosition ViscaLensDriver::currentZoom() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_currentZoom;
}

std::uint8_t ViscaLensDriver::limitFlags() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_limitFlags;
}

void ViscaLensDriver::sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len == 0U) {
        return;
    }
    std::lock_guard<std::mutex> lock { m_mutex };
    transmit(data, len);
}

bool ViscaLensDriver::hasBuiltinAutofocus() const noexcept
{
    return true;
}

bool ViscaLensDriver::triggerBuiltinAutofocus() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    // One-Push Trigger AF: 8x 01 04 18 01 FF
    const std::array<std::uint8_t, 6> frame { makeHeader(), 0x01U, 0x04U, 0x18U, 0x01U, 0xFFU };
    transmit(frame.data(), frame.size());
    return true;
}

bool ViscaLensDriver::hasBuiltinZoomTracking() const noexcept
{
    return true;
}

bool ViscaLensDriver::setBuiltinZoomTracking(bool enable) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_trackingEnabled = enable;

    // AF Mode Auto (0x02) or Manual (0x03): 8x 01 04 38 02/03 FF
    const std::uint8_t mode = enable ? 0x02U : 0x03U;
    const std::array<std::uint8_t, 6> frame { makeHeader(), 0x01U, 0x04U, 0x38U, mode, 0xFFU };
    transmit(frame.data(), frame.size());
    return true;
}

LensRanges ViscaLensDriver::lensRanges() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_ranges;
}

void ViscaLensDriver::setTransmitCallback(TransmitCallback cb, void* userCtx) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_txCallback = cb;
    m_userCtx = userCtx;
}

void ViscaLensDriver::processIncomingBytes(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len < 7U) {
        return;
    }

    std::lock_guard<std::mutex> lock { m_mutex };
    // Look for 7-byte position inquiry reply: y0 50 0p 0q 0r 0s FF
    for (std::size_t i = 0U; i + 6U < len; ++i) {
        if ((data[i] & 0x80U) != 0U && data[i + 1U] == 0x50U && data[i + 6U] == 0xFFU) {
            const std::uint16_t p = static_cast<std::uint16_t>(data[i + 2U] & 0x0FU);
            const std::uint16_t q = static_cast<std::uint16_t>(data[i + 3U] & 0x0FU);
            const std::uint16_t r = static_cast<std::uint16_t>(data[i + 4U] & 0x0FU);
            const std::uint16_t s = static_cast<std::uint16_t>(data[i + 5U] & 0x0FU);
            const std::uint16_t val = static_cast<std::uint16_t>((p << 12U) | (q << 8U) | (r << 4U) | s);

            if (m_lastInquiry == LastInquiry::Zoom) {
                m_currentZoom = MotorPosition { val };
                m_lastInquiry = LastInquiry::None;
                updateLimitFlagsLocked();
            } else if (m_lastInquiry == LastInquiry::Focus) {
                m_currentFocus = MotorPosition { val };
                m_lastInquiry = LastInquiry::None;
                updateLimitFlagsLocked();
            }
            break;
        }
    }
}

std::uint8_t ViscaLensDriver::cameraAddress() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_cameraAddress;
}

void ViscaLensDriver::setCameraAddress(std::uint8_t address) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_cameraAddress = std::clamp(address, static_cast<std::uint8_t>(1U), static_cast<std::uint8_t>(7U));
}

void ViscaLensDriver::queryZoomPosition() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_lastInquiry = LastInquiry::Zoom;
    // Zoom Pos Inquiry: 8x 09 04 47 FF
    const std::array<std::uint8_t, 5> frame { makeHeader(), 0x09U, 0x04U, 0x47U, 0xFFU };
    transmit(frame.data(), frame.size());
}

void ViscaLensDriver::queryFocusPosition() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_lastInquiry = LastInquiry::Focus;
    // Focus Pos Inquiry: 8x 09 04 48 FF
    const std::array<std::uint8_t, 5> frame { makeHeader(), 0x09U, 0x04U, 0x48U, 0xFFU };
    transmit(frame.data(), frame.size());
}

std::vector<std::uint8_t> ViscaLensDriver::lastCommand() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_lastCmd;
}

void ViscaLensDriver::transmit(const std::uint8_t* data, std::size_t len) noexcept
{
    m_lastCmd.assign(data, data + len);
    if (m_txCallback != nullptr) {
        m_txCallback(data, len, m_userCtx);
    }
}

void ViscaLensDriver::updateLimitFlagsLocked() noexcept
{
    std::uint8_t flags = static_cast<std::uint8_t>(LensLimitFlags::None);
    if (m_currentFocus.value() <= m_ranges.focusMin) {
        flags = static_cast<std::uint8_t>(flags | static_cast<std::uint8_t>(LensLimitFlags::NearLimit));
    }
    if (m_currentFocus.value() >= m_ranges.focusMax) {
        flags = static_cast<std::uint8_t>(flags | static_cast<std::uint8_t>(LensLimitFlags::FarLimit));
    }
    if (m_currentZoom.value() <= m_ranges.zoomMin) {
        flags = static_cast<std::uint8_t>(flags | static_cast<std::uint8_t>(LensLimitFlags::WideLimit));
    }
    if (m_currentZoom.value() >= m_ranges.zoomMax) {
        flags = static_cast<std::uint8_t>(flags | static_cast<std::uint8_t>(LensLimitFlags::TeleLimit));
    }
    m_limitFlags = flags;
}

} // namespace LensControl
