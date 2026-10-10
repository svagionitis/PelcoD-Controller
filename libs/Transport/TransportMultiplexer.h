#pragma once

/// @file TransportMultiplexer.h
/// @brief Multi-channel RX fan-out and shared TX multiplexer for ITransport.

#include "ITransport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Transport {

/// @class TransportMultiplexer
/// @brief Multiplexes a single underlying transport across multiple virtual channel subscribers.
/// @details Enables concurrent protocol sessions (e.g. PelcoDDevice and BusScanner) to share a
///          single physical transport without clashing or overwriting RX callbacks. Transmitted
///          data is serialized thread-safely across all channels, and incoming RX data is fanned
///          out to all registered subscribers.
class TransportMultiplexer final : public std::enable_shared_from_this<TransportMultiplexer> {
public:
    /// @brief Construct a TransportMultiplexer wrapping an underlying transport.
    /// @param[in] underlying Shared pointer to physical or virtual transport to multiplex.
    explicit TransportMultiplexer(std::shared_ptr<ITransport> underlying);

    /// @brief Destructor detaching underlying transport callbacks.
    ~TransportMultiplexer();

    // Non-copyable, non-movable
    TransportMultiplexer(const TransportMultiplexer&) = delete;
    TransportMultiplexer& operator=(const TransportMultiplexer&) = delete;
    TransportMultiplexer(TransportMultiplexer&&) = delete;
    TransportMultiplexer& operator=(TransportMultiplexer&&) = delete;

    /// @brief Creates and attaches underlying callbacks if not already attached.
    void initialize();

    /// @brief Creates an independent virtual ITransport channel.
    /// @return Shared pointer to a virtual ITransport channel.
    [[nodiscard]] std::shared_ptr<ITransport> createChannel();

    /// @brief Queries number of active virtual channels.
    /// @return Active channel count.
    [[nodiscard]] std::size_t channelCount() const;

    /// @brief Retrieves the wrapped physical transport instance.
    /// @return Shared pointer to underlying ITransport.
    [[nodiscard]] std::shared_ptr<ITransport> getUnderlying() const;

private:
    class Channel;

    void onDataReceived(const std::vector<std::uint8_t>& data);
    void onStateChanged(TransportState state, const std::string& errorMsg);

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data);
    void registerChannel(const std::shared_ptr<Channel>& channel);
    void unregisterChannel(std::uint64_t channelId);

    std::shared_ptr<ITransport> m_underlying {};
    mutable std::mutex m_writeMutex {};

    mutable std::mutex m_channelsMutex {};
    std::vector<std::weak_ptr<Channel>> m_channels {};
    std::uint64_t m_nextChannelId { 1U };
    bool m_initialized { false };
};

} // namespace Transport
