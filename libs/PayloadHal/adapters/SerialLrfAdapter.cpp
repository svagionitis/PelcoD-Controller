/// @file SerialLrfAdapter.cpp
/// @brief Implementation of the physical serial/stream LRF hardware adapter.

#include "SerialLrfAdapter.h"

#include <utility>

namespace PayloadHal {

SerialLrfAdapter::SerialLrfAdapter(std::shared_ptr<Transport::ITransport> transport, SerialLrfConfig config)
    : m_transport(std::move(transport))
    , m_config(std::move(config))
    , m_parser(createLrfParser(m_config))
    , m_gateMin(m_config.minRangeMeters)
    , m_gateMax(m_config.maxRangeMeters)
{
}

SerialLrfAdapter::~SerialLrfAdapter()
{
    disconnect();
}

bool SerialLrfAdapter::connect()
{
    std::shared_ptr<Transport::ITransport> trans;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_connected) {
            return true;
        }
        if (!m_transport) {
            return false;
        }
        trans = m_transport;
    }

    trans->setDataCallback([this](const std::vector<std::uint8_t>& data) {
        handleIncomingBytes(data);
    });

    trans->setStateCallback([this](Transport::TransportState st, const std::string& err) {
        handleTransportState(st, err);
    });

    if (!trans->isOpen()) {
        if (!trans->open()) {
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected = true;
        m_armed = false;
        m_mode = LrfMode::Standby;
        m_lastActivityTime = std::chrono::steady_clock::now();
        m_lastContinuousPulseTime = m_lastActivityTime;

        if (!m_running) {
            m_running = true;
            m_workerThread = std::thread(&SerialLrfAdapter::workerLoop, this);
        }
    }

    return true;
}

void SerialLrfAdapter::disconnect()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected && !m_running) {
            return;
        }
        m_running = false;
        disarmLaserLocked();
        m_connected = false;
    }
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (m_transport && m_transport->isOpen()) {
        m_transport->close();
    }
}

bool SerialLrfAdapter::isConnected() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected && m_transport && m_transport->isOpen();
}

DeviceState SerialLrfAdapter::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected || !m_transport || !m_transport->isOpen()) {
        return DeviceState::Disconnected;
    }
    return DeviceState::Ready;
}

DeviceInfo SerialLrfAdapter::info() const noexcept
{
    DeviceInfo d {};
    d.manufacturer = "Serial LRF Hardware";
    switch (m_config.protocolType) {
    case LrfProtocolType::Nmea:
        d.model = "NMEA-0183 Laser Range Finder";
        break;
    case LrfProtocolType::Ascii:
        d.model = "ASCII Delimited Laser Range Finder";
        break;
    case LrfProtocolType::Binary:
        d.model = "Binary Framed Laser Range Finder";
        break;
    }
    d.firmwareVersion = "1.0.0-serial";
    return d;
}

void SerialLrfAdapter::registerStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCb = std::move(cb);
}

bool SerialLrfAdapter::armLaser()
{
    std::vector<std::uint8_t> cmd;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected || !m_transport || !m_transport->isOpen()) {
            return false;
        }
        m_armed = true;
        m_lastActivityTime = std::chrono::steady_clock::now();
        cmd = m_parser->buildArmCommand();
    }

    if (!cmd.empty()) {
        return m_transport->sendData(cmd);
    }
    return true;
}

bool SerialLrfAdapter::disarmLaser()
{
    std::vector<std::uint8_t> cmd;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        disarmLaserLocked();
        if (m_connected && m_transport && m_transport->isOpen()) {
            cmd = m_parser->buildDisarmCommand();
        }
    }

    if (!cmd.empty()) {
        (void)m_transport->sendData(cmd);
    }
    return true;
}

void SerialLrfAdapter::disarmLaserLocked()
{
    m_armed = false;
    m_mode = LrfMode::Standby;
}

bool SerialLrfAdapter::isArmed() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_armed;
}

bool SerialLrfAdapter::triggerSingleMeasurement()
{
    std::vector<std::uint8_t> cmd;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected || !m_transport || !m_transport->isOpen()) {
            return false;
        }
        if (m_config.enforceArmingInterlock && !m_armed) {
            // Strict eye safety interlock violation
            return false;
        }

        m_lastActivityTime = std::chrono::steady_clock::now();
        cmd = m_parser->buildFireCommand();
    }

    if (!cmd.empty()) {
        return m_transport->sendData(cmd);
    }
    return false;
}

