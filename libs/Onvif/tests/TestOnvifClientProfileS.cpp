/// @file TestOnvifClientProfileS.cpp
/// @brief Conformance tests for Client-Side Profile S Support (P2 Phase 6).

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Onvif/OnvifClient.h"
#include "Onvif/OnvifServer.h"
#include "Onvif/OnvifServerTypes.h"
#include "Onvif/OnvifTypes.h"
#include "OnvifTestClient.h"
#include "OnvifTestFixtures.h"

using namespace Onvif;

namespace {

struct TestServerFixture {
    OnvifServerConfig config { };
    int port { static_cast<int>(Onvif::Test::getFreeEphemeralPort()) };
    std::unique_ptr<OnvifServer> server { };

    TestServerFixture()
    {
        config.port = port;
        config.bindAddress = "127.0.0.1";
        config.defaultUsers = OnvifTest::defaultTestUsers();

        server = std::make_unique<OnvifServer>(config);
        EXPECT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestServerFixture()
    {
        if (server) {
            server->stop();
        }
    }

    [[nodiscard]] std::string deviceUrl() const
    {
        return "http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service";
    }

    [[nodiscard]] SecurityCredentials adminCredentials() const
    {
        return OnvifTest::defaultAdmin();
    }
};

} // namespace

// =============================================================================
// Static XML Parser Tests
// =============================================================================

TEST(TestOnvifClientProfileS, ParseProfileResponses)
{
    const std::string validProfileXml = "<trt:GetProfileResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                                        "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                        "  <trt:Profile token=\"ProfileToken_1\" fixed=\"true\">\r\n"
                                        "    <tt:Name>MainProfile</tt:Name>\r\n"
                                        "    <tt:VideoSourceConfiguration token=\"VSC_1\">\r\n"
                                        "      <tt:Name>VSC_Name</tt:Name>\r\n"
                                        "      <tt:UseCount>1</tt:UseCount>\r\n"
                                        "      <tt:SourceToken>VS_1</tt:SourceToken>\r\n"
                                        "      <tt:Bounds x=\"0\" y=\"0\" width=\"1920\" height=\"1080\"/>\r\n"
                                        "    </tt:VideoSourceConfiguration>\r\n"
                                        "    <tt:VideoEncoderConfiguration token=\"VEC_1\">\r\n"
                                        "      <tt:Name>VEC_Name</tt:Name>\r\n"
                                        "      <tt:UseCount>1</tt:UseCount>\r\n"
                                        "      <tt:Encoding>H264</tt:Encoding>\r\n"
                                        "      <tt:Resolution>\r\n"
                                        "        <tt:Width>1920</tt:Width>\r\n"
                                        "        <tt:Height>1080</tt:Height>\r\n"
                                        "      </tt:Resolution>\r\n"
                                        "      <tt:Quality>5.0</tt:Quality>\r\n"
                                        "    </tt:VideoEncoderConfiguration>\r\n"
                                        "    <tt:PTZConfiguration token=\"PTZ_1\"/>\r\n"
                                        "    <tt:MetadataConfiguration token=\"MC_1\"/>\r\n"
                                        "  </trt:Profile>\r\n"
                                        "</trt:GetProfileResponse>";

    const auto prof = OnvifClient::parseProfileResponse(validProfileXml);
    ASSERT_TRUE(prof.has_value());
    EXPECT_EQ(prof->token, "ProfileToken_1");
    EXPECT_EQ(prof->name, "MainProfile");
    EXPECT_TRUE(prof->fixed);
    EXPECT_EQ(prof->videoWidth, 1920);
    EXPECT_EQ(prof->videoHeight, 1080);
    EXPECT_EQ(prof->videoEncoding, "H264");
    EXPECT_EQ(prof->vscToken, "VSC_1");
    EXPECT_EQ(prof->videoSourceToken, "VS_1");
    EXPECT_EQ(prof->vecToken, "VEC_1");
    EXPECT_EQ(prof->ptzToken, "PTZ_1");
    EXPECT_EQ(prof->metadataToken, "MC_1");

    EXPECT_FALSE(OnvifClient::parseProfileResponse("<invalid>xml</invalid>").has_value());
    EXPECT_FALSE(OnvifClient::parseProfileResponse("").has_value());
}

TEST(TestOnvifClientProfileS, ParseCreateProfileResponse)
{
    const std::string createXml = "<trt:CreateProfileResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                                  "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                  "  <trt:Profile token=\"Dyn_1\" fixed=\"false\">\r\n"
                                  "    <tt:Name>Dynamic_Profile</tt:Name>\r\n"
                                  "  </trt:Profile>\r\n"
                                  "</trt:CreateProfileResponse>";

    const auto prof = OnvifClient::parseCreateProfileResponse(createXml);
    ASSERT_TRUE(prof.has_value());
    EXPECT_EQ(prof->token, "Dyn_1");
    EXPECT_EQ(prof->name, "Dynamic_Profile");
    EXPECT_FALSE(prof->fixed);

    EXPECT_FALSE(OnvifClient::parseCreateProfileResponse("<empty/>").has_value());
}

TEST(TestOnvifClientProfileS, ParseVideoEncoderConfigs)
{
    const std::string configsXml
        = "<trt:GetVideoEncoderConfigurationsResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <trt:Configurations token=\"VEC_1\">\r\n"
          "    <tt:Name>Main_H264</tt:Name>\r\n"
          "    <tt:UseCount>2</tt:UseCount>\r\n"
          "    <tt:Encoding>H264</tt:Encoding>\r\n"
          "    <tt:Resolution>\r\n"
          "      <tt:Width>1920</tt:Width>\r\n"
          "      <tt:Height>1080</tt:Height>\r\n"
          "    </tt:Resolution>\r\n"
          "    <tt:Quality>6.5</tt:Quality>\r\n"
          "    <tt:RateControl>\r\n"
          "      <tt:FrameRateLimit>30.0</tt:FrameRateLimit>\r\n"
          "      <tt:EncodingInterval>1</tt:EncodingInterval>\r\n"
          "      <tt:BitrateLimit>4096</tt:BitrateLimit>\r\n"
          "    </tt:RateControl>\r\n"
          "    <tt:H264>\r\n"
          "      <tt:GovLength>60</tt:GovLength>\r\n"
          "      <tt:H264Profile>High</tt:H264Profile>\r\n"
          "    </tt:H264>\r\n"
          "    <tt:SessionTimeout>PT45S</tt:SessionTimeout>\r\n"
          "  </trt:Configurations>\r\n"
          "  <trt:Configurations token=\"VEC_2\">\r\n"
          "    <tt:Name>Sub_JPEG</tt:Name>\r\n"
          "    <tt:UseCount>1</tt:UseCount>\r\n"
          "    <tt:Encoding>JPEG</tt:Encoding>\r\n"
          "    <tt:Resolution>\r\n"
          "      <tt:Width>640</tt:Width>\r\n"
          "      <tt:Height>480</tt:Height>\r\n"
          "    </tt:Resolution>\r\n"
          "    <tt:Quality>4.0</tt:Quality>\r\n"
          "    <tt:RateControl>\r\n"
          "      <tt:FrameRateLimit>15.0</tt:FrameRateLimit>\r\n"
          "      <tt:EncodingInterval>1</tt:EncodingInterval>\r\n"
          "      <tt:BitrateLimit>1024</tt:BitrateLimit>\r\n"
          "    </tt:RateControl>\r\n"
          "    <tt:SessionTimeout>PT30S</tt:SessionTimeout>\r\n"
          "  </trt:Configurations>\r\n"
          "</trt:GetVideoEncoderConfigurationsResponse>";

    const auto configs = OnvifClient::parseVideoEncoderConfigsResp(configsXml);
    ASSERT_EQ(configs.size(), 2U);

    EXPECT_EQ(configs[0].token, "VEC_1");
    EXPECT_EQ(configs[0].name, "Main_H264");
    EXPECT_EQ(configs[0].useCount, 2);
    EXPECT_EQ(configs[0].encoding, "H264");
    EXPECT_EQ(configs[0].width, 1920);
    EXPECT_EQ(configs[0].height, 1080);
    EXPECT_FLOAT_EQ(configs[0].quality, 6.5f);
    EXPECT_FLOAT_EQ(configs[0].frameRateLimit, 30.0f);
    EXPECT_EQ(configs[0].bitrateLimitKbps, 4096);
    EXPECT_EQ(configs[0].govLength, 60);
    EXPECT_EQ(configs[0].h264Profile, "High");
    EXPECT_EQ(configs[0].sessionTimeout, "PT45S");

    EXPECT_EQ(configs[1].token, "VEC_2");
    EXPECT_EQ(configs[1].name, "Sub_JPEG");
    EXPECT_EQ(configs[1].encoding, "JPEG");
    EXPECT_EQ(configs[1].width, 640);
    EXPECT_EQ(configs[1].height, 480);
    EXPECT_FLOAT_EQ(configs[1].frameRateLimit, 15.0f);

    const std::string singleXml
        = "<trt:GetVideoEncoderConfigurationResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <trt:Configuration token=\"VEC_Single\">\r\n"
          "    <tt:Name>SingleConfig</tt:Name>\r\n"
          "    <tt:UseCount>1</tt:UseCount>\r\n"
          "    <tt:Encoding>H264</tt:Encoding>\r\n"
          "    <tt:Resolution>\r\n"
          "      <tt:Width>1280</tt:Width>\r\n"
          "      <tt:Height>720</tt:Height>\r\n"
          "    </tt:Resolution>\r\n"
          "  </trt:Configuration>\r\n"
          "</trt:GetVideoEncoderConfigurationResponse>";

    const auto singleCfg = OnvifClient::parseVideoEncoderConfigResp(singleXml);
    ASSERT_TRUE(singleCfg.has_value());
    EXPECT_EQ(singleCfg->token, "VEC_Single");
    EXPECT_EQ(singleCfg->name, "SingleConfig");
    EXPECT_EQ(singleCfg->width, 1280);
    EXPECT_EQ(singleCfg->height, 720);
}

TEST(TestOnvifClientProfileS, ParseVideoEncoderOptions)
{
    const std::string optsXml
        = "<trt:GetVideoEncoderConfigurationOptionsResponse xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <trt:Options>\r\n"
          "    <tt:QualityRange>\r\n"
          "      <tt:Min>1</tt:Min>\r\n"
          "      <tt:Max>10</tt:Max>\r\n"
          "    </tt:QualityRange>\r\n"
          "    <tt:H264>\r\n"
          "      <tt:ResolutionsAvailable>\r\n"
          "        <tt:Width>1920</tt:Width>\r\n"
          "        <tt:Height>1080</tt:Height>\r\n"
          "      </tt:ResolutionsAvailable>\r\n"
          "      <tt:ResolutionsAvailable>\r\n"
          "        <tt:Width>1280</tt:Width>\r\n"
          "        <tt:Height>720</tt:Height>\r\n"
          "      </tt:ResolutionsAvailable>\r\n"
          "      <tt:GovLengthRange Min=\"1\" Max=\"120\">\r\n"
          "        <tt:Min>1</tt:Min>\r\n"
          "        <tt:Max>120</tt:Max>\r\n"
          "      </tt:GovLengthRange>\r\n"
          "      <tt:FrameRateRange Min=\"1\" Max=\"30\">\r\n"
          "        <tt:Min>1</tt:Min>\r\n"
          "        <tt:Max>30</tt:Max>\r\n"
          "      </tt:FrameRateRange>\r\n"
          "      <tt:EncodingIntervalRange Min=\"1\" Max=\"1\">\r\n"
          "        <tt:Min>1</tt:Min>\r\n"
          "        <tt:Max>1</tt:Max>\r\n"
          "      </tt:EncodingIntervalRange>\r\n"
          "      <tt:H264ProfilesSupported>Baseline</tt:H264ProfilesSupported>\r\n"
          "      <tt:H264ProfilesSupported>Main</tt:H264ProfilesSupported>\r\n"
          "      <tt:H264ProfilesSupported>High</tt:H264ProfilesSupported>\r\n"
          "    </tt:H264>\r\n"
          "  </trt:Options>\r\n"
          "</trt:GetVideoEncoderConfigurationOptionsResponse>";

    const auto opts = OnvifClient::parseVideoEncoderOptionsResp(optsXml);
    ASSERT_TRUE(opts.has_value());
    EXPECT_EQ(opts->qualityRange.min, 1);
    EXPECT_EQ(opts->qualityRange.max, 10);
    EXPECT_EQ(opts->govLengthRange.min, 1);
    EXPECT_EQ(opts->govLengthRange.max, 120);
    EXPECT_EQ(opts->frameRateRange.min, 1);
    EXPECT_EQ(opts->frameRateRange.max, 30);
    EXPECT_EQ(opts->encodingIntervalRange.min, 1);
    EXPECT_EQ(opts->encodingIntervalRange.max, 1);

    ASSERT_EQ(opts->resolutions.size(), 2U);
    EXPECT_EQ(opts->resolutions[0].width, 1920);
    EXPECT_EQ(opts->resolutions[0].height, 1080);
    EXPECT_EQ(opts->resolutions[1].width, 1280);
    EXPECT_EQ(opts->resolutions[1].height, 720);

    ASSERT_EQ(opts->h264Profiles.size(), 3U);
    EXPECT_EQ(opts->h264Profiles[0], "Baseline");
    EXPECT_EQ(opts->h264Profiles[1], "Main");
    EXPECT_EQ(opts->h264Profiles[2], "High");

    EXPECT_FALSE(OnvifClient::parseVideoEncoderOptionsResp("<bad/>").has_value());
}

TEST(TestOnvifClientProfileS, ParseDiscoveryMode)
{
    const std::string discXml
        = "<tds:GetDiscoveryModeResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
          "  <tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>\r\n"
          "</tds:GetDiscoveryModeResponse>";

    const auto mode1 = OnvifClient::parseDiscoveryModeResponse(discXml);
    ASSERT_TRUE(mode1.has_value());
    EXPECT_EQ(*mode1, DiscoveryMode::Discoverable);

    const std::string nonDiscXml
        = "<tds:GetDiscoveryModeResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
          "  <tds:DiscoveryMode>NonDiscoverable</tds:DiscoveryMode>\r\n"
          "</tds:GetDiscoveryModeResponse>";

    const auto mode2 = OnvifClient::parseDiscoveryModeResponse(nonDiscXml);
    ASSERT_TRUE(mode2.has_value());
    EXPECT_EQ(*mode2, DiscoveryMode::NonDiscoverable);

    EXPECT_FALSE(OnvifClient::parseDiscoveryModeResponse("<empty/>").has_value());
    EXPECT_FALSE(OnvifClient::parseDiscoveryModeResponse(
        "<tds:GetDiscoveryModeResponse><tds:DiscoveryMode>Unknown</tds:DiscoveryMode></tds:GetDiscoveryModeResponse>")
            .has_value());
}

TEST(TestOnvifClientProfileS, ParseNetworkProtocols)
{
    const std::string protoXml
        = "<tds:GetNetworkProtocolsResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <tds:NetworkProtocols>\r\n"
          "    <tt:Name>HTTP</tt:Name>\r\n"
          "    <tt:Enabled>true</tt:Enabled>\r\n"
          "    <tt:Port>8080</tt:Port>\r\n"
          "  </tds:NetworkProtocols>\r\n"
          "  <tds:NetworkProtocols>\r\n"
          "    <tt:Name>HTTPS</tt:Name>\r\n"
          "    <tt:Enabled>false</tt:Enabled>\r\n"
          "    <tt:Port>8443</tt:Port>\r\n"
          "  </tds:NetworkProtocols>\r\n"
          "  <tds:NetworkProtocols>\r\n"
          "    <tt:Name>RTSP</tt:Name>\r\n"
          "    <tt:Enabled>true</tt:Enabled>\r\n"
          "    <tt:Port>554</tt:Port>\r\n"
          "  </tds:NetworkProtocols>\r\n"
          "</tds:GetNetworkProtocolsResponse>";

    const auto protos = OnvifClient::parseNetworkProtocolsResponse(protoXml);
    ASSERT_EQ(protos.size(), 3U);

    EXPECT_EQ(protos[0].name, "HTTP");
    EXPECT_TRUE(protos[0].enabled);
    EXPECT_EQ(protos[0].port, 8080);

    EXPECT_EQ(protos[1].name, "HTTPS");
    EXPECT_FALSE(protos[1].enabled);
    EXPECT_EQ(protos[1].port, 8443);

    EXPECT_EQ(protos[2].name, "RTSP");
    EXPECT_TRUE(protos[2].enabled);
    EXPECT_EQ(protos[2].port, 554);
}

// =============================================================================
// End-to-End Client <-> Server Roundtrip Tests
// =============================================================================

TEST(TestOnvifClientProfileS, ClientServerMediaRoundtrip)
{
    TestServerFixture fixture;
    OnvifClient client(fixture.deviceUrl(), fixture.adminCredentials());

    // 1. GetProfile
    const auto prof = client.getProfile("ProfileToken_1");
    ASSERT_TRUE(prof.has_value());
    EXPECT_EQ(prof->token, "ProfileToken_1");
    EXPECT_EQ(prof->name, "MainProfile");
    EXPECT_TRUE(prof->fixed);
    EXPECT_EQ(prof->videoWidth, 1920);
    EXPECT_EQ(prof->videoHeight, 1080);
    EXPECT_EQ(prof->videoEncoding, "H264");

    // Invalid profile returns nullopt
    EXPECT_FALSE(client.getProfile("NonExistentToken").has_value());

    // 2. CreateProfile
    const auto created = client.createProfile("DynamicProfile", "DynProf_Test");
    ASSERT_TRUE(created.has_value());
    EXPECT_EQ(created->token, "DynProf_Test");
    EXPECT_EQ(created->name, "DynamicProfile");
    EXPECT_FALSE(created->fixed);

    // Verify it is now retrievable via getProfile
    const auto retrieved = client.getProfile("DynProf_Test");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "DynamicProfile");

