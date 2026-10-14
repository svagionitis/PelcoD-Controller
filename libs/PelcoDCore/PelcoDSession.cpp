#include "PelcoDSession.h"
#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"

#include <algorithm>
#include <glog/logging.h>

namespace PelcoD {

PelcoDSession::PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : m_transport { std::move(transport) }
    , m_address { address }
    , m_asyncWatchdog { std::make_unique<AsyncWatchdog>() }
{
    m_queue.setEvictionHandler([this](const CommandItem& evicted) {
        if (!evicted.queryTag.empty()) {
            notifyQueryFailed(evicted.queryTag);
        }
    });
}

PelcoDSession::~PelcoDSession()
{
    stop();
}

bool PelcoDSession::start()
{
    std::unique_lock<std::recursive_mutex> lock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }
    if (!m_transport) {
        LOG(ERROR) << "PelcoDSession::start: No transport configured.";
        return false;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    auto rxGate = std::make_shared<CallbackGate>();
    m_rxGate = rxGate;

    try {
        if (!m_transport->isOpen()) {
            if (!m_transport->open()) {
                LOG(ERROR) << "PelcoDSession::start: Failed to open transport.";
                m_rxGate.reset();
                return false;
            }
        }

        m_transport->setDataCallback([this, rxGate](const std::vector<std::uint8_t>& data) {
            const CallbackGate::Pass pass { *rxGate };
            if (pass) {
                onDataReceived(data);
            }
        });

        m_transport->setStateCallback([this, rxGate](TransportState state, const std::string& msg) {
            const CallbackGate::Pass pass { *rxGate };
            if (pass) {
                onTransportState(state, msg);
            }
        });

        m_running.store(true);
        m_connected.store(m_transport->isOpen());
        if (!m_asyncWatchdog) {
            m_asyncWatchdog = std::make_unique<AsyncWatchdog>();
        }
        m_workerThread = std::thread(&PelcoDSession::workerLoop, this);
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "PelcoDSession::start encountered exception: " << e.what();
        m_running.store(false);
        m_connected.store(false);
        m_rxGate.reset();
        if (m_asyncWatchdog) {
            m_asyncWatchdog->shutdown();
            m_asyncWatchdog.reset();
        }
        if (m_transport) {
            m_transport->setDataCallback(nullptr);
            m_transport->setStateCallback(nullptr);
        }
        return false;
    }
}

void PelcoDSession::stop()
{
    std::unique_lock<std::recursive_mutex> lifecycleLock { m_lifecycleMutex };

    for (auto gate = m_rxGate; gate && !gate->isClosed(); gate = m_rxGate) {
        lifecycleLock.unlock();
        gate->close();
        lifecycleLock.lock();
    }

    if (m_transport && m_transport->isOpen()) {
        const auto stopFrame = ProtocolBuilder::buildStop(m_address.load(std::memory_order_relaxed));
        static_cast<void>(m_transport->sendData(stopFrame));
    }

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

    m_running.store(false);
    m_connected.store(false);
    m_abortQueryWait.store(true);
    m_queue.wakeAll();
    m_responseCv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (m_asyncWatchdog) {
        m_asyncWatchdog->shutdown();
        m_asyncWatchdog.reset();
    }

    if (m_transport) {
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
        m_transport->close();
    }

    m_rxGate.reset();
    markDisconnected();
}

bool PelcoDSession::isConnected() const noexcept
{
    return m_connected.load(std::memory_order_relaxed);
}

void PelcoDSession::setAddress(std::uint8_t address) noexcept
{
    m_address.store(address, std::memory_order_relaxed);
}

std::uint8_t PelcoDSession::getAddress() const noexcept
{
    return m_address.load(std::memory_order_relaxed);
}

SendResult PelcoDSession::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    return enqueueCommand(frame, "", CommandPriority::Normal);
}

SendResult PelcoDSession::sendQueryFrame(const std::vector<std::uint8_t>& frame, const std::string& queryTag)
{
    return enqueueCommand(frame, queryTag, CommandPriority::Low);
}

SendResult PelcoDSession::enqueueCommand(
    const std::vector<std::uint8_t>& frame, const std::string& queryTag, CommandPriority priority)
{
    if (frame.empty()) {
        if (!queryTag.empty()) {
            notifyQueryFailed(queryTag);
        }
        return SendResult::InvalidFrame;
    }

    std::uint64_t motionGen { 0U };
    if (PelcoDFrame::isStandardMotion(frame)) {
        motionGen = ++m_motionGeneration;
    } else if (PelcoDFrame::isStandardStop(frame)) {
        motionGen = ++m_motionGeneration;
    }

    m_queue.enqueue(frame, queryTag, priority, motionGen);

    if (priority == CommandPriority::Urgent && m_awaitingResponse.load()) {
        m_abortQueryWait.store(true, std::memory_order_relaxed);
        m_responseCv.notify_all();
    }

    return SendResult::Success;
}

