/// @file RtspPassthroughBridge.cpp
/// @brief Direct Ingest RTSP/RTP bridge implementation demuxing H.264/H.265 with zero transcoding.

#include "RtspPassthroughBridge.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "RtpDepacketizer.h"
#include "Transport/SocketUtils.h"

namespace Video {

namespace {

    [[nodiscard]] bool startsWith(std::string_view str, std::string_view prefix) noexcept
    {
        if (str.size() < prefix.size()) {
            return false;
        }
        return str.substr(0U, prefix.size()) == prefix;
    }

    struct ParsedUrl {
        std::string host { };
        std::uint16_t port { 554U };
        std::string path { "/" };
        std::string username { };
        std::string password { };
        bool valid { false };
    };

    [[nodiscard]] ParsedUrl parseRtspUrl(std::string_view rawUrl)
    {
        ParsedUrl out { };
        if (!startsWith(rawUrl, "rtsp://")) {
            return out;
        }

        std::string_view remainder { rawUrl.substr(7U) };
        const std::size_t slashPos { remainder.find('/') };
        std::string_view authAndHost { (slashPos == std::string_view::npos) ? remainder
                                                                            : remainder.substr(0U, slashPos) };
        out.path = (slashPos == std::string_view::npos) ? "/" : std::string(remainder.substr(slashPos));

        const std::size_t atPos { authAndHost.find('@') };
        std::string_view hostPort { authAndHost };
        if (atPos != std::string_view::npos) {
            const std::string_view userPass { authAndHost.substr(0U, atPos) };
            hostPort = authAndHost.substr(atPos + 1U);
            const std::size_t colonPos { userPass.find(':') };
            if (colonPos != std::string_view::npos) {
                out.username = std::string(userPass.substr(0U, colonPos));
                out.password = std::string(userPass.substr(colonPos + 1U));
            } else {
                out.username = std::string(userPass);
            }
        }

        const std::size_t colonPortPos { hostPort.find(':') };
        if (colonPortPos != std::string_view::npos) {
            out.host = std::string(hostPort.substr(0U, colonPortPos));
            const std::string_view portStr { hostPort.substr(colonPortPos + 1U) };
            std::uint32_t parsedPort { 0U };
            for (const char c : portStr) {
                if ((c >= '0') && (c <= '9')) {
                    parsedPort = (parsedPort * 10U) + static_cast<std::uint32_t>(c - '0');
                } else {
                    return out;
                }
            }
            if ((parsedPort > 0U) && (parsedPort <= 65535U)) {
                out.port = static_cast<std::uint16_t>(parsedPort);
            } else {
                return out;
            }
        } else {
            out.host = std::string(hostPort);
            out.port = 554U;
        }

        out.valid = !out.host.empty();
        return out;
    }

} // namespace

class RtspPassthroughBridge::Impl {
public:
    explicit Impl(const RtspPassthroughConfig& config)
        : m_config(config)
        , m_depacketizer(config.preferredCodec)
    {
    }

    ~Impl()
    {
        stop();
    }

    void setUnitCallback(AccessUnitCallback callback)
    {
        m_depacketizer.setUnitCallback(std::move(callback));
    }

    void setChunkCallback(RawChunkCallback callback)
    {
        m_depacketizer.setChunkCallback(std::move(callback));
    }

    void setStateCallback(StateCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stateCallback = std::move(callback);
    }

    [[nodiscard]] bool start()
    {
        if (m_running.load()) {
            return true;
        }

        m_running.store(true);
        m_workerThread = std::thread(&Impl::runWorker, this);
        return true;
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            closeActiveSocket();
            m_cv.notify_all();
        }

        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        transitionState(RtspBridgeState::Disconnected, "Bridge stopped cleanly");
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_running.load();
    }

    [[nodiscard]] RtspPassthroughStats stats() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        RtspPassthroughStats s { m_stats };
        s.packetLossCount = m_depacketizer.packetLossCount();
        s.rtpPacketsReceived = m_depacketizer.packetsReceived();
        return s;
    }

