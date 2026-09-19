#pragma once

/// @file ITransport.h
/// @brief Abstract transport interface for raw Pelco-D communications (zero Qt dependency).

#include "PelcoDTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace PelcoD {

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
};

} // namespace PelcoD
