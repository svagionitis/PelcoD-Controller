/// @file PelcoDDevice.cpp
/// @brief Implementation of Pelco-D device controller.

#include "PelcoDDevice.h"
#include "PelcoDFrame.h"

#include <chrono>
#include <glog/logging.h>

namespace PelcoD {

PelcoDDevice::PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : m_transport { std::move(transport) }
    , m_address { address }
{
    m_status.address = address;
}

PelcoDDevice::~PelcoDDevice()
{
    stop();
}

bool PelcoDDevice::start()
{
    std::lock_guard<std::recursive_mutex> lifecycleLock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }

    if (!m_transport) {
        return false;
    }

    m_transport->setDataCallback([this](const std::vector<std::uint8_t>& data) { onDataReceived(data); });
    m_transport->setStateCallback([this](TransportState state, const std::string& msg) {
        if (state == TransportState::Disconnected || state == TransportState::Error) {
            LOG(WARNING) << "Transport disconnected or error: " << msg;
            bool wasConn = false;
            {
                std::lock_guard<std::mutex> lock(m_statusMutex);
                wasConn = m_status.connected;
                m_status.connected = false;
            }
            if (wasConn) {
                std::shared_ptr<const std::vector<StatusCallback>> sbs;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    sbs = m_statusCallbacks;
                }
                DeviceStatus copy;
                {
                    std::lock_guard<std::mutex> lock(m_statusMutex);
                    copy = m_status;
                }
                for (const auto& cb : *sbs) {
                    if (cb) {
                        cb(copy);
                    }
                }
            }
        }
    });

    if (!m_transport->isOpen()) {
        if (!m_transport->open()) {
            m_transport->setDataCallback(nullptr);
            m_transport->setStateCallback(nullptr);
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.connected = true;
    }

    LOG(INFO) << "Starting PelcoDDevice controller (address: " << static_cast<int>(m_address) << ")";

    m_rxRing.clear();
    m_running = true;
    m_rxThread = std::thread(&PelcoDDevice::rxLoop, this);
    m_workerThread = std::thread(&PelcoDDevice::workerLoop, this);
    m_pollThread = std::thread(&PelcoDDevice::pollingLoop, this);

    return true;
}

void PelcoDDevice::stop()
{
    std::lock_guard<std::recursive_mutex> lifecycleLock(m_lifecycleMutex);
    if (!m_running.load() && !m_rxThread.joinable() && !m_workerThread.joinable() && !m_pollThread.joinable()) {
        if (m_transport) {
            if (m_transport->isOpen()) {
                m_transport->close();
            }
            m_transport->setDataCallback(nullptr);
            m_transport->setStateCallback(nullptr);
        }
        return;
    }

    LOG(INFO) << "Stopping PelcoDDevice controller";
    m_running = false;
    m_queueCv.notify_all();
    m_rxCv.notify_all();
    m_responseCv.notify_all();
    m_pollCv.notify_all();

    if (m_rxThread.joinable()) {
        m_rxThread.join();
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    if (m_pollThread.joinable()) {
        m_pollThread.join();
    }
    m_awaitingResponse = false;

    if (m_transport) {
        if (m_transport->isOpen()) {
            m_transport->close();
        }
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.connected = false;
    }
}

bool PelcoDDevice::isConnected() const noexcept
{
    return m_running && m_transport && m_transport->isOpen();
}

void PelcoDDevice::setAddress(std::uint8_t address)
{
    m_address = address;
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status.address = address;
}

std::uint8_t PelcoDDevice::getAddress() const noexcept
{
    return m_address;
}

void PelcoDDevice::addStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newCallbacks = std::make_shared<std::vector<StatusCallback>>(*m_statusCallbacks);
    newCallbacks->push_back(std::move(cb));
    m_statusCallbacks = std::move(newCallbacks);
}

void PelcoDDevice::addTrafficCallback(TrafficCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newCallbacks = std::make_shared<std::vector<TrafficCallback>>(*m_trafficCallbacks);
    newCallbacks->push_back(std::move(cb));
    m_trafficCallbacks = std::move(newCallbacks);
}

