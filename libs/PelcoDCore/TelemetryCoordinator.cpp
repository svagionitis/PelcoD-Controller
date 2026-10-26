#include "TelemetryCoordinator.h"
#include "ProtocolParser.h"

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace PelcoD {

TelemetryCoordinator::TelemetryCoordinator() = default;
TelemetryCoordinator::~TelemetryCoordinator() = default;

DeviceStatus TelemetryCoordinator::getStatus() const
{
    std::scoped_lock lock(m_mutex);
    return m_status;
}

DeviceInfo TelemetryCoordinator::getInfo() const
{
    std::scoped_lock lock(m_mutex);
    return m_info;
}

void TelemetryCoordinator::setSerialNumber(const std::string& serial)
{
    std::scoped_lock lock(m_mutex);
    m_info.serialNumber = serial;
}

void TelemetryCoordinator::setConnected(bool connected) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.connected = connected;
}

void TelemetryCoordinator::setAddress(std::uint8_t address) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.address = address;
}

void TelemetryCoordinator::setSpeeds(std::uint8_t zoomSpeed, std::uint8_t focusSpeed) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.zoomSpeed = std::min<std::uint8_t>(zoomSpeed, 0x03U);
    m_status.focusSpeed = std::min<std::uint8_t>(focusSpeed, 0x03U);
}

void TelemetryCoordinator::setActivePreset(std::uint8_t preset) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.activePreset = preset;
}

void TelemetryCoordinator::setFocusPosition(std::uint16_t pos) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.focusPosition = pos;
}

void TelemetryCoordinator::setIrisPosition(std::uint16_t pos) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.irisPosition = pos;
}

void TelemetryCoordinator::setPanLimit(bool isLeft, std::uint16_t centidegrees) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.lastLimitCentidegrees = centidegrees;
    if (isLeft) {
        m_status.manualLeftLimitCentidegrees = centidegrees;
    } else {
        m_status.manualRightLimitCentidegrees = centidegrees;
    }
}

void TelemetryCoordinator::setScanLimit(bool isLeft, std::uint16_t centidegrees) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.lastLimitCentidegrees = centidegrees;
    if (isLeft) {
        m_status.scanLeftLimitCentidegrees = centidegrees;
    } else {
        m_status.scanRightLimitCentidegrees = centidegrees;
    }
}

void TelemetryCoordinator::setAutoFocus(AutoMode mode) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.autoFocus = mode;
}

void TelemetryCoordinator::setAutoIris(AutoMode mode) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.autoIris = mode;
}

void TelemetryCoordinator::setAgc(AutoMode mode) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.agc = mode;
}

void TelemetryCoordinator::setBacklightComp(SwitchState state) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.backlightComp = state;
}

void TelemetryCoordinator::setAutoWhiteBalance(SwitchState state) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_status.autoWhiteBalance = state;
}

bool TelemetryCoordinator::updateFromFrame(
    const std::vector<std::uint8_t>& frame, std::optional<EverestLimitId> limitId)
{
    std::scoped_lock lock(m_mutex);
    if (frame.size() >= 2U && frame[1] != m_status.address) {
        return false;
    }
    return ProtocolParser::updateStatus(frame, m_status, m_info, limitId);
}

void TelemetryCoordinator::setPolling(bool enable, std::uint32_t intervalMs) noexcept
{
    m_polling.store(enable, std::memory_order_relaxed);
    m_pollIntervalMs.store(intervalMs, std::memory_order_relaxed);
}

bool TelemetryCoordinator::isPolling() const noexcept
{
    return m_polling.load(std::memory_order_relaxed);
}

std::uint32_t TelemetryCoordinator::getPollIntervalMs() const noexcept
{
    return m_pollIntervalMs.load(std::memory_order_relaxed);
}

} // namespace PelcoD
