#include "MpegTsNetworkPublisher.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace Transport {

namespace {

constexpr std::size_t TsPacketSize { 188U };
constexpr std::size_t RtpHeaderSize { 12U };
constexpr std::uint8_t TsSyncByte { 0x47U };
constexpr std::uint8_t RtpVersion2 { 0x80U };
constexpr std::uint8_t RtpPayloadTypeMp2t { 33U }; // RFC 3551 MP2T

[[nodiscard]] std::uint64_t currentSystemTimeUs() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

} // namespace

MpegTsNetworkPublisher::MpegTsNetworkPublisher(TsNetworkConfig config)
    : m_config { std::move(config) } {
    if (m_config.packetsPerDatagram == 0U) {
        m_config.packetsPerDatagram = 1U;
    } else if (m_config.packetsPerDatagram > 7U) {
        m_config.packetsPerDatagram = 7U;
    }
    m_sendBuffer.reserve(RtpHeaderSize + (7U * TsPacketSize));
}

MpegTsNetworkPublisher::~MpegTsNetworkPublisher() {
    close();
}

bool MpegTsNetworkPublisher::open() {
    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    if (isOpen()) {
        return true;
    }
    return initSocket();
}

void MpegTsNetworkPublisher::close() {
    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    if (!isOpen()) {
        return;
    }

    if (m_bufferedPackets > 0U && !m_sendBuffer.empty()) {
        (void)sendDatagram(m_sendBuffer.data(), m_sendBuffer.size());
        m_sendBuffer.clear();
        m_bufferedPackets = 0U;
    }

    const SocketHandle sock = m_socket.exchange(Net::InvalidSocket);
    if (sock != Net::InvalidSocket) {
        Net::closeSocket(sock);
    }
}

bool MpegTsNetworkPublisher::isOpen() const noexcept {
    return m_socket.load() != Net::InvalidSocket;
}

