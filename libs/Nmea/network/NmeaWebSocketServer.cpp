/// @file NmeaWebSocketServer.cpp
/// @brief Implementation of RFC 6455 compliant WebSocket server for marine telemetry.

#include "NmeaWebSocketServer.h"
#include "Sha1.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace Nmea::Network {

NmeaWebSocketServer::NmeaWebSocketServer(std::uint16_t port, std::string bindIp)
    : m_port { port }
    , m_bindIp { std::move(bindIp) }
{
}

NmeaWebSocketServer::~NmeaWebSocketServer()
{
    stop();
}

bool NmeaWebSocketServer::start()
{
    if (m_running.load()) {
        return true;
    }

    Transport::Net::ensureWinsockInitialized();

    const auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == Transport::Net::InvalidSocket) {
        return false;
    }

    int opt { 1 };
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    if (m_bindIp == "0.0.0.0" || m_bindIp.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        ::inet_pton(AF_INET, m_bindIp.c_str(), &addr.sin_addr);
    }

    if (::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        Transport::Net::closeSocket(s);
        return false;
    }

    if (::listen(s, SOMAXCONN) != 0) {
        Transport::Net::closeSocket(s);
        return false;
    }

    Transport::Net::setNonBlocking(s, true);
    m_listenSocket.store(s);
    m_running.store(true);

    m_serverThread = std::thread(&NmeaWebSocketServer::workerLoop, this);
    return true;
}

void NmeaWebSocketServer::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }

    const auto listenSock = m_listenSocket.exchange(Transport::Net::InvalidSocket);
    if (listenSock != Transport::Net::InvalidSocket) {
        Transport::Net::closeSocket(listenSock);
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    std::lock_guard<std::mutex> lock { m_clientMutex };
    for (auto& client : m_clients) {
        if (client.socket != Transport::Net::InvalidSocket) {
            Transport::Net::closeSocket(client.socket);
            client.socket = Transport::Net::InvalidSocket;
        }
    }
    m_clients.clear();
}

bool NmeaWebSocketServer::isRunning() const noexcept
{
    return m_running.load();
}

std::uint16_t NmeaWebSocketServer::getPort() const noexcept
{
    return m_port;
}

std::size_t NmeaWebSocketServer::clientCount() const
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    return m_clients.size();
}

void NmeaWebSocketServer::setMessageCallback(MessageCallback cb)
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    m_messageCallback = std::move(cb);
}

std::size_t NmeaWebSocketServer::broadcastJson(std::string_view jsonText)
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    std::size_t sentCount { 0U };

    for (auto& client : m_clients) {
        if (client.socket != Transport::Net::InvalidSocket && client.isHandshakeDone) {
            sendFrame(client.socket, jsonText, 0x01U); // Text frame
            ++sentCount;
        }
    }
    return sentCount;
}

void NmeaWebSocketServer::sendFrame(Transport::Net::SocketHandle s, std::string_view payload, std::uint8_t opcode)
{
    std::vector<std::uint8_t> frame {};
    frame.push_back(static_cast<std::uint8_t>(0x80U | (opcode & 0x0FU))); // FIN = 1 + Opcode

    const auto len = payload.size();
    if (len <= 125U) {
        frame.push_back(static_cast<std::uint8_t>(len));
    } else if (len <= 65535U) {
        frame.push_back(126U);
        frame.push_back(static_cast<std::uint8_t>((len >> 8U) & 0xFFU));
        frame.push_back(static_cast<std::uint8_t>(len & 0xFFU));
    } else {
        frame.push_back(127U);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<std::uint8_t>(
                (static_cast<std::uint64_t>(len) >> (static_cast<unsigned int>(i) * 8U)) & 0xFFU));
        }
    }

    const auto* pPayload = reinterpret_cast<const std::uint8_t*>(payload.data());
    frame.insert(frame.end(), pPayload, pPayload + payload.size());

    ::send(s, reinterpret_cast<const char*>(frame.data()), static_cast<Transport::Net::SockBufLenType>(frame.size()),
        Transport::Net::SendFlags);
}

