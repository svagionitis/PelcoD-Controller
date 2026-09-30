#pragma once

/// @file SightlineUdpTransport.h
/// @brief Dual-port UDP socket transport tailored for Sightline SLA protocols (Linux & Windows).

#include "BaseTransport.h"
#include "SocketUtils.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Transport {

/// @class SightlineUdpTransport
/// @brief Dual-port cross-platform UDP transport tailored for Sightline SLA video processors.
/// @details Transmits commands to remote commandPort (default 14001), while concurrently
/// listening on local replyPort (default 14002) for replies and high-rate telemetry.
class SightlineUdpTransport : public BaseTransport {
public:
    /// @brief Constructs a dual-port Sightline UDP transport.
    /// @param[in] host Destination camera or video processor IP address.
    /// @param[in] commandPort Destination command UDP port (default 14001).
    /// @param[in] replyPort Local listening UDP port for replies and telemetry (default 14002).
    explicit SightlineUdpTransport(
        std::string host = "192.168.1.100", std::uint16_t commandPort = 14001U, std::uint16_t replyPort = 14002U);
    ~SightlineUdpTransport() override;

    SightlineUdpTransport(const SightlineUdpTransport&) = delete;
    SightlineUdpTransport& operator=(const SightlineUdpTransport&) = delete;
    SightlineUdpTransport(SightlineUdpTransport&&) = delete;
    SightlineUdpTransport& operator=(SightlineUdpTransport&&) = delete;

    /// @brief Sets destination remote host address.
    /// @param[in] host Host name or IPv4 string.
    void setHost(const std::string& host);

    /// @brief Retrieves the destination remote host address.
    /// @return Host name or IP string.
    [[nodiscard]] std::string getHost() const;

    /// @brief Sets destination command port.
    /// @param[in] port Command port (e.g. 14001 or 14003).
    void setCommandPort(std::uint16_t port);

    /// @brief Retrieves destination command port.
    /// @return Remote command port.
    [[nodiscard]] std::uint16_t getCommandPort() const noexcept;

    /// @brief Sets local listening reply port.
    /// @param[in] port Local reply port (e.g. 14002 or 14003).
    void setReplyPort(std::uint16_t port);

    /// @brief Retrieves local listening reply port.
    /// @return Local reply port.
    [[nodiscard]] std::uint16_t getReplyPort() const noexcept;

    // ITransport interface

    /// @brief Opens the dual UDP sockets (TX client and RX listener).
    /// @return True if sockets were bound and initialized.
    [[nodiscard]] bool open() override;

    /// @brief Closes both UDP sockets and terminates worker threads.
    void close() override;

    /// @brief Checks whether the transport sockets are open and active.
    [[nodiscard]] bool isOpen() const noexcept override;

    /// @brief Transmits datagram to destination host and command port.
    /// @param[in] data Serialized packet bytes.
    /// @return True if transmission succeeded.
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

    /// @brief Captures transport statistics including socket queue depths.
    [[nodiscard]] TransportStatsSnapshot getStats() const override;

private:
    [[nodiscard]] UdpKernelStats queryKernelStats() const noexcept;
    void readWorker();

    std::string m_host;
    std::uint16_t m_commandPort { 14001U };
    std::uint16_t m_replyPort { 14002U };

    using SocketHandle = Net::SocketHandle;
    static constexpr SocketHandle InvalidSocket { Net::InvalidSocket };

    std::atomic<SocketHandle> m_txSock { InvalidSocket };
    std::atomic<SocketHandle> m_rxSock { InvalidSocket };

    struct sockaddr_storage m_destAddr { };
    socklen_t m_destAddrLen { 0 };
};

} // namespace Transport
