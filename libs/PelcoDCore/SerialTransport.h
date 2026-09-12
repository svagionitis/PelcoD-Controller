#pragma once

/// @file SerialTransport.h
/// @brief Cross-platform serial port transport for Linux (termios) and Windows (Win32 API).

#include "ITransport.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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

namespace PelcoD {

/// @class SerialTransport
/// @brief Cross-platform thread-safe serial transport (zero Qt dependency).
class SerialTransport : public ITransport {
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

    void setBaudRate(std::uint32_t baudRate);
    [[nodiscard]] std::uint32_t getBaudRate() const noexcept;

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
    void setDataCallback(DataReceivedCallback callback) override;
    void setStateCallback(StateChangedCallback callback) override;

private:
    void readWorker();
    void notifyState(TransportState state, const std::string& errorMsg);
    bool configurePort();

    std::string m_portName;
    std::uint32_t m_baudRate { 9600U };
    SerialHandle m_handle { INVALID_SERIAL_HANDLE };

    std::atomic<bool> m_running { false };
    std::thread m_readThread;

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;

    mutable std::mutex m_writeMutex;
};

} // namespace PelcoD
