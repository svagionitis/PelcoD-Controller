#include "PelcoDDevice.h"

#include <algorithm>
#include <glog/logging.h>
#include <stdexcept>
#include <utility>

namespace PelcoD {

PelcoDDevice::PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : m_session { std::make_shared<PelcoDSession>(std::move(transport), address) }
    , m_safetyGuard { std::make_shared<MotionSafetyGuard>([this]() { stopMotion(); }) }
    , m_telemetry { std::make_shared<TelemetryCoordinator>() }
    , m_dispatcher { std::make_shared<CallbackDispatcher>() }
    , m_metrics { std::make_unique<ProtocolMetrics>() }
    , m_commands { std::make_unique<PtzCommands>(m_session, m_safetyGuard, m_telemetry) }
{
    m_telemetry->setAddress(address);
    initSessionHandlers();
}

PelcoDDevice::~PelcoDDevice()
{
    stop();
}

void PelcoDDevice::initSessionHandlers()
{
    m_session->setFrameHandler([this](const std::vector<std::uint8_t>& frame, const std::string& queryTag,
                                   std::chrono::microseconds durationUs, std::optional<EverestLimitId> limitId) {
        const bool statusUpdated = m_telemetry->updateFromFrame(frame, limitId);
        const auto statusSnapshot = m_telemetry->getStatus();
        if (statusUpdated) {
            m_dispatcher->notifyStatus(statusSnapshot);
        }

        if (!queryTag.empty()) {
            m_metrics->recordQuerySuccess(durationUs);
            m_dispatcher->notifyQueryCompleted(queryTag, true, statusSnapshot);
            m_dispatcher->notifyQueryLatency(queryTag, durationUs, true);
        }
    });

    m_session->setTrafficHandler(
        [this](bool isTx, const std::vector<std::uint8_t>& frame) { m_dispatcher->notifyTraffic(isTx, frame); });

    m_session->setTimeoutHandler([this](const std::string& queryTag) {
        m_metrics->recordQueryTimeout();
        m_dispatcher->notifyTimeout(queryTag);
    });

    m_session->setQueryFailedHandler([this](const std::string& queryTag, std::chrono::microseconds durationUs) {
        const auto statusSnapshot = m_telemetry->getStatus();
        m_dispatcher->notifyQueryCompleted(queryTag, false, statusSnapshot);
        m_dispatcher->notifyQueryLatency(queryTag, durationUs, false);
    });

    m_session->setRetryHandler([this](const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries,
                                   std::chrono::milliseconds delay) {
        m_metrics->recordQueryRetry();
        m_dispatcher->notifyRetry(queryTag, attempt, maxRetries, delay);
    });

    m_session->setStateHandler([this](TransportState state, const std::string&) {
        const bool connected = (state == TransportState::Connected);
        m_telemetry->setConnected(connected);
        if (!connected && m_safetyGuard) {
            m_safetyGuard->onDisconnect();
        }
        m_dispatcher->notifyStatus(m_telemetry->getStatus());
    });

    m_session->setPollStepHandler([this]() { pollStep(); }, [this]() { return getPollDeadline(); });

    m_session->setStatusProvider([this]() { return m_telemetry->getStatus(); });
}

void PelcoDDevice::pollStep()
{
    const bool pollingEnabled = m_telemetry->isPolling();
    if (pollingEnabled && !m_lastPollingEnabled) {
        m_nextPollTime = std::chrono::steady_clock::now();
    }
    m_lastPollingEnabled = pollingEnabled;

    const auto now = std::chrono::steady_clock::now();
    if (pollingEnabled && isConnected() && now >= m_nextPollTime) {
        if (!m_session->hasLowPriorityPending()) {
            queryPan();
            queryTilt();
            queryZoom();
        }
        m_nextPollTime = now + std::chrono::milliseconds(m_telemetry->getPollIntervalMs());
    }
}

std::chrono::steady_clock::time_point PelcoDDevice::getPollDeadline() const noexcept
{
    if (m_telemetry->isPolling() && isConnected()) {
        return m_nextPollTime;
    }
    return std::chrono::steady_clock::time_point::max();
}

bool PelcoDDevice::start()
{
    const bool ok = m_session->start();
    if (ok) {
        m_telemetry->setConnected(true);
    }
    return ok;
}

void PelcoDDevice::stop()
{
    if (m_safetyGuard) {
        m_safetyGuard->shutdown();
    }
    m_session->stop();
    m_telemetry->setConnected(false);
}

bool PelcoDDevice::isConnected() const noexcept
{
    return m_session->isConnected();
}

void PelcoDDevice::setAddress(std::uint8_t address)
{
    m_telemetry->setAddress(address);
    m_session->setAddress(address);
}

std::uint8_t PelcoDDevice::getAddress() const noexcept
{
    return m_session->getAddress();
}

Connection PelcoDDevice::addStatusCallback(StatusCallback cb)
{
    return m_dispatcher->addStatusCallback(std::move(cb));
}

Connection PelcoDDevice::addTrafficCallback(TrafficCallback cb)
{
    return m_dispatcher->addTrafficCallback(std::move(cb));
}

Connection PelcoDDevice::addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    return m_dispatcher->addTrafficCallback(std::move(cb), notifyTx, notifyRx);
}

