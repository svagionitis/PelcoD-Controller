#include "RtspServerPublisher.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <iomanip>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SdpGenerator.h"
#include "SocketUtils.h"

namespace Transport {

namespace {

    constexpr std::uint8_t kTsSyncByte { 0x47U };
    constexpr std::size_t kRtpHeaderSize { 12U };
    constexpr std::size_t kInterleavedHeaderSize { 4U };
    constexpr std::uint8_t kInterleavedMagic { 0x24U }; // '$'
    constexpr std::uint8_t kRtpVersion2 { 0x80U };
    constexpr std::uint8_t kRtpPayloadMp2t { 33U };
    constexpr std::string_view kCrLf { "\r\n" };
    constexpr std::string_view kDoubleCrLf { "\r\n\r\n" };

    [[nodiscard]] constexpr char toLowerAscii(char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    [[nodiscard]] bool equalsIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (std::size_t i { 0U }; i < lhs.size(); ++i) {
            if (toLowerAscii(lhs[i]) != toLowerAscii(rhs[i])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string_view trimAscii(std::string_view str) noexcept
    {
        while (
            !str.empty() && (str.front() == ' ' || str.front() == '\t' || str.front() == '\r' || str.front() == '\n')) {
            str.remove_prefix(1U);
        }
        while (!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\r' || str.back() == '\n')) {
            str.remove_suffix(1U);
        }
        return str;
    }

    struct RtspRequest {
        std::string method {};
        std::string url {};
        std::string version {};
        std::vector<std::pair<std::string, std::string>> headers {};
        std::string body {};
        bool complete { false };

        [[nodiscard]] std::string getHeader(std::string_view name) const
        {
            for (const auto& kv : headers) {
                if (equalsIgnoreCaseAscii(kv.first, name)) {
                    return kv.second;
                }
            }
            return {};
        }
    };

    [[nodiscard]] RtspRequest parseRtspRequest(std::string_view raw)
    {
        RtspRequest req {};
        const std::size_t doubleCrLfPos = raw.find(kDoubleCrLf);
        if (doubleCrLfPos == std::string_view::npos) {
            return req;
        }

        const std::string_view headerPart = raw.substr(0U, doubleCrLfPos);
        req.body = std::string(raw.substr(doubleCrLfPos + 4U));

        std::size_t lineStart { 0U };
        bool isFirstLine { true };

        while (lineStart < headerPart.size()) {
            const std::size_t lineEnd = headerPart.find(kCrLf, lineStart);
            const std::string_view line = (lineEnd == std::string_view::npos)
                ? headerPart.substr(lineStart)
                : headerPart.substr(lineStart, lineEnd - lineStart);

            if (isFirstLine) {
                isFirstLine = false;
                const std::string lineStr(line);
                std::istringstream iss { lineStr };
                iss >> req.method >> req.url >> req.version;
            } else if (!line.empty()) {
                const std::size_t colonPos = line.find(':');
                if (colonPos != std::string_view::npos) {
                    const std::string key(trimAscii(line.substr(0U, colonPos)));
                    const std::string val(trimAscii(line.substr(colonPos + 1U)));
                    req.headers.emplace_back(key, val);
                }
            }

            if (lineEnd == std::string_view::npos) {
                break;
            }
            lineStart = lineEnd + 2U;
        }

        req.complete = !req.method.empty() && !req.url.empty();
        return req;
    }

    [[nodiscard]] std::string generateSessionId()
    {
        static thread_local std::mt19937_64 rng(std::random_device {}());
        std::uniform_int_distribution<std::uint64_t> dist(10000000ULL, 99999999ULL);
        return std::to_string(dist(rng));
    }

    [[nodiscard]] std::uint64_t currentSystemTimeUs() noexcept
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
    }

} // namespace

/// @class RtspServerPublisher::Impl
/// @brief Private implementation encapsulating socket management, polling, and streaming sessions.
class RtspServerPublisher::Impl {
public:
    explicit Impl(RtspServerConfig config)
        : m_config(std::move(config))
    {
        if (m_config.packetsPerDatagram == 0U) {
            m_config.packetsPerDatagram = 1U;
        } else if (m_config.packetsPerDatagram > kMaxPacketsPerDatagram) {
            m_config.packetsPerDatagram = kMaxPacketsPerDatagram;
        }
        m_accumulator.reserve(m_config.packetsPerDatagram * kTsPacketSize);
    }

    ~Impl()
    {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    [[nodiscard]] bool start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running.load()) {
            return true;
        }

        Net::ensureWinsockInitialized();

        const Net::SocketHandle listenSock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenSock == Net::InvalidSocket) {
            return false;
        }

        const int reuse = 1;
        static_cast<void>(
            ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)));

