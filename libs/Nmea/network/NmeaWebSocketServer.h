#pragma once

/// @file NmeaWebSocketServer.h
/// @brief RFC 6455 compliant WebSocket server for real-time marine telemetry streaming to HTML5 web dashboards.

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

/// @class NmeaWebSocketServer
/// @brief Pure C++17 RFC 6455 compliant WebSocket server streaming JSON telemetry to web browsers.
/// @details Implements standard HTTP upgrade handshake, text/binary framing, masking, and ping/pong keepalives.
///          Thread-safe with non-blocking polling across all connected browser clients.
class NmeaWebSocketServer {
public:
    using MessageCallback = std::function<void(std::uint32_t clientId, std::string_view jsonMessage)>;

    explicit NmeaWebSocketServer(std::uint16_t port = 8088U, std::string bindIp = "0.0.0.0");
    ~NmeaWebSocketServer();

    // Non-copyable, non-movable
    NmeaWebSocketServer(const NmeaWebSocketServer&) = delete;
    NmeaWebSocketServer& operator=(const NmeaWebSocketServer&) = delete;
    NmeaWebSocketServer(NmeaWebSocketServer&&) = delete;
    NmeaWebSocketServer& operator=(NmeaWebSocketServer&&) = delete;

    /// @brief Starts listening and accepts WebSocket connections.
    /// @return True if bound and listening successfully.
    [[nodiscard]] bool start();

    /// @brief Stops the server and closes all active WebSocket client channels.
    void stop();

    /// @brief Checks if server is actively running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Broadcasts a UTF-8 JSON text frame to all connected WebSocket clients.
    /// @param[in] jsonText Valid JSON payload string.
    /// @return Number of clients frame was transmitted to.
    std::size_t broadcastJson(std::string_view jsonText);

    /// @brief Sets callback for handling inbound JSON commands from web clients.
    /// @param[in] cb Callback invoked with client ID and payload.
    void setMessageCallback(MessageCallback cb);

    /// @brief Returns count of active connected WebSocket clients.
    [[nodiscard]] std::size_t clientCount() const;

    /// @brief Returns the configured listening port.
    [[nodiscard]] std::uint16_t getPort() const noexcept;

private:
    struct WsClient {
        std::uint32_t id { 0U };
        Transport::Net::SocketHandle socket { Transport::Net::InvalidSocket };
        bool isHandshakeDone { false };
        std::string rxBuffer {};
    };

    void workerLoop();
    [[nodiscard]] bool processHandshake(WsClient& client);
    void readClientFrames(WsClient& client, std::vector<std::string>& msgsOut);
    void sendFrame(Transport::Net::SocketHandle s, std::string_view payload, std::uint8_t opcode = 0x01U);

    std::uint16_t m_port { 8088U };
    std::string m_bindIp { "0.0.0.0" };
    std::atomic<bool> m_running { false };
    std::atomic<Transport::Net::SocketHandle> m_listenSocket { Transport::Net::InvalidSocket };

    mutable std::mutex m_clientMutex {};
    std::vector<WsClient> m_clients {};
    std::uint32_t m_nextClientId { 1U };
    MessageCallback m_messageCallback {};
    std::thread m_serverThread {};
};

} // namespace Nmea::Network
