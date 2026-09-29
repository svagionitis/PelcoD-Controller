#pragma once

/// @file NmeaTcpServer.h
/// @brief Multi-client TCP broadcast server for NMEA 0183 navigation software (marine port 10110).

#include "SocketUtils.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Nmea::Network {

/// @brief Client connection telemetry and statistics.
struct TcpClientInfo {
    std::uint32_t clientId { 0U };
    std::string peerIp {};
    std::uint16_t peerPort { 0U };
    std::chrono::steady_clock::time_point connectTime {};
    std::uint64_t bytesSent { 0U };
    std::uint64_t bytesReceived { 0U };
};

/// @class NmeaTcpServer
/// @brief Multi-client TCP broadcast server for NMEA 0183 navigation software (port 10110).
/// @details Listens for incoming connections from ECDIS, OpenCPN, radar displays, and web bridges.
///          Broadcasts navigation sentences to all clients and accepts bidirectional client inputs.
class NmeaTcpServer {
public:
    using SentenceCallback = std::function<void(std::uint32_t clientId, std::string_view sentence)>;

    explicit NmeaTcpServer(std::uint16_t port = 10110U, std::string bindIp = "0.0.0.0");
    ~NmeaTcpServer();

    // Non-copyable, non-movable
    NmeaTcpServer(const NmeaTcpServer&) = delete;
    NmeaTcpServer& operator=(const NmeaTcpServer&) = delete;
    NmeaTcpServer(NmeaTcpServer&&) = delete;
    NmeaTcpServer& operator=(NmeaTcpServer&&) = delete;

    /// @brief Starts listening and begins the accept/worker loop.
    /// @return True if server socket successfully bound and listening.
    [[nodiscard]] bool start();

    /// @brief Stops the server, closes all active client sockets, and terminates worker threads.
    void stop();

    /// @brief Checks if server is actively listening.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Broadcasts an NMEA 0183 sentence to all currently connected TCP clients.
    /// @param[in] sentence Formatted NMEA sentence (e.g. "$GPRMC,...*45\r\n").
    /// @return Number of clients sentence was dispatched to.
    std::size_t broadcastSentence(std::string_view sentence);

    /// @brief Sets callback invoked when a connected client sends an NMEA sentence.
    /// @param[in] cb Callback receiving client ID and sentence text.
    void setSentenceCallback(SentenceCallback cb);

    /// @brief Returns the count of actively connected TCP clients.
    [[nodiscard]] std::size_t clientCount() const;

    /// @brief Retrieves detailed connection info for all active clients.
    [[nodiscard]] std::vector<TcpClientInfo> getConnectedClients() const;

    /// @brief Returns the configured listening port.
    [[nodiscard]] std::uint16_t getPort() const noexcept;

private:
    struct ConnectedClient {
        std::uint32_t id { 0U };
        Transport::Net::SocketHandle socket { Transport::Net::InvalidSocket };
        std::string ip {};
        std::uint16_t port { 0U };
        std::chrono::steady_clock::time_point connectTime {};
        std::uint64_t bytesSent { 0U };
        std::uint64_t bytesReceived { 0U };
        std::string rxBuffer {};
    };

    void serverLoop();
    void processClientRx(ConnectedClient& client, std::vector<std::string>& sentencesOut);

    std::uint16_t m_port { 10110U };
    std::string m_bindIp { "0.0.0.0" };
    std::atomic<bool> m_running { false };
    std::atomic<Transport::Net::SocketHandle> m_listenSocket { Transport::Net::InvalidSocket };

    mutable std::mutex m_clientMutex {};
    std::vector<ConnectedClient> m_clients {};
    std::uint32_t m_nextClientId { 1U };
    SentenceCallback m_sentenceCallback {};
    std::thread m_serverThread {};
};

} // namespace Nmea::Network
