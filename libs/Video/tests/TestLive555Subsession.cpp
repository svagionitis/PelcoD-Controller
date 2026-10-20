#include "Live555LiveSubsession.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <memory>
#include <string>
#include <vector>

#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <UsageEnvironment.hh>
#include <gtest/gtest.h>
#include <liveMedia.hh>

#include "Live555FrameQueue.h"
#include "Live555FrameTypes.h"
#include "Live555H264LiveSource.h"
#include "Live555Mp2tLiveSource.h"
#include "RtspServerConfig.h"

namespace Video {
namespace {

    class Live555SubsessionTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            in_addr addr {};
            if (inet_pton(AF_INET, "127.0.0.1", &addr) == 1) {
                ReceivingInterfaceAddr = addr.s_addr;
            }
            m_scheduler = BasicTaskScheduler::createNew();
            ASSERT_NE(m_scheduler, nullptr);
            m_env = BasicUsageEnvironment::createNew(*m_scheduler);
            ASSERT_NE(m_env, nullptr);
            (void)ourIPv4Address(*m_env);
        }

        void TearDown() override
        {
            if (m_env != nullptr) {
                m_env->reclaim();
                m_env = nullptr;
            }
            if (m_scheduler != nullptr) {
                delete m_scheduler;
                m_scheduler = nullptr;
            }
            ReceivingInterfaceAddr = INADDR_ANY;
        }