private:
    RtspPassthroughConfig m_config;
    RtpDepacketizer m_depacketizer;

    StateCallback m_stateCallback { };
    mutable std::mutex m_mutex { };
    std::condition_variable m_cv { };
    std::thread m_workerThread { };
    std::atomic<bool> m_running { false };

    RtspPassthroughStats m_stats { };
    Transport::Net::SocketHandle m_socket { Transport::Net::InvalidSocket };

    std::uint32_t m_cseq { 1U };
    std::string m_sessionId { };
    std::uint8_t m_rtpChannel { 0U };
    std::string m_controlTrackUrl { };

    void transitionState(RtspBridgeState newState, std::string_view msg)
    {
        StateCallback cb { };
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.state = newState;
            cb = m_stateCallback;
        }
        if (cb) {
            cb(newState, msg);
        }
    }

    void closeActiveSocket() noexcept
    {
        if (m_socket != Transport::Net::InvalidSocket) {
            Transport::Net::closeSocket(m_socket);
            m_socket = Transport::Net::InvalidSocket;
        }
    }

    void runWorker()
    {
        Transport::Net::ensureWinsockInitialized();

        while (m_running.load()) {
            transitionState(RtspBridgeState::Connecting, "Connecting to RTSP source...");
            m_depacketizer.reset();

            const ParsedUrl parsed { parseRtspUrl(m_config.rtspUrl) };
            if (!parsed.valid) {
                transitionState(RtspBridgeState::Error, "Invalid RTSP URL format");
                if (!m_config.autoReconnect) {
                    break;
                }
                waitForReconnect();
                continue;
            }

            if (!connectTcp(parsed.host, parsed.port)) {
                transitionState(RtspBridgeState::Error, "TCP connection failed");
                if (!m_config.autoReconnect) {
                    break;
                }
                waitForReconnect();
                continue;
            }

            if (!executeHandshake(parsed)) {
                transitionState(RtspBridgeState::Error, "RTSP handshake negotiation failed");
                closeActiveSocket();
                if (!m_config.autoReconnect) {
                    break;
                }
                waitForReconnect();
                continue;
            }

            // Stream Ingestion Loop
            transitionState(RtspBridgeState::Playing, "RTSP stream active; demuxing RTP");
            runStreamingLoop();

            closeActiveSocket();
            if (!m_config.autoReconnect || !m_running.load()) {
                break;
            }

            transitionState(RtspBridgeState::Reconnecting, "Connection dropped; attempting reconnection...");
            waitForReconnect();
        }
    }

    void waitForReconnect()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(
            lock, std::chrono::milliseconds(m_config.reconnectDelayMs), [this]() { return !m_running.load(); });
    }

    [[nodiscard]] bool connectTcp(const std::string& host, std::uint16_t port)
    {
        closeActiveSocket();

        struct addrinfo hints { };
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* res { nullptr };
        const std::string portStr { std::to_string(port) };
        const int err { ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) };
        if ((err != 0) || (res == nullptr)) {
            return false;
        }

        Transport::Net::SocketHandle s { ::socket(res->ai_family, res->ai_socktype, res->ai_protocol) };
        if (s == Transport::Net::InvalidSocket) {
            ::freeaddrinfo(res);
            return false;
        }

        Transport::Net::setNonBlocking(s, true);
        const int connRes { ::connect(s, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) };
        ::freeaddrinfo(res);

        if (connRes < 0) {
            if (!Transport::Net::isConnectInProgress()) {
                Transport::Net::closeSocket(s);
                return false;
            }

            Transport::Net::PollFd pfd { };
            pfd.fd = s;
            pfd.events = POLLOUT;
            const int polled { Transport::Net::pollSockets(&pfd, 1U, static_cast<int>(m_config.connectTimeoutMs)) };
            if ((polled <= 0) || ((pfd.revents & POLLOUT) == 0)) {
                Transport::Net::closeSocket(s);
                return false;
            }
        }

        m_socket = s;
        return true;
    }

    [[nodiscard]] bool sendAll(std::string_view req)
    {
        if (m_socket == Transport::Net::InvalidSocket) {
            return false;
        }

        std::size_t totalSent { 0U };
        while (totalSent < req.size()) {
            const auto chunk = static_cast<Transport::Net::SockBufLenType>(req.size() - totalSent);
            const auto ret = ::send(m_socket, req.data() + totalSent, chunk, Transport::Net::SendFlags);
            if (ret <= 0) {
                if (Transport::Net::isWouldBlock()) {
                    Transport::Net::PollFd pfd { };
                    pfd.fd = m_socket;
                    pfd.events = POLLOUT;
                    if (Transport::Net::pollSockets(&pfd, 1U, 1000) <= 0) {
                        return false;
                    }
                    continue;
                }
                return false;
            }
            totalSent += static_cast<std::size_t>(ret);
        }
        return true;
    }

    [[nodiscard]] std::string readResponse(std::uint32_t timeoutMs = 3000U)
    {
        std::string response { };
        char buffer[4096] { };

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (m_running.load()) {
            const auto remaining
                = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
                      .count();
            if (remaining <= 0) {
                break;
            }

            Transport::Net::PollFd pfd { };
            pfd.fd = m_socket;
            pfd.events = POLLIN;
            const int polled { Transport::Net::pollSockets(&pfd, 1U, static_cast<int>(remaining)) };
            if (polled <= 0) {
                break;
            }

            const auto ret = ::recv(m_socket, buffer, sizeof(buffer), 0);
            if (ret <= 0) {
                if (Transport::Net::isWouldBlock()) {
                    continue;
                }
                break;
            }

            response.append(buffer, static_cast<std::size_t>(ret));

            // Check if full HTTP/RTSP header completed
            const std::size_t dblCrLf { response.find("\r\n\r\n") };
            if (dblCrLf != std::string::npos) {
                // Check if Content-Length specified
                const std::size_t clPos { response.find("Content-Length:") };
                if (clPos != std::string::npos) {
                    std::size_t clEnd { response.find("\r\n", clPos) };
                    if (clEnd != std::string::npos) {
                        const std::string clStr { response.substr(clPos + 15U, clEnd - (clPos + 15U)) };
                        const auto contentLen = static_cast<std::size_t>(std::stoul(clStr));
                        const std::size_t headerBytes { dblCrLf + 4U };
                        if (response.size() >= (headerBytes + contentLen)) {
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        }

        return response;
    }

    [[nodiscard]] bool executeHandshake(const ParsedUrl& /*parsed*/)
    {
        m_cseq = 1U;

        // 1. OPTIONS
        std::string req { "OPTIONS " + m_config.rtspUrl
            + " RTSP/1.0\r\n"
              "CSeq: "
            + std::to_string(m_cseq++)
            + "\r\n"
              "User-Agent: PelcoD-RtspPassthrough\r\n\r\n" };
        if (!sendAll(req)) {
            return false;
        }

        std::string resp { readResponse() };
        if (resp.find("200 OK") == std::string::npos) {
            return false;
        }

        // 2. DESCRIBE
        transitionState(RtspBridgeState::Describing, "Negotiating stream description (SDP)...");
        req = "DESCRIBE " + m_config.rtspUrl
            + " RTSP/1.0\r\n"
              "CSeq: "
            + std::to_string(m_cseq++)
            + "\r\n"
              "Accept: application/sdp\r\n"
              "User-Agent: PelcoD-RtspPassthrough\r\n\r\n";
        if (!sendAll(req)) {
            return false;
        }

        resp = readResponse();
        if (resp.find("200 OK") == std::string::npos) {
            return false;
        }

        parseSdp(resp);

        // 3. SETUP
        transitionState(RtspBridgeState::SettingUp, "Configuring RTP transport session...");
        std::string setupUrl { m_controlTrackUrl.empty() ? m_config.rtspUrl : m_controlTrackUrl };
        if (setupUrl.find("://") == std::string::npos) {
            // Relative URL, concatenate with base URL
            if (m_config.rtspUrl.back() == '/') {
                setupUrl = m_config.rtspUrl + setupUrl;
            } else {
                setupUrl = m_config.rtspUrl + "/" + setupUrl;
            }
        }

        req = "SETUP " + setupUrl
            + " RTSP/1.0\r\n"
              "CSeq: "
            + std::to_string(m_cseq++)
            + "\r\n"
              "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
              "User-Agent: PelcoD-RtspPassthrough\r\n\r\n";
        if (!sendAll(req)) {
            return false;
        }

        resp = readResponse();
        if (resp.find("200 OK") == std::string::npos) {
            return false;
        }

        const std::size_t sessPos { resp.find("Session:") };
        if (sessPos != std::string::npos) {
            std::size_t sessEnd { resp.find_first_of(";\r\n", sessPos + 8U) };
            if (sessEnd != std::string::npos) {
                m_sessionId = resp.substr(sessPos + 8U, sessEnd - (sessPos + 8U));
                // Trim whitespace
                m_sessionId.erase(0, m_sessionId.find_first_not_of(" \t"));
            }
        }

        // 4. PLAY
        req = "PLAY " + m_config.rtspUrl
            + " RTSP/1.0\r\n"
              "CSeq: "
            + std::to_string(m_cseq++)
            + "\r\n"
              "Session: "
            + m_sessionId
            + "\r\n"
              "User-Agent: PelcoD-RtspPassthrough\r\n\r\n";
        if (!sendAll(req)) {
            return false;
        }

        resp = readResponse();
        return resp.find("200 OK") != std::string::npos;
    }

    void parseSdp(std::string_view sdp)
    {
        std::size_t start { 0U };
        while (start < sdp.size()) {
            const std::size_t end { sdp.find("\r\n", start) };
            const std::string_view line { (end == std::string_view::npos) ? sdp.substr(start)
                                                                          : sdp.substr(start, end - start) };
            if (startsWith(line, "a=rtpmap:")) {
                if ((line.find("H264") != std::string_view::npos) || (line.find("h264") != std::string_view::npos)) {
                    m_depacketizer.setCodec(Klv::VideoCodec::H264);
                } else if ((line.find("H265") != std::string_view::npos)
                    || (line.find("h265") != std::string_view::npos) || (line.find("HEVC") != std::string_view::npos)) {
                    m_depacketizer.setCodec(Klv::VideoCodec::H265);
                }
            } else if (startsWith(line, "a=fmtp:")) {
                const std::size_t spropPos { line.find("sprop-parameter-sets=") };
                if (spropPos != std::string_view::npos) {
                    const std::size_t spropEnd { line.find_first_of(";\r\n", spropPos + 21U) };
                    const std::string_view spropStr { (spropEnd == std::string_view::npos)
                            ? line.substr(spropPos + 21U)
                            : line.substr(spropPos + 21U, spropEnd - (spropPos + 21U)) };
                    m_depacketizer.setParameterSets(RtpDepacketizer::parseSprop(spropStr));
                }
            } else if (startsWith(line, "a=control:")) {
                m_controlTrackUrl = std::string(line.substr(10U));
            }

            if (end == std::string_view::npos) {
                break;
            }
            start = end + 2U;
        }
    }

    void runStreamingLoop()
    {
        std::vector<std::uint8_t> recvBuf { };
        recvBuf.reserve(65536U);
        std::uint8_t temp[8192] { };

        auto lastKeepalive = std::chrono::steady_clock::now();
        auto lastBitrateCalc = std::chrono::steady_clock::now();
        std::size_t bytesSinceLastBitrate { 0U };

        while (m_running.load()) {
            Transport::Net::PollFd pfd { };
            pfd.fd = m_socket;
            pfd.events = POLLIN;

            const int polled { Transport::Net::pollSockets(&pfd, 1U, 200) };
            if (polled < 0) {
                break;
            }

            if (polled > 0) {
                const auto recvd = ::recv(m_socket, reinterpret_cast<char*>(temp), sizeof(temp), 0);
                if (recvd <= 0) {
                    if (Transport::Net::isWouldBlock()) {
                        continue;
                    }
                    break; // Connection closed or socket failure
                }

                const auto recvdSize = static_cast<std::size_t>(recvd);
                recvBuf.insert(recvBuf.end(), temp, temp + recvdSize);
                bytesSinceLastBitrate += recvdSize;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stats.rtpBytesReceived += recvdSize;
                }

                // Process RFC 2326 Interleaved frames: '$' <1-byte channel> <2-byte length> <payload>
                while (recvBuf.size() >= 4U) {
                    if (recvBuf[0] != 0x24U) {
                        // Scan forward for next '$' delimiter
                        auto it = std::find(recvBuf.begin() + 1U, recvBuf.end(), 0x24U);
                        recvBuf.erase(recvBuf.begin(), it);
                        if (recvBuf.size() < 4U) {
                            break;
                        }
                    }

                    const std::uint8_t channel { recvBuf[1] };
                    const auto frameLen = static_cast<std::size_t>(
                        (static_cast<std::size_t>(recvBuf[2]) << 8U) | static_cast<std::size_t>(recvBuf[3]));

                    if (recvBuf.size() < (4U + frameLen)) {
                        break; // Wait for full frame
                    }

                    if (channel == m_rtpChannel) {
                        const bool ok { m_depacketizer.processPacket(recvBuf.data() + 4U, frameLen) };
                        if (!ok) {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_stats.corruptPackets += 1U;
                        }
                    }

                    recvBuf.erase(recvBuf.begin(), recvBuf.begin() + static_cast<std::ptrdiff_t>(4U + frameLen));
                }
            }

            const auto now = std::chrono::steady_clock::now();

            // Periodic Keepalive
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeepalive).count()
                >= static_cast<std::int64_t>(m_config.keepaliveIntervalMs)) {
                lastKeepalive = now;
                const std::string keepaliveReq { "GET_PARAMETER " + m_config.rtspUrl
                    + " RTSP/1.0\r\n"
                      "CSeq: "
                    + std::to_string(m_cseq++)
                    + "\r\n"
                      "Session: "
                    + m_sessionId
                    + "\r\n"
                      "User-Agent: PelcoD-RtspPassthrough\r\n\r\n" };
                if (!sendAll(keepaliveReq)) {
                    break;
                }
            }

            // Bitrate Calculation
            const auto elapsedBitrateMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBitrateCalc).count();
            if (elapsedBitrateMs >= 1000) {
                lastBitrateCalc = now;
                const double kbps { (static_cast<double>(bytesSinceLastBitrate) * 8.0)
                    / static_cast<double>(elapsedBitrateMs) };
                bytesSinceLastBitrate = 0U;
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stats.currentBitrateKbps = kbps;
            }
        }
    }
};

RtspPassthroughBridge::RtspPassthroughBridge(const RtspPassthroughConfig& config)
    : m_impl(std::make_unique<Impl>(config))
{
}

RtspPassthroughBridge::~RtspPassthroughBridge() = default;

void RtspPassthroughBridge::setUnitCallback(AccessUnitCallback callback)
{
    m_impl->setUnitCallback(std::move(callback));
}

void RtspPassthroughBridge::setChunkCallback(RawChunkCallback callback)
{
    m_impl->setChunkCallback(std::move(callback));
}

void RtspPassthroughBridge::setStateCallback(StateCallback callback)
{
    m_impl->setStateCallback(std::move(callback));
}

[[nodiscard]] bool RtspPassthroughBridge::start()
{
    return m_impl->start();
}

void RtspPassthroughBridge::stop()
{
    m_impl->stop();
}

[[nodiscard]] bool RtspPassthroughBridge::isRunning() const noexcept
{
    return m_impl->isRunning();
}

[[nodiscard]] RtspPassthroughStats RtspPassthroughBridge::stats() const
{
    return m_impl->stats();
}

} // namespace Video