        if (!Net::setNonBlocking(listenSock, true)) {
            Net::closeSocket(listenSock);
            return false;
        }

        sockaddr_in serverAddr {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_config.listenPort.value());
        if (m_config.listenAddress.empty() || m_config.listenAddress == "0.0.0.0") {
            serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            if (::inet_pton(AF_INET, m_config.listenAddress.c_str(), &serverAddr.sin_addr) <= 0) {
                serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            }
        }

        if (::bind(listenSock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            Net::closeSocket(listenSock);
            return false;
        }

        if (::listen(listenSock, 128) < 0) {
            Net::closeSocket(listenSock);
            return false;
        }

        m_listenSocket = listenSock;
        m_running.store(true);
        m_lastBitrateCalcTime = std::chrono::steady_clock::now();
        m_bytesSinceLastCalc = 0U;

        m_workerThread = std::thread(&Impl::workerLoop, this);
        return true;
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }

        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_listenSocket != Net::InvalidSocket) {
            Net::closeSocket(m_listenSocket);
            m_listenSocket = Net::InvalidSocket;
        }

        for (auto& client : m_clients) {
            if (client->tcpSocket != Net::InvalidSocket) {
                Net::closeSocket(client->tcpSocket);
                client->tcpSocket = Net::InvalidSocket;
            }
            if (client->serverUdpSocket != Net::InvalidSocket) {
                Net::closeSocket(client->serverUdpSocket);
                client->serverUdpSocket = Net::InvalidSocket;
            }
        }
        m_clients.clear();
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_running.load();
    }

    bool pushPacket(const std::uint8_t* packet, std::size_t size, std::uint64_t ptsUs)
    {
        if (packet == nullptr || size != kTsPacketSize || packet[0] != kTsSyncByte) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running.load()) {
            return false;
        }

        m_stats.tsPacketsIngested++;

        if (m_accumulator.empty()) {
            m_accumulatorPtsUs = (ptsUs != 0U) ? ptsUs : currentSystemTimeUs();
        }

        m_accumulator.insert(m_accumulator.end(), packet, packet + kTsPacketSize);

        if ((m_accumulator.size() / kTsPacketSize) >= m_config.packetsPerDatagram) {
            flushInternal();
        }

        return true;
    }

    std::size_t pushPackets(const std::uint8_t* buffer, std::size_t size, std::uint64_t ptsUs)
    {
        if (buffer == nullptr || (size % kTsPacketSize) != 0U) {
            return 0U;
        }

        const std::size_t count = size / kTsPacketSize;
        std::size_t ingested { 0U };
        for (std::size_t i { 0U }; i < count; ++i) {
            if (pushPacket(buffer + (i * kTsPacketSize), kTsPacketSize, ptsUs)) {
                ingested++;
            }
        }
        return ingested;
    }

    void flush()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        flushInternal();
    }

    [[nodiscard]] RtspServerConfig config() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config;
    }

    [[nodiscard]] RtspServerStats stats() const
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        RtspServerStats s = m_stats;
        s.activeClients = m_activeClientsCount.load();
        s.streamingClients = m_streamingClientsCount.load();
        return s;
    }

    void resetStats() noexcept
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats = RtspServerStats {};
        m_bytesSinceLastCalc = 0U;
        m_lastBitrateCalcTime = std::chrono::steady_clock::now();
    }

    [[nodiscard]] std::string streamUrl(std::string_view hostname) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string host = hostname.empty() ? m_config.listenAddress : std::string(hostname);
        if (host.empty() || host == "0.0.0.0") {
            host = "127.0.0.1";
        }

        std::string path = m_config.streamPath;
        if (path.empty() || path[0] != '/') {
            path = "/" + path;
        }

        return "rtsp://" + host + ":" + std::to_string(m_config.listenPort.value()) + path;
    }

