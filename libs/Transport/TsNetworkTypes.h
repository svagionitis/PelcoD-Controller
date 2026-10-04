#pragma once

/// @file TsNetworkTypes.h
/// @brief Type definitions and configuration structures for MPEG-TS network streaming.

#include <cstdint>
#include <string>

namespace Transport {

/// @brief Network streaming protocol encapsulation for MPEG-TS.
enum class TsNetworkProtocol : std::uint8_t {
    RawUdp = 0, ///< Raw UDP datagrams containing N * 188-byte TS packets.
    Rtp         ///< RFC 3550 / RFC 2250 RTP datagrams with Payload Type 33 (MP2T).
};

/// @brief Configuration settings for MPEG-TS network streaming.
struct TsNetworkConfig {
    std::string destinationIp { "239.255.0.1" }; ///< Unicast IP or Multicast group.
    std::uint16_t destinationPort { 1234U };    ///< Target UDP destination port.
    std::string networkInterface { "" };        ///< Outbound interface IP (optional).
    TsNetworkProtocol protocol { TsNetworkProtocol::RawUdp }; ///< Encapsulation protocol.
    std::uint8_t packetsPerDatagram { 7U };      ///< TS packets per datagram (1..7, default 7 = 1316 B).
    std::uint8_t multicastTtl { 16U };          ///< Multicast TTL (hop count).
    bool multicastLoop { true };                ///< Enable multicast loopback for local clients.
    std::uint32_t socketSendBufferSize { 1048576U }; ///< Kernel send buffer size (1 MB default).
    std::uint32_t rtpSsrc { 0x4B4C5631U };      ///< RTP Synchronization Source identifier ('KLV1').
};

/// @brief Diagnostic telemetry and throughput statistics for the network publisher.
struct TsNetworkStats {
    std::uint64_t tsPacketsSent { 0U };   ///< Cumulative 188-byte TS packets sent.
    std::uint64_t datagramsSent { 0U };   ///< Cumulative network datagrams (UDP/RTP) sent.
    std::uint64_t bytesSent { 0U };       ///< Cumulative payload bytes transmitted.
    std::uint64_t txErrors { 0U };        ///< Socket transmission error count.
    std::uint64_t flushesTriggered { 0U }; ///< Manual or forced flushes executed.
    double currentBitrateMbps { 0.0 };    ///< Instantaneous transmission rate in Mbps.
};

} // namespace Transport
