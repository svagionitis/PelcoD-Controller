#pragma once

/// @file NmeaUdpEndpoint.h
/// @brief Bidirectional UDP unicast/broadcast transceiver for NMEA 0183 (port 10110).

#include "BaseTransport.h"
#include "SocketUtils.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Nmea::Network {

/// @class NmeaUdpEndpoint
/// @brief Bidirectional UDP unicast and subnet broadcast transceiver for NMEA 0183 (port 10110).
/// @details Implements Transport::ITransport, allowing direct attachment to NmeaDevice.
class NmeaUdpEndpoint : public Transport::BaseTransport {
public:
    explicit NmeaUdpEndpoint(std::uint16_t rxPort = 10110U, std::string broadcastIp = "255.255.255.255",
                             std::uint16_t txPort = 10110U);
    ~NmeaUdpEndpoint() override;

    // Non-copyable, non-movable
    NmeaUdpEndpoint(const NmeaUdpEndpoint&) = delete;
    NmeaUdpEndpoint& operator=(const NmeaUdpEndpoint&) = delete;
    NmeaUdpEndpoint(NmeaUdpEndpoint&&) = delete;
    NmeaUdpEndpoint& operator=(NmeaUdpEndpoint&&) = delete;

    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;

    /// @brief Sends an NMEA sentence over UDP broadcast or unicast.
    /// @param[in] sentence Formatted sentence.
    /// @return True if sent successfully.
    [[nodiscard]] bool sendSentence(std::string_view sentence);

    [[nodiscard]] std::uint16_t getRxPort() const noexcept;
    [[nodiscard]] std::uint16_t getTxPort() const noexcept;
    [[nodiscard]] std::string getBroadcastIp() const;

private:
    void rxWorkerLoop();

    std::uint16_t m_rxPort { 10110U };
    std::string m_broadcastIp { "255.255.255.255" };
    std::uint16_t m_txPort { 10110U };

    std::atomic<Transport::Net::SocketHandle> m_sockfd { Transport::Net::InvalidSocket };
    std::atomic<bool> m_running { false };
    std::thread m_rxThread {};
};

} // namespace Nmea::Network