private:
    struct ClientSession {
        Net::SocketHandle tcpSocket { Net::InvalidSocket };
        RtspSessionState state { RtspSessionState::Init };
        RtspTransportMode transportMode { RtspTransportMode::TcpInterleaved };
        std::string sessionId {};
        std::string clientIp {};
        std::uint16_t clientRtpPort { 0U };
        std::uint16_t clientRtcpPort { 0U };
        Net::SocketHandle serverUdpSocket { Net::InvalidSocket };
        std::uint16_t serverUdpPort { 0U };
        sockaddr_in clientUdpDest {};
        std::uint8_t rtpChannel { 0U };
        std::uint8_t rtcpChannel { 1U };
        std::string recvBuffer {};
        std::vector<std::uint8_t> pendingTcpWrite {};
        std::deque<std::vector<std::uint8_t>> frameQueue {};
        std::chrono::steady_clock::time_point lastActivity { std::chrono::steady_clock::now() };
    };

    void workerLoop()
    {
        while (m_running.load()) {
            std::vector<Net::PollFd> pollFds;
            std::vector<std::shared_ptr<ClientSession>> currentClients;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_listenSocket != Net::InvalidSocket) {
                    Net::PollFd pfd {};
                    pfd.fd = m_listenSocket;
                    pfd.events = POLLIN;
                    pollFds.push_back(pfd);
                }

                currentClients = m_clients;
                for (const auto& client : currentClients) {
                    if (client->tcpSocket != Net::InvalidSocket) {
                        Net::PollFd pfd {};
                        pfd.fd = client->tcpSocket;
                        pfd.events = POLLIN;
                        if (!client->pendingTcpWrite.empty() || !client->frameQueue.empty()) {
                            pfd.events |= POLLOUT;
                        }
                        pollFds.push_back(pfd);
                    }
                }
            }

            const int ready = Net::pollSockets(pollFds.data(), static_cast<unsigned long>(pollFds.size()), 50);
            if (!m_running.load()) {
                break;
            }

            if (ready > 0 && !pollFds.empty()) {
                std::size_t pfdIdx { 0U };

                // 1. Check listen socket for new incoming connections
                if (m_listenSocket != Net::InvalidSocket && pfdIdx < pollFds.size()) {
                    if ((pollFds[pfdIdx].revents & POLLIN) != 0) {
                        acceptNewClients();
                    }
                    pfdIdx++;
                }

                // 2. Service active client sockets
                for (std::size_t i { 0U }; i < currentClients.size() && pfdIdx < pollFds.size(); ++i, ++pfdIdx) {
                    auto& client = currentClients[i];
                    const short revents = pollFds[pfdIdx].revents;

                    if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                        client->state = RtspSessionState::Closed;
                        continue;
                    }

                    if ((revents & POLLIN) != 0) {
                        handleClientRead(client);
                    }

                    if ((revents & POLLOUT) != 0) {
                        handleClientWrite(client);
                    }
                }
            }

            checkClientTimeouts();
            cleanupClosedClients();
        }
    }

    void acceptNewClients()
    {
        while (m_running.load()) {
            sockaddr_in clientAddr {};
            Net::SockOptLenType addrLen = sizeof(clientAddr);
            const Net::SocketHandle clientSock
                = ::accept(m_listenSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

            if (clientSock == Net::InvalidSocket) {
                break;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_clients.size() >= m_config.maxClients) {
                Net::closeSocket(clientSock);
                continue;
            }

            if (!Net::setNonBlocking(clientSock, true)) {
                Net::closeSocket(clientSock);
                continue;
            }

            char ipStr[INET_ADDRSTRLEN] { 0 };
            ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));

            auto session = std::make_shared<ClientSession>();
            session->tcpSocket = clientSock;
            session->state = RtspSessionState::Init;
            session->sessionId = generateSessionId();
            session->clientIp = std::string(ipStr);
            session->lastActivity = std::chrono::steady_clock::now();

            m_clients.push_back(std::move(session));
            m_activeClientsCount.store(m_clients.size());

            {
                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                m_stats.totalConnectionsAccepted++;
            }
        }
    }

    void handleClientRead(const std::shared_ptr<ClientSession>& client)
    {
        std::array<char, 4096> readBuf {};
        const auto bytesRead
            = ::recv(client->tcpSocket, readBuf.data(), static_cast<Net::SockBufLenType>(readBuf.size()), 0);

        if (bytesRead <= 0) {
            if (bytesRead == 0 || !Net::isWouldBlock()) {
                client->state = RtspSessionState::Closed;
            }
            return;
        }

        client->lastActivity = std::chrono::steady_clock::now();
        client->recvBuffer.append(readBuf.data(), static_cast<std::size_t>(bytesRead));

        processClientRequests(client);
    }

    void processClientRequests(const std::shared_ptr<ClientSession>& client)
    {
        while (true) {
            const std::size_t headerEnd = client->recvBuffer.find(kDoubleCrLf);
            if (headerEnd == std::string_view::npos) {
                // If receive buffer is abnormally large without finding header termination, drop client
                if (client->recvBuffer.size() > 65536U) {
                    client->state = RtspSessionState::Closed;
                }
                break;
            }

            const std::string rawReq = client->recvBuffer.substr(0U, headerEnd + 4U);
            const RtspRequest req = parseRtspRequest(rawReq);

            if (!req.complete) {
                client->recvBuffer.erase(0U, headerEnd + 4U);
                sendErrorResponse(client, "400 Bad Request", "0");
                continue;
            }

            std::size_t totalReqLen = headerEnd + 4U;
            const std::string contentLenStr = req.getHeader("Content-Length");
            if (!contentLenStr.empty()) {
                try {
                    const std::size_t bodyLen = std::stoul(contentLenStr);
                    if (client->recvBuffer.size() < (totalReqLen + bodyLen)) {
                        // Wait for full body to arrive
                        break;
                    }
                    totalReqLen += bodyLen;
                } catch (...) {
                    // Ignore malformed content-length
                }
            }

            client->recvBuffer.erase(0U, totalReqLen);
            dispatchRtspMethod(client, req);
        }
    }

    void dispatchRtspMethod(const std::shared_ptr<ClientSession>& client, const RtspRequest& req)
    {
        const std::string cseq = req.getHeader("CSeq");
        const std::string method = req.method;

        if (equalsIgnoreCaseAscii(method, "OPTIONS")) {
            handleOptions(client, cseq);
        } else if (equalsIgnoreCaseAscii(method, "DESCRIBE")) {
            handleDescribe(client, req, cseq);
        } else if (equalsIgnoreCaseAscii(method, "SETUP")) {
            handleSetup(client, req, cseq);
        } else if (equalsIgnoreCaseAscii(method, "PLAY")) {
            handlePlay(client, cseq);
        } else if (equalsIgnoreCaseAscii(method, "TEARDOWN")) {
            handleTeardown(client, cseq);
        } else if (equalsIgnoreCaseAscii(method, "GET_PARAMETER") || equalsIgnoreCaseAscii(method, "SET_PARAMETER")) {
            handleGetParameter(client, cseq);
        } else {
            sendErrorResponse(client, "501 Not Implemented", cseq);
        }
    }

    void handleOptions(const std::shared_ptr<ClientSession>& client, const std::string& cseq)
    {
        std::ostringstream ss;
        ss << "RTSP/1.0 200 OK\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER, SET_PARAMETER\r\n"
           << "\r\n";
        enqueueClientText(client, ss.str());
    }

    void handleDescribe(const std::shared_ptr<ClientSession>& client, const RtspRequest& req, const std::string& cseq)
    {
        std::string expectedPath = m_config.streamPath;
        if (!expectedPath.empty() && expectedPath[0] != '/') {
            expectedPath = "/" + expectedPath;
        }

        // Verify request URL targets streamPath
        if (req.url.find(expectedPath) == std::string::npos && req.url != "*") {
            sendErrorResponse(client, "404 Not Found", cseq);
            return;
        }

        SdpSessionParams params {};
        params.sessionName = m_config.sessionName;
        params.originAddress = (m_config.listenAddress == "0.0.0.0" || m_config.listenAddress.empty())
            ? "127.0.0.1"
            : m_config.listenAddress;
        params.destinationIp = "0.0.0.0";
        params.payloadType = SdpPayloadType::Mpeg2Ts;
        params.transportProtocol = SdpProtocol::RtpAvp;
        params.destinationPort = 0U;

        std::string sdpContent = SdpGenerator::generate(params);
        if (sdpContent.find("a=control:") == std::string::npos) {
            sdpContent += "a=control:track0\r\n";
        }

        std::string base = req.url;
        if (!base.empty() && base.back() != '/') {
            base += "/";
        }

        std::ostringstream ss;
        ss << "RTSP/1.0 200 OK\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "Content-Type: application/sdp\r\n"
           << "Content-Base: " << base << "\r\n"
           << "Content-Length: " << sdpContent.size() << "\r\n"
           << "\r\n"
           << sdpContent;

        enqueueClientText(client, ss.str());
    }

    void handleSetup(const std::shared_ptr<ClientSession>& client, const RtspRequest& req, const std::string& cseq)
    {
        const std::string transportHdr = req.getHeader("Transport");
        if (transportHdr.empty()) {
            sendErrorResponse(client, "461 Unsupported Transport", cseq);
            return;
        }

        std::ostringstream ssrcHex;
        ssrcHex << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << m_config.rtpSsrc;

        if (transportHdr.find("interleaved") != std::string::npos || transportHdr.find("TCP") != std::string::npos) {
            // TCP Interleaved transport
            client->transportMode = RtspTransportMode::TcpInterleaved;

            // Parse interleaved=X-Y
            std::uint8_t rtpCh { 0U };
            std::uint8_t rtcpCh { 1U };
            const std::size_t interPos = transportHdr.find("interleaved=");
            if (interPos != std::string_view::npos) {
                const std::string sub = transportHdr.substr(interPos + 12U);
                const std::size_t dashPos = sub.find('-');
                if (dashPos != std::string_view::npos) {
                    try {
                        rtpCh = static_cast<std::uint8_t>(std::stoul(sub.substr(0U, dashPos)));
                        rtcpCh = static_cast<std::uint8_t>(std::stoul(sub.substr(dashPos + 1U)));
                    } catch (...) {
                        rtpCh = 0U;
                        rtcpCh = 1U;
                    }
                }
            }

            client->rtpChannel = rtpCh;
            client->rtcpChannel = rtcpCh;
            client->state = RtspSessionState::Ready;

            std::ostringstream ss;
            ss << "RTSP/1.0 200 OK\r\n"
               << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
               << "Transport: RTP/AVP/TCP;unicast;interleaved=" << static_cast<int>(rtpCh) << "-"
               << static_cast<int>(rtcpCh) << ";ssrc=" << ssrcHex.str() << "\r\n"
               << "Session: " << client->sessionId << ";timeout=" << m_config.sessionTimeoutSeconds << "\r\n"
               << "\r\n";
            enqueueClientText(client, ss.str());
        } else {
            // UDP Unicast transport
            client->transportMode = RtspTransportMode::UdpUnicast;

            std::uint16_t clientRtp { 0U };
            std::uint16_t clientRtcp { 0U };
            const std::size_t portPos = transportHdr.find("client_port=");
            if (portPos != std::string_view::npos) {
                const std::string sub = transportHdr.substr(portPos + 12U);
                const std::size_t dashPos = sub.find('-');
                if (dashPos != std::string_view::npos) {
                    try {
                        clientRtp = static_cast<std::uint16_t>(std::stoul(sub.substr(0U, dashPos)));
                        clientRtcp = static_cast<std::uint16_t>(std::stoul(sub.substr(dashPos + 1U)));
                    } catch (...) {
                        clientRtp = 0U;
                        clientRtcp = 0U;
                    }
                }
            }

            if (clientRtp == 0U) {
                sendErrorResponse(client, "461 Unsupported Transport", cseq);
                return;
            }

            client->clientRtpPort = clientRtp;
            client->clientRtcpPort = clientRtcp;

            // Allocate local UDP sender socket
            allocateClientUdpSocket(client);

            client->state = RtspSessionState::Ready;

            std::ostringstream ss;
            ss << "RTSP/1.0 200 OK\r\n"
               << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
               << "Transport: RTP/AVP;unicast;client_port=" << clientRtp << "-" << clientRtcp
               << ";server_port=" << client->serverUdpPort << "-" << (client->serverUdpPort + 1U)
               << ";ssrc=" << ssrcHex.str() << "\r\n"
               << "Session: " << client->sessionId << ";timeout=" << m_config.sessionTimeoutSeconds << "\r\n"
               << "\r\n";
            enqueueClientText(client, ss.str());
        }
    }

    void allocateClientUdpSocket(const std::shared_ptr<ClientSession>& client)
    {
        const Net::SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == Net::InvalidSocket) {
            return;
        }

        static_cast<void>(Net::setNonBlocking(sock, true));

        // Bind to ephemeral port
        sockaddr_in localAddr {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        for (std::uint16_t port = m_config.udpPortRangeStart; port <= m_config.udpPortRangeEnd; port += 2U) {
            localAddr.sin_port = htons(port);
            if (::bind(sock, reinterpret_cast<struct sockaddr*>(&localAddr), sizeof(localAddr)) == 0) {
                client->serverUdpSocket = sock;
                client->serverUdpPort = port;
                break;
            }
        }

        if (client->serverUdpSocket == Net::InvalidSocket) {
            localAddr.sin_port = 0; // Kernel assigned
            if (::bind(sock, reinterpret_cast<struct sockaddr*>(&localAddr), sizeof(localAddr)) == 0) {
                Net::SockOptLenType len = sizeof(localAddr);
                if (::getsockname(sock, reinterpret_cast<struct sockaddr*>(&localAddr), &len) == 0) {
                    client->serverUdpPort = ntohs(localAddr.sin_port);
                    client->serverUdpSocket = sock;
                }
            }
        }

        // Configure destination sockaddr
        client->clientUdpDest.sin_family = AF_INET;
        client->clientUdpDest.sin_port = htons(client->clientRtpPort);
        ::inet_pton(AF_INET, client->clientIp.c_str(), &client->clientUdpDest.sin_addr);
    }

    void handlePlay(const std::shared_ptr<ClientSession>& client, const std::string& cseq)
    {
        client->state = RtspSessionState::Streaming;
        updateStreamingClientsCount();

        std::ostringstream ss;
        ss << "RTSP/1.0 200 OK\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "Range: npt=0.000-\r\n"
           << "Session: " << client->sessionId << "\r\n"
           << "RTP-Info: url=track0;seq=" << m_rtpSequence.load() << ";rtptime=0\r\n"
           << "\r\n";
        enqueueClientText(client, ss.str());
    }

    void handleTeardown(const std::shared_ptr<ClientSession>& client, const std::string& cseq)
    {
        client->state = RtspSessionState::Closed;
        updateStreamingClientsCount();

        std::ostringstream ss;
        ss << "RTSP/1.0 200 OK\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "Session: " << client->sessionId << "\r\n"
           << "\r\n";
        enqueueClientText(client, ss.str());
    }

    void handleGetParameter(const std::shared_ptr<ClientSession>& client, const std::string& cseq)
    {
        std::ostringstream ss;
        ss << "RTSP/1.0 200 OK\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "Session: " << client->sessionId << "\r\n"
           << "\r\n";
        enqueueClientText(client, ss.str());
    }

    void sendErrorResponse(
        const std::shared_ptr<ClientSession>& client, std::string_view status, const std::string& cseq)
    {
        std::ostringstream ss;
        ss << "RTSP/1.0 " << status << "\r\n"
           << "CSeq: " << (cseq.empty() ? "1" : cseq) << "\r\n"
           << "\r\n";
        enqueueClientText(client, ss.str());
    }

    void enqueueClientText(const std::shared_ptr<ClientSession>& client, const std::string& text)
    {
        client->pendingTcpWrite.insert(client->pendingTcpWrite.end(), text.begin(), text.end());
        handleClientWrite(client);
    }

    void handleClientWrite(const std::shared_ptr<ClientSession>& client)
    {
        if (client->tcpSocket == Net::InvalidSocket) {
            return;
        }

        // 1. Drain pendingTcpWrite buffer
        while (!client->pendingTcpWrite.empty()) {
            const auto sent = ::send(client->tcpSocket, reinterpret_cast<const char*>(client->pendingTcpWrite.data()),
                static_cast<Net::SockBufLenType>(client->pendingTcpWrite.size()), Net::SendFlags);

            if (sent <= 0) {
                if (sent < 0 && Net::isWouldBlock()) {
                    return;
                }
                client->state = RtspSessionState::Closed;
                return;
            }

            client->pendingTcpWrite.erase(client->pendingTcpWrite.begin(), client->pendingTcpWrite.begin() + sent);
        }

        // 2. Drain frameQueue
        while (!client->frameQueue.empty()) {
            const auto& frame = client->frameQueue.front();
            const auto sent = ::send(client->tcpSocket, reinterpret_cast<const char*>(frame.data()),
                static_cast<Net::SockBufLenType>(frame.size()), Net::SendFlags);

            if (sent <= 0) {
                if (sent < 0 && Net::isWouldBlock()) {
                    return;
                }
                client->state = RtspSessionState::Closed;
                return;
            }

            if (static_cast<std::size_t>(sent) < frame.size()) {
                client->pendingTcpWrite.assign(frame.begin() + sent, frame.end());
                client->frameQueue.pop_front();
                return;
            }

            client->frameQueue.pop_front();
        }
    }

    void checkClientTimeouts()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto timeoutSec = std::chrono::seconds(m_config.sessionTimeoutSeconds);

        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& client : m_clients) {
            if (client->state != RtspSessionState::Closed) {
                if ((now - client->lastActivity) > timeoutSec) {
                    client->state = RtspSessionState::Closed;
                }
            }
        }
    }

    void cleanupClosedClients()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        bool changed { false };

        auto it = m_clients.begin();
        while (it != m_clients.end()) {
            if ((*it)->state == RtspSessionState::Closed) {
                if ((*it)->tcpSocket != Net::InvalidSocket) {
                    Net::closeSocket((*it)->tcpSocket);
                    (*it)->tcpSocket = Net::InvalidSocket;
                }
                if ((*it)->serverUdpSocket != Net::InvalidSocket) {
                    Net::closeSocket((*it)->serverUdpSocket);
                    (*it)->serverUdpSocket = Net::InvalidSocket;
                }
                it = m_clients.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }

        if (changed) {
            m_activeClientsCount.store(m_clients.size());
            updateStreamingClientsCount();
        }
    }

    void updateStreamingClientsCount()
    {
        std::size_t streaming { 0U };
        for (const auto& client : m_clients) {
            if (client->state == RtspSessionState::Streaming) {
                streaming++;
            }
        }
        m_streamingClientsCount.store(streaming);
    }

    void flushInternal()
    {
        if (m_accumulator.empty()) {
            return;
        }

        const std::size_t tsPacketsCount = m_accumulator.size() / kTsPacketSize;
        if (tsPacketsCount == 0U) {
            m_accumulator.clear();
            return;
        }

        const std::uint16_t seq = static_cast<std::uint16_t>(m_rtpSequence.fetch_add(1U) & 0xFFFFU);
        const std::uint32_t rtpTs = static_cast<std::uint32_t>((m_accumulatorPtsUs * 90ULL) / 1000ULL);
        const std::uint32_t ssrc = m_config.rtpSsrc;

        // Build RFC 3550 RTP Header (12 bytes)
        std::array<std::uint8_t, kRtpHeaderSize> rtpHeader {};
        rtpHeader[0] = kRtpVersion2;
        rtpHeader[1] = kRtpPayloadMp2t;
        rtpHeader[2] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
        rtpHeader[3] = static_cast<std::uint8_t>(seq & 0xFFU);
        rtpHeader[4] = static_cast<std::uint8_t>((rtpTs >> 24U) & 0xFFU);
        rtpHeader[5] = static_cast<std::uint8_t>((rtpTs >> 16U) & 0xFFU);
        rtpHeader[6] = static_cast<std::uint8_t>((rtpTs >> 8U) & 0xFFU);
        rtpHeader[7] = static_cast<std::uint8_t>(rtpTs & 0xFFU);
        rtpHeader[8] = static_cast<std::uint8_t>((ssrc >> 24U) & 0xFFU);
        rtpHeader[9] = static_cast<std::uint8_t>((ssrc >> 16U) & 0xFFU);
        rtpHeader[10] = static_cast<std::uint8_t>((ssrc >> 8U) & 0xFFU);
        rtpHeader[11] = static_cast<std::uint8_t>(ssrc & 0xFFU);

        const std::size_t rtpTotalLen = kRtpHeaderSize + m_accumulator.size();

        // Dispatch to all streaming clients
        for (auto& client : m_clients) {
            if (client->state != RtspSessionState::Streaming) {
                continue;
            }

            if (client->transportMode == RtspTransportMode::TcpInterleaved) {
                // Prepend 4-byte '$' framing header: $ + channel + length
                std::vector<std::uint8_t> frame;
                frame.reserve(kInterleavedHeaderSize + rtpTotalLen);
                frame.push_back(kInterleavedMagic);
                frame.push_back(client->rtpChannel);
                frame.push_back(static_cast<std::uint8_t>((rtpTotalLen >> 8U) & 0xFFU));
                frame.push_back(static_cast<std::uint8_t>(rtpTotalLen & 0xFFU));
                frame.insert(frame.end(), rtpHeader.begin(), rtpHeader.end());
                frame.insert(frame.end(), m_accumulator.begin(), m_accumulator.end());

                if (client->frameQueue.size() >= m_config.maxClientQueuePackets) {
                    client->frameQueue.pop_front();
                    std::lock_guard<std::mutex> statsLock(m_statsMutex);
                    m_stats.droppedPacketsSlowClients++;
                }
                client->frameQueue.push_back(std::move(frame));
                handleClientWrite(client);
            } else if (client->transportMode == RtspTransportMode::UdpUnicast) {
                if (client->serverUdpSocket != Net::InvalidSocket) {
                    std::vector<std::uint8_t> dgram;
                    dgram.reserve(rtpTotalLen);
                    dgram.insert(dgram.end(), rtpHeader.begin(), rtpHeader.end());
                    dgram.insert(dgram.end(), m_accumulator.begin(), m_accumulator.end());

                    static_cast<void>(::sendto(client->serverUdpSocket, reinterpret_cast<const char*>(dgram.data()),
                        static_cast<Net::SockBufLenType>(dgram.size()), 0,
                        reinterpret_cast<const struct sockaddr*>(&client->clientUdpDest),
                        sizeof(client->clientUdpDest)));
                }
            }
        }

        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.rtpPacketsDispatched++;
            m_stats.bytesTransmitted += rtpTotalLen;
            m_bytesSinceLastCalc += rtpTotalLen;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastBitrateCalcTime).count();
            if (elapsedMs >= 500) {
                const double bits = static_cast<double>(m_bytesSinceLastCalc) * 8.0;
                m_stats.currentBitrateKbps = bits / static_cast<double>(elapsedMs);
                m_bytesSinceLastCalc = 0U;
                m_lastBitrateCalcTime = now;
            }
        }

        m_accumulator.clear();
        m_accumulatorPtsUs = 0U;
    }

    RtspServerConfig m_config {};
    mutable std::mutex m_mutex {};
    mutable std::mutex m_statsMutex {};

    std::atomic<bool> m_running { false };
    Net::SocketHandle m_listenSocket { Net::InvalidSocket };
    std::thread m_workerThread {};

    std::vector<std::shared_ptr<ClientSession>> m_clients {};
    std::atomic<std::size_t> m_activeClientsCount { 0U };
    std::atomic<std::size_t> m_streamingClientsCount { 0U };

    std::vector<std::uint8_t> m_accumulator {};
    std::uint64_t m_accumulatorPtsUs { 0U };
    std::atomic<std::uint32_t> m_rtpSequence { 1U };

    RtspServerStats m_stats {};
    std::chrono::steady_clock::time_point m_lastBitrateCalcTime { std::chrono::steady_clock::now() };
    std::size_t m_bytesSinceLastCalc { 0U };
};