bool PelcoDSession::setFrameExt(std::shared_ptr<IFrameExtension> ext)
{
    std::scoped_lock lock(m_lifecycleMutex);
    if (m_running.load()) {
        return false;
    }
    m_frameExt = std::move(ext);
    return true;
}

void PelcoDSession::resolveQueryWait()
{
    {
        std::scoped_lock lock(m_sessionMutex);
        m_awaitingResponse.store(false, std::memory_order_relaxed);
        m_pendingQueryTag.clear();
        m_pendingLimitId = std::nullopt;
    }
    m_responseCv.notify_all();
}

void PelcoDSession::setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept
{
    m_queryTimeoutMs.store(timeoutMs, std::memory_order_relaxed);
}

std::uint32_t PelcoDSession::getQueryTimeoutMs() const noexcept
{
    return m_queryTimeoutMs.load(std::memory_order_relaxed);
}

void PelcoDSession::setRetryConfig(const RetryConfig& config) noexcept
{
    std::scoped_lock lock(m_retryMutex);
    m_retryConfig = config;
}

RetryConfig PelcoDSession::getRetryConfig() const noexcept
{
    std::scoped_lock lock(m_retryMutex);
    return m_retryConfig;
}

void PelcoDSession::setFrameHandler(FrameHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_frameHandler = std::move(handler);
}

void PelcoDSession::setTrafficHandler(TrafficHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_trafficHandler = std::move(handler);
}

void PelcoDSession::setTimeoutHandler(TimeoutHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_timeoutHandler = std::move(handler);
}

void PelcoDSession::setQueryFailedHandler(QueryFailedHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_queryFailedHandler = std::move(handler);
}

void PelcoDSession::setRetryHandler(RetryHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_retryHandler = std::move(handler);
}

void PelcoDSession::setStateHandler(StateHandler handler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_stateHandler = std::move(handler);
}

void PelcoDSession::setPollStepHandler(PollStepHandler handler, PollDeadlineHandler deadlineHandler)
{
    std::scoped_lock lock(m_handlerMutex);
    m_pollStepHandler = std::move(handler);
    m_pollDeadlineHandler = std::move(deadlineHandler);
}

void PelcoDSession::setStatusProvider(StatusProvider provider)
{
    std::scoped_lock lock(m_handlerMutex);
    m_statusProvider = std::move(provider);
}

bool PelcoDSession::hasLowPriorityPending() const noexcept
{
    return m_queue.hasLowPriorityPending();
}

void PelcoDSession::wakeQueue() noexcept
{
    m_queue.wakeAll();
}

void PelcoDSession::setPendingLimitId(std::optional<EverestLimitId> limitId) noexcept
{
    std::scoped_lock lock(m_sessionMutex);
    m_pendingLimitId = limitId;
}

std::optional<EverestLimitId> PelcoDSession::getPendingLimitId() const noexcept
{
    std::scoped_lock lock(m_sessionMutex);
    return m_pendingLimitId;
}

Transport::TransportStatsSnapshot PelcoDSession::getTransportStats() const
{
    if (m_transport) {
        return m_transport->getStats();
    }
    return Transport::TransportStatsSnapshot {};
}

void PelcoDSession::markConnected()
{
    const bool wasConnected = m_connected.exchange(true, std::memory_order_relaxed);
    if (wasConnected) {
        return;
    }
    m_rxAccumulator.clear();
    m_queue.wakeAll();
    StateHandler stateHandler;
    {
        std::scoped_lock lock(m_handlerMutex);
        stateHandler = m_stateHandler;
    }
    if (stateHandler) {
        stateHandler(TransportState::Connected, "Connected");
    }
}

void PelcoDSession::markDisconnected()
{
    const bool wasConnected = m_connected.exchange(false, std::memory_order_relaxed);
    if (!wasConnected) {
        return;
    }
    resolveQueryWait();
    StateHandler stateHandler;
    {
        std::scoped_lock lock(m_handlerMutex);
        stateHandler = m_stateHandler;
    }
    if (stateHandler) {
        stateHandler(TransportState::Disconnected, "Disconnected");
    }
}

void PelcoDSession::onTransportState(TransportState state, const std::string& /* msg */)
{
    if (!m_running.load(std::memory_order_relaxed)) {
        return;
    }
    if (state == TransportState::Connected) {
        markConnected();
    } else if (state == TransportState::Disconnected || state == TransportState::Error) {
        markDisconnected();
    }
}

