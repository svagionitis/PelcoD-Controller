#include "PelcoDSession.h"
#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <glog/logging.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PelcoD {

PelcoDSession::PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : PelcoDSession(std::move(transport), address, SessionHandlers { })
{
}

PelcoDSession::PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address, SessionHandlers handlers)
    : m_transport { std::move(transport) }
    , m_address { address }
    , m_asyncWatchdog { std::make_unique<AsyncWatchdog>() }
    , m_handlers { std::move(handlers) }
{
    m_queue.setEvictionHandler([this](const CommandItem& evicted) {
        if (isQueryExpected(evicted.queryKind) || !evicted.queryTag.empty()) {
            const std::string tag
                = evicted.queryTag.empty() ? std::string(toQueryTagName(evicted.queryKind)) : evicted.queryTag;
            notifyQueryFailed(tag);
        } else {
            notifyCommandDropped(evicted.frame, SendResult::QueueFull);
        }
    });
}

PelcoDSession::~PelcoDSession()
{
    stop();
}

std::shared_ptr<ITransport> PelcoDSession::activeTransport() const noexcept
{
    return m_sessionChannel ? m_sessionChannel : m_transport;
}

bool PelcoDSession::start()
{
    std::unique_lock<std::recursive_mutex> lock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }
    auto transport = activeTransport();
    if (!transport) {
        LOG(ERROR) << "PelcoDSession::start: No transport configured.";
        return false;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    auto rxGate = std::make_shared<CallbackGate>();
    m_rxGate = rxGate;

    try {
        if (!transport->isOpen()) {
            if (!transport->open()) {
                LOG(ERROR) << "PelcoDSession::start: Failed to open transport.";
                m_rxGate.reset();
                return false;
            }
        }

        bindTransport(transport, rxGate);

        m_running.store(true);
        m_connected.store(transport->isOpen());
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
        unbindTransport(transport);
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

    auto transport = activeTransport();

    if (transport && transport->isOpen()) {
        const auto stopFrame = ProtocolBuilder::buildStopFrame(m_address.load(std::memory_order_relaxed));
        static_cast<void>(transport->sendFrame(stopFrame));
    }

    if (!m_running.load() && !m_workerThread.joinable()) {
        if (transport) {
            if (transport->isOpen()) {
                transport->close();
            }
            unbindTransport(transport);
        }
        return;
    }

    m_running.store(false);
    m_connected.store(false);
    m_abortQueryWait.store(true);
    static_cast<void>(m_queue.purgeMotionCommands());
    m_queue.wakeAll();
    m_responseCv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (m_asyncWatchdog) {
        m_asyncWatchdog->shutdown();
        m_asyncWatchdog.reset();
    }

    if (transport) {
        unbindTransport(transport);
        transport->close();
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

SendResult PelcoDSession::sendRawFrame(const StandardFrame& frame)
{
    return enqueueCommand(frame, QueryRef { }, CommandPriority::Normal);
}

SendResult PelcoDSession::sendRawFrame(const std::vector<std::uint8_t>& frame)
{
    return enqueueCommand(frame, QueryRef { }, CommandPriority::Normal);
}

SendResult PelcoDSession::sendQueryFrame(const StandardFrame& frame, QueryRef query)
{
    return enqueueCommand(frame, std::move(query), CommandPriority::Low);
}

SendResult PelcoDSession::sendQueryFrame(const std::vector<std::uint8_t>& frame, QueryRef query)
{
    return enqueueCommand(frame, std::move(query), CommandPriority::Low);
}

SendResult PelcoDSession::enqueueCommand(const StandardFrame& frame, QueryRef query, CommandPriority priority)
{
    auto transport = activeTransport();
    if (!m_running.load() || !transport || !transport->isOpen()) {
        if (query.isQuery()) {
            notifyQueryFailed(query.tag);
        }
        return SendResult::TransportClosed;
    }

    if (!PelcoDFrame::isValidStandardFrame(frame)) {
        if (query.isQuery()) {
            notifyQueryFailed(query.tag);
        }
        return SendResult::InvalidFrame;
    }

    std::uint64_t motionGen { 0U };
    if (PelcoDFrame::isStandardMotion(frame) || PelcoDFrame::isStandardStop(frame)) {
        motionGen = ++m_motionGeneration;
    }

    const SendResult qResult = m_queue.enqueue(frame, query, priority, motionGen);
    if (qResult != SendResult::Success) {
        if (query.isQuery()) {
            notifyQueryFailed(query.tag);
        }
        return qResult;
    }

    if (priority == CommandPriority::Urgent && m_awaitingResponse.load()) {
        m_abortQueryWait.store(true, std::memory_order_relaxed);
        m_responseCv.notify_all();
    }

    return SendResult::Success;
}

SendResult PelcoDSession::enqueueCommand(
    const std::vector<std::uint8_t>& frame, QueryRef query, CommandPriority priority)
{
    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        StandardFrame stdFrame { };
        std::copy(frame.begin(), frame.end(), stdFrame.begin());
        return enqueueCommand(stdFrame, std::move(query), priority);
    }

    if (frame.empty() || !PelcoDFrame::isValidFrame(frame) || frame.size() > PelcoDFrame::StandardFrameSize) {
        if (query.isQuery()) {
            notifyQueryFailed(query.tag);
        }
        return SendResult::InvalidFrame;
    }

    StandardFrame stdFrame { };
    std::copy(frame.begin(), frame.end(), stdFrame.begin());
    return enqueueCommand(stdFrame, std::move(query), priority);
}

SendResult PelcoDSession::enqueueCommand(const StandardFrame& frame, CommandPriority priority)
{
    return enqueueCommand(frame, QueryRef { }, priority);
}

SendResult PelcoDSession::enqueueCommand(const std::vector<std::uint8_t>& frame, CommandPriority priority)
{
    return enqueueCommand(frame, QueryRef { }, priority);
}

std::size_t PelcoDSession::purgeMotionCommands() noexcept
{
    return m_queue.purgeMotionCommands();
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
        m_pendingQueryKind = QueryKind::None;
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

bool PelcoDSession::setHandlers(SessionHandlers handlers)
{
    std::unique_lock<std::recursive_mutex> lock(m_lifecycleMutex);
    if (m_running.load()) {
        return false;
    }
    m_handlers = std::move(handlers);
    return true;
}

std::size_t PelcoDSession::getPendingCommandCount() const noexcept
{
    return m_queue.size();
}

std::uint64_t PelcoDSession::getChecksumErrors() const noexcept
{
    return m_rxAccumulator.checksumErrors();
}

std::uint64_t PelcoDSession::getDiscardedBytes() const noexcept
{
    return m_rxAccumulator.discardedBytes();
}

void PelcoDSession::resetAccumulatorStats() noexcept
{
    m_rxAccumulator.resetStats();
}

void PelcoDSession::wakeQueue() noexcept
{
    m_queue.wakeAll();
}

bool PelcoDSession::hasLowPriorityPending() const noexcept
{
    return m_queue.hasLowPriorityPending();
}

void PelcoDSession::setPendingLimitId(std::optional<EverestLimitId> limitId) noexcept
{
    std::scoped_lock lock(m_sessionMutex);
    m_pendingLimitId = limitId;
}

Transport::TransportStatsSnapshot PelcoDSession::getTransportStats() const
{
    auto transport = activeTransport();
    if (transport) {
        return transport->getStats();
    }
    return Transport::TransportStatsSnapshot { };
}

void PelcoDSession::bindTransport(
    const std::shared_ptr<ITransport>& transport, const std::shared_ptr<CallbackGate>& gate)
{
    if (!transport) {
        return;
    }
    if (!gate) {
        transport->setDataCallback(nullptr);
        transport->setStateCallback(nullptr);
        return;
    }
    transport->setDataCallback([this, gate](const std::vector<std::uint8_t>& data) {
        const CallbackGate::Pass pass { *gate };
        if (pass) {
            onDataReceived(data);
        }
    });
    transport->setStateCallback([this, gate](TransportState state, const std::string& msg) {
        const CallbackGate::Pass pass { *gate };
        if (pass) {
            onTransportState(state, msg);
        }
    });
}

void PelcoDSession::unbindTransport(const std::shared_ptr<ITransport>& transport)
{
    if (transport) {
        transport->setDataCallback(nullptr);
        transport->setStateCallback(nullptr);
    }
}

std::shared_ptr<ITransport> PelcoDSession::createVirtualChannel()
{
    std::unique_lock<std::recursive_mutex> lifecycleLock { m_lifecycleMutex };
    if (!m_transport) {
        return nullptr;
    }
    if (!m_multiplexer) {
        m_multiplexer = std::make_shared<TransportMultiplexer>(m_transport);
        m_sessionChannel = m_multiplexer->createChannel();

        if (m_running.load()) {
            unbindTransport(m_transport);

            auto rxGate = m_rxGate;
            if (rxGate) {
                bindTransport(m_sessionChannel, rxGate);
            }
        }
    }
    return m_multiplexer->createChannel();
}

void PelcoDSession::markConnected()
{
    const bool wasConnected = m_connected.exchange(true, std::memory_order_relaxed);
    if (wasConnected) {
        return;
    }
    m_rxAccumulator.clear();
    m_queue.wakeAll();
    if (m_handlers.stateHandler) {
        m_handlers.stateHandler(TransportState::Connected, "Connected");
    }
}

void PelcoDSession::markDisconnected()
{
    const bool wasConnected = m_connected.exchange(false, std::memory_order_relaxed);
    if (!wasConnected) {
        return;
    }
    static_cast<void>(m_queue.purgeMotionCommands());
    resolveQueryWait();
    if (m_handlers.stateHandler) {
        m_handlers.stateHandler(TransportState::Disconnected, "Disconnected");
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
    QueryKind kind, const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept
{
    if (m_frameExt) {
        const ExtMatch verdict { m_frameExt->matchQuery(queryTag, frame) };
        if (verdict != ExtMatch::NotHandled) {
            return verdict == ExtMatch::Matched;
        }
    }
    if (kind != QueryKind::None && kind != QueryKind::Custom) {
        return ProtocolParser::isResponseMatchingQuery(kind, frame);
    }
    return ProtocolParser::isResponseMatchingQuery(queryTag, frame);
}

void PelcoDSession::notifyQueryFailed(const std::string& queryTag, std::chrono::microseconds durationUs)
{
    if (m_handlers.queryFailedHandler) {
        m_handlers.queryFailedHandler(queryTag, durationUs);
    }
}

void PelcoDSession::notifyCommandDropped(const StandardFrame& frame, SendResult reason)
{
    notifyCommandDropped(PelcoDFrame::toVector(frame), reason);
}

void PelcoDSession::notifyCommandDropped(const std::vector<std::uint8_t>& frame, SendResult reason)
{
    if (m_handlers.commandDroppedHandler) {
        m_handlers.commandDroppedHandler(frame, reason);
    }
}

void PelcoDSession::checkQueryTimeout()
{
    std::string timedOutTag { };
    std::chrono::microseconds durationUs { 0 };
    {
        std::scoped_lock lock(m_sessionMutex);
        if (m_awaitingResponse.load()) {
            timedOutTag = m_pendingQueryTag;
            durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - m_querySentTime);
            m_awaitingResponse.store(false, std::memory_order_relaxed);
            m_pendingQueryKind = QueryKind::None;
            m_pendingQueryTag.clear();
            m_pendingLimitId = std::nullopt;
        }
    }
    if (!timedOutTag.empty()) {
        if (m_handlers.timeoutHandler) {
            m_handlers.timeoutHandler(timedOutTag);
        }
        if (m_handlers.queryFailedHandler) {
            m_handlers.queryFailedHandler(timedOutTag, durationUs);
        }
    }
}

void PelcoDSession::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    if (m_frameExt && m_frameExt->onFrame(frame)) {
        resolveQueryWait();
        if (m_handlers.statusProvider) {
            m_frameExt->publish(m_handlers.statusProvider());
        }
    }

    if (m_handlers.trafficHandler) {
        m_handlers.trafficHandler(false, frame);
    }

    if (frame.size() < 2U) {
        return;
    }

    if (frame[1] != m_address.load(std::memory_order_relaxed)) {
        return;
    }

    std::string satisfiedTag { };
    std::chrono::microseconds durationUs { 0 };
    std::optional<EverestLimitId> limitId { };

    {
        std::scoped_lock lock(m_sessionMutex);
        if (m_awaitingResponse.load()) {
            const bool matches = isResponseMatchingQuery(m_pendingQueryKind, m_pendingQueryTag, frame);
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
                m_pendingQueryKind = QueryKind::None;
                m_pendingQueryTag.clear();
                m_pendingLimitId = std::nullopt;
                m_responseCv.notify_all();
            }
        }
    }

    if (m_handlers.frameHandler) {
        m_handlers.frameHandler(frame, satisfiedTag, durationUs, limitId);
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

        if (m_handlers.pollStepHandler) {
            m_handlers.pollStepHandler();
        }

        const auto pollDeadline = m_handlers.pollDeadlineHandler ? m_handlers.pollDeadlineHandler()
                                                                 : std::chrono::steady_clock::time_point::max();

        CommandItem item { };
        const auto idleWaitTimeout
            = (m_rxAccumulator.size() > 0U) ? m_rxAccumulator.interByteTimeout() : std::chrono::milliseconds(50);

        const bool hasItem
            = m_queue.popReady(item, [this] { return !m_running.load(); }, pollDeadline, idleWaitTimeout);

        if (!m_running.load(std::memory_order_relaxed)) {
            break;
        }
        if (!hasItem || item.frame[0] != PelcoDFrame::SyncByte) {
            continue;
        }

        const auto sendStartTime = std::chrono::steady_clock::now();
        auto transport = activeTransport();
        if (transport && transport->isOpen()) {
            const bool isQuery = isQueryExpected(item.queryKind) || !item.queryTag.empty();
            const std::string tagStr
                = item.queryTag.empty() ? std::string(toQueryTagName(item.queryKind)) : item.queryTag;
            if (isQuery) {
                std::scoped_lock lock(m_sessionMutex);
                m_abortQueryWait.store(false, std::memory_order_relaxed);
                m_pendingQueryKind = item.queryKind;
                m_pendingQueryTag = tagStr;
                m_querySentTime = sendStartTime;
                m_awaitingResponse.store(true, std::memory_order_relaxed);
            }

            const bool sendSuccess = transport->sendFrame(item.frame);
            if (!sendSuccess) {
                if (isQuery) {
                    std::scoped_lock lock(m_sessionMutex);
                    m_awaitingResponse.store(false, std::memory_order_relaxed);
                    m_pendingQueryKind = QueryKind::None;
                    m_pendingQueryTag.clear();
                    m_pendingLimitId = std::nullopt;
                }

                RetryConfig retryCfg = getRetryConfig();
                if (retryCfg.retryOnTransportError && item.retryCount < retryCfg.maxRetries) {
                    const std::string reason = "Transport transmission failed for "
                        + (tagStr.empty() ? "command" : "query '" + tagStr + "'");
                    const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                    if (backoffDelay.count() > 0) {
                        if (m_handlers.retryHandler) {
                            m_handlers.retryHandler(tagStr.empty() ? "Command" : tagStr, item.retryCount + 1U,
                                retryCfg.maxRetries, backoffDelay);
                        }
                    }
                } else if (isQuery) {
                    notifyQueryFailed(tagStr);
                } else {
                    notifyCommandDropped(item.frame,
                        retryCfg.retryOnTransportError ? SendResult::RetriesExhausted : SendResult::TransportClosed);
                }
            } else {
                if (m_handlers.trafficHandler) {
                    m_handlers.trafficHandler(true, PelcoDFrame::toVector(item.frame));
                }

                if (isQuery) {
                    bool queryAborted { false };
                    std::string abortedTag { };
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
                            m_pendingQueryKind = QueryKind::None;
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
                            const std::string reason = "Query '" + tagStr + "' timed out";
                            const auto backoffDelay = m_queue.scheduleRetry(item, retryCfg, reason);
                            if (backoffDelay.count() > 0) {
                                if (m_handlers.retryHandler) {
                                    m_handlers.retryHandler(
                                        tagStr, item.retryCount + 1U, retryCfg.maxRetries, backoffDelay);
                                }
                            }
                        } else {
                            checkQueryTimeout();
                            notifyCommandDropped(item.frame, SendResult::RetriesExhausted);
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
