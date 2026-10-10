/// @file PelcoDDevice.cpp
/// @brief Implementation of Pelco-D device controller.

#include "PelcoDDevice.h"
#include "PelcoDFrame.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <glog/logging.h>

namespace PelcoD {

PelcoDDevice::PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : m_transport { std::move(transport) }
    , m_address { address }
{
    m_status.address = address;
    m_safetyGuard = std::make_unique<MotionSafetyGuard>([this] {
        LOG(WARNING) << "PelcoDDevice: dead-man watchdog expired, triggering stopMotion()";
        stopMotion();
    });
}

PelcoDDevice::~PelcoDDevice()
{
    try {
        stop();
        if (m_safetyGuard) {
            m_safetyGuard->shutdown();
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "PelcoDDevice: exception during destruction: " << ex.what();
    } catch (...) {
        LOG(ERROR) << "PelcoDDevice: unknown exception during destruction";
    }
}

bool PelcoDDevice::start()
{
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }

    if (!m_transport) {
        return false;
    }

    // C3b: every transport callback of this session enters a fresh gate. stop() closes it and waits for
    // in-flight invocations; copies of these lambdas that a transport still holds after stop() (or a
    // restart) are rejected because their gate stays closed.
    auto gate = std::make_shared<CallbackGate>();
    m_rxGate = gate;
    m_transport->setDataCallback([this, gate](const std::vector<std::uint8_t>& data) {
        const CallbackGate::Pass pass { *gate };
        if (pass) {
            onDataReceived(data);
        }
    });
    m_transport->setStateCallback([this, gate](TransportState state, const std::string& msg) {
        const CallbackGate::Pass pass { *gate };
        if (pass) {
            onTransportState(state, msg);
        }
    });

    if (!m_transport->isOpen()) {
        if (!m_transport->open()) {
            m_transport->setDataCallback(nullptr);
            m_transport->setStateCallback(nullptr);
            gate->close();
            return false;
        }
    }

    {
        std::scoped_lock lock(m_statusMutex);
        m_status.connected = true;
    }

    LOG(INFO) << "Starting PelcoDDevice controller (address: " << static_cast<std::uint32_t>(m_address) << ")";

    m_rxAccumulator.clear();
    m_running = true;
    m_workerThread = std::thread(&PelcoDDevice::workerLoop, this);

    return true;
}

void PelcoDDevice::stop()
{
    std::unique_lock<std::recursive_mutex> lifecycleLock { m_lifecycleMutex };

    // C3b: drain in-flight transport callbacks *without* holding the lifecycle lock, so that a callback
    // which itself calls stop()/start() cannot deadlock against us. Re-check after re-locking in case a
    // concurrent stop()+start() installed a new session gate meanwhile.
    for (auto gate = m_rxGate; gate && !gate->isClosed(); gate = m_rxGate) {
        lifecycleLock.unlock();
        gate->close();
        lifecycleLock.lock();
    }

    // C5: Fail-safe stop. Transmit a synchronous best-effort Stop command directly across transport
    // before closing the connection or terminating worker threads.
    if (m_transport && m_transport->isOpen()) {
        const auto stopFrame = ProtocolBuilder::buildStop(m_address.load());
        (void)m_transport->sendData(stopFrame);
    }

    if (m_safetyGuard) {
        m_safetyGuard->onDisconnect();
    }
    (void)m_queue.purgeMotionCommands();

    if (!m_running.load() && !m_workerThread.joinable()) {
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
    m_abortQueryWait.store(true);
    m_queue.wakeAll();
    m_responseCv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_rxAccumulator.clear();
    {
        std::scoped_lock lock(m_statusMutex);
        m_awaitingResponse = false;
        m_pendingQueryTag.clear();
        m_pendingLimitId = std::nullopt;
    }

    if (m_transport) {
        if (m_transport->isOpen()) {
            m_transport->close();
        }
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
    }

    // The gate is closed, so the transport's own Disconnected notification (if any) was rejected;
    // publish the disconnect here instead, exactly once.
    markDisconnected();
}

void PelcoDDevice::onTransportState(TransportState state, const std::string& msg)
{
    if (state == TransportState::Disconnected || state == TransportState::Error) {
        LOG(WARNING) << "Transport disconnected or error: " << msg;
        if (m_safetyGuard) {
            m_safetyGuard->onDisconnect();
        }
        (void)m_queue.purgeMotionCommands();
        if (m_transport && m_transport->isOpen()) {
            const auto stopFrame = ProtocolBuilder::buildStop(m_address.load());
            (void)m_transport->sendData(stopFrame);
        }
        markDisconnected();
    }
}

void PelcoDDevice::markDisconnected()
{
    bool wasConn { false };
    DeviceStatus copy {};
    {
        std::scoped_lock lock(m_statusMutex);
        wasConn = m_status.connected;
        m_status.connected = false;
        copy = m_status;
    }
    if (!wasConn) {
        return;
    }
    std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> sbs {};
    {
        std::scoped_lock lock(m_callbackState->mutex);
        sbs = m_callbackState->statusCallbacks;
    }
    for (const auto& entry : *sbs) {
        if (entry.cb && entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                entry.cb(copy);
            }
        }
    }
}

bool PelcoDDevice::setFrameExt(std::shared_ptr<IFrameExtension> ext)
{
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
    if (m_running.load() || (m_rxGate && !m_rxGate->isClosed())) {
        return false;
    }
    m_frameExt = std::move(ext);
    return true;
}

bool PelcoDDevice::isConnected() const noexcept
{
    return m_running && m_transport && m_transport->isOpen();
}

void PelcoDDevice::setAddress(std::uint8_t address)
{
    m_address = address;
    std::scoped_lock lock(m_statusMutex);
    m_status.address = address;
}

std::uint8_t PelcoDDevice::getAddress() const noexcept
{
    return m_address;
}

bool PelcoDDevice::CallbackState::removeStatus(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(statusCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

bool PelcoDDevice::CallbackState::removeTraffic(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(trafficCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

bool PelcoDDevice::CallbackState::removeTimeout(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(timeoutCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

bool PelcoDDevice::CallbackState::removeQueryCompleted(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(queryCompletedCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

bool PelcoDDevice::CallbackState::removeRetry(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(retryCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

bool PelcoDDevice::CallbackState::removeQueryLatency(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate {};
    const bool removed = removeCallbackEntry(queryLatencyCallbacks, id, mutex, gate);
    if (gate) {
        gate->close();
    }
    return removed;
}

void PelcoDDevice::CallbackState::clear()
{
    std::vector<std::shared_ptr<CallbackGate>> gatesToClose;
    {
        std::scoped_lock lock(mutex);
        auto collectGates = [&gatesToClose](const auto& list) {
            for (const auto& entry : *list) {
                if (entry.gate) {
                    gatesToClose.push_back(entry.gate);
                }
            }
        };
        collectGates(statusCallbacks);
        collectGates(trafficCallbacks);
        collectGates(timeoutCallbacks);
        collectGates(queryCompletedCallbacks);
        collectGates(retryCallbacks);
        collectGates(queryLatencyCallbacks);

        statusCallbacks = std::make_shared<const std::vector<CallbackEntry<StatusCallback>>>();
        trafficCallbacks = std::make_shared<const std::vector<CallbackEntry<TrafficCallback>>>();
        timeoutCallbacks = std::make_shared<const std::vector<CallbackEntry<TimeoutCallback>>>();
        queryCompletedCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryCompletedCallback>>>();
        retryCallbacks = std::make_shared<const std::vector<CallbackEntry<RetryCallback>>>();
        queryLatencyCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryLatencyCallback>>>();
    }
    for (const auto& gate : gatesToClose) {
        if (gate) {
            gate->close();
        }
    }
}

template <typename CallbackT, typename RemoveMemFn>
Connection PelcoDDevice::registerCallbackHelper(CallbackT cb,
    std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackState::*listMember, RemoveMemFn removeFn)
{
    if (!cb) {
        return Connection {};
    }
    const CallbackId id = m_callbackState->nextId.fetch_add(1U, std::memory_order_relaxed);
    auto gate = std::make_shared<CallbackGate>();
    {
        std::scoped_lock lock(m_callbackState->mutex);
        auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>(*(m_callbackState.get()->*listMember));
        nextList->push_back({ id, std::move(cb), gate });
        m_callbackState.get()->*listMember = std::move(nextList);
    }
    std::weak_ptr<CallbackState> weakState = m_callbackState;
    return Connection([weakState, id, removeFn, gate]() {
        if (auto state = weakState.lock()) {
            (state.get()->*removeFn)(id);
        } else {
            gate->close();
        }
    });
}

Connection PelcoDDevice::addStatusCallback(StatusCallback cb)
{
    return registerCallbackHelper(std::move(cb), &CallbackState::statusCallbacks, &CallbackState::removeStatus);
}

Connection PelcoDDevice::addTrafficCallback(TrafficCallback cb)
{
    return registerCallbackHelper(std::move(cb), &CallbackState::trafficCallbacks, &CallbackState::removeTraffic);
}

Connection PelcoDDevice::addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    if (!cb) {
        return Connection {};
    }
    return addTrafficCallback(
        [cb = std::move(cb), notifyTx, notifyRx](bool isTx, const std::vector<std::uint8_t>& frame) {
            if (isTx && !notifyTx) {
                return;
            }
            if (!isTx && !notifyRx) {
                return;
            }
            cb(isTx, frame);
        });
}

Connection PelcoDDevice::addTrafficCallback(
    std::uint8_t addressFilter, TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    if (!cb) {
        return Connection {};
    }
    return addTrafficCallback(
        [cb = std::move(cb), addressFilter, notifyTx, notifyRx](bool isTx, const std::vector<std::uint8_t>& frame) {
            if (isTx && !notifyTx) {
                return;
            }
            if (!isTx && !notifyRx) {
                return;
            }
            if (frame.size() >= 2U && frame[1] != addressFilter) {
                return;
            }
            cb(isTx, frame);
        });
}

Connection PelcoDDevice::addTimeoutCallback(TimeoutCallback cb)
{
    return registerCallbackHelper(std::move(cb), &CallbackState::timeoutCallbacks, &CallbackState::removeTimeout);
}

Connection PelcoDDevice::addQueryCompletedCallback(QueryCompletedCallback cb)
{
    return registerCallbackHelper(
        std::move(cb), &CallbackState::queryCompletedCallbacks, &CallbackState::removeQueryCompleted);
}

Connection PelcoDDevice::addRetryCallback(RetryCallback cb)
{
    return registerCallbackHelper(std::move(cb), &CallbackState::retryCallbacks, &CallbackState::removeRetry);
}

Connection PelcoDDevice::addQueryLatencyCallback(QueryLatencyCallback cb)
{
    return registerCallbackHelper(
        std::move(cb), &CallbackState::queryLatencyCallbacks, &CallbackState::removeQueryLatency);
}

bool PelcoDDevice::removeStatusCallback(CallbackId id)
{
    return m_callbackState->removeStatus(id);
}

bool PelcoDDevice::removeTrafficCallback(CallbackId id)
{
    return m_callbackState->removeTraffic(id);
}

bool PelcoDDevice::removeTimeoutCallback(CallbackId id)
{
    return m_callbackState->removeTimeout(id);
}

bool PelcoDDevice::removeQueryCompletedCallback(CallbackId id)
{
    return m_callbackState->removeQueryCompleted(id);
}

bool PelcoDDevice::removeRetryCallback(CallbackId id)
{
    return m_callbackState->removeRetry(id);
}

bool PelcoDDevice::removeQueryLatencyCallback(CallbackId id)
{
    return m_callbackState->removeQueryLatency(id);
}

void PelcoDDevice::clearCallbacks()
{
    m_callbackState->clear();
}

DeviceStatus PelcoDDevice::getStatus() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

DeviceInfo PelcoDDevice::getInfo() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_info;
}

Transport::TransportStatsSnapshot PelcoDDevice::getTransportStats() const
{
    if (m_transport) {
        return m_transport->getStats();
    }
    return {};
}

PelcoDProtocolStats PelcoDDevice::getProtocolStats() const
{
    PelcoDProtocolStats stats {};
    stats.queriesSent = m_queriesSent.load(std::memory_order_relaxed);
    stats.queriesCompleted = m_queriesCompleted.load(std::memory_order_relaxed);
    stats.queryTimeouts = m_queryTimeouts.load(std::memory_order_relaxed);
    stats.queryRetries = m_queryRetries.load(std::memory_order_relaxed);
    stats.checksumErrors = m_rxAccumulator.checksumErrors();
    stats.discardedSyncBytes = m_rxAccumulator.discardedBytes();
    stats.pendingCommands = m_queue.size();

    const std::uint64_t completed = stats.queriesCompleted;
    const std::uint64_t totalUs = m_totalRttUs.load(std::memory_order_relaxed);
    if (completed > 0U) {
        stats.avgRttMs = static_cast<double>(totalUs) / (1000.0 * static_cast<double>(completed));
    }
    stats.lastRttMs = static_cast<double>(m_lastRttUs.load(std::memory_order_relaxed)) / 1000.0;
    stats.minRttMs = static_cast<double>(m_minRttUs.load(std::memory_order_relaxed)) / 1000.0;
    stats.maxRttMs = static_cast<double>(m_maxRttUs.load(std::memory_order_relaxed)) / 1000.0;
    return stats;
}

void PelcoDDevice::resetProtocolStats() noexcept
{
    m_queriesSent.store(0U, std::memory_order_relaxed);
    m_queriesCompleted.store(0U, std::memory_order_relaxed);
    m_queryTimeouts.store(0U, std::memory_order_relaxed);
    m_queryRetries.store(0U, std::memory_order_relaxed);
    m_totalRttUs.store(0U, std::memory_order_relaxed);
    m_lastRttUs.store(0U, std::memory_order_relaxed);
    m_minRttUs.store(0U, std::memory_order_relaxed);
    m_maxRttUs.store(0U, std::memory_order_relaxed);
    m_rxAccumulator.resetStats();
}

void PelcoDDevice::setTelemetryPolling(bool enable, std::uint32_t intervalMs) noexcept
{
    m_telemetryPolling.store(enable);
    m_pollIntervalMs.store((intervalMs > 0U) ? intervalMs : 1000U);
    m_queue.wakeAll();
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

void PelcoDDevice::setRetryConfig(const RetryConfig& config) noexcept
{
    std::scoped_lock lock(m_retryMutex);
    m_retryConfig = config;
}

RetryConfig PelcoDDevice::getRetryConfig() const noexcept
{
    std::scoped_lock lock(m_retryMutex);
    return m_retryConfig;
}

void PelcoDDevice::setDeadManTimeout(std::chrono::milliseconds timeout) noexcept
{
    if (m_safetyGuard) {
        m_safetyGuard->setDeadManTimeout(timeout);
    }
}

std::chrono::milliseconds PelcoDDevice::getDeadManTimeout() const noexcept
{
    if (m_safetyGuard) {
        return m_safetyGuard->getDeadManTimeout();
    }
    return std::chrono::milliseconds { 0 };
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
    enqueueCommand(ProtocolBuilder::buildStop(m_address), "", CommandPriority::Urgent);
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
    enqueueCommand(ProtocolBuilder::buildZoom(m_address, ZoomAction::Stop), "", CommandPriority::Urgent);
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
    enqueueCommand(ProtocolBuilder::buildFocus(m_address, FocusAction::Stop), "", CommandPriority::Urgent);
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
    enqueueCommand(ProtocolBuilder::buildIris(m_address, IrisAction::Stop), "", CommandPriority::Urgent);
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

void PelcoDDevice::presetScan(std::uint8_t dwellSeconds)
{
    enqueueCommand(ProtocolBuilder::buildPresetScan(m_address, dwellSeconds));
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
    const auto frame = ProtocolBuilder::buildSetMagnification(m_address, value, relative);
    if (frame.has_value()) {
        enqueueCommand(*frame);
    }
}

void PelcoDDevice::setBaudRate(std::uint32_t baud)
{
    const auto frame = ProtocolBuilder::buildSetBaudRate(m_address, baud);
    if (frame.has_value()) {
        enqueueCommand(*frame);
    }
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

void PelcoDDevice::activateEchoMode()
{
    enqueueCommand(ProtocolBuilder::buildEchoMode(m_address));
}

void PelcoDDevice::prepareForDownload()
{
    enqueueCommand(ProtocolBuilder::buildPrepareForDownload(m_address));
}

void PelcoDDevice::startDownload()
{
    enqueueCommand(ProtocolBuilder::buildStartDownload(m_address));
}

void PelcoDDevice::writeCharacter(std::uint8_t column, char asciiChar)
{
    enqueueCommand(ProtocolBuilder::buildWriteChar(m_address, column, asciiChar));
}

void PelcoDDevice::clearScreen()
{
    enqueueCommand(ProtocolBuilder::buildClearScreen(m_address));
}

void PelcoDDevice::sendDummy()
{
    enqueueCommand(ProtocolBuilder::buildDummy(m_address));
}

void PelcoDDevice::screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative)
{
    enqueueCommand(ProtocolBuilder::buildScreenMove(m_address, panPercent, tiltPercent, relative));
}

void PelcoDDevice::querySoftwareVersion()
{
    sendQueryFrame(ProtocolBuilder::buildQuerySoftwareVersion(m_address), "QuerySoftwareVersion");
}

void PelcoDDevice::queryBuildNumber()
{
    sendQueryFrame(ProtocolBuilder::buildQueryBuildNumber(m_address), "QueryBuildNumber");
}

void PelcoDDevice::setSeconds(std::uint8_t seconds)
{
    enqueueCommand(ProtocolBuilder::buildSetSeconds(m_address, seconds));
}

void PelcoDDevice::setHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    enqueueCommand(ProtocolBuilder::buildSetHourMinute(m_address, hour, minute));
}

void PelcoDDevice::setMonthDay(std::uint8_t month, std::uint8_t day)
{
    enqueueCommand(ProtocolBuilder::buildSetMonthDay(m_address, month, day));
}

void PelcoDDevice::setYear(std::uint16_t year)
{
    enqueueCommand(ProtocolBuilder::buildSetYear(m_address, year));
}

void PelcoDDevice::setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second)
{
    setHourMinute(hour, minute);
    setSeconds(second);
}

void PelcoDDevice::setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day)
{
    setMonthDay(month, day);
    setYear(year);
}

void PelcoDDevice::queryTime(TimeSubOpcode queryType)
{
    sendQueryFrame(ProtocolBuilder::buildQueryTime(m_address, queryType), "QueryTime");
}

void PelcoDDevice::setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths)
{
    enqueueCommand(ProtocolBuilder::buildSetAuxLed(m_address, ledIdOrColor, onTimeTenths));
}

void PelcoDDevice::clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths)
{
    enqueueCommand(ProtocolBuilder::buildClearAuxLed(m_address, ledIdOrColor, offTimeTenths));
}

void PelcoDDevice::queryAzimuthZero()
{
    sendQueryFrame(ProtocolBuilder::buildQueryAzimuthZero(m_address), "QueryAzimuthZero");
}

void PelcoDDevice::setZoomLimit(std::uint16_t limitHundredths)
{
    enqueueCommand(ProtocolBuilder::buildSetZoomLimit(m_address, limitHundredths));
}

void PelcoDDevice::queryZoomLimit()
{
    sendQueryFrame(ProtocolBuilder::buildQueryZoomLimit(m_address), "QueryZoomLimit");
}

void PelcoDDevice::queryEverestAlarms()
{
    sendQueryFrame(ProtocolBuilder::buildQueryEverestAlarms(m_address), "QueryEverestAlarms");
}

void PelcoDDevice::deletePattern(std::uint8_t patternId)
{
    enqueueCommand(ProtocolBuilder::buildDeletePattern(m_address, patternId));
}

void PelcoDDevice::setManualLeftPanLimit(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetManualLeftPanLimit(m_address, centidegrees));
}

void PelcoDDevice::setManualRightPanLimit(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetManualRightPanLimit(m_address, centidegrees));
}

void PelcoDDevice::setScanLeftPanLimit(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetScanLeftPanLimit(m_address, centidegrees));
}

void PelcoDDevice::setScanRightPanLimit(std::uint16_t centidegrees)
{
    enqueueCommand(ProtocolBuilder::buildSetScanRightPanLimit(m_address, centidegrees));
}

void PelcoDDevice::queryLimit(EverestLimitId limitId)
{
    {
        std::scoped_lock lock(m_statusMutex);
        m_pendingLimitId = limitId;
    }
    sendQueryFrame(ProtocolBuilder::buildQueryLimit(m_address, limitId), "QueryLimit");
}

void PelcoDDevice::enableLimits(bool enable)
{
    enqueueCommand(ProtocolBuilder::buildEnableLimits(m_address, enable));
}

void PelcoDDevice::queryDefinedPresets(std::uint8_t group)
{
    sendQueryFrame(ProtocolBuilder::buildQueryDefinedPresets(m_address, group), "QueryDefinedPresets");
}

void PelcoDDevice::queryDefinedPatterns(std::uint8_t group)
{
    sendQueryFrame(ProtocolBuilder::buildQueryDefinedPatterns(m_address, group), "QueryDefinedPatterns");
}

namespace {

    template <typename ResultT, typename Extractor>
    std::future<ResultT> executeAsyncQuery(PelcoDDevice* device, const std::string& queryTag,
        std::function<void()> triggerQuery, Extractor extractResult, std::chrono::milliseconds timeout)
    {
        auto promise = std::make_shared<std::promise<ResultT>>();
        auto future = promise->get_future();

        if (!device->isConnected()) {
            promise->set_exception(std::make_exception_ptr(std::runtime_error("Device is not connected")));
            return future;
        }

        auto fulfilled = std::make_shared<std::atomic<bool>>(false);
        auto conn = std::make_shared<ScopedConnection>();

        *conn
            = device->addQueryCompletedCallback([promise, fulfilled, conn, queryTag, extractResult](
                                                    const std::string& tag, bool success, const DeviceStatus& status) {
                  if (tag != queryTag) {
                      return;
                  }
                  if (fulfilled->exchange(true)) {
                      return;
                  }
                  conn->disconnect();
                  if (success) {
                      promise->set_value(extractResult(status));
                  } else {
                      promise->set_exception(
                          std::make_exception_ptr(std::runtime_error("Query '" + queryTag + "' timed out")));
                  }
              });

        if (timeout > std::chrono::milliseconds(0)) {
            std::thread([promise, fulfilled, conn, queryTag, timeout]() {
                std::this_thread::sleep_for(timeout);
                if (!fulfilled->exchange(true)) {
                    conn->disconnect();
                    try {
                        promise->set_exception(
                            std::make_exception_ptr(std::runtime_error("Query '" + queryTag + "' timed out")));
                    } catch (const std::future_error& ex) {
                        LOG(WARNING) << "PelcoDDevice: future error setting query timeout promise: " << ex.what();
                    } catch (const std::exception& ex) {
                        LOG(WARNING) << "PelcoDDevice: exception setting query timeout promise: " << ex.what();
                    } catch (...) {
                        LOG(WARNING) << "PelcoDDevice: unknown exception setting query timeout promise";
                    }
                }
            }).detach();
        }

        triggerQuery();
        return future;
    }

} // namespace

std::future<std::uint16_t> PelcoDDevice::queryPanAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryPan", [this] { queryPan(); }, [](const DeviceStatus& s) { return s.panCentidegrees; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryTiltAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryTilt", [this] { queryTilt(); }, [](const DeviceStatus& s) { return s.tiltCentidegrees; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryZoomAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryZoom", [this] { queryZoom(); }, [](const DeviceStatus& s) { return s.zoomPosition; }, timeout);
}

std::future<DeviceStatus> PelcoDDevice::queryStatusAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<DeviceStatus>(
        this, "QueryPan", [this] { queryPan(); }, [](const DeviceStatus& s) { return s; }, timeout);
}

std::future<std::pair<std::uint8_t, std::uint8_t>> PelcoDDevice::querySoftwareVersionAsync(
    std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::pair<std::uint8_t, std::uint8_t>>(
        this, "QuerySoftwareVersion", [this] { querySoftwareVersion(); },
        [this](const DeviceStatus&) {
            const auto info = getInfo();
            return std::make_pair(info.softwareMajor, info.softwareMinor);
        },
        timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryBuildNumberAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryBuildNumber", [this] { queryBuildNumber(); },
        [this](const DeviceStatus&) { return getInfo().buildNumber; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryAzimuthZeroAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryAzimuthZero", [this] { queryAzimuthZero(); },
        [](const DeviceStatus& s) { return s.azimuthZeroOffsetCentidegrees; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryZoomLimitAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryZoomLimit", [this] { queryZoomLimit(); }, [](const DeviceStatus& s) { return s.zoomLimit; },
        timeout);
}

void PelcoDDevice::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    enqueueCommand(frame);
}

void PelcoDDevice::sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag)
{
    enqueueCommand(frame, std::move(queryTag), CommandPriority::Low);
}

void PelcoDDevice::enqueueCommand(
    const std::vector<std::uint8_t>& frame, std::string queryTag, CommandPriority priority)
{
    if (frame.empty()) {
        return;
    }
    std::uint64_t motionGen { 0U };
    if (PelcoDFrame::isStandardMotion(frame)) {
        motionGen = ++m_motionGeneration;
        if (m_safetyGuard) {
            m_safetyGuard->onMotionCommand(frame);
        }
    } else if (PelcoDFrame::isStandardStop(frame)) {
        motionGen = ++m_motionGeneration;
        if (m_safetyGuard) {
            m_safetyGuard->onStopCommand();
        }
    }
    m_queue.enqueue(frame, std::move(queryTag), priority, motionGen);

    // If an urgent command arrives while waiting for a query response, abort the query wait immediately
    if (priority == CommandPriority::Urgent && m_awaitingResponse.load()) {
        m_abortQueryWait.store(true);
        m_responseCv.notify_all();
    }
}

void PelcoDDevice::checkQueryTimeout()
{
    if (!m_awaitingResponse.load()) {
        return;
    }

    std::string tag;
    std::chrono::microseconds durationUs { 0 };
    bool timedOut { false };
    const auto now = std::chrono::steady_clock::now();
    const auto timeoutMs = m_queryTimeoutMs.load();
    DeviceStatus statusCopy {};

    {
        std::scoped_lock lock(m_statusMutex);
        if (m_awaitingResponse.load()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_querySentTime).count();
            if (elapsed >= static_cast<std::int64_t>(timeoutMs)) {
                timedOut = true;
                tag = m_pendingQueryTag;
                durationUs = std::chrono::duration_cast<std::chrono::microseconds>(now - m_querySentTime);
                m_awaitingResponse = false;
                m_pendingQueryTag.clear();
                statusCopy = m_status;
            }
        }
    }

    if (timedOut) {
        m_queryTimeouts.fetch_add(1U, std::memory_order_relaxed);
        m_responseCv.notify_all();

        LOG(WARNING) << "Query timeout: No response received for query '" << tag << "' within " << timeoutMs << " ms";

        std::shared_ptr<const std::vector<CallbackEntry<TimeoutCallback>>> cbs;
        std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> qcbs;
        std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> lcbs;
        {
            std::scoped_lock lock(m_callbackState->mutex);
            cbs = m_callbackState->timeoutCallbacks;
            qcbs = m_callbackState->queryCompletedCallbacks;
            lcbs = m_callbackState->queryLatencyCallbacks;
        }
        for (const auto& entry : *cbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(tag);
                }
            }
        }
        for (const auto& entry : *qcbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(tag, false, statusCopy);
                }
            }
        }
        for (const auto& entry : *lcbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(tag, durationUs, false);
                }
            }
        }
    }
}