void PelcoDSession::onDataReceived(const std::vector<std::uint8_t>& data)
{
    const RxFrameExpectation expectation = (m_awaitingResponse.load() || m_frameExt != nullptr)
        ? RxFrameExpectation::AwaitingQuery
        : RxFrameExpectation::StandardOnly;
    const auto frames = m_rxAccumulator.push(data, expectation);
    for (const auto& frame : frames) {
        dispatchFrame(frame);
    }
    if (m_rxAccumulator.size() > 0U) {
        m_queue.wakeAll();
        m_responseCv.notify_all();
    }
}

bool PelcoDSession::isResponseMatchingQuery(
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

void PelcoDSession::notifyQueryFailed(const std::string& queryTag, std::chrono::microseconds durationUs)
{
    QueryFailedHandler handler;
    {
        std::scoped_lock lock(m_handlerMutex);
        handler = m_queryFailedHandler;
    }
    if (handler) {
        handler(queryTag, durationUs);
    }
}

void PelcoDSession::checkQueryTimeout()
{
    std::string timedOutTag {};
    std::chrono::microseconds durationUs { 0 };
    {
        std::scoped_lock lock(m_sessionMutex);
        if (m_awaitingResponse.load()) {
            timedOutTag = m_pendingQueryTag;
            durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - m_querySentTime);
            m_awaitingResponse.store(false, std::memory_order_relaxed);
            m_pendingQueryTag.clear();
            m_pendingLimitId = std::nullopt;
        }
    }
    if (!timedOutTag.empty()) {
        TimeoutHandler tHandler;
        QueryFailedHandler fHandler;
        {
            std::scoped_lock lock(m_handlerMutex);
            tHandler = m_timeoutHandler;
            fHandler = m_queryFailedHandler;
        }
        if (tHandler) {
            tHandler(timedOutTag);
        }
        if (fHandler) {
            fHandler(timedOutTag, durationUs);
        }
    }
}

void PelcoDSession::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    if (m_frameExt && m_frameExt->onFrame(frame)) {
        resolveQueryWait();
        StatusProvider sProvider;
        {
            std::scoped_lock lock(m_handlerMutex);
            sProvider = m_statusProvider;
        }
        if (sProvider) {
            m_frameExt->publish(sProvider());
        }
    }

    TrafficHandler tHandler;
    FrameHandler fHandler;
    {
        std::scoped_lock lock(m_handlerMutex);
        tHandler = m_trafficHandler;
        fHandler = m_frameHandler;
    }

    if (tHandler) {
        tHandler(false, frame);
    }

    if (frame.size() < 2U) {
        return;
    }

    if (frame[1] != m_address.load(std::memory_order_relaxed)) {
        return;
    }

    std::string satisfiedTag {};
    std::chrono::microseconds durationUs { 0 };
    std::optional<EverestLimitId> limitId {};

    {
        std::scoped_lock lock(m_sessionMutex);
        if (m_awaitingResponse.load()) {
            const bool matches = isResponseMatchingQuery(m_pendingQueryTag, frame);
            const bool isUnsolicited = (frame.size() == PelcoDFrame::GeneralResponseSize)
                || (frame.size() == PelcoDFrame::StandardFrameSize
                    && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::StandardExtended));

            if (!matches && !isUnsolicited) {
                return;
            }

            if (matches) {
                satisfiedTag = m_pendingQueryTag;
                durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - m_querySentTime);
                limitId = m_pendingLimitId;
                m_awaitingResponse.store(false, std::memory_order_relaxed);
                m_pendingQueryTag.clear();
                m_pendingLimitId = std::nullopt;
                m_responseCv.notify_all();
            }
        }
    }

    if (fHandler) {
        fHandler(frame, satisfiedTag, durationUs, limitId);
    }
}

