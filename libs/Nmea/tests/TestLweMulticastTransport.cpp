#include "NmeaChecksum.h"
#include "NmeaTagBlockParser.h"
#include "lwe/LweMulticastTransport.h"
#include "lwe/LweTypes.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Nmea::Lwe {

TEST(TestLweMulticastTransport, StandardGroupResolution)
{
    EXPECT_EQ(getTransmissionGroupIp(TransmissionGroup::Misc), "239.192.0.1");
    EXPECT_EQ(getTransmissionGroupPort(TransmissionGroup::Misc), 60001U);
    EXPECT_EQ(getTransmissionGroupName(TransmissionGroup::Misc), "MISC");

    EXPECT_EQ(getTransmissionGroupIp(TransmissionGroup::Tgtd), "239.192.0.2");
    EXPECT_EQ(getTransmissionGroupPort(TransmissionGroup::Tgtd), 60002U);
    EXPECT_EQ(getTransmissionGroupName(TransmissionGroup::Tgtd), "TGTD");

    EXPECT_EQ(getTransmissionGroupIp(TransmissionGroup::Satd), "239.192.0.3");
    EXPECT_EQ(getTransmissionGroupPort(TransmissionGroup::Satd), 60003U);
    EXPECT_EQ(getTransmissionGroupName(TransmissionGroup::Satd), "SATD");

    EXPECT_EQ(getTransmissionGroupIp(TransmissionGroup::Navd), "239.192.0.4");
    EXPECT_EQ(getTransmissionGroupPort(TransmissionGroup::Navd), 60004U);
    EXPECT_EQ(getTransmissionGroupName(TransmissionGroup::Navd), "NAVD");
}

TEST(TestLweMulticastTransport, LifecycleOpenClose)
{
    // Use custom high port to avoid collision with production bridges
    LweMulticastTransport transport("239.192.0.2", 61002U, "127.0.0.1", "RA0001");
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(transport.getGroupAddress(), "239.192.0.2");
    EXPECT_EQ(transport.getPort(), 61002U);
    EXPECT_EQ(transport.getSystemId(), "RA0001");

    ASSERT_TRUE(transport.open());
    EXPECT_TRUE(transport.isOpen());

    transport.close();
    EXPECT_FALSE(transport.isOpen());
}

TEST(TestLweMulticastTransport, SocketReuseMultipleListeners)
{
    // Two independent listeners on the same multicast group & port must bind without error (SO_REUSEADDR/SO_REUSEPORT)
    LweMulticastTransport listenerA("239.192.0.2", 61003U, "127.0.0.1", "LS0001");
    LweMulticastTransport listenerB("239.192.0.2", 61003U, "127.0.0.1", "LS0002");

    ASSERT_TRUE(listenerA.open());
    ASSERT_TRUE(listenerB.open());

    EXPECT_TRUE(listenerA.isOpen());
    EXPECT_TRUE(listenerB.isOpen());

    listenerA.close();
    listenerB.close();
}

TEST(TestLweMulticastTransport, LoopbackTransmissionAndTagBlockInjection)
{
    // Setup receiver on local loopback interface
    LweMulticastTransport receiver("239.192.0.2", 61004U, "127.0.0.1", "RC0001");
    receiver.setLoopbackEnabled(true);

    std::atomic<bool> packetReceived { false };
    std::string receivedData {};
    receiver.setDataCallback([&](const std::vector<std::uint8_t>& data) {
        receivedData.assign(data.begin(), data.end());
        packetReceived = true;
    });

    ASSERT_TRUE(receiver.open());

    // Setup sender
    LweMulticastTransport sender("239.192.0.2", 61004U, "127.0.0.1", "RA0001");
    sender.setLoopbackEnabled(true);
    ASSERT_TRUE(sender.open());

    // Send standard sentence with automated tag block injection
    const std::string sentence
        = NmeaChecksum::frameSentence("RATTM,01,2.0,030.0,T,15.0,030.0,T,0.0,0.0,K,TARGET1,T,,123519,A");
    ASSERT_TRUE(sender.sendSentence(sentence, true));

    // Wait for loopback delivery
    for (int i = 0; i < 20 && !packetReceived.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    receiver.close();
    sender.close();

    ASSERT_TRUE(packetReceived.load());
    EXPECT_FALSE(receivedData.empty());

    // Verify Tag Block was attached
    NmeaTagBlock block {};
    std::string_view remainder {};
    ASSERT_TRUE(NmeaTagBlockParser::parse(receivedData, block, remainder));
    EXPECT_EQ(block.sourceId, "RA0001");
    EXPECT_EQ(block.lineCount, 1U);
    EXPECT_TRUE(remainder.find("$RATTM,01") != std::string_view::npos);
}

} // namespace Nmea::Lwe
