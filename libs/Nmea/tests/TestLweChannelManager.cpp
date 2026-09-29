#include "NmeaChecksum.h"
#include "NmeaDevice.h"
#include "NmeaTagBlockBuilder.h"
#include "lwe/LweChannelManager.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Nmea::Lwe {

TEST(TestLweChannelManager, ChannelLifecycleAndCount)
{
    LweChannelManager manager;
    EXPECT_EQ(manager.channelCount(), 0U);

    EXPECT_TRUE(manager.joinTransmissionGroup(TransmissionGroup::Tgtd, "127.0.0.1"));
    EXPECT_TRUE(manager.joinTransmissionGroup(TransmissionGroup::Satd, "127.0.0.1"));
    EXPECT_EQ(manager.channelCount(), 2U);

    // Joining same group again should be idempotent
    EXPECT_TRUE(manager.joinTransmissionGroup(TransmissionGroup::Tgtd, "127.0.0.1"));
    EXPECT_EQ(manager.channelCount(), 2U);

    auto agg = manager.getAggregatedTransport();
    ASSERT_NE(agg, nullptr);

    manager.leaveAll();
    EXPECT_EQ(manager.channelCount(), 0U);
}

TEST(TestLweChannelManager, SourceFiltering)
{
    LweChannelManager manager;
    EXPECT_TRUE(manager.joinCustomGroup("239.192.0.2", 61005U, "127.0.0.1"));

    // Only allow RA0001 (Radar 1)
    manager.setSourceFilter({ "RA0001" });

    auto agg = manager.getAggregatedTransport();
    std::atomic<int> receivedCount { 0 };
    agg->setDataCallback([&](const std::vector<std::uint8_t>&) { receivedCount.fetch_add(1); });

    ASSERT_TRUE(agg->open());

    // Setup senders
    LweMulticastTransport senderAuthorized("239.192.0.2", 61005U, "127.0.0.1", "RA0001");
    senderAuthorized.setLoopbackEnabled(true);
    ASSERT_TRUE(senderAuthorized.open());

    LweMulticastTransport senderUnauthorized("239.192.0.2", 61005U, "127.0.0.1", "RA0002");
    senderUnauthorized.setLoopbackEnabled(true);
    ASSERT_TRUE(senderUnauthorized.open());

    // Send unauthorized packet
    const std::string sUnauth
        = NmeaChecksum::frameSentence("RATTM,01,1.0,000.0,T,10.0,000.0,T,0.0,0.0,K,TARGET1,T,,123519,A");
    ASSERT_TRUE(senderUnauthorized.sendSentence(sUnauth, true));

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(receivedCount.load(), 0);

    // Send authorized packet
    const std::string sAuth
        = NmeaChecksum::frameSentence("RATTM,02,1.5,045.0,T,12.0,045.0,T,0.0,0.0,K,TARGET2,T,,123519,A");
    ASSERT_TRUE(senderAuthorized.sendSentence(sAuth, true));

    for (int i = 0; i < 20 && receivedCount.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    agg->close();
    senderAuthorized.close();
    senderUnauthorized.close();

    EXPECT_EQ(receivedCount.load(), 1);
}

TEST(TestLweChannelManager, NmeaDeviceIntegration)
{
    LweChannelManager manager;
    EXPECT_TRUE(manager.joinCustomGroup("239.192.0.2", 61006U, "127.0.0.1"));

    auto agg = manager.getAggregatedTransport();
    auto nmeaDevice = std::make_shared<NmeaDevice>(agg);
    ASSERT_TRUE(nmeaDevice->start());

    std::atomic<bool> radarTargetReceived { false };
    const auto subId = nmeaDevice->addRadarCallback([&](const TtmData& ttm) {
        if (ttm.targetNumber == 5U) {
            radarTargetReceived = true;
        }
    });
    (void)subId;

    LweMulticastTransport sender("239.192.0.2", 61006U, "127.0.0.1", "RA0001");
    sender.setLoopbackEnabled(true);
    ASSERT_TRUE(sender.open());

    const std::string ttmSentence
        = NmeaChecksum::frameSentence("RATTM,05,2.4,090.0,T,18.0,090.0,T,0.0,0.0,K,VESSEL5,T,,123519,A");
    ASSERT_TRUE(sender.sendSentence(ttmSentence, true));

    for (int i = 0; i < 20 && !radarTargetReceived.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    nmeaDevice->stop();
    sender.close();

    EXPECT_TRUE(radarTargetReceived.load());
}

} // namespace Nmea::Lwe