void PelcoDSession::workerLoop()
{
    while (m_running.load(std::memory_order_relaxed)) {
        checkQueryTimeout();
        const auto expiredFrames = m_rxAccumulator.flushExpired(RxFrameExpectation::AllowGeneralResponse);
        for (const auto& frame : expiredFrames) {
            dispatchFrame(frame);
        }

        PollStepHandler pollHandler;
        PollDeadlineHandler deadlineHandler;
        {
            std::scoped_lock lock(m_handlerMutex);
            pollHandler = m_pollStepHandler;
            deadlineHandler = m_pollDeadlineHandler;
        }
        if (pollHandler) {
            pollHandler();
        }

        const auto pollDeadline = deadlineHandler ? deadlineHandler() : std::chrono::steady_clock::time_point::max();

        CommandItem item {};
        const auto idleWaitTimeout
            = (m_rxAccumulator.size() > 0U) ? m_rxAccumulator.interByteTimeout() : std::chrono::milliseconds(50);

        const bool hasItem = m_queue.popReady(
            item, [this] { return !m_running.load(); }, pollDeadline, idleWaitTimeout);

        if (!m_running.load(std::memory_order_relaxed)) {
            break;
        }
        if (!hasItem || item.frame.empty()) {
            continue;
        }

        const auto sendStartTime = std::chrono::steady_clock::now();
        if (m_transport && m_transport->isOpen()) {
            if (!item.queryTag.empty()) {
                std::scoped_lock lock(m_sessionMutex);
                m_abortQueryWait.store(false, std::memory_order_relaxed);
                m_pendingQueryTag = item.queryTag;
                m_querySentTime = sendStartTime;
                m_awaitingResponse.store(true, std::memory_order_relaxed);
            }

            const bool sendSuccess = m_transport->sendData(item.frame);
            if (!sendSuccess) {
                if (!item.queryTag.empty()) {
                    std::scoped_lock lock(m_sessionMutex);
                    m_awaitingResponse.store(false, std::memory_order_relaxed);
                    m_pendingQueryTag.clear();
                    m_pendingLimitId = std::nullopt;
                }

                RetryConfig retryCfg = getRetryConfig();
                if (retryCfg.retryOnTransportError && item.retryCount < retryCfg.maxRetries) {
                    const std::string reason = "Transport transmission failed for "
                        + (item.queryTag.empty() ? "command" : "query '" + item.queryTag + "'");
                    const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                    if (backoffDelay.count() > 0) {
                        RetryHandler rHandler;
                        {
                            std::scoped_lock lock(m_handlerMutex);
                            rHandler = m_retryHandler;
                        }
                        if (rHandler) {
                            rHandler(item.queryTag.empty() ? "Command" : item.queryTag, item.retryCount + 1U,
                                retryCfg.maxRetries, backoffDelay);
                        }
                    }
                } else if (!item.queryTag.empty()) {
                    notifyQueryFailed(item.queryTag);
                }
            } else {
                TrafficHandler tHandler;
                {
                    std::scoped_lock lock(m_handlerMutex);
                    tHandler = m_trafficHandler;
                }
                if (tHandler) {
                    tHandler(true, item.frame);
                }

                if (!item.queryTag.empty()) {
                    bool queryAborted { false };
                    std::string abortedTag {};
                    std::chrono::microseconds abortedDurationUs { 0 };

                    {
                        std::unique_lock<std::mutex> lock(m_sessionMutex);
                        while (m_awaitingResponse.load() && m_running.load() && !m_abortQueryWait.load()) {
                            const auto nowTime = std::chrono::steady_clock::now();
                            const auto elapsed
                                = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - m_querySentTime);
                            const auto timeoutLimit = std::chrono::milliseconds(m_queryTimeoutMs.load());
                            if (elapsed >= timeoutLimit) {
                                break;
                            }
                            const auto remaining = timeoutLimit - elapsed;
                            const auto waitStep = (m_rxAccumulator.size() > 0U)
                                ? std::min(remaining, m_rxAccumulator.interByteTimeout())
                                : remaining;
                            m_responseCv.wait_for(lock, waitStep, [this] {
                                return !m_awaitingResponse.load() || !m_running.load() || m_abortQueryWait.load()
                                    || (m_rxAccumulator.size() > 0U);
                            });

                            if (m_rxAccumulator.size() > 0U) {
                                lock.unlock();
                                const auto expired
                                    = m_rxAccumulator.flushExpired(RxFrameExpectation::AllowGeneralResponse);
                                for (const auto& f : expired) {
                                    dispatchFrame(f);
                                }
                                lock.lock();
                            }
                        }

                        if (m_abortQueryWait.load()) {
                            queryAborted = true;
                            m_abortQueryWait.store(false, std::memory_order_relaxed);
                            abortedTag = m_pendingQueryTag;
                            abortedDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - m_querySentTime);
                            m_awaitingResponse.store(false, std::memory_order_relaxed);
                            m_pendingQueryTag.clear();
                            m_pendingLimitId = std::nullopt;
                        }
                    }

                    if (queryAborted && !abortedTag.empty()) {
                        notifyQueryFailed(abortedTag, abortedDurationUs);
                    } else if (m_awaitingResponse.load()) {
                        const RetryConfig retryCfg = getRetryConfig();
                        if (item.retryCount < retryCfg.maxRetries) {
                            resolveQueryWait();
                            const std::string reason = "Query '" + item.queryTag + "' timed out";
                            const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                            RetryHandler rHandler;
                            {
                                std::scoped_lock lock(m_handlerMutex);
                                rHandler = m_retryHandler;
                            }
                            if (rHandler) {
                                rHandler(item.queryTag, item.retryCount + 1U, retryCfg.maxRetries, backoffDelay);
                            }
                        } else {
                            checkQueryTimeout();
                        }
                    }
                }
            }
        }

        const auto commandEndTime = std::chrono::steady_clock::now();
        const auto elapsedMs
            = std::chrono::duration_cast<std::chrono::milliseconds>(commandEndTime - sendStartTime).count();
        if (elapsedMs < 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20 - elapsedMs));
        }
    }
}

} // namespace PelcoD
