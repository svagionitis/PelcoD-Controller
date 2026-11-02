/// @file KlvRtpPublisher.cpp
/// @brief Implementation of RFC 6597 Standalone KLV over RTP network publisher.

#include "KlvRtpPublisher.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "KlvRtpPacketizer.h"
#include "KlvRtpTypes.h"
#include "SdpGenerator.h"
#include "SocketUtils.h"

namespace Transport {

class KlvRtpPublisher::Impl {
public:
    explicit Impl(KlvRtpConfig config)
        : m_config(std::move(config))
        , m_packetizer(m_config.payloadType, m_config.ssrc, m_config.maxPayloadSize)
        , m_seqNumber(0U)
    {
    }

    ~Impl()
    {
        stop();
    }

    [[nodiscard]] bool start()
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        if (m_running.load()) {
            return true;
        }

        Net::ensureWinsockInitialized();

        const Net::SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == Net::InvalidSocket) {
            return false;
        }

        // Configure destination sockaddr_in
        struct sockaddr_in destIn { };
        destIn.sin_family = AF_INET;
        destIn.sin_port = htons(m_config.destinationPort);

        if (::inet_pton(AF_INET, m_config.destinationIp.c_str(), &destIn.sin_addr) != 1) {
            Net::closeSocket(sock);
            return false;
        }

        std::memcpy(&m_destAddr, &destIn, sizeof(destIn));
        m_destAddrLen = static_cast<socklen_t>(sizeof(destIn));

        // Detect IPv4 multicast (224.0.0.0 to 239.255.255.255)
        const std::uint32_t ipHost = ntohl(destIn.sin_addr.s_addr);
        m_isMulticast = ((ipHost & 0xF0000000U) == 0xE0000000U);

        if (m_isMulticast) {
#ifdef _WIN32
            const DWORD ttl = static_cast<DWORD>(m_config.timeToLive);
            (void)::setsockopt(
                sock, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), static_cast<int>(sizeof(ttl)));

            const DWORD loop = m_config.enableMulticastLoopback ? 1UL : 0UL;
            (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop),
                static_cast<int>(sizeof(loop)));
#else
            const unsigned char ttl = static_cast<unsigned char>(m_config.timeToLive);
            (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

            const unsigned char loop = m_config.enableMulticastLoopback ? 1U : 0U;
            (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
#endif
        }

        m_socket = sock;
        m_running.store(true);
        return true;
    }

    void stop()
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        if (m_socket != Net::InvalidSocket) {
            Net::closeSocket(m_socket);
            m_socket = Net::InvalidSocket;
        }
        m_running.store(false);
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_running.load() && (m_socket != Net::InvalidSocket);
    }

    [[nodiscard]] bool sendKlvPacket(const std::uint8_t* klvData, std::size_t size, std::uint64_t timestampUs)
    {
        if (klvData == nullptr || size == 0U) {
            return false;
        }

        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        if (!isRunning() && !start()) {
            return false;
        }

        const RtpTimestamp90kHz rtpTs = RtpTimestamp90kHz::fromMicroseconds(timestampUs);
        bool allSentSuccessfully { true };

        const std::size_t packetCount = m_packetizer.packetize(
            klvData, size, rtpTs, m_seqNumber, [&](const std::uint8_t* packetData, std::size_t packetSize) {
#ifdef _WIN32
                const int sendLen = static_cast<int>(packetSize);
                const int sent = ::sendto(m_socket, reinterpret_cast<const char*>(packetData), sendLen, Net::SendFlags,
                    reinterpret_cast<const struct sockaddr*>(&m_destAddr), static_cast<int>(m_destAddrLen));
                if (sent == sendLen) {
                    m_stats.packetsSent++;
                    m_stats.bytesSent += static_cast<std::uint64_t>(sent);
                } else {
                    allSentSuccessfully = false;
                }
#else
                const auto sendLen = static_cast<size_t>(packetSize);
                const ssize_t sent = ::sendto(
                    m_socket,
                    packetData,
                    sendLen,
                    Net::SendFlags,
                    reinterpret_cast<const struct sockaddr*>(&m_destAddr),
                    m_destAddrLen);
                if (sent > 0 && static_cast<size_t>(sent) == sendLen) {
                    m_stats.packetsSent++;
                    m_stats.bytesSent += static_cast<std::uint64_t>(sent);
                } else {
                    allSentSuccessfully = false;
                }
#endif
            });

        if (packetCount > 0U && allSentSuccessfully) {
            m_stats.messagesSent++;
            if (packetCount > 1U) {
                m_stats.fragmentsSent += (packetCount - 1U);
            }
        }

        return (packetCount > 0U) && allSentSuccessfully;
    }

    [[nodiscard]] KlvRtpStats stats() const noexcept
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        return m_stats;
    }

    void resetStats() noexcept
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        m_stats = KlvRtpStats {};
    }

    [[nodiscard]] KlvRtpConfig config() const
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        return m_config;
    }

    [[nodiscard]] bool setConfig(const KlvRtpConfig& config)
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        const bool destChanged = (m_config.destinationIp != config.destinationIp)
            || (m_config.destinationPort != config.destinationPort) || (m_config.timeToLive != config.timeToLive)
            || (m_config.enableMulticastLoopback != config.enableMulticastLoopback);

        m_config = config;
        m_packetizer = KlvRtpPacketizer(m_config.payloadType, m_config.ssrc, m_config.maxPayloadSize);

        if (isRunning() && destChanged) {
            stop();
            return start();
        }
        return true;
    }

    [[nodiscard]] std::string generateSdp() const
    {
        std::lock_guard<std::recursive_mutex> lock { m_mutex };
        return SdpGenerator::generateKlvRtpSdp(m_config);
    }

private:
    KlvRtpConfig m_config;
    KlvRtpPacketizer m_packetizer;
    RtpSequenceNumber m_seqNumber { 0U };
    Net::SocketHandle m_socket { Net::InvalidSocket };
    struct sockaddr_in m_destAddr { };
    socklen_t m_destAddrLen { 0U };
    bool m_isMulticast { false };
    std::atomic<bool> m_running { false };
    mutable std::recursive_mutex m_mutex;
    KlvRtpStats m_stats {};
};

KlvRtpPublisher::KlvRtpPublisher(KlvRtpConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config)))
{
}

KlvRtpPublisher::~KlvRtpPublisher() = default;

KlvRtpPublisher::KlvRtpPublisher(KlvRtpPublisher&&) noexcept = default;
KlvRtpPublisher& KlvRtpPublisher::operator=(KlvRtpPublisher&&) noexcept = default;

bool KlvRtpPublisher::start()
{
    return m_impl->start();
}

void KlvRtpPublisher::stop()
{
    m_impl->stop();
}

bool KlvRtpPublisher::isRunning() const noexcept
{
    return m_impl->isRunning();
}

bool KlvRtpPublisher::sendKlvPacket(const std::uint8_t* klvData, std::size_t size, std::uint64_t timestampUs)
{
    return m_impl->sendKlvPacket(klvData, size, timestampUs);
}

KlvRtpStats KlvRtpPublisher::stats() const noexcept
{
    return m_impl->stats();
}

void KlvRtpPublisher::resetStats() noexcept
{
    m_impl->resetStats();
}

KlvRtpConfig KlvRtpPublisher::config() const
{
    return m_impl->config();
}

bool KlvRtpPublisher::setConfig(const KlvRtpConfig& config)
{
    return m_impl->setConfig(config);
}

std::string KlvRtpPublisher::generateSdp() const
{
    return m_impl->generateSdp();
}

} // namespace Transport