Connection PelcoDDevice::addTrafficCallback(
    std::uint8_t addressFilter, TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    return m_dispatcher->addTrafficCallback(addressFilter, std::move(cb), notifyTx, notifyRx);
}

Connection PelcoDDevice::addTimeoutCallback(TimeoutCallback cb)
{
    return m_dispatcher->addTimeoutCallback(std::move(cb));
}

Connection PelcoDDevice::addQueryCompletedCallback(QueryCompletedCallback cb)
{
    return m_dispatcher->addQueryCompletedCallback(std::move(cb));
}

Connection PelcoDDevice::addRetryCallback(RetryCallback cb)
{
    return m_dispatcher->addRetryCallback(std::move(cb));
}

Connection PelcoDDevice::addQueryLatencyCallback(QueryLatencyCallback cb)
{
    return m_dispatcher->addQueryLatencyCallback(std::move(cb));
}

bool PelcoDDevice::removeStatusCallback(CallbackId id)
{
    return m_dispatcher->removeStatusCallback(id);
}

bool PelcoDDevice::removeTrafficCallback(CallbackId id)
{
    return m_dispatcher->removeTrafficCallback(id);
}

bool PelcoDDevice::removeTimeoutCallback(CallbackId id)
{
    return m_dispatcher->removeTimeoutCallback(id);
}

bool PelcoDDevice::removeQueryCompletedCallback(CallbackId id)
{
    return m_dispatcher->removeQueryCompletedCallback(id);
}

bool PelcoDDevice::removeRetryCallback(CallbackId id)
{
    return m_dispatcher->removeRetryCallback(id);
}

bool PelcoDDevice::removeQueryLatencyCallback(CallbackId id)
{
    return m_dispatcher->removeQueryLatencyCallback(id);
}

void PelcoDDevice::clearCallbacks()
{
    m_dispatcher->clearCallbacks();
}

DeviceStatus PelcoDDevice::getStatus() const
{
    return m_telemetry->getStatus();
}

DeviceInfo PelcoDDevice::getInfo() const
{
    return m_telemetry->getInfo();
}

Transport::TransportStatsSnapshot PelcoDDevice::getTransportStats() const
{
    return m_session->getTransportStats();
}

PelcoDProtocolStats PelcoDDevice::getProtocolStats() const
{
    PelcoDProtocolStats stats = m_metrics->getStats();
    stats.checksumErrors = m_session->getAccumulator().checksumErrors();
    stats.discardedSyncBytes = m_session->getAccumulator().discardedBytes();
    stats.pendingCommands = m_session->getQueue().size();
    return stats;
}

void PelcoDDevice::resetProtocolStats() noexcept
{
    m_metrics->resetStats();
    m_session->getAccumulator().resetStats();
}

void PelcoDDevice::setTelemetryPolling(bool enable, std::uint32_t intervalMs) noexcept
{
    m_telemetry->setPolling(enable, intervalMs);
    m_session->getQueue().wakeAll();
}

bool PelcoDDevice::getTelemetryPolling() const noexcept
{
    return m_telemetry->isPolling();
}

void PelcoDDevice::setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept
{
    m_session->setQueryTimeoutMs(timeoutMs);
}

