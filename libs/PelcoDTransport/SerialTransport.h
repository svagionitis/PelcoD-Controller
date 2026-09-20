#pragma once

/// @file SerialTransport.h
/// @brief Cross-platform serial port transport for Linux (termios) and Windows (Win32 API).

#include "BaseTransport.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
using SerialHandle = HANDLE;
#define INVALID_SERIAL_HANDLE INVALID_HANDLE_VALUE
#else
using SerialHandle = int;
#define INVALID_SERIAL_HANDLE (-1)
#endif

namespace PelcoD::Transport {

/// @class SerialTransport
/// @brief Cross-platform thread-safe serial transport (zero Qt dependency).
class SerialTransport : public BaseTransport {
public:
    explicit SerialTransport(std::string portName = "", std::uint32_t baudRate = 9600U);
    ~SerialTransport() override;

    // Non-copyable, non-movable
    SerialTransport(const SerialTransport&) = delete;
    SerialTransport& operator=(const SerialTransport&) = delete;
    SerialTransport(SerialTransport&&) = delete;
    SerialTransport& operator=(SerialTransport&&) = delete;

    void setPortName(const std::string& portName);
    [[nodiscard]] std::string getPortName() const;

    /// @brief Configures serial communication baud rate.
    /// @param[in] baudRate Baud rate in bits per second.
    /// @return True if baud rate is supported and applied, false otherwise.
    bool setBaudRate(std::uint32_t baudRate) override;
    [[nodiscard]] std::uint32_t getBaudRate() const noexcept override;

    /// @brief Standard RS-485 serial communication baud rates supported by Pelco-D devices.
    static constexpr std::array<std::uint32_t, 7> StandardBaudRates { 2400U, 4800U, 9600U, 19200U, 38400U, 57600U,
        115200U };

    /// @brief Validates if a given baud rate is a recognized standard rate.
    /// @param[in] baudRate Baud rate in bits per second.
    /// @return True if supported by the transport.
    [[nodiscard]] static constexpr bool isValidBaudRate(std::uint32_t baudRate) noexcept
    {
        for (const auto rate : StandardBaudRates) {
            if (rate == baudRate) {
                return true;
            }
        }
        return false;
    }

    /// @brief Discovers and enumerates available hardware and virtual serial communication ports.
    /// @details Scans active Windows Registry serial device mappings and MS-DOS devices on Windows,
    /// or standard /dev device nodes on POSIX/Linux/macOS. Returns naturally sorted port names.
    /// @return Naturally sorted list of detected serial port names (e.g. "COM1", "COM2", "/dev/ttyUSB0").
    [[nodiscard]] static std::vector<std::string> enumeratePorts();

    // ITransport interface
    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

private:
    void readWorker();
    bool configurePort();

    std::string m_portName;
    std::atomic<std::uint32_t> m_baudRate { 9600U };
    std::atomic<SerialHandle> m_handle { INVALID_SERIAL_HANDLE };
};

} // namespace PelcoD::Transport

namespace PelcoD {
using Transport::SerialTransport;
} // namespace PelcoD
