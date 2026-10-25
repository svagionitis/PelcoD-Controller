/// @file TestOnvifPtzProfileS.cpp
/// @brief Conformance tests for ONVIF Profile S PTZ operations and configuration lifecycle.

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
#include "OnvifTestFixtures.h"

using namespace Onvif;

namespace {

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

/// @brief Wraps inner body XML with SOAP 1.2 envelope and PTZ namespaces.
/// @param[in] bodyXml Inner SOAP body payload.
/// @return Full SOAP envelope string.
std::string wrapSoapPtz(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tptz=\"http://www.onvif.org/ver10/ptz/wsdl\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
           "  <s:Body>\r\n"
        + bodyXml
        + "\r\n  </s:Body>\r\n"
          "</s:Envelope>";
}

/// @struct TestPtzServerContext
/// @brief Scoped test fixture holding a running OnvifServer instance.
struct TestPtzServerContext {
    OnvifServerConfig config {};
    int port { static_cast<int>(Onvif::Test::getFreeEphemeralPort()) };
    std::unique_ptr<OnvifServer> server {};

    TestPtzServerContext()
    {
        config.port = port;
        config.bindAddress = "127.0.0.1";
        config.defaultUsers = OnvifTest::defaultTestUsers();

        server = std::make_unique<OnvifServer>(config);
        EXPECT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestPtzServerContext()
    {
        if (server) {
            server->stop();
        }
    }
};

} // namespace

// ============================================================================
// 1. AccessPolicy PTZ Classifications
// ============================================================================

TEST(TestOnvifPtzProfileS, AccessPolicyClassifications)
{
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:GetServiceCapabilities"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:GetCompatibleConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:GetConfigurations"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:GetConfiguration"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:SetConfiguration"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:ContinuousMove"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:Stop"), AccessClass::Actuate);
}

// ============================================================================
// 2. PTZ Service Capabilities (Task 5.1)
// ============================================================================

TEST(TestOnvifPtzProfileS, GetServiceCapabilities)
{
    TestPtzServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    const std::string req = wrapSoapPtz("<tptz:GetServiceCapabilities/>");
    const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    const auto respNode = doc.select_node(".//*[local-name()='GetServiceCapabilitiesResponse']").node();
    ASSERT_TRUE(respNode);

    const auto capsNode = respNode.select_node(".//*[local-name()='Capabilities']").node();
    ASSERT_TRUE(capsNode);

    EXPECT_STREQ(capsNode.attribute("EFlip").as_string(), "false");
    EXPECT_STREQ(capsNode.attribute("Reverse").as_string(), "false");
    EXPECT_STREQ(capsNode.attribute("GetCompatibleConfigurations").as_string(), "true");
    EXPECT_STREQ(capsNode.attribute("MoveStatus").as_string(), "true");
    EXPECT_STREQ(capsNode.attribute("StatusPosition").as_string(), "true");
}

// ============================================================================
// 3. PTZ Configurations and Compatible Configurations Queries
// ============================================================================