std::uint32_t PelcoDDevice::getQueryTimeoutMs() const noexcept
{
    return m_session->getQueryTimeoutMs();
}

void PelcoDDevice::setRetryConfig(const RetryConfig& config) noexcept
{
    m_session->setRetryConfig(config);
}

RetryConfig PelcoDDevice::getRetryConfig() const noexcept
{
    return m_session->getRetryConfig();
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
    m_commands->panLeft(speed);
}

void PelcoDDevice::panRight(std::uint8_t speed)
{
    m_commands->panRight(speed);
}

void PelcoDDevice::tiltUp(std::uint8_t speed)
{
    m_commands->tiltUp(speed);
}

void PelcoDDevice::tiltDown(std::uint8_t speed)
{
    m_commands->tiltDown(speed);
}

void PelcoDDevice::stopMotion()
{
    m_commands->stopMotion();
}

void PelcoDDevice::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed)
{
    m_commands->move(panDir, panSpeed, tiltDir, tiltSpeed);
}

void PelcoDDevice::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed,
    ZoomAction zoom, FocusAction focus, IrisAction iris)
{
    m_commands->move(panDir, panSpeed, tiltDir, tiltSpeed, zoom, focus, iris);
}

void PelcoDDevice::setPower(bool on)
{
    m_commands->setPower(on);
}

void PelcoDDevice::setAutoScan(bool enable)
{
    m_commands->setAutoScan(enable);
}

void PelcoDDevice::zoomTele()
{
    m_commands->zoomTele();
}

void PelcoDDevice::zoomWide()
{
    m_commands->zoomWide();
}

void PelcoDDevice::zoomStop()
{
    m_commands->zoomStop();
}

void PelcoDDevice::focusNear()
{
    m_commands->focusNear();
}

void PelcoDDevice::focusFar()
{
    m_commands->focusFar();
}

void PelcoDDevice::focusStop()
{
    m_commands->focusStop();
}

void PelcoDDevice::irisOpen()
{
    m_commands->irisOpen();
}

void PelcoDDevice::irisClose()
{
    m_commands->irisClose();
}

void PelcoDDevice::irisStop()
{
    m_commands->irisStop();
}

void PelcoDDevice::setPanAngle(std::uint16_t centidegrees)
{
    m_commands->setPanAngle(centidegrees);
}

void PelcoDDevice::setTiltAngle(std::uint16_t centidegrees)
{
    m_commands->setTiltAngle(centidegrees);
}

void PelcoDDevice::setZoomPosition(std::uint16_t position)
{
    m_commands->setZoomPosition(position);
}

void PelcoDDevice::setPreset(std::uint8_t presetId)
{
    m_commands->setPreset(presetId);
}

void PelcoDDevice::clearPreset(std::uint8_t presetId)
{
    m_commands->clearPreset(presetId);
}

void PelcoDDevice::goToPreset(std::uint8_t presetId)
{
    m_commands->goToPreset(presetId);
}

void PelcoDDevice::goToPreset(std::uint8_t presetId, std::uint8_t speed)
{
    m_commands->goToPreset(presetId, speed);
}

void PelcoDDevice::flip180()
{
    m_commands->flip180();
}

void PelcoDDevice::zeroPan()
{
    m_commands->zeroPan();
}

void PelcoDDevice::presetScan(std::uint8_t dwellSeconds)
{
    m_commands->presetScan(dwellSeconds);
}

void PelcoDDevice::setAuxiliary(std::uint8_t auxId)
{
    m_commands->setAuxiliary(auxId);
}

void PelcoDDevice::clearAuxiliary(std::uint8_t auxId)
{
    m_commands->clearAuxiliary(auxId);
}

void PelcoDDevice::setZoneStart(std::uint8_t zoneId)
{
    m_commands->setZoneStart(zoneId);
}

void PelcoDDevice::setZoneEnd(std::uint8_t zoneId)
{
    m_commands->setZoneEnd(zoneId);
}

void PelcoDDevice::setZoneScan(bool enable)
{
    m_commands->setZoneScan(enable);
}

void PelcoDDevice::recordPatternStart(std::uint8_t patternId)
{
    m_commands->recordPatternStart(patternId);
}

void PelcoDDevice::recordPatternStop()
{
    m_commands->recordPatternStop();
}

