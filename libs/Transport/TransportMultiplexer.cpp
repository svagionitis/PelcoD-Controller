/// @file TransportMultiplexer.cpp
/// @brief Implementation of multi-channel transport multiplexer.

#include "TransportMultiplexer.h"

#include <cstdint>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <glog/logging.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Transport {

namespace {

    /// @class ChannelGate
    /// @brief Admission gate guarding virtual channel callbacks during teardown.
    class ChannelGate final {
    public:
        ChannelGate() noexcept = default;
        ~ChannelGate() = default;

        ChannelGate(const ChannelGate&) = delete;
        ChannelGate& operator=(const ChannelGate&) = delete;
        ChannelGate(ChannelGate&&) = delete;
        ChannelGate& operator=(ChannelGate&&) = delete;

        class Pass final {
        public:
            explicit Pass(ChannelGate& gate) noexcept
                : m_gate(gate)
            {
                m_entered = m_gate.enter();
            }

            ~Pass()
            {
                if (m_entered) {
                    m_gate.leave();
                }
            }

            Pass(const Pass&) = delete;
            Pass& operator=(const Pass&) = delete;
            Pass(Pass&&) = delete;
            Pass& operator=(Pass&&) = delete;

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return m_entered;
            }

        private:
            ChannelGate& m_gate;
            bool m_entered { false };
        };

        void close() noexcept
        {
            m_state.fetch_or(kClosedBit, std::memory_order_acq_rel);
            while ((m_state.load(std::memory_order_acquire) & kCountMask) > 0U) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        [[nodiscard]] bool isClosed() const noexcept
        {
            return (m_state.load(std::memory_order_acquire) & kClosedBit) != 0U;
        }

    private:
        [[nodiscard]] bool enter() noexcept
        {
            std::uint32_t current = m_state.load(std::memory_order_relaxed);
            while (true) {
                if ((current & kClosedBit) != 0U) {
                    return false;
                }
                if ((current & kCountMask) >= kCountMask) {
                    return false;
                }
                if (m_state.compare_exchange_weak(
                        current, current + 1U, std::memory_order_acquire, std::memory_order_relaxed)) {
                    return true;
                }
            }
        }

        void leave() noexcept
        {
            m_state.fetch_sub(1U, std::memory_order_release);
        }

        static constexpr std::uint32_t kClosedBit { 0x80000000U };
        static constexpr std::uint32_t kCountMask { 0x7FFFFFFFU };
        std::atomic<std::uint32_t> m_state { 0U };
    };

} // namespace

/// @class TransportMultiplexer::Channel
/// @brief Virtual transport endpoint bound to a parent TransportMultiplexer.
class TransportMultiplexer::Channel final : public ITransport {
public:
    Channel(std::shared_ptr<TransportMultiplexer> parent, std::shared_ptr<ITransport> underlying, std::uint64_t id)
        : m_parent(std::move(parent))
        , m_underlying(std::move(underlying))
        , m_id(id)
    {
    }

    ~Channel() override
    {
        close();
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
        std::shared_ptr<ChannelGate> gate;
        {
            std::scoped_lock lock(m_mutex);
            gate = m_gate;
        }
        if (gate) {
            gate->close();
        }
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
        std::shared_ptr<ChannelGate> oldGate;
        {
            std::scoped_lock lock(m_mutex);
            oldGate = m_gate;
            m_dataCallback = std::move(callback);
            m_gate = std::make_shared<ChannelGate>();
        }
        if (oldGate) {
            oldGate->close();
        }
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::shared_ptr<ChannelGate> oldGate;
        {
            std::scoped_lock lock(m_mutex);
            oldGate = m_gate;
            m_stateCallback = std::move(callback);
            m_gate = std::make_shared<ChannelGate>();
        }
        if (oldGate) {
            oldGate->close();
        }
    }

    void deliverData(const std::vector<std::uint8_t>& data)
    {
        if (m_closed.load()) {
            return;
        }
        DataReceivedCallback cb;
        std::shared_ptr<ChannelGate> gate;
        {
            std::scoped_lock lock(m_mutex);
            cb = m_dataCallback;
            gate = m_gate;
        }
        if (!cb || !gate) {
            return;
        }
        const ChannelGate::Pass pass(*gate);
        if (!pass) {
            return;
        }
        try {
            cb(data);
        } catch (const std::exception& e) {
            LOG(WARNING) << "TransportMultiplexer: exception in channel data callback: " << e.what();
        } catch (...) {
            LOG(WARNING) << "TransportMultiplexer: unknown exception in channel data callback";
        }
    }

    void deliverState(TransportState state, const std::string& errorMsg)
    {
        if (m_closed.load()) {
            return;
        }
        StateChangedCallback cb;
        std::shared_ptr<ChannelGate> gate;
        {
            std::scoped_lock lock(m_mutex);
            cb = m_stateCallback;
            gate = m_gate;
        }
        if (!cb || !gate) {
            return;
        }
        const ChannelGate::Pass pass(*gate);
        if (!pass) {
            return;
        }
        try {
            cb(state, errorMsg);
        } catch (const std::exception& e) {
            LOG(WARNING) << "TransportMultiplexer: exception in channel state callback: " << e.what();
        } catch (...) {
            LOG(WARNING) << "TransportMultiplexer: unknown exception in channel state callback";
        }
    }

    bool setBaudRate(std::uint32_t baudRate) override
    {
        if (auto p = m_parent.lock()) {
            if (p->channelCount() > 1U) {
                // Reject baud rate modification when multiplexed across multiple active channels (M11)
                return false;
            }
        }
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
        return m_underlying ? m_underlying->getStats() : TransportStatsSnapshot { };
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

private:
    std::weak_ptr<TransportMultiplexer> m_parent { };
    std::shared_ptr<ITransport> m_underlying { };
    std::uint64_t m_id { 0U };
    std::atomic<bool> m_closed { false };

    mutable std::mutex m_mutex { };
    DataReceivedCallback m_dataCallback { };
    StateChangedCallback m_stateCallback { };
    std::shared_ptr<ChannelGate> m_gate { std::make_shared<ChannelGate>() };
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
    m_channels.erase(std::remove_if(m_channels.begin(), m_channels.end(),
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
    std::vector<std::shared_ptr<Channel>> activeChannels;
    {
        std::scoped_lock lock(m_channelsMutex);
        for (auto it = m_channels.begin(); it != m_channels.end();) {
            if (auto ch = it->lock()) {
                activeChannels.push_back(std::move(ch));
                ++it;
            } else {
                it = m_channels.erase(it);
            }
        }
    }

    for (const auto& ch : activeChannels) {
        ch->deliverData(data);
    }
}

void TransportMultiplexer::onStateChanged(TransportState state, const std::string& errorMsg)
{
    std::vector<std::shared_ptr<Channel>> activeChannels;
    {
        std::scoped_lock lock(m_channelsMutex);
        for (auto it = m_channels.begin(); it != m_channels.end();) {
            if (auto ch = it->lock()) {
                activeChannels.push_back(std::move(ch));
                ++it;
            } else {
                it = m_channels.erase(it);
            }
        }
    }

    for (const auto& ch : activeChannels) {
        ch->deliverState(state, errorMsg);
    }
}

} // namespace Transport
