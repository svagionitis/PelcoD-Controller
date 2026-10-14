#pragma once

/// @file ITransport.h
/// @brief Abstract transport interface for raw byte-oriented communications (zero Qt/protocol dependency).

#include "TransportStats.h"
#include "TransportTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Transport {

/// @class ITransport
/// @brief Pure virtual interface defining raw byte streaming and transport lifecycle.
class ITransport {
public:
    using DataReceivedCallback = std::function<void(const std::vector<std::uint8_t>& data)>;
    using StateChangedCallback = std::function<void(TransportState state, const std::string& errorMsg)>;

    virtual ~ITransport() = default;

    /// @brief Opens the underlying physical or virtual channel.
    /// @return True on success, false on failure.
    [[nodiscard]] virtual bool open() = 0;

    /// @brief Closes the channel and releases system handles.
    virtual void close() = 0;

    /// @brief Checks whether the channel is currently established and open.
    /// @return True if connected and ready for I/O.
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    /// @brief Transmits raw bytes across the medium.
    /// @param[in] data Byte buffer to send.
    /// @return True if write succeeded.
    [[nodiscard]] virtual bool sendData(const std::vector<std::uint8_t>& data) = 0;

    /// @brief Transmits raw bytes across the medium from a contiguous buffer.
    /// @param[in] data Pointer to raw byte sequence.
    /// @param[in] length Number of bytes to transmit.
    /// @return True if write succeeded; false otherwise.
    [[nodiscard]] virtual bool sendBytes(const std::uint8_t* data, std::size_t length)
    {
        if (data == nullptr || length == 0U) {
            return false;
        }
        const std::vector<std::uint8_t> vec(data, data + length);
        return sendData(vec);
    }

    /// @brief Transmits a fixed-size byte array without dynamic heap allocation.
    /// @tparam N Size of array buffer.
    /// @param[in] frame Array containing frame bytes.
    /// @return True if transmission succeeded.
    template <std::size_t N> [[nodiscard]] bool sendFrame(const std::array<std::uint8_t, N>& frame)
    {
        return sendBytes(frame.data(), frame.size());
    }

    /// @brief Registers callback for incoming bytes.
    /// @param[in] callback Function invoked when new data arrives.
    virtual void setDataCallback(DataReceivedCallback callback) = 0;

    /// @brief Registers callback for transport state changes.
    /// @param[in] callback Function invoked on connect/disconnect/error.
    virtual void setStateCallback(StateChangedCallback callback) = 0;

    /// @brief Configures transport baud rate if supported by underlying channel.
    /// @param[in] baudRate Baud rate in bits per second.
    /// @return True if rate was supported and applied, false otherwise.
    virtual bool setBaudRate(std::uint32_t /*baudRate*/)
    {
        return false;
    }

    /// @brief Retrieves current baud rate if supported by underlying channel.
    /// @return Current baud rate in bits per second, or 0 if unsupported.
    [[nodiscard]] virtual std::uint32_t getBaudRate() const noexcept
    {
        return 0U;
    }

    /// @brief Captures real-time transport telemetry and diagnostic counters.
    /// @return Aggregated snapshot containing generic and kernel-level metrics.
    [[nodiscard]] virtual TransportStatsSnapshot getStats() const
    {
        return {};
    }

    /// @brief Resets transport error and performance counters.
    virtual void resetStats() noexcept
    {
    }
};

} // namespace Transport
