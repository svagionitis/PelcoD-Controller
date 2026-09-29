/// @file TestNmeaUdpEndpoint.cpp
/// @brief Unit tests for NMEA 0183 UDP transceiver.

#include "network/NmeaUdpEndpoint.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <string>

using namespace Nmea::Network;

TEST(TestNmeaUdpEndpoint, LoopbackSendAndReceive)
{
    static constexpr std::uint16_t kPortA = 10120U;
    static constexpr std::uint16_t kPortB = 10121U;

    // Endpoint A listens on kPortA, sends to kPortB on loopback
    NmeaUdpEndpoint epA(kPortA, "127.0.0.1", kPortB);

    // Endpoint B listens on kPortB, sends to kPortA on loopback
    NmeaUdpEndpoint epB(kPortB, "127.0.0.1", kPortA);

    ASSERT_TRUE(epA.open());
    ASSERT_TRUE(epB.open());
    EXPECT_TRUE(epA.isOpen());
    EXPECT_TRUE(epB.isOpen());

    std::promise<std::string> bReceivedPromise;
    auto bReceivedFuture = bReceivedPromise.get_future();

    epB.setDataCallback([&bReceivedPromise](const std::vector<std::uint8_t>& data) {
        bReceivedPromise.set_value(std::string(data.begin(), data.end()));
    });

    const std::string testSentence = "$HEHDT,224.5,T*25";
    EXPECT_TRUE(epA.sendSentence(testSentence));

    ASSERT_EQ(bReceivedFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    const std::string receivedText = bReceivedFuture.get();
    EXPECT_NE(receivedText.find("$HEHDT,224.5,T"), std::string::npos);

    epA.close();
    epB.close();
    EXPECT_FALSE(epA.isOpen());
    EXPECT_FALSE(epB.isOpen());
}
