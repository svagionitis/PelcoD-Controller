/// @file TestOnvifDeviceProfileS.cpp
/// @brief Conformance tests for ONVIF Profile S Device Management operations (P2 Phase 2).

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

std::atomic<int> g_nextPort { 19550 };

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

std::string wrapSoap(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
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
// 1. AccessPolicy Classification (Task 2.1, 2.2, 2.3, 2.4)
// ============================================================================

TEST(TestOnvifDeviceProfileS, AccessPolicyClassifications)
{
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:GetDiscoveryMode"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:SetDiscoveryMode"), AccessClass::WriteSystem);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:GetNetworkProtocols"), AccessClass::ReadSystem);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:SetNetworkProtocols"), AccessClass::WriteSystem);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:GetWsdlUrl"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:GetServiceCapabilities"), AccessClass::PreAuth);
}

// ============================================================================
// 2. GetDiscoveryMode & SetDiscoveryMode (Task 2.1)
// ============================================================================

TEST(TestOnvifDeviceProfileS, GetSetDiscoveryMode)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Initial GetDiscoveryMode -> Discoverable
    {
        const std::string req = wrapSoap("<tds:GetDiscoveryMode/>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto modeNode = doc.select_node(".//*[local-name()='DiscoveryMode']").node();
        ASSERT_TRUE(modeNode);
        EXPECT_STREQ(modeNode.text().as_string(), "Discoverable");
    }

    // 2. SetDiscoveryMode -> NonDiscoverable
    {
        const std::string req = wrapSoap("<tds:SetDiscoveryMode>\r\n"
                                         "  <tds:DiscoveryMode>NonDiscoverable</tds:DiscoveryMode>\r\n"
                                         "</tds:SetDiscoveryMode>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='SetDiscoveryModeResponse']"));
    }

    // 3. Verify GetDiscoveryMode reflects NonDiscoverable
    {
        const std::string req = wrapSoap("<tds:GetDiscoveryMode/>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        const auto modeNode = doc.select_node(".//*[local-name()='DiscoveryMode']").node();
        ASSERT_TRUE(modeNode);
        EXPECT_STREQ(modeNode.text().as_string(), "NonDiscoverable");
    }

    // 4. SetDiscoveryMode with invalid mode returns 400 sender fault
    {
        const std::string req = wrapSoap("<tds:SetDiscoveryMode>\r\n"
                                         "  <tds:DiscoveryMode>InvalidModeString</tds:DiscoveryMode>\r\n"
                                         "</tds:SetDiscoveryMode>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Fault']"));
    }

    // 5. Restore to Discoverable
    {
        const std::string req = wrapSoap("<tds:SetDiscoveryMode>\r\n"
                                         "  <tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>\r\n"
                                         "</tds:SetDiscoveryMode>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);
    }
}

// ============================================================================
// 3. GetNetworkProtocols & SetNetworkProtocols (Task 2.2)
// ============================================================================