bool SerialLrfAdapter::setContinuousMode(LrfMode mode)
{
    std::vector<std::uint8_t> cmd;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected || !m_transport || !m_transport->isOpen()) {
            return false;
        }
        if (m_config.enforceArmingInterlock && !m_armed && mode != LrfMode::Standby) {
            return false;
        }

        m_mode = mode;
        m_lastActivityTime = std::chrono::steady_clock::now();
        m_lastContinuousPulseTime = m_lastActivityTime;

        if (mode == LrfMode::Standby) {
            cmd = m_parser->buildStopCommand();
        } else {
            cmd = m_parser->buildContinuousCommand(mode);
        }
    }

    if (!cmd.empty()) {
        return m_transport->sendData(cmd);
    }
    return true;
}

bool SerialLrfAdapter::stopRanging()
{
    return setContinuousMode(LrfMode::Standby);
}

bool SerialLrfAdapter::setRangeGating(double minRangeMeters, double maxRangeMeters)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (minRangeMeters < 0.0 || maxRangeMeters < minRangeMeters) {
        return false;
    }
    m_gateMin = minRangeMeters;
    m_gateMax = maxRangeMeters;
    return true;
}

void SerialLrfAdapter::registerMeasurementCallback(MeasurementCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_measurementCb = std::move(cb);
}

std::optional<LrfTargetMeasurement> SerialLrfAdapter::lastMeasurement() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastMeasurement;
}

const SerialLrfConfig& SerialLrfAdapter::config() const noexcept
{
    return m_config;
}

LrfMode SerialLrfAdapter::activeMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mode;
}

std::pair<double, double> SerialLrfAdapter::rangeGate() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return { m_gateMin, m_gateMax };
}

std::shared_ptr<Transport::ITransport> SerialLrfAdapter::transport() const noexcept
{
    return m_transport;
}

void SerialLrfAdapter::handleIncomingBytes(const std::vector<std::uint8_t>& data)
{
    std::vector<LrfTargetMeasurement> measurements;
    MeasurementCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_parser) {
            return;
        }

        measurements = m_parser->parseIncomingBytes(data.data(), data.size());
        for (auto& meas : measurements) {
            // Apply Range Gating
            if (meas.valid) {
                if (meas.slantRangeMeters < m_gateMin || meas.slantRangeMeters > m_gateMax) {
                    meas.valid = false;
                }
            }
            m_lastMeasurement = meas;
        }
        cb = m_measurementCb;
    }

    if (cb) {
        for (const auto& meas : measurements) {
            cb(meas);
        }
    }
}

void SerialLrfAdapter::handleTransportState(Transport::TransportState state, const std::string& errorMsg)
{
    StateCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (state != Transport::TransportState::Connected) {
            // Disconnect or error: immediately disarm laser for eye safety
            disarmLaserLocked();
        }
        cb = m_stateCb;
    }

    if (cb) {
        const auto devState = (state == Transport::TransportState::Connected) ? DeviceState::Ready : DeviceState::Disconnected;
        cb(devState, errorMsg);
    }
}

void SerialLrfAdapter::workerLoop()
{
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(50), [this] {
            return !m_running;
        });

        if (!m_running) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();

        // 1. Auto-Disarm Watchdog Check (ANSI Z136 eye-safety compliance)
        if (m_armed && m_config.autoDisarmTimeout.count() > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastActivityTime);
            if (elapsed >= m_config.autoDisarmTimeout) {
                disarmLaserLocked();
                const auto cb = m_stateCb;
                lock.unlock();

                if (m_transport && m_transport->isOpen()) {
                    (void)m_transport->sendData(m_parser->buildDisarmCommand());
                }
                if (cb) {
                    cb(DeviceState::Ready, "Laser transmitter auto-disarmed due to safety inactivity timeout");
                }
                continue;
            }
        }

        // 2. Continuous Pulse Repetition Loop
        if (m_armed && m_mode != LrfMode::Standby) {
            std::chrono::milliseconds periodMs { 1000 };
            if (m_mode == LrfMode::Continuous5Hz) {
                periodMs = std::chrono::milliseconds(200);
            } else if (m_mode == LrfMode::Continuous10Hz) {
                periodMs = std::chrono::milliseconds(100);
            }

            const auto elapsedPulse = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastContinuousPulseTime);
            if (elapsedPulse >= periodMs) {
                m_lastContinuousPulseTime = now;
                m_lastActivityTime = now;
                const auto fireCmd = m_parser->buildFireCommand();
                lock.unlock();

                if (!fireCmd.empty() && m_transport && m_transport->isOpen()) {
                    (void)m_transport->sendData(fireCmd);
                }
            }
        }
    }
}

} // namespace PayloadHal
