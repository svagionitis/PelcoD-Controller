/// @file TestMaritimeNetworkCoordinator.cpp
/// @brief Integration tests for MaritimeNetworkCoordinator, firewall filtering, and telemetry distribution.

#include "bam/BridgeAlertManager.h"
#include "network/Iec61162_460Firewall.h"
#include "network/MaritimeNetworkCoordinator.h"
#include "network/NmeaJsonSerializer.h"
#include "TcpTransport.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <string>

using namespace Nmea::Network;
using namespace Nmea::Bam;

TEST(TestMaritimeNetworkCoordinator, EndToEndOrchestration)
{
    static constexpr std::uint16_t kTcpPort = 10130U;
    static constexpr std::uint16_t kUdpPort = 10131U;
    static constexpr std::uint16_t kWsPort = 8095U;

    auto firewall = std::make_shared<Iec61162_460Firewall>();
    BamConfig cfg {};
    cfg.talkerId = "GP";
    auto bam = std::make_shared<BridgeAlertManager>(cfg);

    MaritimeNetworkCoordinator coordinator(firewall, bam, kTcpPort, kUdpPort, kWsPort);
    ASSERT_TRUE(coordinator.start());

    // Verify sub-components initialized
    EXPECT_NE(coordinator.tcpServer(), nullptr);
    EXPECT_NE(coordinator.udpEndpoint(), nullptr);
    EXPECT_NE(coordinator.webSocketServer(), nullptr);
    EXPECT_NE(coordinator.firewall(), nullptr);
    EXPECT_NE(coordinator.alertManager(), nullptr);

    // Register callback for inbound sentences
    std::promise<std::string> inboundPromise;
    auto inboundFuture = inboundPromise.get_future();

    coordinator.setInboundCallback([&inboundPromise](std::string_view source, std::string_view sentence) {
        inboundPromise.set_value(std::string { source } + ":" + std::string { sentence });
    });

    // Connect a TCP client
    Transport::TcpTransport client("127.0.0.1", kTcpPort);
    ASSERT_TRUE(client.open());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(coordinator.tcpServer()->clientCount(), 1U);

    // Client sends an NMEA sentence
    const std::string testSentence = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    const auto* pData = reinterpret_cast<const std::uint8_t*>(testSentence.data());
    EXPECT_TRUE(client.sendData(std::vector<std::uint8_t>(pData, pData + testSentence.size())));

    ASSERT_EQ(inboundFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string receivedResult = inboundFuture.get();
    EXPECT_NE(receivedResult.find("TCP:$GPRMC"), std::string::npos);

    // Broadcast telemetry to WebSocket
    const std::string vesselJson = NmeaJsonSerializer::serializeVessel(48.0, 11.0, 10.0, 90.0, 90.0);
    coordinator.broadcastTelemetry(vesselJson);

    client.close();
    coordinator.stop();
}
