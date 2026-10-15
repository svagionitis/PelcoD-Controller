#include "PelcoDDevice.h"

#include <algorithm>
#include <glog/logging.h>
#include <stdexcept>
#include <utility>

namespace PelcoD {

PelcoDDevice::PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : m_session { std::make_shared<PelcoDSession>(std::move(transport), address) }
    , m_safetyGuard { std::make_shared<MotionSafetyGuard>(m_session) }
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

    m_session->setCommandDroppedHandler([this](const std::vector<std::uint8_t>& frame, SendResult reason) {
        m_metrics->recordCommandDropped();
        if (reason == SendResult::QueueFull) {
            m_metrics->recordQueueOverflow();
        } else if (reason == SendResult::RetriesExhausted) {
            m_metrics->recordCommandRetryExhausted();
        } else {
            // Other drop reasons (e.g. TransportClosed) are covered by commandsDropped alone.
        }
        m_dispatcher->notifyCommandDropped(frame, reason);
    });

    if (m_safetyGuard) {
        m_safetyGuard->setSafetyEventCallback([this](MotionSafetyEvent event, MotionAxis axes) {
            if (event == MotionSafetyEvent::DeadManTimeout) {
                m_metrics->recordDeadManTrip();
            } else if (event == MotionSafetyEvent::ContinuousMotionLimit) {
                m_metrics->recordContinuousMotionTrip();
            } else if (event == MotionSafetyEvent::DisconnectStop || event == MotionSafetyEvent::ShutdownStop) {
                m_metrics->recordFailSafeStop();
            }
            m_dispatcher->notifyMotionSafety(event, axes);
        });
    }