bool MpegTsNetworkPublisher::pushPacket(const std::uint8_t* packet,
                                       std::size_t size,
                                       std::uint64_t ptsUs) {
    if ((packet == nullptr) || (size != TsPacketSize) || (packet[0] != TsSyncByte)) {
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    if (!isOpen() && !initSocket()) {
        return false;
    }

    if (m_bufferedPackets == 0U) {
        m_lastPtsUs = (ptsUs != 0U) ? ptsUs : currentSystemTimeUs();
        m_sendBuffer.clear();
        if (m_config.protocol == TsNetworkProtocol::Rtp) {
            // Reserve 12 bytes for RTP header
            m_sendBuffer.resize(RtpHeaderSize, 0U);
        }
    }

    m_sendBuffer.insert(m_sendBuffer.end(), packet, packet + TsPacketSize);
    m_bufferedPackets++;

    if (m_bufferedPackets >= m_config.packetsPerDatagram) {
        const bool ok = flush();
        return ok;
    }

    return true;
}

std::size_t MpegTsNetworkPublisher::pushPackets(const std::uint8_t* buffer,
                                                std::size_t size,
                                                std::uint64_t ptsUs) {
    if ((buffer == nullptr) || (size == 0U) || ((size % TsPacketSize) != 0U)) {
        return 0U;
    }

    const std::size_t packetCount = size / TsPacketSize;
    std::size_t pushed = 0U;

    for (std::size_t i = 0U; i < packetCount; ++i) {
        const std::uint8_t* ptr = buffer + (i * TsPacketSize);
        if (pushPacket(ptr, TsPacketSize, ptsUs)) {
            pushed++;
        } else {
            break;
        }
    }

    return pushed;
}

bool MpegTsNetworkPublisher::flush() {
    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    if (m_bufferedPackets == 0U || m_sendBuffer.empty()) {
        return true;
    }

    if (m_config.protocol == TsNetworkProtocol::Rtp) {
        // Construct 12-byte RTP header
        // Byte 0: V=2, P=0, X=0, CC=0
        m_sendBuffer[0] = RtpVersion2;
        // Byte 1: M=0, PT=33 (MP2T)
        m_sendBuffer[1] = RtpPayloadTypeMp2t;

        // Sequence number (16-bit)
        const std::uint16_t seq = m_rtpSeqNum++;
        m_sendBuffer[2] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
        m_sendBuffer[3] = static_cast<std::uint8_t>(seq & 0xFFU);

        // 90 kHz timestamp (32-bit): (ptsUs * 90) / 1000 = (ptsUs * 9) / 100
        const std::uint32_t rtpTimestamp = static_cast<std::uint32_t>((m_lastPtsUs * 9ULL) / 100ULL);
        m_sendBuffer[4] = static_cast<std::uint8_t>((rtpTimestamp >> 24U) & 0xFFU);
        m_sendBuffer[5] = static_cast<std::uint8_t>((rtpTimestamp >> 16U) & 0xFFU);
        m_sendBuffer[6] = static_cast<std::uint8_t>((rtpTimestamp >> 8U) & 0xFFU);
        m_sendBuffer[7] = static_cast<std::uint8_t>(rtpTimestamp & 0xFFU);

        // SSRC (32-bit)
        const std::uint32_t ssrc = m_config.rtpSsrc;
        m_sendBuffer[8] = static_cast<std::uint8_t>((ssrc >> 24U) & 0xFFU);
        m_sendBuffer[9] = static_cast<std::uint8_t>((ssrc >> 16U) & 0xFFU);
        m_sendBuffer[10] = static_cast<std::uint8_t>((ssrc >> 8U) & 0xFFU);
        m_sendBuffer[11] = static_cast<std::uint8_t>(ssrc & 0xFFU);
    }

    const bool sent = sendDatagram(m_sendBuffer.data(), m_sendBuffer.size());
    m_sendBuffer.clear();
    m_bufferedPackets = 0U;

    {
        std::lock_guard<std::mutex> statsLock { m_statsMutex };
        m_stats.flushesTriggered++;
    }

    return sent;
}

bool MpegTsNetworkPublisher::setConfig(const TsNetworkConfig& config) {
    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    const bool destinationChanged = (m_config.destinationIp != config.destinationIp) ||
                                    (m_config.destinationPort != config.destinationPort) ||
                                    (m_config.networkInterface != config.networkInterface);

    m_config = config;
    if (m_config.packetsPerDatagram == 0U) {
        m_config.packetsPerDatagram = 1U;
    } else if (m_config.packetsPerDatagram > 7U) {
        m_config.packetsPerDatagram = 7U;
    }

    if (isOpen() && destinationChanged) {
        close();
        return initSocket();
    }
    return true;
}

TsNetworkConfig MpegTsNetworkPublisher::config() const {
    std::lock_guard<std::recursive_mutex> lock { m_mutex };
    return m_config;
}

TsNetworkStats MpegTsNetworkPublisher::stats() const {
    std::lock_guard<std::mutex> lock { m_statsMutex };
    return m_stats;
}

void MpegTsNetworkPublisher::resetStats() noexcept {
    std::lock_guard<std::mutex> lock { m_statsMutex };
    m_stats = TsNetworkStats {};
    m_lastRateTime = std::chrono::steady_clock::now();
    m_bytesSinceRateCalc = 0U;
}

bool MpegTsNetworkPublisher::initSocket() {
    Net::ensureWinsockInitialized();

    const SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == Net::InvalidSocket) {
        return false;
    }

    // Configure destination address
    struct sockaddr_in destIn {};
    destIn.sin_family = AF_INET;
    destIn.sin_port = htons(m_config.destinationPort);

    if (::inet_pton(AF_INET, m_config.destinationIp.c_str(), &destIn.sin_addr) != 1) {
        Net::closeSocket(sock);
        return false;
    }

    std::memcpy(&m_destAddr, &destIn, sizeof(destIn));
    m_destAddrLen = static_cast<socklen_t>(sizeof(destIn));

    // Determine if destination is IPv4 Multicast (224.0.0.0 to 239.255.255.255)
    const std::uint32_t ipHost = ntohl(destIn.sin_addr.s_addr);
    m_isMulticast = ((ipHost & 0xF0000000U) == 0xE0000000U);

    if (m_isMulticast) {
#ifdef _WIN32
        const DWORD ttl = static_cast<DWORD>(m_config.multicastTtl);
        (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                           reinterpret_cast<const char*>(&ttl), static_cast<int>(sizeof(ttl)));

        const DWORD loop = m_config.multicastLoop ? 1UL : 0UL;
        (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                           reinterpret_cast<const char*>(&loop), static_cast<int>(sizeof(loop)));
#else
        const unsigned char ttl = static_cast<unsigned char>(m_config.multicastTtl);
        (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                           &ttl, static_cast<socklen_t>(sizeof(ttl)));

        const unsigned char loop = m_config.multicastLoop ? 1U : 0U;
        (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                           &loop, static_cast<socklen_t>(sizeof(loop)));
#endif

        if (!m_config.networkInterface.empty()) {
            struct in_addr ifAddr {};
            if (::inet_pton(AF_INET, m_config.networkInterface.c_str(), &ifAddr) == 1) {
#ifdef _WIN32
                (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
                                   reinterpret_cast<const char*>(&ifAddr),
                                   static_cast<int>(sizeof(ifAddr)));
#else
                (void)::setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
                                   &ifAddr, static_cast<socklen_t>(sizeof(ifAddr)));
#endif
            }
        }
    }

    // Set send buffer size to handle bursts
    const int sndBuf = static_cast<int>(m_config.socketSendBufferSize);