TEST(TestOnvifDeviceProfileS, GetSetNetworkProtocols)
{
    TestServerContext ctx;
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Initial GetNetworkProtocols returns HTTP, HTTPS, and RTSP
    {
        const std::string req = wrapSoap("<tds:GetNetworkProtocols/>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto protoNodes = doc.select_nodes(".//*[local-name()='NetworkProtocols']");
        EXPECT_GE(protoNodes.size(), 3U);

        bool foundHttp { false };
        bool foundHttps { false };
        bool foundRtsp { false };

        for (const auto& sel : protoNodes) {
            const auto nameNode = sel.node().select_node(".//*[local-name()='Name']").node();
            const auto portNode = sel.node().select_node(".//*[local-name()='Port']").node();
            ASSERT_TRUE(nameNode && portNode);

            const std::string name = nameNode.text().as_string();
            const int p = portNode.text().as_int();

            if (name == "HTTP") {
                foundHttp = true;
                EXPECT_EQ(p, ctx.port);
            } else if (name == "HTTPS") {
                foundHttps = true;
                EXPECT_EQ(p, 8443);
            } else if (name == "RTSP") {
                foundRtsp = true;
                EXPECT_EQ(p, 8554);
            }
        }

        EXPECT_TRUE(foundHttp);
        EXPECT_TRUE(foundHttps);
        EXPECT_TRUE(foundRtsp);
    }

    // 2. SetNetworkProtocols: update HTTP port to 8088
    {
        const std::string req = wrapSoap("<tds:SetNetworkProtocols>\r\n"
                                         "  <tds:NetworkProtocols>\r\n"
                                         "    <tt:Name>HTTP</tt:Name>\r\n"
                                         "    <tt:Enabled>true</tt:Enabled>\r\n"
                                         "    <tt:Port>8088</tt:Port>\r\n"
                                         "  </tds:NetworkProtocols>\r\n"
                                         "</tds:SetNetworkProtocols>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='SetNetworkProtocolsResponse']"));
    }

    // 3. Verify GetNetworkProtocols reports updated port 8088
    {
        const std::string req = wrapSoap("<tds:GetNetworkProtocols/>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto protoNodes = doc.select_nodes(".//*[local-name()='NetworkProtocols']");
        bool verifiedHttp { false };
        for (const auto& sel : protoNodes) {
            const auto nameNode = sel.node().select_node(".//*[local-name()='Name']").node();
            const auto portNode = sel.node().select_node(".//*[local-name()='Port']").node();
            if (nameNode && portNode && std::string(nameNode.text().as_string()) == "HTTP") {
                EXPECT_EQ(portNode.text().as_int(), 8088);
                verifiedHttp = true;
            }
        }
        EXPECT_TRUE(verifiedHttp);
    }

    // 4. SetNetworkProtocols with invalid port (e.g. 70000) returns 400
    {
        const std::string req = wrapSoap("<tds:SetNetworkProtocols>\r\n"
                                         "  <tds:NetworkProtocols>\r\n"
                                         "    <tt:Name>HTTP</tt:Name>\r\n"
                                         "    <tt:Enabled>true</tt:Enabled>\r\n"
                                         "    <tt:Port>70000</tt:Port>\r\n"
                                         "  </tds:NetworkProtocols>\r\n"
                                         "</tds:SetNetworkProtocols>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 400);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Fault']"));
    }
}

// ============================================================================
// 4. GetWsdlUrl (Task 2.3)
// ============================================================================