void PelcoDDevice::addTimeoutCallback(TimeoutCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newCallbacks = std::make_shared<std::vector<TimeoutCallback>>(*m_timeoutCallbacks);
    newCallbacks->push_back(std::move(cb));
    m_timeoutCallbacks = std::move(newCallbacks);
}

void PelcoDDevice::clearCallbacks()
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statusCallbacks = std::make_shared<const std::vector<StatusCallback>>();
    m_trafficCallbacks = std::make_shared<const std::vector<TrafficCallback>>();
    m_timeoutCallbacks = std::make_shared<const std::vector<TimeoutCallback>>();
}

DeviceStatus PelcoDDevice::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

DeviceInfo PelcoDDevice::getInfo() const
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_info;
}

void PelcoDDevice::setTelemetryPolling(bool enable, std::uint32_t intervalMs) noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_pollMutex);
        m_telemetryPolling.store(enable);
        m_pollIntervalMs.store((intervalMs > 0U) ? intervalMs : 1000U);
        ++m_pollEpoch;
    }
    m_pollCv.notify_all();
}

bool PelcoDDevice::getTelemetryPolling() const noexcept
{
    return m_telemetryPolling.load();
}

void PelcoDDevice::setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept
{
    m_queryTimeoutMs.store((timeoutMs > 0U) ? timeoutMs : 1000U);
}

std::uint32_t PelcoDDevice::getQueryTimeoutMs() const noexcept
{
    return m_queryTimeoutMs.load();
}

void PelcoDDevice::panLeft(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildPan(m_address, PanDirection::Left, speed));
}

void PelcoDDevice::panRight(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildPan(m_address, PanDirection::Right, speed));
}

void PelcoDDevice::tiltUp(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildTilt(m_address, TiltDirection::Up, speed));
}

void PelcoDDevice::tiltDown(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildTilt(m_address, TiltDirection::Down, speed));
}

void PelcoDDevice::stopMotion()
{
    enqueueCommand(ProtocolBuilder::buildStop(m_address));
}

void PelcoDDevice::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed)
{
    enqueueCommand(ProtocolBuilder::buildMotion(m_address, panDir, panSpeed, tiltDir, tiltSpeed));
}

void PelcoDDevice::zoomTele()
{
    enqueueCommand(ProtocolBuilder::buildZoom(m_address, ZoomAction::Tele));
}

void PelcoDDevice::zoomWide()
{
    enqueueCommand(ProtocolBuilder::buildZoom(m_address, ZoomAction::Wide));
}

void PelcoDDevice::zoomStop()
{
    enqueueCommand(ProtocolBuilder::buildZoom(m_address, ZoomAction::Stop));
}

void PelcoDDevice::focusNear()
{
    enqueueCommand(ProtocolBuilder::buildFocus(m_address, FocusAction::Near));
}

void PelcoDDevice::focusFar()
{
    enqueueCommand(ProtocolBuilder::buildFocus(m_address, FocusAction::Far));
}

void PelcoDDevice::focusStop()
{
    enqueueCommand(ProtocolBuilder::buildFocus(m_address, FocusAction::Stop));
}

void PelcoDDevice::irisOpen()
{
    enqueueCommand(ProtocolBuilder::buildIris(m_address, IrisAction::Open));
}

void PelcoDDevice::irisClose()
{
    enqueueCommand(ProtocolBuilder::buildIris(m_address, IrisAction::Close));
}

void PelcoDDevice::irisStop()
{
    enqueueCommand(ProtocolBuilder::buildIris(m_address, IrisAction::Stop));
}

void PelcoDDevice::setPanAngle(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetPan(m_address, centidegrees));
}

void PelcoDDevice::setTiltAngle(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetTilt(m_address, centidegrees));
}

void PelcoDDevice::setZoomPosition(std::uint16_t position)
{
    enqueueCommand(ProtocolBuilder::buildSetZoom(m_address, position));
}

