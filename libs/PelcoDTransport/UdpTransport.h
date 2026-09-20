#pragma once

/// @file UdpTransport.h
/// @brief Cross-platform UDP socket transport for Pelco-D over IP bridges (Linux & Windows).

#include "BaseTransport.h"
#include "SocketUtils.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD::Transport {

/// @class UdpTransport
/// @brief Standard UDP socket implementation of ITransport (zero Qt dependency).
class UdpTransport : public BaseTransport {
public:
    explicit UdpTransport(std::string host = "192.168.1.100", std::uint16_t port = 4001U, std::uint16_t localPort = 0U);
    ~UdpTransport() override;

    // Non-copyable, non-movable
    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;
    UdpTransport(UdpTransport&&) = delete;
    UdpTransport& operator=(UdpTransport&&) = delete;

    void setHost(const std::string& host);
    [[nodiscard]] std::string getHost() const;

    void setPort(std::uint16_t port);
    [[nodiscard]] std::uint16_t getPort() const noexcept;

    void setLocalPort(std::uint16_t localPort);
    [[nodiscard]] std::uint16_t getLocalPort() const noexcept;

    // ITransport interface
    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

private:
    void readWorker();

    std::string m_host;
    std::uint16_t m_port { 4001U };
    std::uint16_t m_localPort { 0U };

    using SocketHandle = Net::SocketHandle;
    static constexpr SocketHandle InvalidSocket { Net::InvalidSocket };

    std::atomic<SocketHandle> m_sockfd { InvalidSocket };
};

} // namespace PelcoD::Transport

namespace PelcoD {
using Transport::UdpTransport;
} // namespace PelcoD