void NmeaWebSocketServer::workerLoop()
{
    while (m_running.load()) {
        const auto listenSock = m_listenSocket.load();
        if (listenSock == Transport::Net::InvalidSocket) {
            break;
        }

        // Accept new connections
        sockaddr_in clientAddr {};
        auto addrLen = static_cast<Transport::Net::SockOptLenType>(sizeof(clientAddr));
        const auto clientSock = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSock != Transport::Net::InvalidSocket) {
            Transport::Net::setNonBlocking(clientSock, true);
            WsClient client {};
            client.id = m_nextClientId++;
            client.socket = clientSock;

            std::lock_guard<std::mutex> lock { m_clientMutex };
            m_clients.push_back(std::move(client));
        }

        // Poll existing clients
        std::vector<std::pair<std::uint32_t, std::string>> msgsToDispatch {};
        {
            std::lock_guard<std::mutex> lock { m_clientMutex };
            for (auto it = m_clients.begin(); it != m_clients.end();) {
                if (!it->isHandshakeDone) {
                    if (!processHandshake(*it)) {
                        // Handshake failed or still incomplete
                        if (it->socket == Transport::Net::InvalidSocket) {
                            it = m_clients.erase(it);
                            continue;
                        }
                    }
                } else {
                    std::vector<std::string> msgs {};
                    readClientFrames(*it, msgs);
                    for (auto& m : msgs) {
                        msgsToDispatch.emplace_back(it->id, std::move(m));
                    }
                    if (it->socket == Transport::Net::InvalidSocket) {
                        it = m_clients.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }

        if (m_messageCallback) {
            for (const auto& [cid, msg] : msgsToDispatch) {
                m_messageCallback(cid, msg);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

bool NmeaWebSocketServer::processHandshake(WsClient& client)
{
    std::array<char, 2048> buf {};
    const auto n = ::recv(client.socket, buf.data(), static_cast<Transport::Net::SockBufLenType>(buf.size()), 0);

    if (n > 0) {
        client.rxBuffer.append(buf.data(), static_cast<std::size_t>(n));
    } else if (n == 0 || (!Transport::Net::isWouldBlock())) {
        Transport::Net::closeSocket(client.socket);
        client.socket = Transport::Net::InvalidSocket;
        return false;
    }

    const auto endOfHeader = client.rxBuffer.find("\r\n\r\n");
    if (endOfHeader == std::string::npos) {
        return false; // Incomplete HTTP header
    }

    // Locate Sec-WebSocket-Key
    static constexpr std::string_view kKeyHeader = "Sec-WebSocket-Key:";
    const auto keyPos = client.rxBuffer.find(kKeyHeader);
    if (keyPos == std::string_view::npos) {
        Transport::Net::closeSocket(client.socket);
        client.socket = Transport::Net::InvalidSocket;
        return false;
    }

    auto valStart = keyPos + kKeyHeader.size();
    while (valStart < endOfHeader && (client.rxBuffer[valStart] == ' ' || client.rxBuffer[valStart] == '\t')) {
        ++valStart;
    }
    const auto valEnd = client.rxBuffer.find("\r\n", valStart);
    if (valEnd == std::string_view::npos || valEnd > endOfHeader) {
        Transport::Net::closeSocket(client.socket);
        client.socket = Transport::Net::InvalidSocket;
        return false;
    }

    const std::string clientKey = client.rxBuffer.substr(valStart, valEnd - valStart);
    const std::string acceptToken = Sha1::computeWebSocketAccept(clientKey);

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: "
        + acceptToken + "\r\n\r\n";

    ::send(client.socket, response.data(), static_cast<Transport::Net::SockBufLenType>(response.size()),
        Transport::Net::SendFlags);

    client.rxBuffer.erase(0U, endOfHeader + 4U);
    client.isHandshakeDone = true;
    return true;
}

void NmeaWebSocketServer::readClientFrames(WsClient& client, std::vector<std::string>& msgsOut)
{
    std::array<char, 4096> buf {};
    const auto n = ::recv(client.socket, buf.data(), static_cast<Transport::Net::SockBufLenType>(buf.size()), 0);

    if (n > 0) {
        client.rxBuffer.append(buf.data(), static_cast<std::size_t>(n));
    } else if (n == 0 || (!Transport::Net::isWouldBlock())) {
        Transport::Net::closeSocket(client.socket);
        client.socket = Transport::Net::InvalidSocket;
        return;
    }

    while (client.rxBuffer.size() >= 2U) {
        const auto b0 = static_cast<std::uint8_t>(client.rxBuffer[0]);
        const auto b1 = static_cast<std::uint8_t>(client.rxBuffer[1]);

        const std::uint8_t opcode = b0 & 0x0FU;
        const bool isMasked = (b1 & 0x80U) != 0U;
        std::uint64_t payloadLen = b1 & 0x7FU;

        std::size_t headerSize = 2U;
        if (payloadLen == 126U) {
            if (client.rxBuffer.size() < 4U) {
                break;
            }
            payloadLen
                = (static_cast<std::uint8_t>(client.rxBuffer[2]) << 8U) | static_cast<std::uint8_t>(client.rxBuffer[3]);
            headerSize = 4U;
        } else if (payloadLen == 127U) {
            if (client.rxBuffer.size() < 10U) {
                break;
            }
            payloadLen = 0U;
            for (std::size_t i = 0; i < 8U; ++i) {
                payloadLen = (payloadLen << 8U) | static_cast<std::uint8_t>(client.rxBuffer[2U + i]);
            }
            headerSize = 10U;
        }

        std::array<std::uint8_t, 4> maskKey {};
        if (isMasked) {
            if (client.rxBuffer.size() < headerSize + 4U) {
                break;
            }
            for (std::size_t i = 0; i < 4U; ++i) {
                maskKey[i] = static_cast<std::uint8_t>(client.rxBuffer[headerSize + i]);
            }
            headerSize += 4U;
        }

        if (client.rxBuffer.size() < headerSize + payloadLen) {
            break; // Await full frame payload
        }

        std::string unmaskedPayload {};
        unmaskedPayload.reserve(payloadLen);
        for (std::size_t i = 0; i < payloadLen; ++i) {
            auto b = static_cast<std::uint8_t>(client.rxBuffer[headerSize + i]);
            if (isMasked) {
                b ^= maskKey[i % 4U];
            }
            unmaskedPayload.push_back(static_cast<char>(b));
        }

        client.rxBuffer.erase(0U, headerSize + payloadLen);

        if (opcode == 0x08U) { // Close
            Transport::Net::closeSocket(client.socket);
            client.socket = Transport::Net::InvalidSocket;
            break;
        } else if (opcode == 0x09U) { // Ping
            sendFrame(client.socket, unmaskedPayload, 0x0AU); // Pong
        } else if (opcode == 0x01U) { // Text
            msgsOut.push_back(std::move(unmaskedPayload));
        }
    }
}

} // namespace Nmea::Network