void PelcoDDevice::setPreset(std::uint8_t presetId)
{
    enqueueCommand(ProtocolBuilder::buildSetPreset(m_address, presetId));
}

void PelcoDDevice::clearPreset(std::uint8_t presetId)
{
    enqueueCommand(ProtocolBuilder::buildClearPreset(m_address, presetId));
}

void PelcoDDevice::goToPreset(std::uint8_t presetId)
{
    enqueueCommand(ProtocolBuilder::buildGoToPreset(m_address, presetId));
}

void PelcoDDevice::flip180()
{
    enqueueCommand(ProtocolBuilder::buildFlip180(m_address));
}

void PelcoDDevice::zeroPan()
{
    enqueueCommand(ProtocolBuilder::buildZeroPan(m_address));
}

void PelcoDDevice::setAuxiliary(std::uint8_t auxId)
{
    enqueueCommand(ProtocolBuilder::buildSetAux(m_address, auxId));
}

void PelcoDDevice::clearAuxiliary(std::uint8_t auxId)
{
    enqueueCommand(ProtocolBuilder::buildClearAux(m_address, auxId));
}

void PelcoDDevice::setZoneStart(std::uint8_t zoneId)
{
    enqueueCommand(ProtocolBuilder::buildSetZoneStart(m_address, zoneId));
}

void PelcoDDevice::setZoneEnd(std::uint8_t zoneId)
{
    enqueueCommand(ProtocolBuilder::buildSetZoneEnd(m_address, zoneId));
}

void PelcoDDevice::setZoneScan(bool enable)
{
    enqueueCommand(ProtocolBuilder::buildZoneScan(m_address, enable));
}

void PelcoDDevice::recordPatternStart(std::uint8_t patternId)
{
    enqueueCommand(ProtocolBuilder::buildPatternStart(m_address, patternId));
}

void PelcoDDevice::recordPatternStop()
{
    enqueueCommand(ProtocolBuilder::buildPatternStop(m_address));
}

void PelcoDDevice::runPattern(std::uint8_t patternId)
{
    enqueueCommand(ProtocolBuilder::buildRunPattern(m_address, patternId));
}

void PelcoDDevice::setZoomSpeed(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildZoomSpeed(m_address, speed));
}

void PelcoDDevice::setFocusSpeed(std::uint8_t speed)
{
    enqueueCommand(ProtocolBuilder::buildFocusSpeed(m_address, speed));
}

void PelcoDDevice::setAutoFocus(AutoMode mode)
{
    enqueueCommand(ProtocolBuilder::buildAutoFocus(m_address, mode));
}

void PelcoDDevice::setAutoIris(AutoMode mode)
{
    enqueueCommand(ProtocolBuilder::buildAutoIris(m_address, mode));
}

void PelcoDDevice::setAgc(AutoMode mode)
{
    enqueueCommand(ProtocolBuilder::buildAgc(m_address, mode));
}

void PelcoDDevice::setBacklightComp(SwitchState state)
{
    enqueueCommand(ProtocolBuilder::buildBacklight(m_address, state));
}

void PelcoDDevice::setAutoWhiteBalance(SwitchState state)
{
    enqueueCommand(ProtocolBuilder::buildWhiteBalance(m_address, state));
}

void PelcoDDevice::setShutterSpeed(std::uint16_t speed)
{
    enqueueCommand(ProtocolBuilder::buildShutterSpeed(m_address, speed));
}

void PelcoDDevice::setGain(std::uint16_t gain)
{
    enqueueCommand(ProtocolBuilder::buildGain(m_address, gain));
}

void PelcoDDevice::setAutoIrisLevel(std::uint8_t level)
{
    enqueueCommand(ProtocolBuilder::buildAutoIrisLevel(m_address, level));
}

void PelcoDDevice::setAutoIrisPeak(std::uint8_t peak)
{
    enqueueCommand(ProtocolBuilder::buildAutoIrisPeak(m_address, peak));
}