    // 3. DeleteProfile
    EXPECT_TRUE(client.deleteProfile("DynProf_Test"));
    EXPECT_FALSE(client.getProfile("DynProf_Test").has_value());

    // Deleting fixed profile fails
    EXPECT_FALSE(client.deleteProfile("ProfileToken_1"));
}

TEST(TestOnvifClientProfileS, ClientServerVideoEncoderRoundtrip)
{
    TestServerFixture fixture;
    OnvifClient client(fixture.deviceUrl(), fixture.adminCredentials());

    // 1. GetVideoEncoderConfigurations
    const auto configs = client.getVideoEncoderConfigurations();
    ASSERT_FALSE(configs.empty());
    EXPECT_EQ(configs[0].token, "VideoEncoderConfig_1");

    // 2. GetVideoEncoderConfiguration
    const auto vec = client.getVideoEncoderConfiguration("VideoEncoderConfig_1");
    ASSERT_TRUE(vec.has_value());
    EXPECT_EQ(vec->width, 1920);
    EXPECT_EQ(vec->height, 1080);
    EXPECT_FLOAT_EQ(vec->quality, 5.0f);
    EXPECT_FLOAT_EQ(vec->frameRateLimit, 30.0f);
    EXPECT_EQ(vec->bitrateLimitKbps, 4096);

    // 3. GetVideoEncoderConfigurationOptions
    const auto opts = client.getVideoEncoderConfigOptions("VideoEncoderConfig_1");
    ASSERT_TRUE(opts.has_value());
    EXPECT_EQ(opts->qualityRange.min, 1);
    EXPECT_EQ(opts->qualityRange.max, 10);
    ASSERT_FALSE(opts->resolutions.empty());
    EXPECT_EQ(opts->resolutions[0].width, 1920);
    EXPECT_EQ(opts->resolutions[0].height, 1080);

    // Also verify alias method
    const auto optsAlias = client.getVideoEncoderOptions("VideoEncoderConfig_1");
    ASSERT_TRUE(optsAlias.has_value());
    EXPECT_EQ(optsAlias->govLengthRange.max, 120);

    // 4. SetVideoEncoderConfiguration
    VideoEncoderConfig updatedConfig = *vec;
    updatedConfig.width = 1280;
    updatedConfig.height = 720;
    updatedConfig.bitrateLimitKbps = 2048;
    updatedConfig.frameRateLimit = 25.0f;
    updatedConfig.govLength = 50;

    EXPECT_TRUE(client.setVideoEncoderConfiguration(updatedConfig));

    // Verify modification persisted
    const auto verified = client.getVideoEncoderConfiguration("VideoEncoderConfig_1");
    ASSERT_TRUE(verified.has_value());
    EXPECT_EQ(verified->width, 1280);
    EXPECT_EQ(verified->height, 720);
    EXPECT_EQ(verified->bitrateLimitKbps, 2048);
    EXPECT_FLOAT_EQ(verified->frameRateLimit, 25.0f);
    EXPECT_EQ(verified->govLength, 50);

    // 5. Media SetSynchronizationPoint
    EXPECT_TRUE(client.mediaSetSyncPoint("ProfileToken_1"));
    EXPECT_FALSE(client.mediaSetSyncPoint("NonExistentProfileToken"));
}

