/// @file MockPelcoDDevice.cpp
/// @brief Implementation of in-memory simulated Pelco-D device.

#include "MockPelcoDDevice.h"
#include "PelcoDFrame.h"

#include <algorithm>
#include <cstring>

namespace PelcoD {

MockPelcoDDevice::MockPelcoDDevice(std::uint8_t address) noexcept
    : m_address { address }
{
}

MockDeviceState MockPelcoDDevice::getInternalState() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state;
}

void MockPelcoDDevice::setInternalState(const MockDeviceState& state)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_state = state;
}

bool MockPelcoDDevice::open()
{
    m_open.store(true);
    StateChangedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(TransportState::Connected, "Mock device connected");
    }
    return true;
}

void MockPelcoDDevice::close()
{
    m_open.store(false);
    StateChangedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(TransportState::Disconnected, "Mock device closed");
    }
}

bool MockPelcoDDevice::isOpen() const noexcept
{
    return m_open.load();
}

bool MockPelcoDDevice::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    const auto frames = PelcoDFrame::splitStream(data);
    for (const auto& frame : frames) {
        processFrame(frame);
    }
    return true;
}

void MockPelcoDDevice::setDataCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataCallback = std::move(callback);
}

void MockPelcoDDevice::setStateCallback(StateChangedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

void MockPelcoDDevice::processFrame(const std::vector<std::uint8_t>& frame)
{
    if (frame.size() != PelcoDFrame::StandardFrameSize) {
        return;
    }

    const std::uint8_t addr = frame[1];
    if (addr != m_address && addr != 0x00U) {
        return; // Frame for different address
    }

    const std::uint8_t cmd1 = frame[2];
    const std::uint8_t cmd2 = frame[3];
    const std::uint8_t data1 = frame[4];
    const std::uint8_t data2 = frame[5];
    const std::uint8_t cksm = frame[6];

    // Standard Command: bit 0 of cmd2 is 0
    if ((cmd2 & 0x01U) == 0U) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);

            // Pan motion: data1 is pan speed (0..63)
            const std::uint16_t panStep = static_cast<std::uint16_t>((data1 & 0x3FU) * 10U);
            if ((cmd2 & 0x02U) != 0U) {
                // Right: increase pan (modulo 36000 centidegrees)
                m_state.panCentidegrees = static_cast<std::uint16_t>((m_state.panCentidegrees + panStep) % 36000U);
            } else if ((cmd2 & 0x04U) != 0U) {
                // Left: decrease pan
                if (m_state.panCentidegrees >= panStep) {
                    m_state.panCentidegrees = static_cast<std::uint16_t>(m_state.panCentidegrees - panStep);
                } else {
                    m_state.panCentidegrees = static_cast<std::uint16_t>(36000U - (panStep - m_state.panCentidegrees));
                }
            }

            // Tilt motion: data2 is tilt speed (0..63)
            const std::uint16_t tiltStep = static_cast<std::uint16_t>((data2 & 0x3FU) * 10U);
            if ((cmd2 & 0x08U) != 0U) {
                // Up
                m_state.tiltCentidegrees = static_cast<std::uint16_t>((m_state.tiltCentidegrees + tiltStep) % 36000U);
            } else if ((cmd2 & 0x10U) != 0U) {
                // Down
                if (m_state.tiltCentidegrees >= tiltStep) {
                    m_state.tiltCentidegrees = static_cast<std::uint16_t>(m_state.tiltCentidegrees - tiltStep);
                } else {
                    m_state.tiltCentidegrees
                        = static_cast<std::uint16_t>(36000U - (tiltStep - m_state.tiltCentidegrees));
                }
            }

            // Zoom: cmd2 bit 5 = tele, bit 6 = wide
            if ((cmd2 & 0x20U) != 0U && m_state.zoomPosition < 65000U) {
                m_state.zoomPosition = static_cast<std::uint16_t>(m_state.zoomPosition + 100U);
            } else if ((cmd2 & 0x40U) != 0U && m_state.zoomPosition >= 100U) {
                m_state.zoomPosition = static_cast<std::uint16_t>(m_state.zoomPosition - 100U);
            }

            // Focus: cmd1 bit 0 = near, cmd2 bit 7 = far
            if ((cmd1 & 0x01U) != 0U && m_state.focusPosition >= 50U) {
                m_state.focusPosition = static_cast<std::uint16_t>(m_state.focusPosition - 50U);
            } else if ((cmd2 & 0x80U) != 0U && m_state.focusPosition < 65000U) {
                m_state.focusPosition = static_cast<std::uint16_t>(m_state.focusPosition + 50U);
            }

            // Iris: cmd1 bit 1 = open, bit 2 = close
            if ((cmd1 & 0x02U) != 0U && m_state.irisPosition < 1000U) {
                m_state.irisPosition = static_cast<std::uint16_t>(m_state.irisPosition + 10U);
            } else if ((cmd1 & 0x04U) != 0U && m_state.irisPosition >= 10U) {
                m_state.irisPosition = static_cast<std::uint16_t>(m_state.irisPosition - 10U);
            }
        }

        sendGeneralReply(cksm);
        return;
    }

    // Extended Commands: bit 0 of cmd2 is 1
    const auto opcode = static_cast<CommandOpcode>(cmd2);
    switch (opcode) {
    case CommandOpcode::SetPreset: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.presets[data2]
                = PresetPosition { m_state.panCentidegrees, m_state.tiltCentidegrees, m_state.zoomPosition };
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::ClearPreset: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.presets.erase(data2);
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::GoToPreset: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (data2 == 0x21U) {
                // Flip 180 deg
                m_state.panCentidegrees = static_cast<std::uint16_t>((m_state.panCentidegrees + 18000U) % 36000U);
            } else if (data2 == 0x22U) {
                // Go to zero pan
                m_state.panCentidegrees = 0U;
            } else {
                const auto it = m_state.presets.find(data2);
                if (it != m_state.presets.end()) {
                    m_state.panCentidegrees = it->second.pan;
                    m_state.tiltCentidegrees = it->second.tilt;
                    m_state.zoomPosition = it->second.zoom;
                }
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetAuxiliary: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (data2 >= 1U && data2 <= 8U) {
                m_state.auxStates[data2 - 1U] = true;
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::ClearAuxiliary: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (data2 >= 1U && data2 <= 8U) {
                m_state.auxStates[data2 - 1U] = false;
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetPanPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            const std::uint16_t pan = static_cast<std::uint16_t>((data1 << 8U) | data2);
            m_state.panCentidegrees = static_cast<std::uint16_t>(pan % 36000U);
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetTiltPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            const std::uint16_t tilt = static_cast<std::uint16_t>((data1 << 8U) | data2);
            m_state.tiltCentidegrees = static_cast<std::uint16_t>(tilt % 36000U);
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetZoomPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.zoomPosition = static_cast<std::uint16_t>((data1 << 8U) | data2);
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetZeroPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.panCentidegrees = 0U;
        }
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::StandardExtended),
            static_cast<std::uint8_t>(CommandOpcode::SetZeroPosition), 0x01U);
        break;
    }

    case CommandOpcode::SetMagnification: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.magnification = static_cast<std::uint16_t>((data1 << 8U) | data2);
        }
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::StandardExtended),
            static_cast<std::uint8_t>(CommandOpcode::SetMagnification), 0x01U);
        break;
    }

    case CommandOpcode::QueryMagnification: {
        std::uint16_t mag { 0U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            mag = m_state.magnification;
        }
        const std::uint8_t msb = static_cast<std::uint8_t>((mag >> 8U) & 0xFFU);
        const std::uint8_t lsb = static_cast<std::uint8_t>(mag & 0xFFU);
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification), msb, lsb);
        break;
    }

    case CommandOpcode::QueryDiagnostics: {
        std::uint8_t temp { 0U };
        std::uint8_t sensor { 0U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            temp = m_state.diagnosticTemp;
            sensor = m_state.diagnosticSensorId;
        }
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics), temp, sensor);
        break;
    }

    case CommandOpcode::QueryPanPosition: {
        std::uint16_t pan { 0U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            pan = m_state.panCentidegrees;
        }
        const std::uint8_t msb = static_cast<std::uint8_t>((pan >> 8U) & 0xFFU);
        const std::uint8_t lsb = static_cast<std::uint8_t>(pan & 0xFFU);
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryPan), msb, lsb);
        break;
    }

    case CommandOpcode::QueryTiltPosition: {
        std::uint16_t tilt { 0U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            tilt = m_state.tiltCentidegrees;
        }
        const std::uint8_t msb = static_cast<std::uint8_t>((tilt >> 8U) & 0xFFU);
        const std::uint8_t lsb = static_cast<std::uint8_t>(tilt & 0xFFU);
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryTilt), msb, lsb);
        break;
    }

    case CommandOpcode::QueryZoomPosition: {
        std::uint16_t zoom { 0U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            zoom = m_state.zoomPosition;
        }
        const std::uint8_t msb = static_cast<std::uint8_t>((zoom >> 8U) & 0xFFU);
        const std::uint8_t lsb = static_cast<std::uint8_t>(zoom & 0xFFU);
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryZoom), msb, lsb);
        break;
    }

    case CommandOpcode::QueryDeviceType: {
        std::uint8_t sw { 0x05U };
        std::uint8_t hw { 0x01U };
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            sw = m_state.swType;
            hw = m_state.hwType;
        }
        sendExtendedReply(0x00U, static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType), sw, hw);
        break;
    }

    case CommandOpcode::Query: {
        sendQueryReply(cksm);
        break;
    }

    default:
        sendGeneralReply(cksm);
        break;
    }
}

