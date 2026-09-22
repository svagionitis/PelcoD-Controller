#pragma once

/// @file TcpTransport.h
/// @brief Cross-platform TCP socket transport for Pelco-D over IP bridges (Linux & Windows).

#include "BaseTransport.h"
#include "SocketUtils.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Transport {

/// @class TcpTransport
/// @brief Standard TCP client socket implementation of ITransport (zero Qt dependency).
class TcpTransport : public BaseTransport {
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

    /// @brief Set the maximum time to wait for a TCP connect to complete.
    /// @param ms Timeout in milliseconds. Must be > 0. Default: 5000.
    void setConnectTimeout(int ms) noexcept;
    [[nodiscard]] int getConnectTimeout() const noexcept;

    // ITransport interface
    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

private:
    void readWorker();

    std::string m_host;
    std::uint16_t m_port { 4001U };
    int m_connectTimeoutMs { 5000 };

    using SocketHandle = Net::SocketHandle;
    static constexpr SocketHandle InvalidSocket { Net::InvalidSocket };

    std::atomic<SocketHandle> m_sockfd { InvalidSocket };
};

} // namespace Transport

namespace PelcoD {
namespace Transport = ::Transport;
using ::Transport::TcpTransport;
} // namespace PelcoD