        TaskScheduler* m_scheduler { nullptr };
        UsageEnvironment* m_env { nullptr };
    };

    TEST_F(Live555SubsessionTest, SubsessionLifecycleAndProperties)
    {
        Live555FrameQueue queue { 10U };

        // Test H.264 subsession creation
        auto* h264Sub = Live555H264LiveSubsession::createNew(*m_env, queue, true, 2500U);
        ASSERT_NE(h264Sub, nullptr);
        EXPECT_EQ(h264Sub->streamType(), RtspStreamType::H264ElementaryStream);
        EXPECT_EQ(h264Sub->estimatedBitrateKbps(), 2500U);
        EXPECT_FALSE(h264Sub->hasSpsPps());
        EXPECT_EQ(&h264Sub->frameQueue(), &queue);

        h264Sub->setEstimatedBitrate(3200U);
        EXPECT_EQ(h264Sub->estimatedBitrateKbps(), 3200U);

        Medium::close(h264Sub);

        // Test MP2T subsession creation
        auto* mp2tSub = Live555Mp2tLiveSubsession::createNew(*m_env, queue, true, 6000U);
        ASSERT_NE(mp2tSub, nullptr);
        EXPECT_EQ(mp2tSub->streamType(), RtspStreamType::Mpeg2TransportStream);
        EXPECT_EQ(mp2tSub->estimatedBitrateKbps(), 6000U);
        EXPECT_EQ(&mp2tSub->frameQueue(), &queue);

        mp2tSub->setEstimatedBitrate(7500U);
        EXPECT_EQ(mp2tSub->estimatedBitrateKbps(), 7500U);

        Medium::close(mp2tSub);
    }

    TEST_F(Live555SubsessionTest, PolymorphicFactoryCreation)
    {
        Live555FrameQueue queue { 10U };

        auto* sub1 = Live555LiveSubsession::createNew(*m_env, queue, RtspStreamType::H264ElementaryStream, true, 0U);
        ASSERT_NE(sub1, nullptr);
        EXPECT_EQ(sub1->streamType(), RtspStreamType::H264ElementaryStream);
        EXPECT_EQ(sub1->estimatedBitrateKbps(), 2000U); // default bitrate
        Medium::close(sub1);

        auto* sub2 = Live555LiveSubsession::createNew(*m_env, queue, RtspStreamType::Mpeg2TransportStream, true, 0U);
        ASSERT_NE(sub2, nullptr);
        EXPECT_EQ(sub2->streamType(), RtspStreamType::Mpeg2TransportStream);
        EXPECT_EQ(sub2->estimatedBitrateKbps(), 5000U); // default bitrate
        Medium::close(sub2);
    }

    TEST_F(Live555SubsessionTest, H264StreamSourceCreation)
    {
        Live555FrameQueue queue { 10U };
        auto* subsession = Live555H264LiveSubsession::createNew(*m_env, queue, true, 2000U);
        ASSERT_NE(subsession, nullptr);

        unsigned estBitrate { 0U };
        FramedSource* source = subsession->createNewStreamSource(1U, estBitrate);
        ASSERT_NE(source, nullptr);
        EXPECT_EQ(estBitrate, 2000U);
        Medium::close(source);

        auto* sms = ServerMediaSession::createNew(*m_env, "live");
        ASSERT_NE(sms, nullptr);
        sms->addSubsession(subsession);

        char* sdp = sms->generateSDPDescription(AF_INET);
        ASSERT_NE(sdp, nullptr);

        std::string sdpStr(sdp);
        delete[] sdp;

        EXPECT_NE(sdpStr.find("m=video 0 RTP/AVP"), std::string::npos);
        EXPECT_NE(sdpStr.find("a=rtpmap:"), std::string::npos);
        EXPECT_NE(sdpStr.find("H264/90000"), std::string::npos);

        Medium::close(sms);
    }

    TEST_F(Live555SubsessionTest, Mp2tStreamSourceAndSDP)
    {
        Live555FrameQueue queue { 10U };
        auto* subsession = Live555Mp2tLiveSubsession::createNew(*m_env, queue, true, 5000U);
        ASSERT_NE(subsession, nullptr);

        unsigned estBitrate { 0U };
        FramedSource* source = subsession->createNewStreamSource(1U, estBitrate);
        ASSERT_NE(source, nullptr);
        EXPECT_EQ(estBitrate, 5000U);
        Medium::close(source);

        auto* sms = ServerMediaSession::createNew(*m_env, "stanag4609.ts");
        ASSERT_NE(sms, nullptr);
        sms->addSubsession(subsession);

        char* sdp = sms->generateSDPDescription(AF_INET);
        ASSERT_NE(sdp, nullptr);

        std::string sdpStr(sdp);
        delete[] sdp;

        // RFC 2250 uses static payload format 33
        EXPECT_NE(sdpStr.find("m=video 0 RTP/AVP 33"), std::string::npos);
        EXPECT_NE(sdpStr.find("c=IN IP4 0.0.0.0"), std::string::npos);

        Medium::close(sms);
    }

    TEST_F(Live555SubsessionTest, H264SpsPpsConfigurationAndFmtpSDP)
    {
        Live555FrameQueue queue { 10U };
        auto* subsession = Live555H264LiveSubsession::createNew(*m_env, queue, true, 2000U);
        ASSERT_NE(subsession, nullptr);

        // Baseline profile SPS: profile_idc = 0x42, constraint_flags = 0xC0, level_idc = 0x1F
        const std::vector<std::uint8_t> sps { 0x67, 0x42, 0xC0, 0x1F, 0x8C, 0x68, 0x05, 0x00, 0x5B, 0xA1, 0x00, 0x00,
            0x03, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x3C, 0x8F, 0x18, 0x32, 0x48 };
        const std::vector<std::uint8_t> pps { 0x68, 0xCE, 0x3C, 0x80 };

        subsession->setSpsPps(sps, pps);
        EXPECT_TRUE(subsession->hasSpsPps());

        auto* sms = ServerMediaSession::createNew(*m_env, "live");
        ASSERT_NE(sms, nullptr);
        sms->addSubsession(subsession);

        char* sdp = sms->generateSDPDescription(AF_INET);
        ASSERT_NE(sdp, nullptr);

        std::string sdpStr(sdp);
        delete[] sdp;

        EXPECT_NE(sdpStr.find("m=video 0 RTP/AVP"), std::string::npos);
        EXPECT_NE(sdpStr.find("a=rtpmap:"), std::string::npos);
        EXPECT_NE(sdpStr.find("H264/90000"), std::string::npos);
        EXPECT_NE(sdpStr.find("a=fmtp:"), std::string::npos);
        EXPECT_NE(sdpStr.find("packetization-mode=1"), std::string::npos);
        EXPECT_NE(sdpStr.find("profile-level-id=42C01F"), std::string::npos);
        EXPECT_NE(sdpStr.find("sprop-parameter-sets="), std::string::npos);

        Medium::close(sms);
    }

    TEST_F(Live555SubsessionTest, H264StartCodeStrippingInSetSpsPps)
    {
        Live555FrameQueue queue { 10U };
        auto* subsession = Live555H264LiveSubsession::createNew(*m_env, queue, true, 2000U);
        ASSERT_NE(subsession, nullptr);

        // Include 4-byte start code prefix on SPS and 3-byte on PPS
        const std::vector<std::uint8_t> spsWithStartCode { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F, 0x8C, 0x68,
            0x05, 0x00, 0x5B, 0xA1 };
        const std::vector<std::uint8_t> ppsWithStartCode { 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80 };

        subsession->setSpsPps(spsWithStartCode, ppsWithStartCode);
        EXPECT_TRUE(subsession->hasSpsPps());

        auto* sms = ServerMediaSession::createNew(*m_env, "live");
        ASSERT_NE(sms, nullptr);
        sms->addSubsession(subsession);

        char* sdp = sms->generateSDPDescription(AF_INET);
        ASSERT_NE(sdp, nullptr);

        std::string sdpStr(sdp);
        delete[] sdp;

        EXPECT_NE(sdpStr.find("profile-level-id=42C01F"), std::string::npos);
        EXPECT_NE(sdpStr.find("sprop-parameter-sets="), std::string::npos);

        Medium::close(sms);
    }

} // namespace
} // namespace Video