#ifdef _WIN32
    (void)::setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&sndBuf), static_cast<int>(sizeof(sndBuf)));
#else
    (void)::setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
                       &sndBuf, static_cast<socklen_t>(sizeof(sndBuf)));
#endif

    m_socket.store(sock);
    return true;
}

bool MpegTsNetworkPublisher::sendDatagram(const std::uint8_t* data, std::size_t size) {
    const SocketHandle sock = m_socket.load();
    if ((sock == Net::InvalidSocket) || (data == nullptr) || (size == 0U)) {
        return false;
    }

    const auto bytesToSend = static_cast<Net::SockBufLenType>(size);
    const auto sent = ::sendto(sock,
                               reinterpret_cast<const char*>(data),
                               bytesToSend,
                               Net::SendFlags,
                               reinterpret_cast<const struct sockaddr*>(&m_destAddr),
                               m_destAddrLen);

    if (sent < 0) {
        std::lock_guard<std::mutex> statsLock { m_statsMutex };
        m_stats.txErrors++;
        return false;
    }

    {
        std::lock_guard<std::mutex> statsLock { m_statsMutex };
        m_stats.datagramsSent++;
        m_stats.bytesSent += static_cast<std::uint64_t>(sent);
        m_stats.tsPacketsSent += static_cast<std::uint64_t>(m_bufferedPackets);
    }

    updateBitrate(static_cast<std::size_t>(sent));
    return true;
}

void MpegTsNetworkPublisher::updateBitrate(std::size_t bytesSent) noexcept {
    std::lock_guard<std::mutex> statsLock { m_statsMutex };
    m_bytesSinceRateCalc += static_cast<std::uint64_t>(bytesSent);

    const auto now = std::chrono::steady_clock::now();
    if (m_lastRateTime == std::chrono::steady_clock::time_point {}) {
        m_lastRateTime = now;
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRateTime).count();
    if (elapsed >= 500) {
        // Mbps = (bytes * 8) / (ms * 1000)
        const double bits = static_cast<double>(m_bytesSinceRateCalc) * 8.0;
        const double seconds = static_cast<double>(elapsed) / 1000.0;
        m_stats.currentBitrateMbps = (bits / seconds) / 1000000.0;

        m_bytesSinceRateCalc = 0U;
        m_lastRateTime = now;
    }
}

} // namespace Transport
