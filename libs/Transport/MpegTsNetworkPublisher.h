#pragma once

/// @file MpegTsNetworkPublisher.h
/// @brief High-performance cross-platform network publisher for MPEG-TS streams over UDP/RTP.

#include "TsNetworkTypes.h"
#include "SocketUtils.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Transport {

/// @class MpegTsNetworkPublisher
/// @brief Cross-platform network publisher streaming 188-byte TS packets over UDP/RTP multicast or unicast.
/// @details Packages 188-byte MPEG-TS packets into MTU-safe datagrams (default 7 TS packets = 1316 bytes
///          for raw UDP, or 12-byte RTP header + 1316 bytes = 1328 bytes for RTP MP2T).
class MpegTsNetworkPublisher {
public:
    /// @brief Constructs network publisher with optional configuration settings.
    /// @param[in] config Network configuration settings.
    explicit MpegTsNetworkPublisher(TsNetworkConfig config = {});

    /// @brief Destructor closing socket descriptor and flushing pending buffers.
    ~MpegTsNetworkPublisher();

    MpegTsNetworkPublisher(const MpegTsNetworkPublisher&) = delete;
    MpegTsNetworkPublisher& operator=(const MpegTsNetworkPublisher&) = delete;
    MpegTsNetworkPublisher(MpegTsNetworkPublisher&&) = delete;
    MpegTsNetworkPublisher& operator=(MpegTsNetworkPublisher&&) = delete;

    /// @brief Opens the network streaming socket and configures multicast options.
    /// @return True if socket was created, configured, and destination initialized.
    [[nodiscard]] bool open();

    /// @brief Closes the socket descriptor and flushes any pending buffer.
    void close();

    /// @brief Returns whether the socket is currently open and valid.
    /// @return True if active.
    [[nodiscard]] bool isOpen() const noexcept;

    /// @brief Ingests a single 188-byte MPEG-TS packet into the transmission accumulator.
    /// @param[in] packet Pointer to 188-byte TS packet.
    /// @param[in] size Size of packet (must be exactly 188 bytes).
    /// @param[in] ptsUs Optional presentation timestamp for RTP header timing.
    /// @return True if packet was queued or sent without error.
    [[nodiscard]] bool pushPacket(const std::uint8_t* packet,
                                  std::size_t size,
                                  std::uint64_t ptsUs = 0U);

    /// @brief Ingests a contiguous buffer of one or more 188-byte TS packets.
    /// @param[in] buffer Pointer to TS packet buffer (must be multiple of 188 bytes).
    /// @param[in] size Size of buffer in bytes.
    /// @param[in] ptsUs Optional presentation timestamp for RTP header timing.
    /// @return Number of TS packets successfully processed.
    [[nodiscard]] std::size_t pushPackets(const std::uint8_t* buffer,
                                          std::size_t size,
                                          std::uint64_t ptsUs = 0U);

    /// @brief Immediately transmits any partially accumulated TS packets.
    /// @return True if flush transmitted successfully or buffer was empty.
    [[nodiscard]] bool flush();

    /// @brief Updates configuration dynamically and reinitializes socket if destination changed.
    /// @param[in] config New configuration parameters.
    /// @return True if reconfiguration succeeded.
    [[nodiscard]] bool setConfig(const TsNetworkConfig& config);

    /// @brief Retrieves current configuration settings.
    /// @return Copy of active configuration.
    [[nodiscard]] TsNetworkConfig config() const;

    /// @brief Captures real-time diagnostic statistics and bitrate metrics.
    /// @return Diagnostic statistics snapshot.
    [[nodiscard]] TsNetworkStats stats() const;

    /// @brief Resets all operational telemetry counters.
    void resetStats() noexcept;

private:
    [[nodiscard]] bool initSocket();
    [[nodiscard]] bool sendDatagram(const std::uint8_t* data, std::size_t size);
    void updateBitrate(std::size_t bytesSent) noexcept;

    TsNetworkConfig m_config {};
    mutable std::recursive_mutex m_mutex {};

    using SocketHandle = Net::SocketHandle;
    std::atomic<SocketHandle> m_socket { Net::InvalidSocket };

    struct sockaddr_storage m_destAddr {};
    socklen_t m_destAddrLen { 0 };
    bool m_isMulticast { false };

    // Accumulation buffer for 1..7 TS packets (max 7 * 188 = 1316 bytes)
    // Plus 12 bytes if RTP header is prepended
    std::vector<std::uint8_t> m_sendBuffer {};
    std::uint8_t m_bufferedPackets { 0U };
    std::uint64_t m_lastPtsUs { 0U };

    // RTP state
    std::uint16_t m_rtpSeqNum { 0U };

    // Telemetry statistics
    mutable std::mutex m_statsMutex {};
    TsNetworkStats m_stats {};
    std::chrono::steady_clock::time_point m_lastRateTime {};
    std::uint64_t m_bytesSinceRateCalc { 0U };
};

} // namespace Transport
