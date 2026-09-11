#pragma once

/// @file TcpTransport.h
/// @brief Cross-platform TCP socket transport for Pelco-D over IP bridges (Linux & Windows).

#include "ITransport.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class TcpTransport
/// @brief Standard TCP client socket implementation of ITransport (zero Qt dependency).
class TcpTransport : public ITransport {
public:
    explicit TcpTransport(std::string host = "192.168.1.100", std::uint16_t port = 4001U);
    ~TcpTransport() override;

    // Non-copyable, non-movable
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    TcpTransport(TcpTransport&&) = delete;
    TcpTransport& operator=(TcpTransport&&) = delete;

    void setHost(const std::string& host);
    [[nodiscard]] std::string getHost() const;

    void setPort(std::uint16_t port);
    [[nodiscard]] std::uint16_t getPort() const noexcept;

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

    std::string m_host;
    std::uint16_t m_port { 4001U };

#ifdef _WIN32
    using SocketHandle = std::uintptr_t;
    static constexpr SocketHandle InvalidSocket { ~static_cast<SocketHandle>(0) };
#else
    using SocketHandle = int;
    static constexpr SocketHandle InvalidSocket { -1 };
#endif

    SocketHandle m_sockfd { InvalidSocket };

    std::atomic<bool> m_running { false };
    std::thread m_readThread;

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;

    mutable std::mutex m_writeMutex;
};

} // namespace PelcoD
