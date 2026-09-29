#include "FlirPfecDevice.h"

#include <algorithm>
#include <cmath>

namespace Nmea {

FlirPfecDevice::FlirPfecDevice(std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer)
    : m_transport(std::move(transport))
    , m_accumulator(maxAccumulatorBuffer)
{
}

FlirPfecDevice::~FlirPfecDevice()
{
    stop();
}

bool FlirPfecDevice::start(std::chrono::milliseconds pollingInterval)
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }

    m_pollingInterval = pollingInterval;

    if (m_transport) {
        m_transport->setDataCallback([this](const std::vector<std::uint8_t>& data) { handleIncomingBytes(data); });
        m_transport->setStateCallback(
            [this](Transport::TransportState state, const std::string& err) { handleTransportState(state, err); });

        if (!m_transport->isOpen()) {
            (void)m_transport->open();
        }
    }

    m_running.store(true);

    if (m_pollingInterval.count() > 0) {
        m_pollerThread = std::thread(&FlirPfecDevice::pollerLoop, this);
    }

    return isConnected();
}

void FlirPfecDevice::stop()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    {
        std::lock_guard<std::mutex> lk(m_pollerMutex);
        m_cv.notify_all();
    }

    if (m_pollerThread.joinable()) {
        m_pollerThread.join();
    }

    if (m_transport) {
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
        m_transport->close();
    }

    m_accumulator.clear();
}

bool FlirPfecDevice::isConnected() const noexcept
{
    return m_transport && m_transport->isOpen();
}

std::shared_ptr<Transport::ITransport> FlirPfecDevice::transport() const noexcept
{
    return m_transport;
}

bool FlirPfecDevice::setVelocity(float panVel, float tiltVel)
{
    const float clampedPan = std::clamp(panVel, -1.0f, 1.0f);
    const float clampedTilt = std::clamp(tiltVel, -1.0f, 1.0f);

    const int panSpeed = static_cast<int>(std::round(clampedPan * 100.0f));
    const int tiltSpeed = static_cast<int>(std::round(clampedTilt * 100.0f));

    return sendSentence(NmeaSentenceBuilder::buildPfecVelocity(panSpeed, tiltSpeed));
}

bool FlirPfecDevice::setAbsoluteAngles(double panDeg, double tiltDeg)
{
    // Normalize pan to [0.0 .. 360.0)
    double normPan = std::fmod(panDeg, 360.0);
    if (normPan < 0.0) {
        normPan += 360.0;
    }

    // Clamp tilt to [-90.0 .. +90.0]
    const double clampedTilt = std::clamp(tiltDeg, -90.0, 90.0);

    return sendSentence(NmeaSentenceBuilder::buildPfecAbsolute(normPan, clampedTilt));
}

bool FlirPfecDevice::stopMotion()
{
    return setVelocity(0.0f, 0.0f);
}

bool FlirPfecDevice::savePreset(std::uint8_t presetId)
{
    return sendSentence(NmeaSentenceBuilder::buildPfecPreset('s', presetId));
}

bool FlirPfecDevice::recallPreset(std::uint8_t presetId)
{
    return sendSentence(NmeaSentenceBuilder::buildPfecPreset('g', presetId));
}

bool FlirPfecDevice::setZoomRate(float zoomVel)
{
    const float clampedZoom = std::clamp(zoomVel, -1.0f, 1.0f);
    const int zoomSpeed = static_cast<int>(std::round(clampedZoom * 100.0f));
    return sendSentence(NmeaSentenceBuilder::buildPfecZoom(zoomSpeed));
}

bool FlirPfecDevice::selectSensor(FlirSensorType sensor)
{
    const std::string_view cmd = (sensor == FlirSensorType::DaylightVisible) ? "c,vis" : "c,ir";
    return sendSentence(NmeaSentenceBuilder::buildPfecCameraCommand(cmd));
}

bool FlirPfecDevice::setColorPalette(FlirColorPalette palette)
{
    char buf[16] {};
    std::snprintf(buf, sizeof(buf), "p,%u", static_cast<unsigned int>(palette));
    return sendSentence(NmeaSentenceBuilder::buildPfecCameraCommand(buf));
}

bool FlirPfecDevice::triggerNuc()
{
    return sendSentence(NmeaSentenceBuilder::buildPfecCameraCommand("nuc"));
}

bool FlirPfecDevice::queryPosition()
{
    return sendSentence(NmeaSentenceBuilder::buildPfecQueryPos());
}

PfecGimbalPosition FlirPfecDevice::currentPosition() const
{
    std::lock_guard<std::mutex> lock(m_posMutex);
    return m_currentPosition;
}

std::size_t FlirPfecDevice::addPositionCallback(PositionCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    const std::size_t id = m_nextCallbackId++;
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, PositionCallback>>>(*m_positionCallbacks);
    newEntries->emplace_back(id, std::move(cb));
    m_positionCallbacks = newEntries;
    return id;
}

void FlirPfecDevice::removePositionCallback(std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, PositionCallback>>>();
    newEntries->reserve(m_positionCallbacks->size());
    for (const auto& item : *m_positionCallbacks) {
        if (item.first != id) {
            newEntries->push_back(item);
        }
    }
    m_positionCallbacks = newEntries;
}

void FlirPfecDevice::feedRawBytes(const std::vector<std::uint8_t>& rawData)
{
    handleIncomingBytes(rawData);
}

void FlirPfecDevice::handleIncomingBytes(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    const auto sentences = m_accumulator.push(data.data(), data.size(), true);

    for (const auto& sentence : sentences) {
        PfecGimbalPosition pos {};
        if (NmeaSentenceParser::parsePfecPos(sentence, pos, true)) {
            {
                std::lock_guard<std::mutex> lock(m_posMutex);
                m_currentPosition = pos;
            }

            std::shared_ptr<const std::vector<std::pair<std::size_t, PositionCallback>>> cbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                cbs = m_positionCallbacks;
            }
            for (const auto& item : *cbs) {
                if (item.second) {
                    item.second(pos);
                }
            }
        }
    }
}

void FlirPfecDevice::handleTransportState(Transport::TransportState /*state*/, const std::string& /*errorMsg*/)
{
}

void FlirPfecDevice::pollerLoop()
{
    while (m_running.load()) {
        (void)queryPosition();

        std::unique_lock<std::mutex> lk(m_pollerMutex);
        m_cv.wait_for(lk, m_pollingInterval, [this]() { return !m_running.load(); });
    }
}

bool FlirPfecDevice::sendSentence(const std::string& sentence)
{
    if (!m_transport || !m_transport->isOpen()) {
        return false;
    }
    const std::vector<std::uint8_t> bytes(sentence.begin(), sentence.end());
    return m_transport->sendData(bytes);
}

} // namespace Nmea