void PelcoDDevice::setPhaseDelayMode(SwitchState state)
{
    enqueueCommand(ProtocolBuilder::buildPhaseDelayMode(m_address, state));
}

void PelcoDDevice::adjustLineLockDelay(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildLineLockDelay(m_address, centidegrees));
}

void PelcoDDevice::adjustWhiteBalanceRB(std::uint16_t value)
{
    enqueueCommand(ProtocolBuilder::buildWhiteBalanceRB(m_address, value));
}

void PelcoDDevice::adjustWhiteBalanceMG(std::uint16_t value)
{
    enqueueCommand(ProtocolBuilder::buildWhiteBalanceMG(m_address, value));
}

void PelcoDDevice::setMagnification(std::uint16_t value, bool relative)
{
    enqueueCommand(ProtocolBuilder::buildSetMagnification(m_address, value, relative));
}

void PelcoDDevice::setBaudRate(std::uint32_t baud)
{
    enqueueCommand(ProtocolBuilder::buildSetBaudRate(m_address, baud));
}

void PelcoDDevice::setZeroPosition()
{
    enqueueCommand(ProtocolBuilder::buildSetZeroPosition(m_address));
}

void PelcoDDevice::resetDefaults()
{
    enqueueCommand(ProtocolBuilder::buildResetDefaults(m_address));
}

void PelcoDDevice::remoteReset()
{
    enqueueCommand(ProtocolBuilder::buildRemoteReset(m_address));
}

void PelcoDDevice::queryPan()
{
    sendQueryFrame(ProtocolBuilder::buildQueryPan(m_address), "QueryPan");
}

void PelcoDDevice::queryTilt()
{
    sendQueryFrame(ProtocolBuilder::buildQueryTilt(m_address), "QueryTilt");
}

void PelcoDDevice::queryZoom()
{
    sendQueryFrame(ProtocolBuilder::buildQueryZoom(m_address), "QueryZoom");
}

void PelcoDDevice::queryMagnification()
{
    sendQueryFrame(ProtocolBuilder::buildQueryMag(m_address), "QueryMagnification");
}

void PelcoDDevice::queryDeviceType()
{
    sendQueryFrame(ProtocolBuilder::buildQueryDevType(m_address), "QueryDeviceType");
}

void PelcoDDevice::queryGeneral()
{
    sendQueryFrame(ProtocolBuilder::buildQueryGeneral(m_address), "QueryGeneral");
}

void PelcoDDevice::queryDiagnostics()
{
    sendQueryFrame(ProtocolBuilder::buildQueryDiagnostics(m_address), "QueryDiagnostics");
}

void PelcoDDevice::queryAll()
{
    queryPan();
    queryTilt();
    queryZoom();
    queryMagnification();
    queryDeviceType();
    queryGeneral();
}

void PelcoDDevice::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    enqueueCommand(frame);
}

void PelcoDDevice::sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag)
{
    enqueueCommand(frame, std::move(queryTag));
}

void PelcoDDevice::enqueueCommand(const std::vector<std::uint8_t>& frame, std::string queryTag)
{
    constexpr std::size_t MaxQueueSize = 256U;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_commandQueue.size() >= MaxQueueSize) {
            LOG(WARNING) << "PelcoDDevice command queue full (" << MaxQueueSize << " items), dropping oldest command";
            m_commandQueue.pop_front();
        }
        m_commandQueue.push_back({ frame, std::move(queryTag) });
    }
    m_queueCv.notify_one();
}

void PelcoDDevice::checkQueryTimeout()
{
    if (!m_awaitingResponse.load()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_querySentTime).count();
    const auto timeoutMs = m_queryTimeoutMs.load();
    if (elapsed >= static_cast<long long>(timeoutMs)) {
        m_awaitingResponse = false;
        m_responseCv.notify_all();
        std::string tag;
        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            tag = m_pendingQueryTag;
        }

        LOG(WARNING) << "Query timeout: No response received for query '" << tag << "' within " << timeoutMs
                     << " ms";

        std::shared_ptr<const std::vector<TimeoutCallback>> cbs;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            cbs = m_timeoutCallbacks;
        }
        for (const auto& cb : *cbs) {
            if (cb) {
                cb(tag);
            }
        }
    }
}

