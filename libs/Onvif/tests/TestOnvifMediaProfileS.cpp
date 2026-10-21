/// @file TestOnvifMediaProfileS.cpp
/// @brief Conformance tests for ONVIF Profile S Media ver10 operations (P2 Phase 3).

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "Onvif/AccessPolicy.h"
#include "Onvif/HttplibInclude.h"
#include "Onvif/OnvifServer.h"
#include "Onvif/OnvifServerTypes.h"
#include "OnvifTestClient.h"

using namespace Onvif;

namespace {

std::atomic<int> g_nextPort { 19650 };

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

std::string wrapSoap(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
           "  <s:Body>\r\n"
        + bodyXml
        + "\r\n  </s:Body>\r\n"
          "</s:Envelope>";
}

struct TestServerContext {
    OnvifServerConfig config {};
    int port { g_nextPort.fetch_add(1) };
    std::unique_ptr<OnvifServer> server {};

    TestServerContext()
    {
        config.port = port;
        config.bindAddress = "127.0.0.1";
        config.defaultUsers = OnvifTest::defaultTestUsers();

        server = std::make_unique<OnvifServer>(config);
        EXPECT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestServerContext()
    {
        if (server) {
            server->stop();
        }
    }
};

} // namespace

// ============================================================================
// 1. AccessPolicy Classifications (Task 3.1 - 3.6)
// ============================================================================

TEST(TestOnvifMediaProfileS, AccessPolicyClassifications)
{
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetProfiles"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetProfile"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:CreateProfile"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:DeleteProfile"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetVideoSourceConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetVideoSourceConfiguration"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetVideoEncoderConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetVideoEncoderConfiguration"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetVideoEncoderConfigurationOptions"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:SetVideoEncoderConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:AddVideoEncoderConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:RemoveVideoEncoderConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:AddVideoSourceConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:RemoveVideoSourceConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:AddPTZConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:RemovePTZConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetCompatibleVideoSourceConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetCompatibleVideoEncoderConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(
        AccessPolicy::classify("Media", "trt:GetGuaranteedNumberOfVideoEncoderInstances"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:SetSynchronizationPoint"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Media", "trt:GetServiceCapabilities"), AccessClass::PreAuth);
}

// ============================================================================
// 2. GetProfiles Contains VSC & VEC (Task 3.1 & 3.2)
// ============================================================================

TEST(TestOnvifMediaProfileS, ProfilesContainVscAndVec)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    const std::string req = wrapSoap("<trt:GetProfiles/>");
    const auto res = client.Post("/onvif/media_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    // Verify root response
    const auto respNode = doc.select_node(".//*[local-name()='GetProfilesResponse']").node();
    ASSERT_TRUE(respNode);

    // ProfileToken_1
    const auto profNode = doc.select_node(".//*[local-name()='Profiles'][@token='ProfileToken_1']").node();
    ASSERT_TRUE(profNode);
    EXPECT_STREQ(profNode.attribute("fixed").as_string(), "true");

    // VSC child
    const auto vscNode = profNode.select_node(".//*[local-name()='VideoSourceConfiguration']").node();
    ASSERT_TRUE(vscNode);
    EXPECT_STREQ(vscNode.attribute("token").as_string(), "VideoSourceConfig_1");
    EXPECT_STREQ(vscNode.select_node(".//*[local-name()='SourceToken']").node().text().as_string(), "VideoSource_1");

    // Bounds
    const auto boundsNode = vscNode.select_node(".//*[local-name()='Bounds']").node();
    ASSERT_TRUE(boundsNode);
    EXPECT_EQ(boundsNode.attribute("width").as_int(), 1920);
    EXPECT_EQ(boundsNode.attribute("height").as_int(), 1080);

    // VEC child (Mandatory for Profile S)
    const auto vecNode = profNode.select_node(".//*[local-name()='VideoEncoderConfiguration']").node();
    ASSERT_TRUE(vecNode);
    EXPECT_STREQ(vecNode.attribute("token").as_string(), "VideoEncoderConfig_1");
    EXPECT_STREQ(vecNode.select_node(".//*[local-name()='Encoding']").node().text().as_string(), "H264");

    const auto resResNode = vecNode.select_node(".//*[local-name()='Resolution']").node();
    ASSERT_TRUE(resResNode);
    EXPECT_EQ(resResNode.select_node(".//*[local-name()='Width']").node().text().as_int(), 1920);
    EXPECT_EQ(resResNode.select_node(".//*[local-name()='Height']").node().text().as_int(), 1080);

    const auto rcNode = vecNode.select_node(".//*[local-name()='RateControl']").node();
    ASSERT_TRUE(rcNode);
    EXPECT_EQ(rcNode.select_node(".//*[local-name()='FrameRateLimit']").node().text().as_int(), 30);
    EXPECT_EQ(rcNode.select_node(".//*[local-name()='BitrateLimit']").node().text().as_int(), 4096);

    const auto h264Node = vecNode.select_node(".//*[local-name()='H264']").node();
    ASSERT_TRUE(h264Node);
    EXPECT_EQ(h264Node.select_node(".//*[local-name()='GovLength']").node().text().as_int(), 30);
    EXPECT_STREQ(h264Node.select_node(".//*[local-name()='H264Profile']").node().text().as_string(), "Main");

    // Multicast & SessionTimeout
    EXPECT_TRUE(vecNode.select_node(".//*[local-name()='Multicast']").node());
    EXPECT_STREQ(vecNode.select_node(".//*[local-name()='SessionTimeout']").node().text().as_string(), "PT30S");
}