void PelcoDDevice::runPattern(std::uint8_t patternId)
{
    m_commands->runPattern(patternId);
}

void PelcoDDevice::setZoomSpeed(std::uint8_t speed)
{
    m_commands->setZoomSpeed(speed);
}

void PelcoDDevice::setFocusSpeed(std::uint8_t speed)
{
    m_commands->setFocusSpeed(speed);
}

void PelcoDDevice::setAutoFocus(AutoMode mode)
{
    m_commands->setAutoFocus(mode);
}

void PelcoDDevice::setAutoIris(AutoMode mode)
{
    m_commands->setAutoIris(mode);
}

void PelcoDDevice::setAgc(AutoMode mode)
{
    m_commands->setAgc(mode);
}

void PelcoDDevice::setBacklightComp(SwitchState state)
{
    m_commands->setBacklightComp(state);
}

void PelcoDDevice::setAutoWhiteBalance(SwitchState state)
{
    m_commands->setAutoWhiteBalance(state);
}

void PelcoDDevice::setShutterSpeed(std::uint16_t speed)
{
    m_commands->setShutterSpeed(speed);
}

void PelcoDDevice::setGain(std::uint16_t gain)
{
    m_commands->setGain(gain);
}

void PelcoDDevice::setAutoIrisLevel(std::uint8_t level)
{
    m_commands->setAutoIrisLevel(level);
}

void PelcoDDevice::setAutoIrisPeak(std::uint8_t peak)
{
    m_commands->setAutoIrisPeak(peak);
}

void PelcoDDevice::setPhaseDelayMode(SwitchState state)
{
    m_commands->setPhaseDelayMode(state);
}

void PelcoDDevice::adjustLineLockDelay(std::uint16_t centidegrees)
{
    m_commands->adjustLineLockDelay(centidegrees);
}

void PelcoDDevice::adjustWhiteBalanceRB(std::uint16_t value)
{
    m_commands->adjustWhiteBalanceRB(value);
}

void PelcoDDevice::adjustWhiteBalanceMG(std::uint16_t value)
{
    m_commands->adjustWhiteBalanceMG(value);
}

void PelcoDDevice::setMagnification(std::uint16_t value, bool relative)
{
    m_commands->setMagnification(value, relative);
}

void PelcoDDevice::setBaudRate(std::uint32_t baud)
{
    m_commands->setBaudRate(baud);
}

void PelcoDDevice::setZeroPosition()
{
    m_commands->setZeroPosition();
}

void PelcoDDevice::setFocusPosition(std::uint16_t pos)
{
    m_commands->setFocusPosition(pos);
}

void PelcoDDevice::setIrisPosition(std::uint16_t pos)
{
    m_commands->setIrisPosition(pos);
}

void PelcoDDevice::setSerialNumber(const std::string& serial)
{
    m_telemetry->setSerialNumber(serial);
}

void PelcoDDevice::querySerialNumber()
{
    m_commands->querySerialNumber();
}

void PelcoDDevice::resetDefaults()
{
    m_commands->resetDefaults();
}

void PelcoDDevice::remoteReset()
{
    m_commands->remoteReset();
}

void PelcoDDevice::queryPan()
{
    m_commands->queryPan();
}

void PelcoDDevice::queryTilt()
{
    m_commands->queryTilt();
}

void PelcoDDevice::queryZoom()
{
    m_commands->queryZoom();
}

void PelcoDDevice::queryMagnification()
{
    m_commands->queryMagnification();
}

void PelcoDDevice::queryDeviceType()
{
    m_commands->queryDeviceType();
}

void PelcoDDevice::queryGeneral()
{
    m_commands->queryGeneral();
}

void PelcoDDevice::queryDiagnostics()
{
    m_commands->queryDiagnostics();
}

void PelcoDDevice::queryAll()
{
    m_commands->queryAll();
}

void PelcoDDevice::activateEchoMode()
{
    m_commands->activateEchoMode();
}

void PelcoDDevice::prepareForDownload()
{
    m_commands->prepareForDownload();
}

void PelcoDDevice::startDownload()
{
    m_commands->startDownload();
}

void PelcoDDevice::writeCharacter(std::uint8_t column, char asciiChar)
{
    m_commands->writeCharacter(column, asciiChar);
}