void PelcoDDevice::workerLoop()
{
    while (m_running) {
        checkQueryTimeout();

        CommandItem item;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait_for(
                lock, std::chrono::milliseconds(50), [this] { return !m_commandQueue.empty() || !m_running; });

            if (!m_running) {
                break;
            }
            if (m_commandQueue.empty()) {
                continue;
            }

            item = std::move(m_commandQueue.front());
            m_commandQueue.pop_front();
        }

        if (m_transport && m_transport->isOpen() && !item.frame.empty()) {
            if (!item.queryTag.empty()) {
                std::lock_guard<std::mutex> lock(m_statusMutex);
                m_pendingQueryTag = item.queryTag;
                m_querySentTime = std::chrono::steady_clock::now();
                m_awaitingResponse = true;
            }

            VLOG(1) << "Sending command (" << item.frame.size() << " bytes, tag: '" << item.queryTag << "')";
            const bool sendSuccess = m_transport->sendData(item.frame);
            if (!sendSuccess) {
                LOG(WARNING) << "Failed to transmit frame across transport.";
            }

            // Dispatch TX traffic callbacks using copy-on-write snapshot (zero heap allocation)
            std::shared_ptr<const std::vector<TrafficCallback>> tbs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                tbs = m_trafficCallbacks;
            }
            for (const auto& cb : *tbs) {
                if (cb) {
                    cb(true, item.frame);
                }
            }

            if (!item.queryTag.empty()) {
                {
                    std::unique_lock<std::mutex> lock(m_statusMutex);
                    m_responseCv.wait_for(lock, std::chrono::milliseconds(m_queryTimeoutMs.load()),
                        [this] { return !m_awaitingResponse.load() || !m_running; });
                }
                checkQueryTimeout();
            }
        }

        // 20ms inter-command delay per Pelco-D RS-485 specification
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void PelcoDDevice::pollingLoop()
{
    while (m_running) {
        std::uint64_t currentEpoch { 0U };
        {
            std::unique_lock<std::mutex> lock(m_pollMutex);
            if (!m_telemetryPolling.load()) {
                m_pollCv.wait(lock, [this] { return m_telemetryPolling.load() || !m_running; });
            } else {
                currentEpoch = m_pollEpoch;
                const auto interval = std::chrono::milliseconds(m_pollIntervalMs.load());
                m_pollCv.wait_for(lock, interval, [this, currentEpoch] {
                    return !m_running || !m_telemetryPolling.load() || m_pollEpoch != currentEpoch;
                });
            }
        }

        if (!m_running) {
            break;
        }

        if (m_telemetryPolling.load() && isConnected()) {
            queryPan();
            queryTilt();
            queryZoom();
        }
    }
}

void PelcoDDevice::onDataReceived(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    if (!m_rxRing.writeExact(data.data(), data.size())) {
        LOG(WARNING) << "PelcoDDevice RX ring buffer overflow: dropped " << data.size() << " bytes";
    }
    m_rxCv.notify_one();
}

