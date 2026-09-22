/// @file PelcoDDevice.cpp
/// @brief Implementation of Pelco-D device controller.

#include "PelcoDDevice.h"
#include "PelcoDFrame.h"

#include <algorithm>
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
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
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
                std::scoped_lock lock(m_statusMutex);
                wasConn = m_status.connected;
                m_status.connected = false;
            }
            if (wasConn) {
                std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> sbs;
                {
                    std::scoped_lock lock(m_callbackState->mutex);
                    sbs = m_callbackState->statusCallbacks;
                }
                DeviceStatus copy;
                {
                    std::scoped_lock lock(m_statusMutex);
                    copy = m_status;
                }
                for (const auto& entry : *sbs) {
                    if (entry.cb) {
                        entry.cb(copy);
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
        std::scoped_lock lock(m_statusMutex);
        m_status.connected = true;
    }

    LOG(INFO) << "Starting PelcoDDevice controller (address: " << static_cast<int>(m_address) << ")";

    m_rxAccumulator.clear();
    m_running = true;
    m_workerThread = std::thread(&PelcoDDevice::workerLoop, this);

    return true;
}

void PelcoDDevice::stop()
{
    std::scoped_lock lifecycleLock(m_lifecycleMutex);
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
    m_awaitingResponse = false;

    if (m_transport) {
        if (m_transport->isOpen()) {
            m_transport->close();
        }
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
    }

    {
        std::scoped_lock lock(m_statusMutex);
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
    std::scoped_lock lock(m_statusMutex);
    m_status.address = address;
}

std::uint8_t PelcoDDevice::getAddress() const noexcept
{
    return m_address;
}

bool PelcoDDevice::CallbackState::removeStatus(CallbackId id)
{
    return removeCallbackEntry(statusCallbacks, id, mutex);
}

bool PelcoDDevice::CallbackState::removeTraffic(CallbackId id)
{
    return removeCallbackEntry(trafficCallbacks, id, mutex);
}

bool PelcoDDevice::CallbackState::removeTimeout(CallbackId id)
{
    return removeCallbackEntry(timeoutCallbacks, id, mutex);
}

bool PelcoDDevice::CallbackState::removeQueryCompleted(CallbackId id)
{
    return removeCallbackEntry(queryCompletedCallbacks, id, mutex);
}

bool PelcoDDevice::CallbackState::removeRetry(CallbackId id)
{
    return removeCallbackEntry(retryCallbacks, id, mutex);
}

bool PelcoDDevice::CallbackState::removeQueryLatency(CallbackId id)
{
    return removeCallbackEntry(queryLatencyCallbacks, id, mutex);
}

void PelcoDDevice::CallbackState::clear()
{
    std::scoped_lock lock(mutex);
    statusCallbacks = std::make_shared<const std::vector<CallbackEntry<StatusCallback>>>();
    trafficCallbacks = std::make_shared<const std::vector<CallbackEntry<TrafficCallback>>>();
    timeoutCallbacks = std::make_shared<const std::vector<CallbackEntry<TimeoutCallback>>>();
    queryCompletedCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryCompletedCallback>>>();
    retryCallbacks = std::make_shared<const std::vector<CallbackEntry<RetryCallback>>>();
    queryLatencyCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryLatencyCallback>>>();
}

template <typename CallbackT, typename RemoveMemFn>
Connection PelcoDDevice::registerCallbackHelper(CallbackT cb,
    std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackState::*listMember, RemoveMemFn removeFn)
{
    if (!cb) {
        return Connection {};
    }
    const CallbackId id = m_callbackState->nextId.fetch_add(1U, std::memory_order_relaxed);
    {
        std::scoped_lock lock(m_callbackState->mutex);
        auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>(*(m_callbackState.get()->*listMember));
        nextList->push_back({ id, std::move(cb) });
        m_callbackState.get()->*listMember = std::move(nextList);
    }
    std::weak_ptr<CallbackState> weakState = m_callbackState;
    return Connection([weakState, id, removeFn]() {
        if (auto state = weakState.lock()) {
            (state.get()->*removeFn)(id);
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
    m_queue.enqueue(frame, std::move(queryTag), priority);

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

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_querySentTime).count();
    const auto timeoutMs = m_queryTimeoutMs.load();
    if (elapsed >= static_cast<long long>(timeoutMs)) {
        m_awaitingResponse = false;
        m_responseCv.notify_all();
        std::string tag;
        {
            std::scoped_lock lock(m_statusMutex);
            tag = m_pendingQueryTag;
        }

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
            if (entry.cb) {
                entry.cb(tag);
            }
        }
        DeviceStatus statusCopy;
        {
            std::scoped_lock lock(m_statusMutex);
            statusCopy = m_status;
        }
        for (const auto& entry : *qcbs) {
            if (entry.cb) {
                entry.cb(tag, false, statusCopy);
            }
        }
        const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(now - m_querySentTime);
        for (const auto& entry : *lcbs) {
            if (entry.cb) {
                entry.cb(tag, durationUs, false);
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
        const auto pollDeadline = (pollingEnabled && isConnected())
            ? nextPollTime
            : std::chrono::steady_clock::time_point::max();

        const bool hasItem = m_queue.popReady(item, [this] { return !m_running.load(); }, pollDeadline);
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
            }

            VLOG(1) << "Sending command (" << item.frame.size() << " bytes, tag: '" << item.queryTag << "')";
            const bool sendSuccess = m_transport->sendData(item.frame);
            if (!sendSuccess) {
                LOG(WARNING) << "Failed to transmit frame across transport.";
                if (!item.queryTag.empty()) {
                    std::scoped_lock lock(m_statusMutex);
                    m_awaitingResponse = false;
                    m_pendingQueryTag.clear();
                }

                RetryConfig retryCfg;
                {
                    std::scoped_lock rLock(m_retryMutex);
                    retryCfg = m_retryConfig;
                }
                if (retryCfg.retryOnTransportError && item.retryCount < retryCfg.maxRetries) {
                    const std::string reason = "Transport transmission failed for "
                        + (item.queryTag.empty() ? "command" : "query '" + item.queryTag + "'");
                    const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                    std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> rcbs;
                    {
                        std::scoped_lock lock(m_callbackState->mutex);
                        rcbs = m_callbackState->retryCallbacks;
                    }
                    for (const auto& entry : *rcbs) {
                        if (entry.cb) {
                            entry.cb(item.queryTag.empty() ? "Command" : item.queryTag,
                                item.retryCount + 1U, retryCfg.maxRetries, backoffDelay);
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
                        if (entry.cb) {
                            entry.cb(item.queryTag, false, statusCopy);
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
                    if (entry.cb) {
                        entry.cb(true, item.frame);
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
                                }
                                const std::string reason = "Query '" + item.queryTag + "' timed out";
                                const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                                std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> rcbs;
                                {
                                    std::scoped_lock lock(m_callbackState->mutex);
                                    rcbs = m_callbackState->retryCallbacks;
                                }
                                for (const auto& entry : *rcbs) {
                                    if (entry.cb) {
                                        entry.cb(item.queryTag, item.retryCount + 1U, retryCfg.maxRetries, backoffDelay);
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
    const auto frames = m_rxAccumulator.push(data, m_awaitingResponse.load());
    for (const auto& frame : frames) {
        dispatchFrame(frame);
    }
}

bool PelcoDDevice::isResponseMatchingQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept
{
    return ProtocolParser::isResponseMatchingQuery(queryTag, frame);
}

void PelcoDDevice::resolveQueryWait()
{
    {
        std::scoped_lock lock(m_statusMutex);
        m_awaitingResponse = false;
        m_pendingQueryTag.clear();
    }
    m_responseCv.notify_all();
}

void PelcoDDevice::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    // Notify RX traffic callbacks using copy-on-write snapshot (zero heap allocation)
    std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> tbs;
    std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> sbs;
    {
        std::scoped_lock lock(m_callbackState->mutex);
        tbs = m_callbackState->trafficCallbacks;
        sbs = m_callbackState->statusCallbacks;
    }

    for (const auto& entry : *tbs) {
        if (entry.cb) {
            entry.cb(false, frame);
        }
    }

    // Ignore frames with invalid structure or addressed to another device on shared bus
    if (frame.size() < 2U || frame[1] != m_address.load()) {
        return;
    }

    bool awaitingQuery { false };
    std::string pendingQueryTag;
    {
        std::scoped_lock lock(m_statusMutex);
        awaitingQuery = m_awaitingResponse.load();
        pendingQueryTag = m_pendingQueryTag;
    }
    if (awaitingQuery && !isResponseMatchingQuery(pendingQueryTag, frame)) {
        return;
    }

    DeviceStatus currentStatus;
    DeviceInfo currentInfo;
    {
        std::scoped_lock lock(m_statusMutex);
        currentStatus = m_status;
        currentInfo = m_info;
    }

    if (ProtocolParser::updateStatus(frame, currentStatus, currentInfo)) {
        bool querySatisfied = false;
        std::string satisfiedTag;
        {
            std::scoped_lock lock(m_statusMutex);
            if (m_awaitingResponse.load() && isResponseMatchingQuery(m_pendingQueryTag, frame)) {
                satisfiedTag = m_pendingQueryTag;
                m_awaitingResponse = false;
                m_pendingQueryTag.clear();
                querySatisfied = true;
            }
            m_status = currentStatus;
            m_info = currentInfo;
        }

        if (querySatisfied) {
            const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - m_querySentTime);
            m_responseCv.notify_all();
            std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> qcbs;
            std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> lcbs;
            {
                std::scoped_lock lock(m_callbackState->mutex);
                qcbs = m_callbackState->queryCompletedCallbacks;
                lcbs = m_callbackState->queryLatencyCallbacks;
            }
            for (const auto& entry : *qcbs) {
                if (entry.cb) {
                    entry.cb(satisfiedTag, true, currentStatus);
                }
            }
            for (const auto& entry : *lcbs) {
                if (entry.cb) {
                    entry.cb(satisfiedTag, durationUs, true);
                }
            }
        }

        for (const auto& entry : *sbs) {
            if (entry.cb) {
                entry.cb(currentStatus);
            }
        }
    }
}

} // namespace PelcoD
