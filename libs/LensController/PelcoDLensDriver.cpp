/// @file PelcoDLensDriver.cpp
/// @brief Implementation of Pelco-D motorized lens driver adapter.

#include "PelcoDLensDriver.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

PelcoDLensDriver::PelcoDLensDriver(std::uint8_t cameraAddress, LensRanges ranges) noexcept
    : m_cameraAddress { std::max(static_cast<std::uint8_t>(1U), cameraAddress) }
    , m_ranges { ranges }
{
}

bool PelcoDLensDriver::initialize(std::uint8_t port, std::uint32_t baud) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_port = port;
    m_baud = baud;
    return true;
}

void PelcoDLensDriver::moveFocus(std::int16_t steps, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (steps == 0) {
        // Stop motion
        sendFrame(0x00U, 0x00U, 0x00U, 0x00U);
    } else if (steps > 0) {
        // Focus Far: Cmd2 Bit 0 (0x01)
        sendFrame(0x00U, 0x01U, 0x00U, 0x00U);
    } else {
        // Focus Near: Cmd1 Bit 0 (0x01)
        sendFrame(0x01U, 0x00U, 0x00U, 0x00U);
    }
}

void PelcoDLensDriver::moveZoom(std::int16_t steps, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    if (steps == 0) {
        // Stop motion
        sendFrame(0x00U, 0x00U, 0x00U, 0x00U);
    } else if (steps > 0) {
        // Zoom Tele: Cmd2 Bit 5 (0x20)
        sendFrame(0x00U, 0x20U, 0x00U, 0x00U);
    } else {
        // Zoom Wide: Cmd2 Bit 6 (0x40)
        sendFrame(0x00U, 0x40U, 0x00U, 0x00U);
    }
}

void PelcoDLensDriver::setFocusPosition(MotorPosition pos, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const MotorPosition clamped = m_ranges.clampFocus(pos);
    const std::uint16_t val = clamped.value();

    const std::uint8_t data1 = static_cast<std::uint8_t>((val >> 8U) & 0xFFU);
    const std::uint8_t data2 = static_cast<std::uint8_t>(val & 0xFFU);

    // Set Focus Position Opcode: 0x4B
    sendFrame(0x00U, 0x4BU, data1, data2);

    m_currentFocus = clamped;
    updateLimitFlagsLocked();
}

void PelcoDLensDriver::setZoomPosition(MotorPosition pos, [[maybe_unused]] MotorSpeed speed) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    const MotorPosition clamped = m_ranges.clampZoom(pos);
    const std::uint16_t val = clamped.value();

    const std::uint8_t data1 = static_cast<std::uint8_t>((val >> 8U) & 0xFFU);
    const std::uint8_t data2 = static_cast<std::uint8_t>(val & 0xFFU);

    // Set Zoom Position Opcode: 0x4F
    sendFrame(0x00U, 0x4FU, data1, data2);

    m_currentZoom = clamped;
    updateLimitFlagsLocked();
}

void PelcoDLensDriver::stopMotion() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    sendFrame(0x00U, 0x00U, 0x00U, 0x00U);
}

MotorPosition PelcoDLensDriver::currentFocus() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_currentFocus;
}

MotorPosition PelcoDLensDriver::currentZoom() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_currentZoom;
}

std::uint8_t PelcoDLensDriver::limitFlags() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_limitFlags;
}

void PelcoDLensDriver::sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len == 0U) {
        return;
    }
    std::lock_guard<std::mutex> lock { m_mutex };
    transmit(data, len);
}

bool PelcoDLensDriver::hasBuiltinAutofocus() const noexcept
{
    return false;
}

bool PelcoDLensDriver::triggerBuiltinAutofocus() noexcept
{
    return false;
}

bool PelcoDLensDriver::hasBuiltinZoomTracking() const noexcept
{
    return false;
}

bool PelcoDLensDriver::setBuiltinZoomTracking([[maybe_unused]] bool enable) noexcept
{
    return false;
}

LensRanges PelcoDLensDriver::lensRanges() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_ranges;
}

void PelcoDLensDriver::setTransmitCallback(TransmitCallback cb, void* userCtx) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_txCallback = cb;
    m_userCtx = userCtx;
}

void PelcoDLensDriver::processIncomingBytes(const std::uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr || len < 7U) {
        return;
    }

    std::lock_guard<std::mutex> lock { m_mutex };
    for (std::size_t i = 0U; i + 6U < len; ++i) {
        if (data[i] == 0xFFU) {
            const std::uint8_t addr = data[i + 1U];
            const std::uint8_t cmd1 = data[i + 2U];
            const std::uint8_t cmd2 = data[i + 3U];
            const std::uint8_t data1 = data[i + 4U];
            const std::uint8_t data2 = data[i + 5U];
            const std::uint8_t expectedChecksum
                = static_cast<std::uint8_t>((static_cast<std::uint32_t>(addr) + cmd1 + cmd2 + data1 + data2) & 0xFFU);

            if (data[i + 6U] == expectedChecksum) {
                const std::uint16_t posVal
                    = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data1) << 8U) | data2);

                if (cmd2 == 0x5DU) {
                    // Zoom response
                    m_currentZoom = MotorPosition { posVal };
                    updateLimitFlagsLocked();
                } else if (cmd2 == 0x57U) {
                    // Focus response
                    m_currentFocus = MotorPosition { posVal };
                    updateLimitFlagsLocked();
                }
                break;
            }
        }
    }
}

std::uint8_t PelcoDLensDriver::cameraAddress() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_cameraAddress;
}

void PelcoDLensDriver::setCameraAddress(std::uint8_t address) noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_cameraAddress = std::max(static_cast<std::uint8_t>(1U), address);
}

void PelcoDLensDriver::queryZoomPosition() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    sendFrame(0x00U, 0x5DU, 0x00U, 0x00U);
}

void PelcoDLensDriver::queryFocusPosition() noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    sendFrame(0x00U, 0x57U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> PelcoDLensDriver::lastCommand() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_lastCmd;
}

void PelcoDLensDriver::sendFrame(std::uint8_t cmd1, std::uint8_t cmd2, std::uint8_t data1, std::uint8_t data2) noexcept
{
    const std::uint8_t checksum = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(m_cameraAddress) + cmd1 + cmd2 + data1 + data2) & 0xFFU);

    const std::array<std::uint8_t, 7> frame { 0xFFU, m_cameraAddress, cmd1, cmd2, data1, data2, checksum };
    transmit(frame.data(), frame.size());
}

void PelcoDLensDriver::transmit(const std::uint8_t* data, std::size_t len) noexcept
{
    m_lastCmd.assign(data, data + len);
    if (m_txCallback != nullptr) {
        m_txCallback(data, len, m_userCtx);
    }
}

void PelcoDLensDriver::updateLimitFlagsLocked() noexcept
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