// ============================================================================
// 3. GetProfile Valid & Invalid (Task 3.2)
// ============================================================================

TEST(TestOnvifMediaProfileS, GetProfile_ValidAndInvalid)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // Valid profile
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>ProfileToken_1</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto profNode = doc.select_node(".//*[local-name()='Profile'][@token='ProfileToken_1']").node();
        ASSERT_TRUE(profNode);
        EXPECT_STREQ(profNode.select_node(".//*[local-name()='Name']").node().text().as_string(), "MainProfile");
        EXPECT_TRUE(profNode.select_node(".//*[local-name()='VideoEncoderConfiguration']").node());
    }

    // Invalid profile token -> HTTP 400 SOAP Fault ter:NoProfile
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>NonExistent_Profile_XYZ</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto subcodeNode = doc.select_node(".//*[local-name()='Subcode']/*[local-name()='Value']").node();
        ASSERT_TRUE(subcodeNode);
        EXPECT_NE(std::string(subcodeNode.text().as_string()).find("InvalidArgVal"), std::string::npos);

        const auto secondSubcode = doc.select_node(".//*/ter:Subcode/ter:Value").node();
        if (secondSubcode) {
            EXPECT_NE(std::string(secondSubcode.text().as_string()).find("NoProfile"), std::string::npos);
        }
    }
}

// ============================================================================
// 4. CreateProfile & DeleteProfile (Task 3.3)
// ============================================================================

TEST(TestOnvifMediaProfileS, CreateAndDeleteProfile)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Create a dynamic profile
    {
        const std::string req = wrapSoap("<trt:CreateProfile>"
                                         "  <trt:Name>DynamicProfile</trt:Name>"
                                         "  <trt:Token>DynamicToken_1</trt:Token>"
                                         "</trt:CreateProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto profNode = doc.select_node(".//*[local-name()='Profile'][@token='DynamicToken_1']").node();
        ASSERT_TRUE(profNode);
        EXPECT_STREQ(profNode.select_node(".//*[local-name()='Name']").node().text().as_string(), "DynamicProfile");
        EXPECT_STREQ(profNode.attribute("fixed").as_string(), "false");
    }

    // 2. Query GetProfile on newly created profile
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>DynamicToken_1</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
    }

    // 3. Attempt to delete fixed profile ProfileToken_1 -> Rejected
    {
        const std::string req = wrapSoap("<trt:DeleteProfile>"
                                         "  <trt:ProfileToken>ProfileToken_1</trt:ProfileToken>"
                                         "</trt:DeleteProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
    }

    // 4. Delete the dynamic profile -> Success
    {
        const std::string req = wrapSoap("<trt:DeleteProfile>"
                                         "  <trt:ProfileToken>DynamicToken_1</trt:ProfileToken>"
                                         "</trt:DeleteProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("DeleteProfileResponse"), std::string::npos);
    }

    // 5. Verify deleted profile no longer exists
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>DynamicToken_1</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
    }
}

// ============================================================================
// 5. Video Source & Encoder Configuration Queries (Task 3.4)
// ============================================================================