void MockPelcoDDevice::sendGeneralReply([[maybe_unused]] std::uint8_t cmdChecksum)
{
    std::uint8_t alarms { 0x00U };
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        alarms = m_state.alarms;
    }

    // 4-byte General Response: [0xFF, addr, alarms, cksm]
    // Standard Pelco-D checksum = (addr + alarms) % 256
    const std::uint8_t replyCksm = static_cast<std::uint8_t>((m_address + alarms) & 0xFFU);
    const std::vector<std::uint8_t> response { PelcoDFrame::SyncByte, m_address, alarms, replyCksm };

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        cb(response);
    }
}

void MockPelcoDDevice::sendExtendedReply(std::uint8_t resp1, std::uint8_t resp2, std::uint8_t d1, std::uint8_t d2)
{
    const std::vector<std::uint8_t> response = PelcoDFrame::createFrame(m_address, resp1, resp2, d1, d2);

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        cb(response);
    }
}

void MockPelcoDDevice::sendQueryReply(std::uint8_t cmdChecksum)
{
    std::vector<std::uint8_t> response(PelcoDFrame::QueryResponseSize, 0x00U);
    response[0] = PelcoDFrame::SyncByte;
    response[1] = m_address;

    std::string model;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        model = m_state.modelName;
    }

    const std::size_t copyLen = std::min(model.size(), static_cast<std::size_t>(15U));
    for (std::size_t i { 0U }; i < copyLen; ++i) {
        response[2U + i] = static_cast<std::uint8_t>(model[i]);
    }

    // Checksum = (sum of bytes 1..16 + cmdChecksum) % 256
    std::uint32_t sum { 0U };
    for (std::size_t i { 1U }; i < 17U; ++i) {
        sum += static_cast<std::uint32_t>(response[i]);
    }
    sum += static_cast<std::uint32_t>(cmdChecksum);
    response[17] = static_cast<std::uint8_t>(sum & 0xFFU);

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        cb(response);
    }
}

void MockPelcoDDevice::injectRxData(const std::vector<std::uint8_t>& data)
{
    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        cb(data);
    }
}

} // namespace PelcoD
