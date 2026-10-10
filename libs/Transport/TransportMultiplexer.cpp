/// @file TransportMultiplexer.cpp
/// @brief Implementation of multi-channel transport multiplexer.

#include "TransportMultiplexer.h"

#include <algorithm>
#include <utility>

namespace Transport {

/// @class TransportMultiplexer::Channel
/// @brief Virtual transport endpoint bound to a parent TransportMultiplexer.
class TransportMultiplexer::Channel final : public ITransport {
public:
    Channel(std::shared_ptr<TransportMultiplexer> parent,
        std::shared_ptr<ITransport> underlying,
        std::uint64_t id)
        : m_parent(std::move(parent))
        , m_underlying(std::move(underlying))
        , m_id(id)
    {
    }

    ~Channel() override
    {
        if (auto p = m_parent.lock()) {
            p->unregisterChannel(m_id);
        }
    }

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;

    [[nodiscard]] bool open() override
    {
        if (m_underlying) {
            return m_underlying->open();
        }
        return false;
    }

    void close() override
    {
        // Channel-level close leaves underlying physical transport operational for other peers
        m_closed.store(true);
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        if (m_closed.load()) {
            return false;
        }
        return m_underlying ? m_underlying->isOpen() : false;
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        if (m_closed.load()) {
            return false;
        }
        if (auto p = m_parent.lock()) {
            return p->sendData(data);
        }
        return false;
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        std::scoped_lock lock(m_mutex);
        m_dataCallback = std::move(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::scoped_lock lock(m_mutex);
        m_stateCallback = std::move(callback);
    }

    bool setBaudRate(std::uint32_t baudRate) override
    {
        if (m_underlying) {
            return m_underlying->setBaudRate(baudRate);
        }
        return false;
    }

    [[nodiscard]] std::uint32_t getBaudRate() const noexcept override
    {
        return m_underlying ? m_underlying->getBaudRate() : 0U;
    }

    [[nodiscard]] TransportStatsSnapshot getStats() const override
    {
        return m_underlying ? m_underlying->getStats() : TransportStatsSnapshot {};
    }

    void resetStats() noexcept override
    {
        if (m_underlying) {
            m_underlying->resetStats();
        }
    }

    [[nodiscard]] std::uint64_t getId() const noexcept
    {
        return m_id;
    }

    [[nodiscard]] DataReceivedCallback getDataCallback() const
    {
        std::scoped_lock lock(m_mutex);
        return m_dataCallback;
    }

    [[nodiscard]] StateChangedCallback getStateCallback() const
    {
        std::scoped_lock lock(m_mutex);
        return m_stateCallback;
    }

private:
    std::weak_ptr<TransportMultiplexer> m_parent {};
    std::shared_ptr<ITransport> m_underlying {};
    std::uint64_t m_id { 0U };
    std::atomic<bool> m_closed { false };

    mutable std::mutex m_mutex {};
    DataReceivedCallback m_dataCallback {};
    StateChangedCallback m_stateCallback {};
};

TransportMultiplexer::TransportMultiplexer(std::shared_ptr<ITransport> underlying)
    : m_underlying(std::move(underlying))
{
}

TransportMultiplexer::~TransportMultiplexer()
{
    if (m_underlying) {
        m_underlying->setDataCallback(nullptr);
        m_underlying->setStateCallback(nullptr);
    }
}

void TransportMultiplexer::initialize()
{
    std::scoped_lock lock(m_channelsMutex);
    if (m_initialized || !m_underlying) {
        return;
    }
    m_initialized = true;

    auto weakSelf = std::weak_ptr<TransportMultiplexer>(shared_from_this());

    m_underlying->setDataCallback([weakSelf](const std::vector<std::uint8_t>& data) {
        if (auto self = weakSelf.lock()) {
            self->onDataReceived(data);
        }
    });

    m_underlying->setStateCallback([weakSelf](TransportState state, const std::string& errorMsg) {
        if (auto self = weakSelf.lock()) {
            self->onStateChanged(state, errorMsg);
        }
    });
}

std::shared_ptr<ITransport> TransportMultiplexer::createChannel()
{
    initialize();

    std::shared_ptr<Channel> channel;
    {
        std::scoped_lock lock(m_channelsMutex);
        const auto channelId = m_nextChannelId++;
        channel = std::make_shared<Channel>(shared_from_this(), m_underlying, channelId);
        m_channels.push_back(channel);
    }
    return channel;
}

std::size_t TransportMultiplexer::channelCount() const
{
    std::scoped_lock lock(m_channelsMutex);
    std::size_t count { 0U };
    for (const auto& weakCh : m_channels) {
        if (!weakCh.expired()) {
            ++count;
        }
    }
    return count;
}

std::shared_ptr<ITransport> TransportMultiplexer::getUnderlying() const
{
    return m_underlying;
}

bool TransportMultiplexer::sendData(const std::vector<std::uint8_t>& data)
{
    std::scoped_lock lock(m_writeMutex);
    if (!m_underlying) {
        return false;
    }
    return m_underlying->sendData(data);
}

void TransportMultiplexer::registerChannel(const std::shared_ptr<Channel>& channel)
{
    if (!channel) {
        return;
    }
    std::scoped_lock lock(m_channelsMutex);
    m_channels.push_back(channel);
}

void TransportMultiplexer::unregisterChannel(std::uint64_t channelId)
{
    std::scoped_lock lock(m_channelsMutex);
    m_channels.erase(
        std::remove_if(m_channels.begin(), m_channels.end(),
            [channelId](const std::weak_ptr<Channel>& wp) {
                if (auto ch = wp.lock()) {
                    return ch->getId() == channelId;
                }
                return true;
            }),
        m_channels.end());
}

void TransportMultiplexer::onDataReceived(const std::vector<std::uint8_t>& data)
{
    std::vector<ITransport::DataReceivedCallback> callbacks;
    {
        std::scoped_lock lock(m_channelsMutex);
        for (auto it = m_channels.begin(); it != m_channels.end();) {
            if (auto ch = it->lock()) {
                auto cb = ch->getDataCallback();
                if (cb) {
                    callbacks.push_back(std::move(cb));
                }
                ++it;
            } else {
                it = m_channels.erase(it);
            }
        }
    }

    for (const auto& cb : callbacks) {
        cb(data);
    }
}

void TransportMultiplexer::onStateChanged(TransportState state, const std::string& errorMsg)
{
    std::vector<ITransport::StateChangedCallback> callbacks;
    {
        std::scoped_lock lock(m_channelsMutex);
        for (auto it = m_channels.begin(); it != m_channels.end();) {
            if (auto ch = it->lock()) {
                auto cb = ch->getStateCallback();
                if (cb) {
                    callbacks.push_back(std::move(cb));
                }
                ++it;
            } else {
                it = m_channels.erase(it);
            }
        }
    }

    for (const auto& cb : callbacks) {
        cb(state, errorMsg);
    }
}

} // namespace Transport