TEST(TestOnvifDeviceProfileS, GetWsdlUrlReturnsValidUrl)
{
    TestServerContext ctx;
    // GetWsdlUrl is PreAuth, can be fetched without credentials
    httplib::Client unauthClient("127.0.0.1", ctx.port);

    const std::string req = wrapSoap("<tds:GetWsdlUrl/>");
    const auto res = unauthClient.Post("/onvif/device_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    const auto wsdlNode = doc.select_node(".//*[local-name()='WsdlUrl']").node();
    ASSERT_TRUE(wsdlNode);

    const std::string wsdlUrl = wsdlNode.text().as_string();
    EXPECT_NE(wsdlUrl.find("/onvif/device.wsdl"), std::string::npos);
    EXPECT_NE(wsdlUrl.find(std::to_string(ctx.port)), std::string::npos);
}

// ============================================================================
// 5. GetServiceCapabilities for Device Service (Task 2.4)
// ============================================================================

TEST(TestOnvifDeviceProfileS, GetServiceCapabilities)
{
    TestServerContext ctx;
    // GetServiceCapabilities is PreAuth
    httplib::Client unauthClient("127.0.0.1", ctx.port);

    const std::string req = wrapSoap("<tds:GetServiceCapabilities/>");
    const auto res = unauthClient.Post("/onvif/device_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    // Verify Network child capabilities
    const auto netNode = doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Network']").node();
    ASSERT_TRUE(netNode);
    EXPECT_STREQ(netNode.attribute("IPFilter").as_string(), "false");
    EXPECT_STREQ(netNode.attribute("ZeroConfiguration").as_string(), "false");
    EXPECT_STREQ(netNode.attribute("DynDNS").as_string(), "false");

    // Verify Security child capabilities
    const auto secNode = doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Security']").node();
    ASSERT_TRUE(secNode);
    EXPECT_STREQ(secNode.attribute("TLS1.2").as_string(), "true");
    EXPECT_STREQ(secNode.attribute("UsernameToken").as_string(), "true");
    EXPECT_STREQ(secNode.attribute("HttpDigest").as_string(), "true");
    EXPECT_STREQ(secNode.attribute("AccessPolicyConfig").as_string(), "true");

    // Verify System child capabilities
    const auto sysNode = doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='System']").node();
    ASSERT_TRUE(sysNode);
    EXPECT_STREQ(sysNode.attribute("DiscoveryBye").as_string(), "true");
    EXPECT_STREQ(sysNode.attribute("SystemBackup").as_string(), "true");
    EXPECT_STREQ(sysNode.attribute("SystemLogging").as_string(), "true");
}

// ============================================================================
// 6. Category Filtering & Schema Completion in GetCapabilities (Task 2.5)
// ============================================================================

TEST(TestOnvifDeviceProfileS, CategoryFilteredCapabilities)
{
    TestServerContext ctx;
    httplib::Client client("127.0.0.1", ctx.port);

    // 1. Category=Media -> Only tt:Media block returned with StreamingCapabilities
    {
        const std::string req = wrapSoap("<tds:GetCapabilities>\r\n"
                                         "  <tds:Category>Media</tds:Category>\r\n"
                                         "</tds:GetCapabilities>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Media']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Media']/*[local-name()='StreamingCapabilities']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='StreamingCapabilities']/*[local-name()='RTP_TCP']"));

        // Other categories must NOT be present
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Device']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='PTZ']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Imaging']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Events']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Analytics']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Extension']"));
    }

    // 2. Category=Events -> Only tt:Events returned with subscription support elements
    {
        const std::string req = wrapSoap("<tds:GetCapabilities>\r\n"
                                         "  <tds:Category>Events</tds:Category>\r\n"
                                         "</tds:GetCapabilities>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Events']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Events']/*[local-name()='WSSubscriptionPolicySupport']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Events']/*[local-name()='WSPullPointSupport']"));

        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Media']"));
        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='PTZ']"));
    }

    // 3. Category=Device -> tt:Device with Network, System, IO, Security child elements
    {
        const std::string req = wrapSoap("<tds:GetCapabilities>\r\n"
                                         "  <tds:Category>Device</tds:Category>\r\n"
                                         "</tds:GetCapabilities>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        const auto devNode = doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Device']").node();
        ASSERT_TRUE(devNode);

        EXPECT_TRUE(devNode.select_node(".//*[local-name()='Network']"));
        EXPECT_TRUE(devNode.select_node(".//*[local-name()='System']"));
        EXPECT_TRUE(devNode.select_node(".//*[local-name()='IO']"));
        EXPECT_TRUE(devNode.select_node(".//*[local-name()='Security']"));

        EXPECT_FALSE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Media']"));
    }

    // 4. Category=All -> All categories returned including Extension
    {
        const std::string req = wrapSoap("<tds:GetCapabilities>\r\n"
                                         "  <tds:Category>All</tds:Category>\r\n"
                                         "</tds:GetCapabilities>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Device']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Media']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='PTZ']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Imaging']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Events']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Analytics']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Extension']"));
    }

    // 5. No Category -> All categories returned by default
    {
        const std::string req = wrapSoap("<tds:GetCapabilities/>");
        const auto res = client.Post("/onvif/device_service", req, kSoapCt);
        ASSERT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        ASSERT_TRUE(doc.load_string(res->body.c_str()));

        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Device']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Media']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='PTZ']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Imaging']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Events']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Analytics']"));
        EXPECT_TRUE(doc.select_node(".//*[local-name()='Capabilities']/*[local-name()='Extension']"));
    }
}
