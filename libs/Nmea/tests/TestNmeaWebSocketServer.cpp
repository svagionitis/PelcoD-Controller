/// @file TestNmeaWebSocketServer.cpp
/// @brief Unit tests for RFC 6455 WebSocket server and JSON telemetry broadcasting.

#include "network/NmeaWebSocketServer.h"
#include "TcpTransport.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <string>

using namespace Nmea::Network;

TEST(TestNmeaWebSocketServer, HandshakeAndJsonStreaming)
{
    static constexpr std::uint16_t kPort = 8092U;
    NmeaWebSocketServer wsServer(kPort, "127.0.0.1");

    ASSERT_TRUE(wsServer.start());
    EXPECT_TRUE(wsServer.isRunning());

    std::promise<std::string> clientCmdPromise;
    auto clientCmdFuture = clientCmdPromise.get_future();

    wsServer.setMessageCallback([&clientCmdPromise](std::uint32_t /*clientId*/, std::string_view msg) {
        clientCmdPromise.set_value(std::string { msg });
    });

    // Connect raw TCP transport to perform WebSocket handshake
    Transport::TcpTransport client("127.0.0.1", kPort);
    ASSERT_TRUE(client.open());

    std::promise<std::string> handshakePromise;
    auto handshakeFuture = handshakePromise.get_future();

    client.setDataCallback([&handshakePromise](const std::vector<std::uint8_t>& data) {
        handshakePromise.set_value(std::string(data.begin(), data.end()));
    });

    // 1. Send HTTP Upgrade request
    const std::string upgradeReq =
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:8092\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    const auto* pReq = reinterpret_cast<const std::uint8_t*>(upgradeReq.data());
    EXPECT_TRUE(client.sendData(std::vector<std::uint8_t>(pReq, pReq + upgradeReq.size())));

    ASSERT_EQ(handshakeFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string handshakeResp = handshakeFuture.get();
    EXPECT_NE(handshakeResp.find("101 Switching Protocols"), std::string::npos);
    EXPECT_NE(handshakeResp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), std::string::npos);

    // 2. Server broadcasts JSON to client
    std::promise<std::string> jsonBroadcastPromise;
    auto jsonBroadcastFuture = jsonBroadcastPromise.get_future();

    client.setDataCallback([&jsonBroadcastPromise](const std::vector<std::uint8_t>& data) {
        // Skip 2-byte unmasked frame header (0x81, len)
        if (data.size() > 2U) {
            jsonBroadcastPromise.set_value(std::string(data.begin() + 2U, data.end()));
        }
    });

    const std::string testJson = "{\"type\":\"vessel\",\"lat\":37.7749,\"lon\":-122.4194}";
    const std::size_t sentCount = wsServer.broadcastJson(testJson);
    EXPECT_EQ(sentCount, 1U);

    ASSERT_EQ(jsonBroadcastFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string receivedJson = jsonBroadcastFuture.get();
    EXPECT_EQ(receivedJson, testJson);

    // 3. Client sends masked frame to server: "ping"
    // Frame: FIN|TEXT (0x81), MASKED|LEN (0x84), 4-byte mask (0x01,0x02,0x03,0x04), masked "ping"
    const std::string payload = "ping";
    std::vector<std::uint8_t> clientFrame {
        0x81U, // FIN + Text
        0x84U, // Masked + len 4
        0x01U, 0x02U, 0x03U, 0x04U // Mask key
    };
    for (std::size_t i = 0; i < payload.size(); ++i) {
        clientFrame.push_back(static_cast<std::uint8_t>(payload[i] ^ clientFrame[2U + (i % 4U)]));
    }
    EXPECT_TRUE(client.sendData(clientFrame));

    ASSERT_EQ(clientCmdFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(clientCmdFuture.get(), "ping");

    client.close();
    wsServer.stop();
    EXPECT_FALSE(wsServer.isRunning());
}