TEST(TestOnvifMediaProfileS, VideoSourceAndEncoderConfigQueries)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // GetVideoSourceConfigurations
    {
        const std::string req = wrapSoap("<trt:GetVideoSourceConfigurations/>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto cfgNode
            = doc.select_node(".//*[local-name()='Configurations'][@token='VideoSourceConfig_1']").node();
        ASSERT_TRUE(cfgNode);
    }

    // GetVideoSourceConfiguration (single)
    {
        const std::string req = wrapSoap("<trt:GetVideoSourceConfiguration>"
                                         "  <trt:ConfigurationToken>VideoSourceConfig_1</trt:ConfigurationToken>"
                                         "</trt:GetVideoSourceConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto cfgNode = doc.select_node(".//*[local-name()='Configuration'][@token='VideoSourceConfig_1']").node();
        ASSERT_TRUE(cfgNode);
    }

    // GetVideoEncoderConfigurations
    {
        const std::string req = wrapSoap("<trt:GetVideoEncoderConfigurations/>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto cfgNode
            = doc.select_node(".//*[local-name()='Configurations'][@token='VideoEncoderConfig_1']").node();
        ASSERT_TRUE(cfgNode);
        EXPECT_STREQ(cfgNode.select_node(".//*[local-name()='Encoding']").node().text().as_string(), "H264");
    }

    // GetVideoEncoderConfiguration (single)
    {
        const std::string req = wrapSoap("<trt:GetVideoEncoderConfiguration>"
                                         "  <trt:ConfigurationToken>VideoEncoderConfig_1</trt:ConfigurationToken>"
                                         "</trt:GetVideoEncoderConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto cfgNode
            = doc.select_node(".//*[local-name()='Configuration'][@token='VideoEncoderConfig_1']").node();
        ASSERT_TRUE(cfgNode);
    }

    // GetVideoEncoderConfigurationOptions
    {
        const std::string req = wrapSoap("<trt:GetVideoEncoderConfigurationOptions>"
                                         "  <trt:ConfigurationToken>VideoEncoderConfig_1</trt:ConfigurationToken>"
                                         "</trt:GetVideoEncoderConfigurationOptions>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto optNode = doc.select_node(".//*[local-name()='Options']").node();
        ASSERT_TRUE(optNode);

        // QualityRange
        EXPECT_TRUE(optNode.select_node(".//*[local-name()='QualityRange']").node());

        // H264 resolutions
        const auto resNodes = optNode.select_nodes(".//*[local-name()='ResolutionsAvailable']");
        EXPECT_GE(resNodes.size(), 3U);

        // GovLengthRange
        const auto govNode = optNode.select_node(".//*[local-name()='GovLengthRange']").node();
        ASSERT_TRUE(govNode);
        EXPECT_EQ(govNode.attribute("Min").as_int(), 1);
        EXPECT_EQ(govNode.attribute("Max").as_int(), 120);

        // FrameRateRange
        const auto fpsNode = optNode.select_node(".//*[local-name()='FrameRateRange']").node();
        ASSERT_TRUE(fpsNode);
        EXPECT_EQ(fpsNode.attribute("Min").as_int(), 1);
        EXPECT_EQ(fpsNode.attribute("Max").as_int(), 30);

        // H.264 Profiles
        const auto profiles = optNode.select_nodes(".//*[local-name()='H264ProfilesSupported']");
        EXPECT_GE(profiles.size(), 3U);
    }
}

// ============================================================================
// 6. SetVideoEncoderConfiguration (Task 3.4)
// ============================================================================

TEST(TestOnvifMediaProfileS, SetVideoEncoderConfig)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Valid update
    {
        const std::string req = wrapSoap("<trt:SetVideoEncoderConfiguration>"
                                         "  <trt:Configuration token=\"VideoEncoderConfig_1\">"
                                         "    <tt:Name>UpdatedEncoderConfig</tt:Name>"
                                         "    <tt:UseCount>1</tt:UseCount>"
                                         "    <tt:Encoding>H264</tt:Encoding>"
                                         "    <tt:Resolution>"
                                         "      <tt:Width>1280</tt:Width>"
                                         "      <tt:Height>720</tt:Height>"
                                         "    </tt:Resolution>"
                                         "    <tt:Quality>4</tt:Quality>"
                                         "    <tt:RateControl>"
                                         "      <tt:FrameRateLimit>25</tt:FrameRateLimit>"
                                         "      <tt:BitrateLimit>2048</tt:BitrateLimit>"
                                         "    </tt:RateControl>"
                                         "    <tt:H264>"
                                         "      <tt:GovLength>25</tt:GovLength>"
                                         "      <tt:H264Profile>High</tt:H264Profile>"
                                         "    </tt:H264>"
                                         "  </trt:Configuration>"
                                         "  <trt:ForcePersistence>true</trt:ForcePersistence>"
                                         "</trt:SetVideoEncoderConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("SetVideoEncoderConfigurationResponse"), std::string::npos);

        // Verify update persisted
        const std::string queryReq = wrapSoap("<trt:GetVideoEncoderConfiguration>"
                                              "  <trt:ConfigurationToken>VideoEncoderConfig_1</trt:ConfigurationToken>"
                                              "</trt:GetVideoEncoderConfiguration>");
        const auto queryRes = client.Post("/onvif/media_service", queryReq, kSoapCt);
        ASSERT_TRUE(queryRes && queryRes->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(queryRes->body.c_str()));
        const auto resNode = doc.select_node(".//*[local-name()='Resolution']").node();
        ASSERT_TRUE(resNode);
        EXPECT_EQ(resNode.select_node(".//*[local-name()='Width']").node().text().as_int(), 1280);
        EXPECT_EQ(resNode.select_node(".//*[local-name()='Height']").node().text().as_int(), 720);
    }

    // 2. Invalid configuration token -> Fault
    {
        const std::string req
            = wrapSoap("<trt:SetVideoEncoderConfiguration>"
                       "  <trt:Configuration token=\"NonExistent_Config\">"
                       "    <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>"
                       "  </trt:Configuration>"
                       "</trt:SetVideoEncoderConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
    }

    // 3. Out of range FrameRateLimit -> Fault
    {
        const std::string req = wrapSoap("<trt:SetVideoEncoderConfiguration>"
                                         "  <trt:Configuration token=\"VideoEncoderConfig_1\">"
                                         "    <tt:RateControl>"
                                         "      <tt:FrameRateLimit>120</tt:FrameRateLimit>"
                                         "      <tt:BitrateLimit>2048</tt:BitrateLimit>"
                                         "    </tt:RateControl>"
                                         "  </trt:Configuration>"
                                         "</trt:SetVideoEncoderConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
    }
}

// ============================================================================
// 7. Configuration Attachment & Compatibility (Task 3.5)
// ============================================================================

TEST(TestOnvifMediaProfileS, ConfigurationAttachments)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Create a dynamic profile
    {
        const std::string req = wrapSoap("<trt:CreateProfile>"
                                         "  <trt:Name>AttachTestProfile</trt:Name>"
                                         "  <trt:Token>AttachTest_1</trt:Token>"
                                         "</trt:CreateProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
    }

    // 2. Add VideoSourceConfiguration
    {
        const std::string req = wrapSoap("<trt:AddVideoSourceConfiguration>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "  <trt:ConfigurationToken>VideoSourceConfig_1</trt:ConfigurationToken>"
                                         "</trt:AddVideoSourceConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("AddVideoSourceConfigurationResponse"), std::string::npos);
    }

    // 3. Add VideoEncoderConfiguration
    {
        const std::string req = wrapSoap("<trt:AddVideoEncoderConfiguration>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "  <trt:ConfigurationToken>VideoEncoderConfig_1</trt:ConfigurationToken>"
                                         "</trt:AddVideoEncoderConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("AddVideoEncoderConfigurationResponse"), std::string::npos);
    }

    // 4. Add PTZConfiguration
    {
        const std::string req = wrapSoap("<trt:AddPTZConfiguration>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "  <trt:ConfigurationToken>PTZConfig_1</trt:ConfigurationToken>"
                                         "</trt:AddPTZConfiguration>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("AddPTZConfigurationResponse"), std::string::npos);
    }

    // 5. Query GetProfile on AttachTest_1 -> verify all 3 attached
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto profNode = doc.select_node(".//*[local-name()='Profile'][@token='AttachTest_1']").node();
        ASSERT_TRUE(profNode);
        EXPECT_TRUE(profNode.select_node(".//*[local-name()='VideoSourceConfiguration']").node());
        EXPECT_TRUE(profNode.select_node(".//*[local-name()='VideoEncoderConfiguration']").node());
        EXPECT_TRUE(profNode.select_node(".//*[local-name()='PTZConfiguration']").node());
    }

    // 6. GetCompatibleVideoSourceConfigurations
    {
        const std::string req = wrapSoap("<trt:GetCompatibleVideoSourceConfigurations>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "</trt:GetCompatibleVideoSourceConfigurations>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("GetCompatibleVideoSourceConfigurationsResponse"), std::string::npos);
    }

    // 7. GetCompatibleVideoEncoderConfigurations
    {
        const std::string req = wrapSoap("<trt:GetCompatibleVideoEncoderConfigurations>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "</trt:GetCompatibleVideoEncoderConfigurations>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("GetCompatibleVideoEncoderConfigurationsResponse"), std::string::npos);
    }

    // 8. GetGuaranteedNumberOfVideoEncoderInstances
    {
        const std::string req = wrapSoap("<trt:GetGuaranteedNumberOfVideoEncoderInstances>"
                                         "  <trt:ConfigurationToken>VideoEncoderConfig_1</trt:ConfigurationToken>"
                                         "</trt:GetGuaranteedNumberOfVideoEncoderInstances>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto totalNode = doc.select_node(".//*[local-name()='TotalNumber']").node();
        ASSERT_TRUE(totalNode);
        EXPECT_GE(totalNode.text().as_int(), 2);
    }

    // 9. Remove configurations
    {
        const std::string rmVec = wrapSoap("<trt:RemoveVideoEncoderConfiguration>"
                                           "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                           "</trt:RemoveVideoEncoderConfiguration>");
        const auto resVec = client.Post("/onvif/media_service", rmVec, kSoapCt);
        ASSERT_TRUE(resVec && resVec->status == 200);

        const std::string rmVsc = wrapSoap("<trt:RemoveVideoSourceConfiguration>"
                                           "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                           "</trt:RemoveVideoSourceConfiguration>");
        const auto resVsc = client.Post("/onvif/media_service", rmVsc, kSoapCt);
        ASSERT_TRUE(resVsc && resVsc->status == 200);

        const std::string rmPtz = wrapSoap("<trt:RemovePTZConfiguration>"
                                           "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                           "</trt:RemovePTZConfiguration>");
        const auto resPtz = client.Post("/onvif/media_service", rmPtz, kSoapCt);
        ASSERT_TRUE(resPtz && resPtz->status == 200);
    }

    // 10. Verify detached
    {
        const std::string req = wrapSoap("<trt:GetProfile>"
                                         "  <trt:ProfileToken>AttachTest_1</trt:ProfileToken>"
                                         "</trt:GetProfile>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto profNode = doc.select_node(".//*[local-name()='Profile'][@token='AttachTest_1']").node();
        ASSERT_TRUE(profNode);
        EXPECT_FALSE(profNode.select_node(".//*[local-name()='VideoSourceConfiguration']").node());
        EXPECT_FALSE(profNode.select_node(".//*[local-name()='VideoEncoderConfiguration']").node());
        EXPECT_FALSE(profNode.select_node(".//*[local-name()='PTZConfiguration']").node());
    }
}

// ============================================================================
// 8. SetSynchronizationPoint (Task 3.6)
// ============================================================================

TEST(TestOnvifMediaProfileS, SetSynchronizationPoint)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // Valid profile
    {
        const std::string req = wrapSoap("<trt:SetSynchronizationPoint>"
                                         "  <trt:ProfileToken>ProfileToken_1</trt:ProfileToken>"
                                         "</trt:SetSynchronizationPoint>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("SetSynchronizationPointResponse"), std::string::npos);
    }

    // Invalid profile token -> Fault ter:NoProfile
    {
        const std::string req = wrapSoap("<trt:SetSynchronizationPoint>"
                                         "  <trt:ProfileToken>InvalidProfileToken</trt:ProfileToken>"
                                         "</trt:SetSynchronizationPoint>");
        const auto res = client.Post("/onvif/media_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
    }
}

// ============================================================================
// 9. Media GetServiceCapabilities (Task 3.6)
// ============================================================================

TEST(TestOnvifMediaProfileS, MediaCapabilities)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    const std::string req = wrapSoap("<trt:GetServiceCapabilities/>");
    const auto res = client.Post("/onvif/media_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    const auto capsNode = doc.select_node(".//*[local-name()='Capabilities']").node();
    ASSERT_TRUE(capsNode);
    EXPECT_STREQ(capsNode.attribute("SnapshotUri").as_string(), "true");

    const auto profCaps = capsNode.select_node(".//*[local-name()='ProfileCapabilities']").node();
    ASSERT_TRUE(profCaps);
    EXPECT_GE(profCaps.attribute("MaximumNumberOfProfiles").as_int(), 4);

    const auto streamCaps = capsNode.select_node(".//*[local-name()='StreamingCapabilities']").node();
    ASSERT_TRUE(streamCaps);
    EXPECT_STREQ(streamCaps.attribute("RTP_TCP").as_string(), "true");
}