void PelcoDDevice::workerLoop()
{
    auto nextPollTime = std::chrono::steady_clock::now();
    bool lastPollingEnabled { false };

    while (m_running) {
        checkQueryTimeout();
        const auto expiredFrames = m_rxAccumulator.flushExpired(RxFrameExpectation::AllowGeneralResponse);
        for (const auto& frame : expiredFrames) {
            dispatchFrame(frame);
        }

        const bool pollingEnabled = m_telemetryPolling.load();
        if (pollingEnabled && !lastPollingEnabled) {
            nextPollTime = std::chrono::steady_clock::now();
        }
        lastPollingEnabled = pollingEnabled;

        const auto now = std::chrono::steady_clock::now();
        if (pollingEnabled && isConnected() && now >= nextPollTime) {
            if (!m_queue.hasLowPriorityPending()) {
                queryPan();
                queryTilt();
                queryZoom();
            }
            nextPollTime = now + std::chrono::milliseconds(m_pollIntervalMs.load());
        }

        CommandItem item;
        const auto pollDeadline
            = (pollingEnabled && isConnected()) ? nextPollTime : std::chrono::steady_clock::time_point::max();
        const auto idleWaitTimeout = (m_rxAccumulator.size() > 0U)
            ? m_rxAccumulator.interByteTimeout()
            : std::chrono::milliseconds(50);

        const bool hasItem = m_queue.popReady(
            item, [this] { return !m_running.load(); }, pollDeadline, idleWaitTimeout);
        if (!m_running) {
            break;
        }
        if (!hasItem || item.frame.empty()) {
            continue;
        }

        const auto sendStartTime = std::chrono::steady_clock::now();
        if (m_transport && m_transport->isOpen() && !item.frame.empty()) {
            if (!item.queryTag.empty()) {
                std::scoped_lock lock(m_statusMutex);
                m_abortQueryWait.store(false);
                m_pendingQueryTag = item.queryTag;
                m_querySentTime = sendStartTime;
                m_awaitingResponse = true;
                m_queriesSent.fetch_add(1U, std::memory_order_relaxed);
            }

            VLOG(1) << "Sending command (" << item.frame.size() << " bytes, tag: '" << item.queryTag << "')";
            const bool sendSuccess = m_transport->sendData(item.frame);
            if (!sendSuccess) {
                LOG(WARNING) << "Failed to transmit frame across transport.";
                if (!item.queryTag.empty()) {
                    std::scoped_lock lock(m_statusMutex);
                    m_awaitingResponse = false;
                    m_pendingQueryTag.clear();
                    m_pendingLimitId = std::nullopt;
                }

                RetryConfig retryCfg;
                {
                    std::scoped_lock rLock(m_retryMutex);
                    retryCfg = m_retryConfig;
                }
                if (retryCfg.retryOnTransportError && item.retryCount < retryCfg.maxRetries) {
                    if (item.motionGeneration > 0U && item.motionGeneration < m_queue.currentMotionGeneration()) {
                        LOG(INFO) << "Skipping retry for obsolete motion command (gen " << item.motionGeneration
                                  << " < " << m_queue.currentMotionGeneration() << ")";
                    } else {
                        m_queryRetries.fetch_add(1U, std::memory_order_relaxed);
                        const std::string reason = "Transport transmission failed for "
                            + (item.queryTag.empty() ? "command" : "query '" + item.queryTag + "'");
                        const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                        if (backoffDelay.count() > 0) {
                            std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> rcbs;
                            {
                                std::scoped_lock lock(m_callbackState->mutex);
                                rcbs = m_callbackState->retryCallbacks;
                            }
                            for (const auto& entry : *rcbs) {
                                if (entry.cb && entry.gate) {
                                    const CallbackGate::Pass pass { *entry.gate };
                                    if (pass) {
                                        entry.cb(item.queryTag.empty() ? "Command" : item.queryTag, item.retryCount + 1U,
                                            retryCfg.maxRetries, backoffDelay);
                                    }
                                }
                            }
                        }
                    }
                } else if (!item.queryTag.empty() && retryCfg.maxRetries > 0U) {
                    DeviceStatus statusCopy;
                    {
                        std::scoped_lock lock(m_statusMutex);
                        statusCopy = m_status;
                    }
                    std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> qcbs;
                    {
                        std::scoped_lock lock(m_callbackState->mutex);
                        qcbs = m_callbackState->queryCompletedCallbacks;
                    }
                    for (const auto& entry : *qcbs) {
                        if (entry.cb && entry.gate) {
                            const CallbackGate::Pass pass { *entry.gate };
                            if (pass) {
                                entry.cb(item.queryTag, false, statusCopy);
                            }
                        }
                    }
                }
            } else {
                // Dispatch TX traffic callbacks using copy-on-write snapshot (zero heap allocation)
                std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> tbs;
                {
                    std::scoped_lock lock(m_callbackState->mutex);
                    tbs = m_callbackState->trafficCallbacks;
                }
                for (const auto& entry : *tbs) {
                    if (entry.cb && entry.gate) {
                        const CallbackGate::Pass pass { *entry.gate };
                        if (pass) {
                            entry.cb(true, item.frame);
                        }
                    }
                }

                if (!item.queryTag.empty()) {
                    bool queryAborted { false };
                    {
                        std::unique_lock<std::mutex> lock(m_statusMutex);
                        m_responseCv.wait_for(lock, std::chrono::milliseconds(m_queryTimeoutMs.load()),
                            [this] { return !m_awaitingResponse.load() || !m_running || m_abortQueryWait.load(); });

                        if (m_abortQueryWait.load()) {
                            queryAborted = true;
                            m_abortQueryWait.store(false);
                            m_awaitingResponse = false;
                            m_pendingQueryTag.clear();
                            m_pendingLimitId = std::nullopt;
                        }
                    }
                    if (!queryAborted) {
                        if (m_awaitingResponse.load()) {
                            RetryConfig retryCfg;
                            {
                                std::scoped_lock rLock(m_retryMutex);
                                retryCfg = m_retryConfig;
                            }
                            if (item.retryCount < retryCfg.maxRetries) {
                                {
                                    std::scoped_lock lock(m_statusMutex);
                                    m_awaitingResponse = false;
                                    m_pendingQueryTag.clear();
                                    m_pendingLimitId = std::nullopt;
                                }
                                const std::string reason = "Query '" + item.queryTag + "' timed out";
                                m_queryRetries.fetch_add(1U, std::memory_order_relaxed);
                                const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                                std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> rcbs;
                                {
                                    std::scoped_lock lock(m_callbackState->mutex);
                                    rcbs = m_callbackState->retryCallbacks;
                                }
                                for (const auto& entry : *rcbs) {
                                    if (entry.cb && entry.gate) {
                                        const CallbackGate::Pass pass { *entry.gate };
                                        if (pass) {
                                            entry.cb(
                                                item.queryTag, item.retryCount + 1U, retryCfg.maxRetries, backoffDelay);
                                        }
                                    }
                                }
                            } else {
                                checkQueryTimeout();
                            }
                        }
                    }
                }
            }
        }

        // Inter-command delay per Pelco-D RS-485 specification (20ms quiet time between frames)
        const auto commandEndTime = std::chrono::steady_clock::now();
        const auto elapsedMs
            = std::chrono::duration_cast<std::chrono::milliseconds>(commandEndTime - sendStartTime).count();
        if (elapsedMs < 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20 - elapsedMs));
        }
    }
}

