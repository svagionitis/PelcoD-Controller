/// @file TestSdpGenerator.cpp
/// @brief Unit tests for RFC 4566 Session Description Protocol (SDP) generator and parser.

#include "SdpGenerator.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace Transport {
namespace {

    TEST(TestSdpGenerator, MulticastIpDetection)
    {
        EXPECT_TRUE(SdpGenerator::isMulticast("224.0.0.1"));
        EXPECT_TRUE(SdpGenerator::isMulticast("239.255.0.1"));
        EXPECT_TRUE(SdpGenerator::isMulticast("239.1.2.3"));

        EXPECT_FALSE(SdpGenerator::isMulticast("127.0.0.1"));
        EXPECT_FALSE(SdpGenerator::isMulticast("192.168.1.157"));
        EXPECT_FALSE(SdpGenerator::isMulticast("10.0.0.1"));
        EXPECT_FALSE(SdpGenerator::isMulticast("invalid.ip"));
    }

    TEST(TestSdpGenerator, RtpMpeg2TsMulticastSdpFormatting)
    {
        SdpSessionParams params {};
        params.sessionName = "Sightline SLA Video Stream";
        params.originAddress = "192.168.1.157";
        params.sessionId = 1609459200ULL;
        params.destinationIp = "239.255.0.1";
        params.destinationPort = 15004U;
        params.ttl = 16U;
        params.isMulticast = true;
        params.payloadType = SdpPayloadType::Mpeg2Ts;
        params.transportProtocol = SdpProtocol::RtpAvp;
        params.clockRate = 90000U;
        params.bitrateKbps = 4000;
        params.frameWidth = 1920;
        params.frameHeight = 1080;

        const std::string sdp = SdpGenerator::generate(params);

        EXPECT_NE(sdp.find("v=0\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("o=- 1609459200 1 IN IP4 192.168.1.157\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("s=Sightline SLA Video Stream\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("c=IN IP4 239.255.0.1/16\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("b=AS:4000\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("t=0 0\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("m=video 15004 RTP/AVP 33\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("a=rtpmap:33 MP2T/90000\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("a=recvonly\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("a=x-dimensions:1920,1080\r\n"), std::string::npos);

        // Test round-trip parse
        const SdpSessionParams parsed = SdpGenerator::parse(sdp);
        EXPECT_EQ(parsed.sessionName, "Sightline SLA Video Stream");
        EXPECT_EQ(parsed.originAddress, "192.168.1.157");
        EXPECT_EQ(parsed.sessionId, 1609459200ULL);
        EXPECT_EQ(parsed.destinationIp, "239.255.0.1");
        EXPECT_EQ(parsed.destinationPort, 15004U);
        EXPECT_EQ(parsed.ttl, 16U);
        EXPECT_TRUE(parsed.isMulticast);
        EXPECT_EQ(parsed.bitrateKbps, 4000);
        EXPECT_EQ(parsed.payloadType, SdpPayloadType::Mpeg2Ts);
        EXPECT_EQ(parsed.frameWidth, 1920);
        EXPECT_EQ(parsed.frameHeight, 1080);
    }

    TEST(TestSdpGenerator, RtpH264UnicastSdpFormatting)
    {
        SdpSessionParams params {};
        params.sessionName = "Sightline H264 Feed";
        params.originAddress = "192.168.1.100";
        params.destinationIp = "192.168.1.200";
        params.destinationPort = 5400U;
        params.payloadType = SdpPayloadType::H264;
        params.transportProtocol = SdpProtocol::RtpAvp;
        params.spropParams = "Z0IAKeNQDwBE/LgLcBAQGgAAD6AAAw1T0hAA,aM48gA==";

        const std::string sdp = SdpGenerator::generate(params);

        EXPECT_NE(sdp.find("c=IN IP4 192.168.1.200\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("m=video 5400 RTP/AVP 96\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("a=rtpmap:96 H264/90000\r\n"), std::string::npos);
        EXPECT_NE(
            sdp.find("a=fmtp:96 "
                     "packetization-mode=1;sprop-parameter-sets=Z0IAKeNQDwBE/LgLcBAQGgAAD6AAAw1T0hAA,aM48gA==\r\n"),
            std::string::npos);

        const SdpSessionParams parsed = SdpGenerator::parse(sdp);
        EXPECT_EQ(parsed.destinationPort, 5400U);
        EXPECT_EQ(parsed.payloadType, SdpPayloadType::H264);
        EXPECT_FALSE(parsed.isMulticast);
    }

    TEST(TestSdpGenerator, DirectUdpMpegTsSdpFormatting)
    {
        SdpSessionParams params {};
        params.destinationIp = "10.0.0.50";
        params.destinationPort = 15004U;
        params.transportProtocol = SdpProtocol::Udp;

        const std::string sdp = SdpGenerator::generate(params);

        EXPECT_NE(sdp.find("m=video 15004 udp 33\r\n"), std::string::npos);
        EXPECT_NE(sdp.find("a=rtpmap:33 MP2T/90000\r\n"), std::string::npos);

        const SdpSessionParams parsed = SdpGenerator::parse(sdp);
        EXPECT_EQ(parsed.destinationPort, 15004U);
        EXPECT_EQ(parsed.transportProtocol, SdpProtocol::Udp);
        EXPECT_EQ(parsed.payloadType, SdpPayloadType::Mpeg2Ts);
    }

    TEST(TestSdpGenerator, SaveAndLoadSdpFile)
    {
        const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_sightline_stream.sdp";

        SdpSessionParams params {};
        params.sessionName = "File Export Test";
        params.destinationIp = "127.0.0.1";
        params.destinationPort = 15004U;
        params.payloadType = SdpPayloadType::Mpeg2Ts;

        EXPECT_TRUE(SdpGenerator::saveToFile(tempPath.string(), params));
        EXPECT_TRUE(std::filesystem::exists(tempPath));

        // Read file back and parse
        std::ifstream in(tempPath.string(), std::ios::in | std::ios::binary);
        ASSERT_TRUE(in.is_open());
        std::stringstream buffer;
        buffer << in.rdbuf();
        in.close();

        const SdpSessionParams parsed = SdpGenerator::parse(buffer.str());
        EXPECT_EQ(parsed.sessionName, "File Export Test");
        EXPECT_EQ(parsed.destinationPort, 15004U);

        std::filesystem::remove(tempPath);
    }

} // namespace
} // namespace Transport