    m_session->setStateHandler([this](TransportState state, const std::string&) {
        const bool connected = (state == TransportState::Connected);
        m_telemetry->setConnected(connected);
        if (!connected) {
            if (m_safetyGuard) {
                m_safetyGuard->handleDisconnect();
            }
            m_metrics->recordMotionPurgeOnDisconnect();
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
            static_cast<void>(queryPan());
            static_cast<void>(queryTilt());
            static_cast<void>(queryZoom());
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

Connection PelcoDDevice::addCommandDroppedCallback(CommandDroppedCallback cb)
{
    return m_dispatcher->addCommandDroppedCallback(std::move(cb));
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

bool PelcoDDevice::removeCommandDroppedCallback(CallbackId id)
{
    return m_dispatcher->removeCommandDroppedCallback(id);
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

std::shared_ptr<ITransport> PelcoDDevice::createVirtualChannel()
{
    return m_session ? m_session->createVirtualChannel() : nullptr;
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

void PelcoDDevice::setMotionSafetyPolicy(const MotionSafetyPolicy& policy) noexcept
{
    if (m_safetyGuard) {
        m_safetyGuard->setPolicy(policy);
    }
}

MotionSafetyPolicy PelcoDDevice::getMotionSafetyPolicy() const noexcept
{
    if (m_safetyGuard) {
        return m_safetyGuard->getPolicy();
    }
    return MotionSafetyPolicy {};
}

Connection PelcoDDevice::addMotionSafetyCallback(MotionSafetyCallback cb)
{
    return m_dispatcher->addMotionSafetyCallback(std::move(cb));
}

bool PelcoDDevice::removeMotionSafetyCallback(CallbackId id)
{
    return m_dispatcher->removeMotionSafetyCallback(id);
}

SendResult PelcoDDevice::panLeft(std::uint8_t speed)
{
    return m_commands->panLeft(speed);
}

SendResult PelcoDDevice::panRight(std::uint8_t speed)
{
    return m_commands->panRight(speed);
}

SendResult PelcoDDevice::tiltUp(std::uint8_t speed)
{
    return m_commands->tiltUp(speed);
}

SendResult PelcoDDevice::tiltDown(std::uint8_t speed)
{
    return m_commands->tiltDown(speed);
}

SendResult PelcoDDevice::stopMotion()
{
    return m_commands->stopMotion();
}

SendResult PelcoDDevice::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed)
{
    return m_commands->move(panDir, panSpeed, tiltDir, tiltSpeed);
}

SendResult PelcoDDevice::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed,
    ZoomAction zoom, FocusAction focus, IrisAction iris)
{
    return m_commands->move(panDir, panSpeed, tiltDir, tiltSpeed, zoom, focus, iris);
}

SendResult PelcoDDevice::setPower(bool on)
{
    return m_commands->setPower(on);
}

SendResult PelcoDDevice::setAutoScan(bool enable)
{
    return m_commands->setAutoScan(enable);
}

SendResult PelcoDDevice::zoomTele()
{
    return m_commands->zoomTele();
}

SendResult PelcoDDevice::zoomWide()
{
    return m_commands->zoomWide();
}

SendResult PelcoDDevice::zoomStop()
{
    return m_commands->zoomStop();
}

SendResult PelcoDDevice::focusNear()
{
    return m_commands->focusNear();
}

SendResult PelcoDDevice::focusFar()
{
    return m_commands->focusFar();
}

SendResult PelcoDDevice::focusStop()
{
    return m_commands->focusStop();
}

SendResult PelcoDDevice::irisOpen()
{
    return m_commands->irisOpen();
}

SendResult PelcoDDevice::irisClose()
{
    return m_commands->irisClose();
}

SendResult PelcoDDevice::irisStop()
{
    return m_commands->irisStop();
}

SendResult PelcoDDevice::setPanAngle(std::uint16_t centidegrees)
{
    return m_commands->setPanAngle(centidegrees);
}

SendResult PelcoDDevice::setTiltAngle(std::uint16_t centidegrees)
{
    return m_commands->setTiltAngle(centidegrees);
}

SendResult PelcoDDevice::setZoomPosition(std::uint16_t position)
{
    return m_commands->setZoomPosition(position);
}

SendResult PelcoDDevice::setPreset(std::uint8_t presetId)
{
    return m_commands->setPreset(presetId);
}

SendResult PelcoDDevice::clearPreset(std::uint8_t presetId)
{
    return m_commands->clearPreset(presetId);
}

SendResult PelcoDDevice::goToPreset(std::uint8_t presetId)
{
    return m_commands->goToPreset(presetId);
}

SendResult PelcoDDevice::goToPreset(std::uint8_t presetId, std::uint8_t speed)
{
    return m_commands->goToPreset(presetId, speed);
}

SendResult PelcoDDevice::flip180()
{
    return m_commands->flip180();
}

SendResult PelcoDDevice::zeroPan()
{
    return m_commands->zeroPan();
}

SendResult PelcoDDevice::presetScan(std::uint8_t dwellSeconds)
{
    return m_commands->presetScan(dwellSeconds);
}

SendResult PelcoDDevice::setAuxiliary(std::uint8_t auxId)
{
    return m_commands->setAuxiliary(auxId);
}

SendResult PelcoDDevice::clearAuxiliary(std::uint8_t auxId)
{
    return m_commands->clearAuxiliary(auxId);
}

SendResult PelcoDDevice::setZoneStart(std::uint8_t zoneId)
{
    return m_commands->setZoneStart(zoneId);
}

SendResult PelcoDDevice::setZoneEnd(std::uint8_t zoneId)
{
    return m_commands->setZoneEnd(zoneId);
}

SendResult PelcoDDevice::setZoneScan(bool enable)
{
    return m_commands->setZoneScan(enable);
}

SendResult PelcoDDevice::recordPatternStart(std::uint8_t patternId)
{
    return m_commands->recordPatternStart(patternId);
}

SendResult PelcoDDevice::recordPatternStop()
{
    return m_commands->recordPatternStop();
}

SendResult PelcoDDevice::runPattern(std::uint8_t patternId)
{
    return m_commands->runPattern(patternId);
}

SendResult PelcoDDevice::setZoomSpeed(std::uint8_t speed)
{
    return m_commands->setZoomSpeed(speed);
}

SendResult PelcoDDevice::setFocusSpeed(std::uint8_t speed)
{
    return m_commands->setFocusSpeed(speed);
}

SendResult PelcoDDevice::setAutoFocus(AutoMode mode)
{
    return m_commands->setAutoFocus(mode);
}

SendResult PelcoDDevice::setAutoIris(AutoMode mode)
{
    return m_commands->setAutoIris(mode);
}

SendResult PelcoDDevice::setAgc(AutoMode mode)
{
    return m_commands->setAgc(mode);
}

SendResult PelcoDDevice::setBacklightComp(SwitchState state)
{
    return m_commands->setBacklightComp(state);
}

SendResult PelcoDDevice::setAutoWhiteBalance(SwitchState state)
{
    return m_commands->setAutoWhiteBalance(state);
}

SendResult PelcoDDevice::setShutterSpeed(std::uint16_t speed)
{
    return m_commands->setShutterSpeed(speed);
}

SendResult PelcoDDevice::setGain(std::uint16_t gain)
{
    return m_commands->setGain(gain);
}

SendResult PelcoDDevice::setAutoIrisLevel(std::uint8_t level)
{
    return m_commands->setAutoIrisLevel(level);
}

SendResult PelcoDDevice::setAutoIrisPeak(std::uint8_t peak)
{
    return m_commands->setAutoIrisPeak(peak);
}

SendResult PelcoDDevice::setPhaseDelayMode(SwitchState state)
{
    return m_commands->setPhaseDelayMode(state);
}

SendResult PelcoDDevice::adjustLineLockDelay(std::uint16_t centidegrees)
{
    return m_commands->adjustLineLockDelay(centidegrees);
}

SendResult PelcoDDevice::adjustWhiteBalanceRB(std::uint16_t value)
{
    return m_commands->adjustWhiteBalanceRB(value);
}

SendResult PelcoDDevice::adjustWhiteBalanceMG(std::uint16_t value)
{
    return m_commands->adjustWhiteBalanceMG(value);
}

SendResult PelcoDDevice::setMagnification(std::uint16_t value, bool relative)
{
    return m_commands->setMagnification(value, relative);
}

SendResult PelcoDDevice::setBaudRate(std::uint32_t baud)
{
    return m_commands->setBaudRate(baud);
}

SendResult PelcoDDevice::setZeroPosition()
{
    return m_commands->setZeroPosition();
}

SendResult PelcoDDevice::querySerialNumber()
{
    return m_commands->querySerialNumber();
}

SendResult PelcoDDevice::resetDefaults()
{
    return m_commands->resetDefaults();
}

SendResult PelcoDDevice::remoteReset()
{
    return m_commands->remoteReset();
}

SendResult PelcoDDevice::queryPan()
{
    return m_commands->queryPan();
}

SendResult PelcoDDevice::queryTilt()
{
    return m_commands->queryTilt();
}

SendResult PelcoDDevice::queryZoom()
{
    return m_commands->queryZoom();
}

SendResult PelcoDDevice::queryMagnification()
{
    return m_commands->queryMagnification();
}

SendResult PelcoDDevice::queryDeviceType()
{
    return m_commands->queryDeviceType();
}

SendResult PelcoDDevice::queryGeneral()
{
    return m_commands->queryGeneral();
}

SendResult PelcoDDevice::queryDiagnostics()
{
    return m_commands->queryDiagnostics();
}

SendResult PelcoDDevice::queryAll()
{
    return m_commands->queryAll();
}

SendResult PelcoDDevice::activateEchoMode()
{
    return m_commands->activateEchoMode();
}

SendResult PelcoDDevice::prepareForDownload()
{
    return m_commands->prepareForDownload();
}

SendResult PelcoDDevice::startDownload()
{
    return m_commands->startDownload();
}

SendResult PelcoDDevice::writeCharacter(std::uint8_t column, char asciiChar)
{
    return m_commands->writeCharacter(column, asciiChar);
}

SendResult PelcoDDevice::clearScreen()
{
    return m_commands->clearScreen();
}

SendResult PelcoDDevice::sendDummy()
{
    return m_commands->sendDummy();
}

SendResult PelcoDDevice::screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative)
{
    return m_commands->screenMove(panPercent, tiltPercent, relative);
}

SendResult PelcoDDevice::querySoftwareVersion()
{
    return m_commands->querySoftwareVersion();
}

SendResult PelcoDDevice::queryBuildNumber()
{
    return m_commands->queryBuildNumber();
}

SendResult PelcoDDevice::setSeconds(std::uint8_t seconds)
{
    return m_commands->setSeconds(seconds);
}

SendResult PelcoDDevice::setHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    return m_commands->setHourMinute(hour, minute);
}

SendResult PelcoDDevice::setMonthDay(std::uint8_t month, std::uint8_t day)
{
    return m_commands->setMonthDay(month, day);
}

SendResult PelcoDDevice::setYear(std::uint16_t year)
{
    return m_commands->setYear(year);
}

SendResult PelcoDDevice::setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second)
{
    return m_commands->setTime(hour, minute, second);
}

SendResult PelcoDDevice::setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day)
{
    return m_commands->setDate(year, month, day);
}

SendResult PelcoDDevice::queryTime(TimeSubOpcode queryType)
{
    return m_commands->queryTime(queryType);
}

SendResult PelcoDDevice::setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths)
{
    return m_commands->setAuxLed(ledIdOrColor, onTimeTenths);
}

