#include "PelcoDDevice.h"

#include <algorithm>
#include <glog/logging.h>
#include <stdexcept>
#include <utility>

namespace PelcoD {

PelcoDDevice::PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : PelcoDDevice(
        [](std::shared_ptr<ITransport> t, std::uint8_t addr) {
            auto session = std::make_shared<PelcoDSession>(std::move(t), addr);
            auto guard = std::make_shared<MotionSafetyGuard>(session);
            auto telemetry = std::make_shared<TelemetryCoordinator>();
            auto dispatcher = std::make_shared<CallbackDispatcher>();
            auto metrics = std::make_unique<ProtocolMetrics>();
            return DeviceCoreBundle { std::move(session), std::move(guard), std::move(telemetry), std::move(dispatcher),
                std::move(metrics) };
        }(std::move(transport), address),
        address)
{
}

PelcoDDevice::PelcoDDevice(DeviceCoreBundle bundle, std::uint8_t address)
    : PtzCommands(bundle.session, bundle.safetyGuard, bundle.telemetry)
    , m_session { std::move(bundle.session) }
    , m_safetyGuard { std::move(bundle.safetyGuard) }
    , m_telemetry { std::move(bundle.telemetry) }
    , m_dispatcher { std::move(bundle.dispatcher) }
    , m_metrics { std::move(bundle.metrics) }
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

    m_session->setStateHandler([this](TransportState state, const std::string&) {
        const bool connected = (state == TransportState::Connected);
        m_telemetry->setConnected(connected);
        if (!connected) {
            m_safetyGuard->handleDisconnect();
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
    m_safetyGuard->shutdown();
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
    m_safetyGuard->setDeadManTimeout(timeout);
}

std::chrono::milliseconds PelcoDDevice::getDeadManTimeout() const noexcept
{
    return m_safetyGuard->getDeadManTimeout();
}

void PelcoDDevice::setMotionSafetyPolicy(const MotionSafetyPolicy& policy) noexcept
{
    m_safetyGuard->setPolicy(policy);
}

MotionSafetyPolicy PelcoDDevice::getMotionSafetyPolicy() const noexcept
{
    return m_safetyGuard->getPolicy();
}

Connection PelcoDDevice::addMotionSafetyCallback(MotionSafetyCallback cb)
{
    return m_dispatcher->addMotionSafetyCallback(std::move(cb));
}

bool PelcoDDevice::removeMotionSafetyCallback(CallbackId id)
{
    return m_dispatcher->removeMotionSafetyCallback(id);
}

AsyncWatchdog& PelcoDDevice::getAsyncWatchdog() noexcept
{
    return m_session->getAsyncWatchdog();
}

SendResult PelcoDDevice::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    return enqueueCommand(frame, "", CommandPriority::Normal);
}

SendResult PelcoDDevice::sendQueryFrame(const std::vector<std::uint8_t>& frame, QueryRef query)
{
    return m_session->sendQueryFrame(frame, std::move(query));
}

SendResult PelcoDDevice::enqueueCommand(
    const std::vector<std::uint8_t>& frame, QueryRef query, CommandPriority priority)
{
    return m_safetyGuard->dispatch(frame, std::move(query), priority);
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