void PelcoDDevice::clearScreen()
{
    m_commands->clearScreen();
}

void PelcoDDevice::sendDummy()
{
    m_commands->sendDummy();
}

void PelcoDDevice::screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative)
{
    m_commands->screenMove(panPercent, tiltPercent, relative);
}

void PelcoDDevice::querySoftwareVersion()
{
    m_commands->querySoftwareVersion();
}

void PelcoDDevice::queryBuildNumber()
{
    m_commands->queryBuildNumber();
}

void PelcoDDevice::setSeconds(std::uint8_t seconds)
{
    m_commands->setSeconds(seconds);
}

void PelcoDDevice::setHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    m_commands->setHourMinute(hour, minute);
}

void PelcoDDevice::setMonthDay(std::uint8_t month, std::uint8_t day)
{
    m_commands->setMonthDay(month, day);
}

void PelcoDDevice::setYear(std::uint16_t year)
{
    m_commands->setYear(year);
}

void PelcoDDevice::setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second)
{
    m_commands->setTime(hour, minute, second);
}

void PelcoDDevice::setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day)
{
    m_commands->setDate(year, month, day);
}

void PelcoDDevice::queryTime(TimeSubOpcode queryType)
{
    m_commands->queryTime(queryType);
}

void PelcoDDevice::setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths)
{
    m_commands->setAuxLed(ledIdOrColor, onTimeTenths);
}

void PelcoDDevice::clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths)
{
    m_commands->clearAuxLed(ledIdOrColor, offTimeTenths);
}

void PelcoDDevice::queryAzimuthZero()
{
    m_commands->queryAzimuthZero();
}

void PelcoDDevice::setZoomLimit(std::uint16_t limitHundredths)
{
    m_commands->setZoomLimit(limitHundredths);
}

void PelcoDDevice::queryZoomLimit()
{
    m_commands->queryZoomLimit();
}

void PelcoDDevice::queryEverestAlarms()
{
    m_commands->queryEverestAlarms();
}

void PelcoDDevice::deletePattern(std::uint8_t patternId)
{
    m_commands->deletePattern(patternId);
}

void PelcoDDevice::setManualLeftPanLimit(std::uint16_t centidegrees)
{
    m_commands->setManualLeftPanLimit(centidegrees);
}

void PelcoDDevice::setManualRightPanLimit(std::uint16_t centidegrees)
{
    m_commands->setManualRightPanLimit(centidegrees);
}

void PelcoDDevice::setScanLeftPanLimit(std::uint16_t centidegrees)
{
    m_commands->setScanLeftPanLimit(centidegrees);
}

void PelcoDDevice::setScanRightPanLimit(std::uint16_t centidegrees)
{
    m_commands->setScanRightPanLimit(centidegrees);
}

void PelcoDDevice::queryLimit(EverestLimitId limitId)
{
    m_commands->queryLimit(limitId);
}

void PelcoDDevice::enableLimits(bool enable)
{
    m_commands->enableLimits(enable);
}

void PelcoDDevice::queryDefinedPresets(std::uint8_t group)
{
    m_commands->queryDefinedPresets(group);
}

void PelcoDDevice::queryDefinedPatterns(std::uint8_t group)
{
    m_commands->queryDefinedPatterns(group);
}

AsyncWatchdog& PelcoDDevice::getAsyncWatchdog() noexcept
{
    return m_session->getAsyncWatchdog();
}

void PelcoDDevice::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    static_cast<void>(m_session->sendRawFrame(frame));
}

void PelcoDDevice::sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag)
{
    static_cast<void>(m_session->sendQueryFrame(frame, queryTag));
}

void PelcoDDevice::enqueueCommand(
    const std::vector<std::uint8_t>& frame, std::string queryTag, CommandPriority priority)
{
    if (m_safetyGuard) {
        if (PelcoDFrame::isStandardStop(frame) || priority == CommandPriority::Urgent) {
            m_safetyGuard->onStopCommand();
        } else if (PelcoDFrame::isStandardMotion(frame)) {
            m_safetyGuard->onMotionCommand(frame);
        }
    }
    static_cast<void>(m_session->enqueueCommand(frame, std::move(queryTag), priority));
}