TEST(TestOnvifPtzProfileS, GetConfigurationsAndOptions)
{
    TestPtzServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. GetConfigurations
    {
        const std::string req = wrapSoapPtz("<tptz:GetConfigurations/>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto cfgNode = doc.select_node(".//*[local-name()='PTZConfiguration'][@token='PTZConfig_1']").node();
        ASSERT_TRUE(cfgNode);
        EXPECT_STREQ(cfgNode.select_node(".//*[local-name()='Name']").node().text().as_string(), "PTZConfig");
        EXPECT_STREQ(cfgNode.select_node(".//*[local-name()='NodeToken']").node().text().as_string(), "PTZNode_1");
    }

    // 2. GetConfiguration valid token
    {
        const std::string req = wrapSoapPtz("<tptz:GetConfiguration>"
                                            "  <tptz:PTZConfigurationToken>PTZConfig_1</tptz:PTZConfigurationToken>"
                                            "</tptz:GetConfiguration>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto respNode = doc.select_node(".//*[local-name()='GetConfigurationResponse']").node();
        ASSERT_TRUE(respNode);
        const auto cfgNode = respNode.select_node(".//*[local-name()='PTZConfiguration']").node();
        ASSERT_TRUE(cfgNode);
        EXPECT_STREQ(cfgNode.attribute("token").as_string(), "PTZConfig_1");
    }

    // 3. GetConfiguration invalid token -> returns ter:NoToken
    {
        const std::string req
            = wrapSoapPtz("<tptz:GetConfiguration>"
                          "  <tptz:PTZConfigurationToken>NonExistentToken</tptz:PTZConfigurationToken>"
                          "</tptz:GetConfiguration>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoToken"), std::string::npos);
    }

    // 4. GetCompatibleConfigurations with valid ProfileToken_1
    {
        const std::string req = wrapSoapPtz("<tptz:GetCompatibleConfigurations>"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
                                            "</tptz:GetCompatibleConfigurations>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto respNode = doc.select_node(".//*[local-name()='GetCompatibleConfigurationsResponse']").node();
        ASSERT_TRUE(respNode);
        const auto cfgNode = respNode.select_node(".//*[local-name()='PTZConfiguration']").node();
        ASSERT_TRUE(cfgNode);
        EXPECT_STREQ(cfgNode.attribute("token").as_string(), "PTZConfig_1");
    }

    // 5. GetCompatibleConfigurations with invalid ProfileToken -> returns ter:NoProfile
    {
        const std::string req = wrapSoapPtz("<tptz:GetCompatibleConfigurations>"
                                            "  <tptz:ProfileToken>InvalidProfile_999</tptz:ProfileToken>"
                                            "</tptz:GetCompatibleConfigurations>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }
}

// ============================================================================
// 4. PTZ SetConfiguration & Store Persistence
// ============================================================================

TEST(TestOnvifPtzProfileS, SetConfigurationAndCaps)
{
    TestPtzServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // Update PTZ configuration
    const std::string updateReq
        = wrapSoapPtz("<tptz:SetConfiguration>"
                      "  <tptz:PTZConfiguration token=\"PTZConfig_1\">"
                      "    <tt:Name>OutdoorPTZConfig</tt:Name>"
                      "    <tt:UseCount>1</tt:UseCount>"
                      "    <tt:NodeToken>PTZNode_1</tt:NodeToken>"
                      "    <tt:DefaultAbsolutePanTiltPositionSpace>"
                      "      http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace"
                      "    </tt:DefaultAbsolutePanTiltPositionSpace>"
                      "    <tt:DefaultContinuousPanTiltVelocitySpace>"
                      "      http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace"
                      "    </tt:DefaultContinuousPanTiltVelocitySpace>"
                      "    <tt:DefaultContinuousZoomVelocitySpace>"
                      "      http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace"
                      "    </tt:DefaultContinuousZoomVelocitySpace>"
                      "    <tt:DefaultPTZTimeout>PT30S</tt:DefaultPTZTimeout>"
                      "    <tt:PanTiltLimits>"
                      "      <tt:Range>"
                      "        <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>"
                      "        <tt:XRange><tt:Min>-0.8</tt:Min><tt:Max>0.8</tt:Max></tt:XRange>"
                      "        <tt:YRange><tt:Min>-0.5</tt:Min><tt:Max>0.5</tt:Max></tt:YRange>"
                      "      </tt:Range>"
                      "    </tt:PanTiltLimits>"
                      "    <tt:ZoomLimits>"
                      "      <tt:Range>"
                      "        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>"
                      "        <tt:XRange><tt:Min>0.0</tt:Min><tt:Max>0.9</tt:Max></tt:XRange>"
                      "      </tt:Range>"
                      "    </tt:ZoomLimits>"
                      "  </tptz:PTZConfiguration>"
                      "  <tptz:ForcePersistence>true</tptz:ForcePersistence>"
                      "</tptz:SetConfiguration>");

    const auto setRes = client.Post("/onvif/ptz_service", updateReq, kSoapCt);
    ASSERT_TRUE(setRes && setRes->status == 200);
    EXPECT_NE(setRes->body.find("SetConfigurationResponse"), std::string::npos);

    // Verify updated values via GetConfiguration
    const std::string getReq = wrapSoapPtz("<tptz:GetConfiguration>"
                                           "  <tptz:PTZConfigurationToken>PTZConfig_1</tptz:PTZConfigurationToken>"
                                           "</tptz:GetConfiguration>");
    const auto getRes = client.Post("/onvif/ptz_service", getReq, kSoapCt);
    ASSERT_TRUE(getRes && getRes->status == 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(getRes->body.c_str()));

    const auto cfgNode = doc.select_node(".//*[local-name()='PTZConfiguration']").node();
    ASSERT_TRUE(cfgNode);
    EXPECT_STREQ(cfgNode.select_node(".//*[local-name()='Name']").node().text().as_string(), "OutdoorPTZConfig");
    EXPECT_STREQ(cfgNode.select_node(".//*[local-name()='DefaultPTZTimeout']").node().text().as_string(), "PT30S");

    const auto ptRange = cfgNode.select_node(".//*[local-name()='PanTiltLimits']/*[local-name()='Range']").node();
    ASSERT_TRUE(ptRange);
    EXPECT_NEAR(ptRange.select_node(".//*[local-name()='XRange']/*[local-name()='Min']").node().text().as_float(),
        -0.8f, 0.001f);
    EXPECT_NEAR(ptRange.select_node(".//*[local-name()='XRange']/*[local-name()='Max']").node().text().as_float(), 0.8f,
        0.001f);

    const auto zRange = cfgNode.select_node(".//*[local-name()='ZoomLimits']/*[local-name()='Range']").node();
    ASSERT_TRUE(zRange);
    EXPECT_NEAR(
        zRange.select_node(".//*[local-name()='XRange']/*[local-name()='Max']").node().text().as_float(), 0.9f, 0.001f);
}

// ============================================================================
// 5. PTZ SetConfiguration Validation Failures
// ============================================================================

TEST(TestOnvifPtzProfileS, SetConfigurationValidationFailures)
{
    TestPtzServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Unknown token -> returns ter:NoToken
    {
        const std::string req = wrapSoapPtz("<tptz:SetConfiguration>"
                                            "  <tptz:PTZConfiguration token=\"UnknownConfig_999\">"
                                            "    <tt:Name>FailConfig</tt:Name>"
                                            "  </tptz:PTZConfiguration>"
                                            "</tptz:SetConfiguration>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoToken"), std::string::npos);
    }

    // 2. Inverted limits range (Min > Max) -> returns ter:InvalidArgVal
    {
        const std::string req
            = wrapSoapPtz("<tptz:SetConfiguration>"
                          "  <tptz:PTZConfiguration token=\"PTZConfig_1\">"
                          "    <tt:Name>BadLimits</tt:Name>"
                          "    <tt:PanTiltLimits>"
                          "      <tt:Range>"
                          "        <tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>"
                          "        <tt:XRange><tt:Min>1.0</tt:Min><tt:Max>-1.0</tt:Max></tt:XRange>"
                          "      </tt:Range>"
                          "    </tt:PanTiltLimits>"
                          "  </tptz:PTZConfiguration>"
                          "</tptz:SetConfiguration>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:InvalidArgVal"), std::string::npos);
    }

    // 3. Unsupported PTZ space URI -> returns ter:InvalidArgVal
    {
        const std::string req = wrapSoapPtz("<tptz:SetConfiguration>"
                                            "  <tptz:PTZConfiguration token=\"PTZConfig_1\">"
                                            "    <tt:DefaultAbsolutePanTiltPositionSpace>"
                                            "      http://unsupported.org/spaces/unknown"
                                            "    </tt:DefaultAbsolutePanTiltPositionSpace>"
                                            "  </tptz:PTZConfiguration>"
                                            "</tptz:SetConfiguration>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:InvalidArgVal"), std::string::npos);
    }
}

// ============================================================================
// 6. Token & Profile Verification Across PTZ Operations
// ============================================================================

TEST(TestOnvifPtzProfileS, TokenAndProfileVerification)
{
    TestPtzServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // ContinuousMove with invalid profile -> ter:NoProfile
    {
        const std::string req = wrapSoapPtz("<tptz:ContinuousMove>"
                                            "  <tptz:ProfileToken>BadProfileToken</tptz:ProfileToken>"
                                            "  <tptz:Velocity><tt:PanTilt x=\"0.5\" y=\"0.0\"/></tptz:Velocity>"
                                            "</tptz:ContinuousMove>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }

    // Stop with invalid profile -> ter:NoProfile
    {
        const std::string req = wrapSoapPtz("<tptz:Stop>"
                                            "  <tptz:ProfileToken>BadProfileToken</tptz:ProfileToken>"
                                            "</tptz:Stop>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }

    // GotoPreset with invalid profile -> ter:NoProfile
    {
        const std::string req = wrapSoapPtz("<tptz:GotoPreset>"
                                            "  <tptz:ProfileToken>BadProfileToken</tptz:ProfileToken>"
                                            "  <tptz:PresetToken>1</tptz:PresetToken>"
                                            "</tptz:GotoPreset>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoProfile"), std::string::npos);
    }

    // GotoPreset with invalid preset token -> ter:NoToken
    {
        const std::string req = wrapSoapPtz("<tptz:GotoPreset>"
                                            "  <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
                                            "  <tptz:PresetToken>NonExistentPreset_999</tptz:PresetToken>"
                                            "</tptz:GotoPreset>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoToken"), std::string::npos);
    }

    // GetNode with invalid node token -> ter:NoToken
    {
        const std::string req = wrapSoapPtz("<tptz:GetNode>"
                                            "  <tptz:NodeToken>NonExistentNode_999</tptz:NodeToken>"
                                            "</tptz:GetNode>");
        const auto res = client.Post("/onvif/ptz_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);
        EXPECT_NE(res->body.find("ter:NoToken"), std::string::npos);
    }
}