void PelcoDDevice::onDataReceived(const std::vector<std::uint8_t>& data)
{
    const RxFrameExpectation expectation = m_awaitingResponse.load()
        ? RxFrameExpectation::AwaitingQuery
        : RxFrameExpectation::StandardOnly;
    const auto frames = m_rxAccumulator.push(data, expectation);
    for (const auto& frame : frames) {
        dispatchFrame(frame);
    }
    if (m_rxAccumulator.size() > 0U) {
        m_queue.wakeAll();
    }
}

bool PelcoDDevice::isResponseMatchingQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept
{
    if (m_frameExt) {
        const ExtMatch verdict { m_frameExt->matchQuery(queryTag, frame) };
        if (verdict != ExtMatch::NotHandled) {
            return verdict == ExtMatch::Matched;
        }
    }
    return ProtocolParser::isResponseMatchingQuery(queryTag, frame);
}

void PelcoDDevice::resolveQueryWait()
{
    {
        std::scoped_lock lock(m_statusMutex);
        m_awaitingResponse = false;
        m_pendingQueryTag.clear();
        m_pendingLimitId = std::nullopt;
    }
    m_responseCv.notify_all();
}

void PelcoDDevice::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    // Vendor extension first (preserves the ordering of the former derived-class override). The
    // extension is shared-owned, so it is alive here even while a derived device is being destroyed.
    if (m_frameExt && m_frameExt->onFrame(frame)) {
        resolveQueryWait();
        m_frameExt->publish(getStatus());
    }

    // Notify RX traffic callbacks using copy-on-write snapshot (zero heap allocation)
    std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> tbs;
    std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> sbs;
    {
        std::scoped_lock lock(m_callbackState->mutex);
        tbs = m_callbackState->trafficCallbacks;
        sbs = m_callbackState->statusCallbacks;
    }

    for (const auto& entry : *tbs) {
        if (entry.cb && entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                entry.cb(false, frame);
            }
        }
    }

    // Fast-path frame validation
    if (frame.size() < 2U) {
        return;
    }

    bool statusUpdated { false };
    bool querySatisfied { false };
    std::string satisfiedTag {};
    std::chrono::microseconds durationUs { 0 };
    DeviceStatus statusSnapshot {};

    {
        std::scoped_lock lock(m_statusMutex);

        // Discard frames addressed to another device or stale address following setAddress()
        if (frame[1] != m_address.load()) {
            return;
        }

        // If awaiting query response, drop frames not matching pending query
        if (m_awaitingResponse.load() && !isResponseMatchingQuery(m_pendingQueryTag, frame)) {
            return;
        }

        // Apply updates directly in-place to m_status and m_info under lock (H2 fix: no TOCTOU)
        if (ProtocolParser::updateStatus(frame, m_status, m_info, m_pendingLimitId)) {
            statusUpdated = true;

            if (m_awaitingResponse.load() && isResponseMatchingQuery(m_pendingQueryTag, frame)) {
                satisfiedTag = m_pendingQueryTag;
                durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - m_querySentTime);
                m_awaitingResponse = false;
                m_pendingQueryTag.clear();
                m_pendingLimitId = std::nullopt;
                querySatisfied = true;
            }

            statusSnapshot = m_status;
        }
    }

    if (querySatisfied) {
        const auto us = static_cast<std::uint64_t>(durationUs.count());
        m_queriesCompleted.fetch_add(1U, std::memory_order_relaxed);
        m_totalRttUs.fetch_add(us, std::memory_order_relaxed);
        m_lastRttUs.store(us, std::memory_order_relaxed);

        std::uint64_t currentMin = m_minRttUs.load(std::memory_order_relaxed);
        while ((currentMin == 0U || us < currentMin)
            && !m_minRttUs.compare_exchange_weak(currentMin, us, std::memory_order_relaxed)) { }
        std::uint64_t currentMax = m_maxRttUs.load(std::memory_order_relaxed);
        while (us > currentMax && !m_maxRttUs.compare_exchange_weak(currentMax, us, std::memory_order_relaxed)) { }

        m_responseCv.notify_all();
        std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> qcbs;
        std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> lcbs;
        {
            std::scoped_lock lock(m_callbackState->mutex);
            qcbs = m_callbackState->queryCompletedCallbacks;
            lcbs = m_callbackState->queryLatencyCallbacks;
        }
        for (const auto& entry : *qcbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(satisfiedTag, true, statusSnapshot);
                }
            }
        }
        for (const auto& entry : *lcbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(satisfiedTag, durationUs, true);
                }
            }
        }
    }

    if (statusUpdated) {
        for (const auto& entry : *sbs) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    entry.cb(statusSnapshot);
                }
            }
        }
    }
}

} // namespace PelcoD