SendResult PelcoDDevice::clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths)
{
    return m_commands->clearAuxLed(ledIdOrColor, offTimeTenths);
}

SendResult PelcoDDevice::queryAzimuthZero()
{
    return m_commands->queryAzimuthZero();
}

SendResult PelcoDDevice::setZoomLimit(std::uint16_t limitHundredths)
{
    return m_commands->setZoomLimit(limitHundredths);
}

SendResult PelcoDDevice::queryZoomLimit()
{
    return m_commands->queryZoomLimit();
}

SendResult PelcoDDevice::queryEverestAlarms()
{
    return m_commands->queryEverestAlarms();
}

SendResult PelcoDDevice::deletePattern(std::uint8_t patternId)
{
    return m_commands->deletePattern(patternId);
}

SendResult PelcoDDevice::setManualLeftPanLimit(std::uint16_t centidegrees)
{
    return m_commands->setManualLeftPanLimit(centidegrees);
}

SendResult PelcoDDevice::setManualRightPanLimit(std::uint16_t centidegrees)
{
    return m_commands->setManualRightPanLimit(centidegrees);
}

SendResult PelcoDDevice::setScanLeftPanLimit(std::uint16_t centidegrees)
{
    return m_commands->setScanLeftPanLimit(centidegrees);
}