RtspServerPublisher::RtspServerPublisher(RtspServerConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config)))
{
}

RtspServerPublisher::~RtspServerPublisher() = default;

bool RtspServerPublisher::start()
{
    return m_impl->start();
}

void RtspServerPublisher::stop()
{
    m_impl->stop();
}

bool RtspServerPublisher::isRunning() const noexcept
{
    return m_impl->isRunning();
}

bool RtspServerPublisher::pushPacket(const std::uint8_t* packet, std::size_t size, std::uint64_t ptsUs)
{
    return m_impl->pushPacket(packet, size, ptsUs);
}

std::size_t RtspServerPublisher::pushPackets(const std::uint8_t* buffer, std::size_t size, std::uint64_t ptsUs)
{
    return m_impl->pushPackets(buffer, size, ptsUs);
}

void RtspServerPublisher::flush()
{
    m_impl->flush();
}

RtspServerConfig RtspServerPublisher::config() const
{
    return m_impl->config();
}

RtspServerStats RtspServerPublisher::stats() const
{
    return m_impl->stats();
}

void RtspServerPublisher::resetStats() noexcept
{
    m_impl->resetStats();
}

std::string RtspServerPublisher::streamUrl(std::string_view hostname) const
{
    return m_impl->streamUrl(hostname);
}

} // namespace Transport