bool PelcoDDevice::setFrameExt(std::shared_ptr<IFrameExtension> ext)
{
    return m_session->setFrameExt(std::move(ext));
}

void PelcoDDevice::resolveQueryWait()
{
    m_session->resolveQueryWait();
}

namespace {

    struct QueryState {
        std::atomic<bool> fulfilled { false };
        Connection conn {};
        std::atomic<std::uint64_t> timerId { 0U };
    };

    struct CompositeQueryState {
        std::atomic<bool> fulfilled { false };
        Connection conn {};
        std::atomic<std::uint64_t> timerId { 0U };
        std::atomic<bool> panReceived { false };
        std::atomic<bool> tiltReceived { false };
        std::atomic<bool> zoomReceived { false };
    };

    template <typename ResultT, typename TriggerFn, typename ExtractFn>
    std::future<ResultT> executeAsyncQuery(PelcoDDevice* device, const std::string& queryTag, TriggerFn trigger,
        ExtractFn extract, std::chrono::milliseconds timeout)
    {
        auto promise = std::make_shared<std::promise<ResultT>>();
        auto future = promise->get_future();
        auto state = std::make_shared<QueryState>();

        const auto effectiveTimeout = (timeout.count() > 0)
            ? timeout
            : std::chrono::milliseconds(device->getQueryTimeoutMs() > 0U ? device->getQueryTimeoutMs() : 1000U);

        state->conn = device->addQueryCompletedCallback(
            [promise, state, queryTag, extract, device](
                const std::string& completedTag, bool success, const DeviceStatus& status) {
                if (completedTag == queryTag) {
                    if (!state->fulfilled.exchange(true)) {
                        device->getAsyncWatchdog().cancel(state->timerId.load());
                        state->conn.disconnect();
                        if (success) {
                            try {
                                promise->set_value(extract(status));
                            } catch (...) {
                                promise->set_exception(std::current_exception());
                            }
                        } else {
                            promise->set_exception(
                                std::make_exception_ptr(std::runtime_error("Query '" + queryTag + "' failed")));
                        }
                    }
                }
            });

        const auto tid = device->getAsyncWatchdog().schedule(effectiveTimeout, [promise, state, queryTag]() {
            if (!state->fulfilled.exchange(true)) {
                state->conn.disconnect();
                promise->set_exception(
                    std::make_exception_ptr(std::runtime_error("Query '" + queryTag + "' timed out")));
            }
        });
        state->timerId.store(tid);

        trigger();
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
    auto promise = std::make_shared<std::promise<DeviceStatus>>();
    auto future = promise->get_future();
    auto state = std::make_shared<CompositeQueryState>();

    const auto effectiveTimeout = (timeout.count() > 0)
        ? timeout
        : std::chrono::milliseconds(getQueryTimeoutMs() > 0U ? getQueryTimeoutMs() : 1000U);

    state->conn = addQueryCompletedCallback(
        [this, promise, state](const std::string& completedTag, bool success, const DeviceStatus& status) {
            if (!success) {
                if (!state->fulfilled.exchange(true)) {
                    getAsyncWatchdog().cancel(state->timerId.load());
                    state->conn.disconnect();
                    promise->set_exception(std::make_exception_ptr(
                        std::runtime_error("queryStatusAsync subquery failed: " + completedTag)));
                }
                return;
            }

            if (completedTag == "QueryPan") {
                state->panReceived.store(true);
            } else if (completedTag == "QueryTilt") {
                state->tiltReceived.store(true);
            } else if (completedTag == "QueryZoom") {
                state->zoomReceived.store(true);
            }

            if (state->panReceived.load() && state->tiltReceived.load() && state->zoomReceived.load()) {
                if (!state->fulfilled.exchange(true)) {
                    getAsyncWatchdog().cancel(state->timerId.load());
                    state->conn.disconnect();
                    promise->set_value(status);
                }
            }
        });

    const auto tid = getAsyncWatchdog().schedule(effectiveTimeout, [promise, state]() {
        if (!state->fulfilled.exchange(true)) {
            state->conn.disconnect();
            promise->set_exception(std::make_exception_ptr(std::runtime_error("queryStatusAsync timed out")));
        }
    });
    state->timerId.store(tid);

    queryPan();
    queryTilt();
    queryZoom();

    return future;
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

} // namespace PelcoD