SendResult PelcoDDevice::setScanRightPanLimit(std::uint16_t centidegrees)
{
    return m_commands->setScanRightPanLimit(centidegrees);
}

SendResult PelcoDDevice::queryLimit(EverestLimitId limitId)
{
    return m_commands->queryLimit(limitId);
}

SendResult PelcoDDevice::enableLimits(bool enable)
{
    return m_commands->enableLimits(enable);
}

SendResult PelcoDDevice::queryDefinedPresets(std::uint8_t group)
{
    return m_commands->queryDefinedPresets(group);
}

SendResult PelcoDDevice::queryDefinedPatterns(std::uint8_t group)
{
    return m_commands->queryDefinedPatterns(group);
}

AsyncWatchdog& PelcoDDevice::getAsyncWatchdog() noexcept
{
    return m_session->getAsyncWatchdog();
}

SendResult PelcoDDevice::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    return enqueueCommand(frame, "", CommandPriority::Normal);
}

SendResult PelcoDDevice::sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag)
{
    return m_session->sendQueryFrame(frame, std::move(queryTag));
}

SendResult PelcoDDevice::enqueueCommand(
    const std::vector<std::uint8_t>& frame, std::string queryTag, CommandPriority priority)
{
    if (m_safetyGuard) {
        return m_safetyGuard->dispatch(frame, std::move(queryTag), priority);
    }
    return m_session->enqueueCommand(frame, std::move(queryTag), priority);
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

        const SendResult triggerRes = trigger();
        if (triggerRes != SendResult::Success) {
            if (!state->fulfilled.exchange(true)) {
                device->getAsyncWatchdog().cancel(state->timerId.load());
                state->conn.disconnect();
                promise->set_exception(std::make_exception_ptr(std::runtime_error(
                    "Query '" + queryTag + "' dispatch failed: " + std::string(sendResultToString(triggerRes)))));
            }
        }
        return future;
    }

} // namespace

std::future<std::uint16_t> PelcoDDevice::queryPanAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryPan", [this] { return queryPan(); }, [](const DeviceStatus& s) { return s.panCentidegrees; },
        timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryTiltAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryTilt", [this] { return queryTilt(); }, [](const DeviceStatus& s) { return s.tiltCentidegrees; },
        timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryZoomAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryZoom", [this] { return queryZoom(); }, [](const DeviceStatus& s) { return s.zoomPosition; },
        timeout);
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

    const SendResult rPan = queryPan();
    const SendResult rTilt = queryTilt();
    const SendResult rZoom = queryZoom();
    if (rPan != SendResult::Success || rTilt != SendResult::Success || rZoom != SendResult::Success) {
        if (!state->fulfilled.exchange(true)) {
            getAsyncWatchdog().cancel(state->timerId.load());
            state->conn.disconnect();
            promise->set_exception(std::make_exception_ptr(std::runtime_error("queryStatusAsync dispatch failed")));
        }
    }

    return future;
}

std::future<std::pair<std::uint8_t, std::uint8_t>> PelcoDDevice::querySoftwareVersionAsync(
    std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::pair<std::uint8_t, std::uint8_t>>(
        this, "QuerySoftwareVersion", [this] { return querySoftwareVersion(); },
        [this](const DeviceStatus&) {
            const auto info = getInfo();
            return std::make_pair(info.softwareMajor, info.softwareMinor);
        },
        timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryBuildNumberAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryBuildNumber", [this] { return queryBuildNumber(); },
        [this](const DeviceStatus&) { return getInfo().buildNumber; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryAzimuthZeroAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryAzimuthZero", [this] { return queryAzimuthZero(); },
        [](const DeviceStatus& s) { return s.azimuthZeroOffsetCentidegrees; }, timeout);
}

std::future<std::uint16_t> PelcoDDevice::queryZoomLimitAsync(std::chrono::milliseconds timeout)
{
    return executeAsyncQuery<std::uint16_t>(
        this, "QueryZoomLimit", [this] { return queryZoomLimit(); }, [](const DeviceStatus& s) { return s.zoomLimit; },
        timeout);
}

} // namespace PelcoD
