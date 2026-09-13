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
    if (m_kinematics.getConfig().enabled) {
        m_kinematics.update(std::chrono::steady_clock::now());
        m_state.panCentidegrees = m_kinematics.currentPanCentidegrees();
        m_state.tiltCentidegrees = m_kinematics.currentTiltCentidegrees();
        m_state.zoomPosition = m_kinematics.currentZoomInt();
    }
    return m_state;
}

void MockPelcoDDevice::setInternalState(const MockDeviceState& state)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_state = state;
    m_kinematics.setPositionImmediate(
        state.panCentidegrees / 100.0, state.tiltCentidegrees / 100.0, static_cast<double>(state.zoomPosition));
}

void MockPelcoDDevice::setKinematicsConfig(const KinematicsConfig& config)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_kinematics.setConfig(config);
    m_kinematics.setPositionImmediate(
        m_state.panCentidegrees / 100.0, m_state.tiltCentidegrees / 100.0, static_cast<double>(m_state.zoomPosition));
}

KinematicsConfig MockPelcoDDevice::getKinematicsConfig() const
{
    return m_kinematics.getConfig();
}

void MockPelcoDDevice::setLatencyConfig(const LatencyConfig& config)
{
    m_latency.setConfig(config);
}

LatencyConfig MockPelcoDDevice::getLatencyConfig() const
{
    return m_latency.getConfig();
}

