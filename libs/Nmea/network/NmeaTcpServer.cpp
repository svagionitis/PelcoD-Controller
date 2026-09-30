/// @file NmeaTcpServer.cpp
/// @brief Implementation of multi-client TCP broadcast server for NMEA 0183 (port 10110).

#include "NmeaTcpServer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace Nmea::Network {

NmeaTcpServer::NmeaTcpServer(std::uint16_t port, std::string bindIp)
    : m_port { port }
    , m_bindIp { std::move(bindIp) }
{
}

NmeaTcpServer::~NmeaTcpServer()
{
    stop();
}

bool NmeaTcpServer::start()
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

    m_serverThread = std::thread(&NmeaTcpServer::serverLoop, this);
    return true;
}

void NmeaTcpServer::stop()
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

bool NmeaTcpServer::isRunning() const noexcept
{
    return m_running.load();
}

std::uint16_t NmeaTcpServer::getPort() const noexcept
{
    return m_port;
}

std::size_t NmeaTcpServer::broadcastSentence(std::string_view sentence)
{
    std::string formatted { sentence };
    if (formatted.empty() || (formatted.back() != '\n' && formatted.back() != '\r')) {
        formatted += "\r\n";
    }

    std::lock_guard<std::mutex> lock { m_clientMutex };
    std::size_t sentCount { 0U };

    for (auto& client : m_clients) {
        if (client.socket == Transport::Net::InvalidSocket) {
            continue;
        }
        const auto res = ::send(client.socket, formatted.data(),
            static_cast<Transport::Net::SockBufLenType>(formatted.size()), Transport::Net::SendFlags);
        if (res > 0) {
            client.bytesSent += static_cast<std::uint64_t>(res);
            ++sentCount;
        }
    }
    return sentCount;
}

void NmeaTcpServer::setSentenceCallback(SentenceCallback cb)
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    m_sentenceCallback = std::move(cb);
}

std::size_t NmeaTcpServer::clientCount() const
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    return m_clients.size();
}

std::vector<TcpClientInfo> NmeaTcpServer::getConnectedClients() const
{
    std::lock_guard<std::mutex> lock { m_clientMutex };
    std::vector<TcpClientInfo> result {};
    result.reserve(m_clients.size());
    for (const auto& c : m_clients) {
        result.push_back(TcpClientInfo { c.id, c.ip, c.port, c.connectTime, c.bytesSent, c.bytesReceived });
    }
    return result;
}

void NmeaTcpServer::serverLoop()
{
    while (m_running.load()) {
        const auto listenSock = m_listenSocket.load();
        if (listenSock == Transport::Net::InvalidSocket) {
            break;
        }

        // Accept any new incoming connections
        sockaddr_in clientAddr {};
        auto addrLen = static_cast<Transport::Net::SockOptLenType>(sizeof(clientAddr));
        const auto clientSock = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientSock != Transport::Net::InvalidSocket) {
            Transport::Net::setNonBlocking(clientSock, true);
            std::array<char, INET_ADDRSTRLEN> ipStr {};
            ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr.data(), ipStr.size());

            ConnectedClient newClient {};
            newClient.id = m_nextClientId++;
            newClient.socket = clientSock;
            newClient.ip = ipStr.data();
            newClient.port = ntohs(clientAddr.sin_port);
            newClient.connectTime = std::chrono::steady_clock::now();

            std::lock_guard<std::mutex> lock { m_clientMutex };
            m_clients.push_back(std::move(newClient));
        }

        // Poll and read from existing clients
        std::vector<std::pair<std::uint32_t, std::string>> sentencesToDispatch {};
        {
            std::lock_guard<std::mutex> lock { m_clientMutex };
            for (auto it = m_clients.begin(); it != m_clients.end();) {
                std::vector<std::string> sentencesOut {};
                processClientRx(*it, sentencesOut);

                for (auto& s : sentencesOut) {
                    sentencesToDispatch.emplace_back(it->id, std::move(s));
                }

                if (it->socket == Transport::Net::InvalidSocket) {
                    it = m_clients.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Dispatch outside client lock
        if (m_sentenceCallback) {
            for (const auto& [cid, s] : sentencesToDispatch) {
                m_sentenceCallback(cid, s);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void NmeaTcpServer::processClientRx(ConnectedClient& client, std::vector<std::string>& sentencesOut)
{
    std::array<char, 2048> buf {};
    const auto n = ::recv(client.socket, buf.data(), static_cast<Transport::Net::SockBufLenType>(buf.size()), 0);

    if (n > 0) {
        client.bytesReceived += static_cast<std::uint64_t>(n);
        client.rxBuffer.append(buf.data(), static_cast<std::size_t>(n));

        std::size_t newlinePos {};
        while ((newlinePos = client.rxBuffer.find('\n')) != std::string::npos) {
            std::string line = client.rxBuffer.substr(0U, newlinePos);
            client.rxBuffer.erase(0U, newlinePos + 1U);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                sentencesOut.push_back(std::move(line));
            }
        }
    } else if (n == 0) {
        // Graceful disconnect
        Transport::Net::closeSocket(client.socket);
        client.socket = Transport::Net::InvalidSocket;
    } else {
        if (!Transport::Net::isWouldBlock()) {
            Transport::Net::closeSocket(client.socket);
            client.socket = Transport::Net::InvalidSocket;
        }
    }
}

} // namespace Nmea::Network