TEST(TestOnvifClientProfileS, ClientServerDiscoveryAndProtocolsRoundtrip)
{
    TestServerFixture fixture;
    OnvifClient client(fixture.deviceUrl(), fixture.adminCredentials());

    // 1. DiscoveryMode
    const auto initialMode = client.getDiscoveryMode();
    ASSERT_TRUE(initialMode.has_value());
    EXPECT_EQ(*initialMode, DiscoveryMode::Discoverable);

    EXPECT_TRUE(client.setDiscoveryMode(DiscoveryMode::NonDiscoverable));

    const auto updatedMode = client.getDiscoveryMode();
    ASSERT_TRUE(updatedMode.has_value());
    EXPECT_EQ(*updatedMode, DiscoveryMode::NonDiscoverable);

    EXPECT_TRUE(client.setDiscoveryMode(DiscoveryMode::Discoverable));
    EXPECT_EQ(client.getDiscoveryMode(), DiscoveryMode::Discoverable);

    // 2. NetworkProtocols
    const auto protocols = client.getNetworkProtocols();
    ASSERT_FALSE(protocols.empty());

    bool foundHttp = false;
    for (const auto& p : protocols) {
        if (p.name == "HTTP") {
            foundHttp = true;
            EXPECT_TRUE(p.enabled);
            EXPECT_EQ(p.port, fixture.port);
        }
    }
    EXPECT_TRUE(foundHttp);

    // Update network protocols
    auto modifiedProtocols = protocols;
    for (auto& p : modifiedProtocols) {
        if (p.name == "RTSP") {
            p.port = 8554;
        }
    }
    EXPECT_TRUE(client.setNetworkProtocols(modifiedProtocols));

    const auto recheck = client.getNetworkProtocols();
    for (const auto& p : recheck) {
        if (p.name == "RTSP") {
            EXPECT_EQ(p.port, 8554);
        }
    }
}

TEST(TestOnvifClientProfileS, ClientServerSnapshotRoundtrip)
{
    TestServerFixture fixture;
    OnvifClient client(fixture.deviceUrl(), fixture.adminCredentials());

    const auto snapshotUri = client.getSnapshotUri("ProfileToken_1");
    ASSERT_TRUE(snapshotUri.has_value());
    EXPECT_NE(snapshotUri->find("/onvif/snapshot"), std::string::npos);

    const auto jpegBytes = client.getSnapshot(*snapshotUri);
    ASSERT_TRUE(jpegBytes.has_value());
    ASSERT_GE(jpegBytes->size(), 4U);

    // Verify JPEG SOI (0xFF, 0xD8) and EOI (0xFF, 0xD9)
    EXPECT_EQ((*jpegBytes)[0], 0xFF);
    EXPECT_EQ((*jpegBytes)[1], 0xD8);
    EXPECT_EQ((*jpegBytes)[jpegBytes->size() - 2], 0xFF);
    EXPECT_EQ((*jpegBytes)[jpegBytes->size() - 1], 0xD9);
}