bool MockPelcoDDevice::isMoving() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_kinematics.getConfig().enabled) {
        m_kinematics.update(std::chrono::steady_clock::now());
        return m_kinematics.isMoving();
    }
    return false;
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
    m_latency.flush();
    m_kinematics.stop();
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

            if (m_kinematics.getConfig().enabled) {
                double panFraction = 0.0;
                const double panSpeed = static_cast<double>(data1 & 0x3FU) / 63.0;
                if ((cmd2 & 0x02U) != 0U) {
                    panFraction = panSpeed;
                } else if ((cmd2 & 0x04U) != 0U) {
                    panFraction = -panSpeed;
                }

                double tiltFraction = 0.0;
                const double tiltSpeed = static_cast<double>(data2 & 0x3FU) / 63.0;
                if ((cmd2 & 0x08U) != 0U) {
                    tiltFraction = tiltSpeed;
                } else if ((cmd2 & 0x10U) != 0U) {
                    tiltFraction = -tiltSpeed;
                }

                double zoomFraction = 0.0;
                if ((cmd2 & 0x20U) != 0U) {
                    zoomFraction = 1.0;
                } else if ((cmd2 & 0x40U) != 0U) {
                    zoomFraction = -1.0;
                }

                if (panFraction == 0.0 && tiltFraction == 0.0 && zoomFraction == 0.0) {
                    m_kinematics.stop();
                } else {
                    m_kinematics.setDirectionalMotion(panFraction, tiltFraction, zoomFraction);
                }
            } else {
                // Instantaneous stepping
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

                m_kinematics.setPositionImmediate(
                    m_state.panCentidegrees / 100.0, m_state.tiltCentidegrees / 100.0, static_cast<double>(m_state.zoomPosition));
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
                if (m_kinematics.getConfig().enabled) {
                    m_kinematics.update(std::chrono::steady_clock::now());
                    const double newPan = std::fmod(m_kinematics.currentPanDeg() + 180.0, 360.0);
                    m_kinematics.slewTo(newPan, m_kinematics.currentTiltDeg());
                } else {
                    m_state.panCentidegrees = static_cast<std::uint16_t>((m_state.panCentidegrees + 18000U) % 36000U);
                    m_kinematics.setPositionImmediate(
                        m_state.panCentidegrees / 100.0, m_kinematics.currentTiltDeg(), m_kinematics.currentZoom());
                }
            } else if (data2 == 0x22U) {
                // Go to zero pan
                if (m_kinematics.getConfig().enabled) {
                    m_kinematics.update(std::chrono::steady_clock::now());
                    m_kinematics.slewTo(0.0, m_kinematics.currentTiltDeg());
                } else {
                    m_state.panCentidegrees = 0U;
                    m_kinematics.setPositionImmediate(0.0, m_kinematics.currentTiltDeg(), m_kinematics.currentZoom());
                }
            } else {
                const auto it = m_state.presets.find(data2);
                if (it != m_state.presets.end()) {
                    if (m_kinematics.getConfig().enabled) {
                        m_kinematics.slewTo(it->second.pan / 100.0, it->second.tilt / 100.0);
                        m_kinematics.slewZoomTo(it->second.zoom);
                    } else {
                        m_state.panCentidegrees = it->second.pan;
                        m_state.tiltCentidegrees = it->second.tilt;
                        m_state.zoomPosition = it->second.zoom;
                        m_kinematics.setPositionImmediate(
                            it->second.pan / 100.0, it->second.tilt / 100.0, static_cast<double>(it->second.zoom));
                    }
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
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.slewTo(m_state.panCentidegrees / 100.0, m_kinematics.currentTiltDeg());
            } else {
                m_kinematics.setPositionImmediate(
                    m_state.panCentidegrees / 100.0, m_kinematics.currentTiltDeg(), m_kinematics.currentZoom());
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetTiltPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            const std::uint16_t tilt = static_cast<std::uint16_t>((data1 << 8U) | data2);
            m_state.tiltCentidegrees = static_cast<std::uint16_t>(tilt % 36000U);
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.slewTo(m_kinematics.currentPanDeg(), m_state.tiltCentidegrees / 100.0);
            } else {
                m_kinematics.setPositionImmediate(
                    m_kinematics.currentPanDeg(), m_state.tiltCentidegrees / 100.0, m_kinematics.currentZoom());
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetZoomPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            const std::uint16_t zoom = static_cast<std::uint16_t>((data1 << 8U) | data2);
            m_state.zoomPosition = zoom;
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.slewZoomTo(zoom);
            } else {
                m_kinematics.setPositionImmediate(
                    m_kinematics.currentPanDeg(), m_kinematics.currentTiltDeg(), static_cast<double>(zoom));
            }
        }
        sendGeneralReply(cksm);
        break;
    }

    case CommandOpcode::SetZeroPosition: {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state.panCentidegrees = 0U;
            m_kinematics.setPositionImmediate(0.0, m_kinematics.currentTiltDeg(), m_kinematics.currentZoom());
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
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.update(std::chrono::steady_clock::now());
                pan = m_kinematics.currentPanCentidegrees();
                m_state.panCentidegrees = pan;
            } else {
                pan = m_state.panCentidegrees;
            }
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
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.update(std::chrono::steady_clock::now());
                tilt = m_kinematics.currentTiltCentidegrees();
                m_state.tiltCentidegrees = tilt;
            } else {
                tilt = m_state.tiltCentidegrees;
            }
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
            if (m_kinematics.getConfig().enabled) {
                m_kinematics.update(std::chrono::steady_clock::now());
                zoom = m_kinematics.currentZoomInt();
                m_state.zoomPosition = zoom;
            } else {
                zoom = m_state.zoomPosition;
            }
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
    std::vector<std::uint8_t> response { PelcoDFrame::SyncByte, m_address, alarms, replyCksm };

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        m_latency.enqueue(std::move(response), std::move(cb));
    }
}

void MockPelcoDDevice::sendExtendedReply(std::uint8_t resp1, std::uint8_t resp2, std::uint8_t d1, std::uint8_t d2)
{
    std::vector<std::uint8_t> response = PelcoDFrame::createFrame(m_address, resp1, resp2, d1, d2);

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        m_latency.enqueue(std::move(response), std::move(cb));
    }
}

void MockPelcoDDevice::sendQueryReply([[maybe_unused]] std::uint8_t cmdChecksum)
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

    // Standard Pelco-D checksum = sum of bytes 1..16 % 256
    response[17] = PelcoDFrame::calculateChecksum(&response[1], 16U);

    DataReceivedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_dataCallback;
    }
    if (cb) {
        m_latency.enqueue(std::move(response), std::move(cb));
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
        m_latency.enqueue(data, std::move(cb));
    }
}

} // namespace PelcoD