void PelcoDDevice::rxLoop()
{
    while (m_running) {
        {
            std::unique_lock<std::mutex> lock(m_rxMutex);
            m_rxCv.wait_for(lock, std::chrono::milliseconds(50),
                [this] { return (m_rxRing.availableRead() >= PelcoDFrame::GeneralResponseSize) || !m_running; });
        }

        if (!m_running) {
            break;
        }

        while (m_rxRing.availableRead() >= PelcoDFrame::GeneralResponseSize) {
            const std::size_t syncOffset = m_rxRing.findByte(PelcoDFrame::SyncByte);
            if (syncOffset == decltype(m_rxRing)::npos) {
                m_rxRing.advanceRead(m_rxRing.availableRead());
                break;
            }

            if (syncOffset > 0U) {
                m_rxRing.advanceRead(syncOffset);
            }

            const std::size_t available = m_rxRing.availableRead();
            if (available < PelcoDFrame::GeneralResponseSize) {
                break;
            }

            constexpr std::size_t candidateSizes[]
                = { PelcoDFrame::StandardFrameSize, PelcoDFrame::GeneralResponseSize, PelcoDFrame::QueryResponseSize };

            bool frameExtracted = false;
            std::array<std::uint8_t, PelcoDFrame::QueryResponseSize> peekBuf {};

            for (const std::size_t candidateSize : candidateSizes) {
                if (available >= candidateSize) {
                    if (m_rxRing.peekBytes(peekBuf.data(), candidateSize)) {
                        std::vector<std::uint8_t> frame(peekBuf.begin(), peekBuf.begin() + candidateSize);
                        if (PelcoDFrame::isValidFrame(frame)) {
                            m_rxRing.advanceRead(candidateSize);
                            frameExtracted = true;
                            dispatchFrame(frame);
                            break;
                        }
                    }
                }
            }

            if (!frameExtracted) {
                const bool awaitingQuery = m_awaitingResponse.load();
                const std::size_t maxExpectedSize
                    = awaitingQuery ? PelcoDFrame::QueryResponseSize : PelcoDFrame::StandardFrameSize;
                if (available < maxExpectedSize) {
                    break;
                }
                m_rxRing.advanceRead(1U);
            }
        }
    }
}

bool PelcoDDevice::isResponseMatchingQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.empty()) {
        return false;
    }

    if (queryTag == "QueryPan") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryPan);
    }
    if (queryTag == "QueryTilt") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt);
    }
    if (queryTag == "QueryZoom") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom);
    }
    if (queryTag == "QueryMagnification") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification);
    }
    if (queryTag == "QueryDeviceType") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType);
    }
    if (queryTag == "QueryDiagnostics") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics);
    }
    if (queryTag == "QueryGeneral") {
        return frame.size() == PelcoDFrame::QueryResponseSize;
    }

    // Generic query fallback: any recognized standard query response opcode or 18-byte query response
    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        const std::uint8_t op = frame[3];
        return op == static_cast<std::uint8_t>(ResponseOpcode::QueryPan)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics);
    }
    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        return true;
    }

    return false;
}

void PelcoDDevice::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    // Notify RX traffic callbacks using copy-on-write snapshot (zero heap allocation)
    std::shared_ptr<const std::vector<TrafficCallback>> tbs;
    std::shared_ptr<const std::vector<StatusCallback>> sbs;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        tbs = m_trafficCallbacks;
        sbs = m_statusCallbacks;
    }

    for (const auto& cb : *tbs) {
        if (cb) {
            cb(false, frame);
        }
    }

    // Ignore frames with invalid structure or addressed to another device on shared bus
    if (frame.size() < 2U || frame[1] != m_address.load()) {
        return;
    }

    bool awaitingQuery { false };
    std::string pendingQueryTag;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        awaitingQuery = m_awaitingResponse.load();
        pendingQueryTag = m_pendingQueryTag;
    }
    if (awaitingQuery && !isResponseMatchingQuery(pendingQueryTag, frame)) {
        return;
    }

    DeviceStatus currentStatus;
    DeviceInfo currentInfo;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        currentStatus = m_status;
        currentInfo = m_info;
    }

    if (ProtocolParser::updateStatus(frame, currentStatus, currentInfo)) {
        bool querySatisfied = false;
        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            if (m_awaitingResponse.load() && isResponseMatchingQuery(m_pendingQueryTag, frame)) {
                m_awaitingResponse = false;
                m_pendingQueryTag.clear();
                querySatisfied = true;
            }
            m_status = currentStatus;
            m_info = currentInfo;
        }

        if (querySatisfied) {
            m_responseCv.notify_all();
        }

        for (const auto& cb : *sbs) {
            if (cb) {
                cb(currentStatus);
            }
        }
    }
}

} // namespace PelcoD

