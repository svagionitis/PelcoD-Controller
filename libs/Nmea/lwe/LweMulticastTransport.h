#pragma once

#include "BaseTransport.h"
#include "LweTypes.h"
#include "SocketUtils.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Nmea::Lwe {

/// @class LweMulticastTransport
/// @brief Cross-platform IEC 61162-450 / Lightweight Ethernet (LWE) UDP Multicast Transport.
/// @details Implements Transport::ITransport, enabling NmeaDevice and NmeaSlavingBridge to receive
///          and transmit maritime NMEA 0183 sentences over standard multicast groups with Tag Blocks.
/// @note Thread-safe. Complies with IEC 61162-450 network specifications.
class LweMulticastTransport : public Transport::BaseTransport {
public:
    /// @brief Constructs an LweMulticastTransport bound to a standard IEC 61162-450 transmission group.
    /// @param[in] tg Standard Transmission Group (e.g. TransmissionGroup::Tgtd).
    /// @param[in] interfaceIp Local network interface IPv4 address ("0.0.0.0" for default).
    /// @param[in] systemId Local IEC 61162-450 System Identifier (e.g. "RA0001", "GP0001").
    explicit LweMulticastTransport(
        TransmissionGroup tg, std::string interfaceIp = "0.0.0.0", std::string systemId = "CC0001");

    /// @brief Constructs an LweMulticastTransport bound to an arbitrary multicast group and port.
    /// @param[in] groupAddress Multicast IPv4 address (e.g. "239.192.0.2").
    /// @param[in] port UDP port (e.g. 60002).
    /// @param[in] interfaceIp Local network interface IPv4 address ("0.0.0.0" for default).
    /// @param[in] systemId Local IEC 61162-450 System Identifier (e.g. "CC0001").
    LweMulticastTransport(std::string groupAddress, std::uint16_t port, std::string interfaceIp = "0.0.0.0",
        std::string systemId = "CC0001");

    /// @brief Destructor. Leaves multicast group and terminates worker thread.
    ~LweMulticastTransport() override;

    // Non-copyable, non-movable
    LweMulticastTransport(const LweMulticastTransport&) = delete;
    LweMulticastTransport& operator=(const LweMulticastTransport&) = delete;
    LweMulticastTransport(LweMulticastTransport&&) = delete;
    LweMulticastTransport& operator=(LweMulticastTransport&&) = delete;

    // --- Configuration Getters & Setters ---

    [[nodiscard]] std::string getGroupAddress() const;
    void setGroupAddress(const std::string& groupAddress);

    [[nodiscard]] std::uint16_t getPort() const noexcept;
    void setPort(std::uint16_t port) noexcept;

    [[nodiscard]] std::string getInterfaceIp() const;
    void setInterfaceIp(const std::string& interfaceIp);

    [[nodiscard]] std::string getSystemId() const;
    void setSystemId(const std::string& systemId);

    [[nodiscard]] std::uint8_t getMulticastTtl() const noexcept;
    void setMulticastTtl(std::uint8_t ttl) noexcept;

    [[nodiscard]] bool isLoopbackEnabled() const noexcept;
    void setLoopbackEnabled(bool enabled) noexcept;

    // --- ITransport Interface ---

    /// @brief Opens the UDP socket, configures multicast group membership, and spawns the RX worker thread.
    /// @return True if socket was bound and joined multicast group successfully, false otherwise.
    [[nodiscard]] bool open() override;

    /// @brief Drops multicast membership, closes socket, and terminates worker thread.
    void close() override;

    /// @brief Checks if the transport is open and actively receiving.
    /// @return True if open, false otherwise.
    [[nodiscard]] bool isOpen() const noexcept override;

    /// @brief Transmits raw bytes over the multicast group.
    /// @param[in] data Byte buffer to send.
    /// @return True if datagram sent successfully, false otherwise.
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

    /// @brief Formats an NMEA 0183 sentence with an IEC 61162-450 Tag Block and transmits via UDP multicast.
    /// @param[in] sentence Standard NMEA 0183 sentence (e.g. "$RATTM,...*3A").
    /// @param[in] attachTagBlock If true, prefixes sentence with `\\s:<systemId>,n:<seq>*hh\\`.
    /// @return True if transmitted successfully.
    [[nodiscard]] bool sendSentence(std::string_view sentence, bool attachTagBlock = true);

private:
    void rxWorkerLoop();

    using SocketHandle = Transport::Net::SocketHandle;
    static constexpr SocketHandle InvalidSocket { Transport::Net::InvalidSocket };

    std::string m_groupAddress {};
    std::uint16_t m_port { 0U };
    std::string m_interfaceIp { "0.0.0.0" };
    std::string m_systemId { "CC0001" };
    std::uint8_t m_multicastTtl { 1U };
    bool m_loopbackEnabled { true };

    mutable std::mutex m_configMutex {};
    std::atomic<SocketHandle> m_sockfd { InvalidSocket };
    std::atomic<bool> m_running { false };
    std::atomic<std::uint32_t> m_nextSeqNumber { 1U };
    std::thread m_rxThread {};
};

} // namespace Nmea::Lwe
