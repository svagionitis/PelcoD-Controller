/// @file TestNmeaTcpServer.cpp
/// @brief Unit tests for multi-client NMEA 0183 TCP server.

#include "network/NmeaTcpServer.h"
#include "TcpTransport.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <string>

using namespace Nmea::Network;

TEST(TestNmeaTcpServer, LifecycleAndBroadcast)
{
    static constexpr std::uint16_t kPort = 10115U;
    NmeaTcpServer server(kPort, "127.0.0.1");

    ASSERT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());
    EXPECT_EQ(server.clientCount(), 0U);

    // Promise for receiving client message
    std::promise<std::string> clientMsgPromise;
    auto clientMsgFuture = clientMsgPromise.get_future();

    server.setSentenceCallback([&clientMsgPromise](std::uint32_t /*clientId*/, std::string_view sentence) {
        clientMsgPromise.set_value(std::string { sentence });
    });

    // Connect a client via TcpTransport
    Transport::TcpTransport client("127.0.0.1", kPort);
    ASSERT_TRUE(client.open());

    // Allow time for server to accept
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(server.clientCount(), 1U);

    // Setup client RX listener
    std::promise<std::string> serverBroadcastPromise;
    auto serverBroadcastFuture = serverBroadcastPromise.get_future();

    client.setDataCallback([&serverBroadcastPromise](const std::vector<std::uint8_t>& data) {
        serverBroadcastPromise.set_value(std::string(data.begin(), data.end()));
    });

    // 1. Broadcast from server to client
    const std::string testSentence = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
    const std::size_t dispatched = server.broadcastSentence(testSentence);
    EXPECT_EQ(dispatched, 1U);

    ASSERT_EQ(serverBroadcastFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string receivedByClient = serverBroadcastFuture.get();
    EXPECT_NE(receivedByClient.find("$GPRMC"), std::string::npos);

    // 2. Client sends sentence to server
    const std::string clientCmd = "$PFLIR,cmd,pan,10.5*2B\r\n";
    const auto* pCmd = reinterpret_cast<const std::uint8_t*>(clientCmd.data());
    EXPECT_TRUE(client.sendData(std::vector<std::uint8_t>(pCmd, pCmd + clientCmd.size())));

    ASSERT_EQ(clientMsgFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string receivedByServer = clientMsgFuture.get();
    EXPECT_EQ(receivedByServer, "$PFLIR,cmd,pan,10.5*2B");

    // 3. Clean disconnect
    client.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.stop();
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(server.clientCount(), 0U);
}
