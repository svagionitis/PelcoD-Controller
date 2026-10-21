/// @file TestOnvifDiscoveryProfileS.cpp
/// @brief Conformance tests for ONVIF Profile S WS-Discovery requirements (finding P2 Phase 1).

#include <cstdint>

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "Onvif/OnvifServer.h"
#include "Onvif/OnvifServerTypes.h"
#include "Onvif/WsDiscoveryCommon.h"
#include "Onvif/WsDiscoveryServer.h"
#include "Onvif/WsDiscoveryValidator.h"

using namespace Onvif;

namespace {

std::string makeProbeXml(const std::string& scopesContent = "", const std::string& matchBy = "")
{
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                      "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                      "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                      "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">\r\n"
                      "  <s:Header>\r\n"
                      "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>\r\n"
                      "    <wsa:MessageID>urn:uuid:test-probe-1234</wsa:MessageID>\r\n"
                      "    <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>\r\n"
                      "  </s:Header>\r\n"
                      "  <s:Body>\r\n"
                      "    <d:Probe>\r\n"
                      "      <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n";

    if (!scopesContent.empty()) {
        xml += "      <d:Scopes";
        if (!matchBy.empty()) {
            xml += " MatchBy=\"" + matchBy + "\"";
        }
        xml += ">" + scopesContent + "</d:Scopes>\r\n";
    }

    xml += "    </d:Probe>\r\n"
           "  </s:Body>\r\n"
           "</s:Envelope>";
    return xml;
}

} // namespace

// ============================================================================
// 1. Profile/Streaming Scope Verification (Task 1.1)
// ============================================================================

TEST(TestOnvifDiscoveryProfileS, StreamingScopeInDefaultConfig)
{
    const OnvifServerConfig config {};
    bool foundStreamingScope { false };
    for (const auto& scope : config.scopes) {
        if (scope == "onvif://www.onvif.org/Profile/Streaming") {
            foundStreamingScope = true;
            break;
        }
    }
    EXPECT_TRUE(foundStreamingScope);
}

TEST(TestOnvifDiscoveryProfileS, StreamingScopeInProbeMatchesAndHello)
{
    OnvifServerConfig config {};
    config.port = 8080;
    config.deviceName = "TestCamera";
    config.model = "Model-S";

    const WsDiscoveryServer server(config);

    const std::string hello = server.createHelloPayload("192.168.1.100");
    EXPECT_NE(hello.find("onvif://www.onvif.org/Profile/Streaming"), std::string::npos);

    const std::string probeMatches = server.createProbeMatchesPayload("test-msg-001", "192.168.1.100");
    EXPECT_NE(probeMatches.find("onvif://www.onvif.org/Profile/Streaming"), std::string::npos);
}

// ============================================================================
// 2. AppSequence Header Verification (Task 1.2)
// ============================================================================

TEST(TestOnvifDiscoveryProfileS, AppSequenceTrackerMonotonic)
{
    AppSequenceTracker tracker(100U);
    EXPECT_EQ(tracker.instanceId(), 100U);

    const std::uint32_t first = tracker.nextMessageNum();
    const std::uint32_t second = tracker.nextMessageNum();
    const std::uint32_t third = tracker.nextMessageNum();

    EXPECT_EQ(first, 1U);
    EXPECT_EQ(second, 2U);
    EXPECT_EQ(third, 3U);

    const std::string header = tracker.formatHeader();
    EXPECT_NE(header.find("InstanceId=\"100\""), std::string::npos);
    EXPECT_NE(header.find("MessageNumber=\"4\""), std::string::npos);
}

TEST(TestOnvifDiscoveryProfileS, PayloadsContainAppSequenceHeader)
{
    OnvifServerConfig config {};
    const WsDiscoveryServer server(config);

    const std::string hello = server.createHelloPayload("127.0.0.1");
    pugi::xml_document helloDoc {};
    ASSERT_TRUE(helloDoc.load_string(hello.c_str()));
    const auto helloSeqNode = helloDoc.select_node("//*[local-name()='AppSequence']").node();
    ASSERT_TRUE(helloSeqNode);
    EXPECT_FALSE(helloSeqNode.attribute("InstanceId").empty());
    EXPECT_FALSE(helloSeqNode.attribute("MessageNumber").empty());

    const std::string pm = server.createProbeMatchesPayload("msg-1", "127.0.0.1");
    pugi::xml_document pmDoc {};
    ASSERT_TRUE(pmDoc.load_string(pm.c_str()));
    const auto pmSeqNode = pmDoc.select_node("//*[local-name()='AppSequence']").node();
    ASSERT_TRUE(pmSeqNode);
    EXPECT_FALSE(pmSeqNode.attribute("InstanceId").empty());
    EXPECT_FALSE(pmSeqNode.attribute("MessageNumber").empty());

    const std::string bye = server.createByePayload();
    pugi::xml_document byeDoc {};
    ASSERT_TRUE(byeDoc.load_string(bye.c_str()));
    const auto byeSeqNode = byeDoc.select_node("//*[local-name()='AppSequence']").node();
    ASSERT_TRUE(byeSeqNode);
    EXPECT_FALSE(byeSeqNode.attribute("InstanceId").empty());
    EXPECT_FALSE(byeSeqNode.attribute("MessageNumber").empty());
}

// ============================================================================
// 3. Probe Scope Matching RFC 3986 (Task 1.4)
// ============================================================================

TEST(TestOnvifDiscoveryProfileS, ProbeScopeMatchingRFC3986)
{
    const std::vector<std::string> activeScopes {
        "onvif://www.onvif.org/type/video_encoder",
        "onvif://www.onvif.org/type/ptz",
        "onvif://www.onvif.org/Profile/Streaming",
        "onvif://www.onvif.org/name/TestDevice",
        "onvif://www.onvif.org/hardware/ModelA",
    };

    // 1. Wildcard / Empty Scopes matches
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("").c_str()));
        EXPECT_TRUE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
        const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "srv-uuid", true, activeScopes);
        EXPECT_TRUE(res.isValidProbe);
    }

    // 2. Exact match on supported scope
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("onvif://www.onvif.org/Profile/Streaming").c_str()));
        EXPECT_TRUE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
        const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "srv-uuid", true, activeScopes);
        EXPECT_TRUE(res.isValidProbe);
    }

    // 3. Prefix match on segment boundary (RFC 3986 default)
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("onvif://www.onvif.org/type").c_str()));
        EXPECT_TRUE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
    }

    // 4. Case-insensitive scheme and authority
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("ONVIF://WWW.ONVIF.ORG/Profile/Streaming").c_str()));
        EXPECT_TRUE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
    }

    // 5. Multiple scopes: all must match
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(
            makeProbeXml("onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/type/video_encoder").c_str()));
        EXPECT_TRUE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
    }

    // 6. Unsupported scope fails match
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("onvif://www.onvif.org/Profile/NonExistentProfile").c_str()));
        EXPECT_FALSE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
        const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "srv-uuid", true, activeScopes);
        EXPECT_FALSE(res.isValidProbe);
    }

    // 7. Partial segment does not match RFC 3986 boundary (e.g. "type/video" must not match "type/video_encoder")
    {
        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(makeProbeXml("onvif://www.onvif.org/type/video").c_str()));
        EXPECT_FALSE(WsDiscoveryValidator::matchesScopes(doc, activeScopes));
    }
}

// ============================================================================
// 4. Discovery Mode State Control (Task 1.5)
// ============================================================================

TEST(TestOnvifDiscoveryProfileS, DiscoveryModeStateTracking)
{
    OnvifServerConfig config {};
    config.port = 8080;
    WsDiscoveryServer server(config);

    EXPECT_EQ(server.getDiscoveryMode(), DiscoveryMode::Discoverable);

    server.setDiscoveryMode(DiscoveryMode::NonDiscoverable);
    EXPECT_EQ(server.getDiscoveryMode(), DiscoveryMode::NonDiscoverable);

    server.setDiscoveryMode(DiscoveryMode::Discoverable);
    EXPECT_EQ(server.getDiscoveryMode(), DiscoveryMode::Discoverable);
}

TEST(TestOnvifDiscoveryProfileS, ServerDiscoveryModeControl)
{
    OnvifServerConfig config {};
    config.port = 18080;
    config.auth.enabled = false;
    config.bindAddress = "127.0.0.1";

    OnvifServer server(config);
    EXPECT_EQ(server.getDiscoveryMode(), DiscoveryMode::Discoverable);

    server.setDiscoveryMode(DiscoveryMode::NonDiscoverable);
    EXPECT_EQ(server.getDiscoveryMode(), DiscoveryMode::NonDiscoverable);
}

// ============================================================================
// 5. Scope Update Increments MetadataVersion (Task 1.5)
// ============================================================================

TEST(TestOnvifDiscoveryProfileS, ScopeUpdateIncrementsMetadataVersion)
{
    OnvifServerConfig config {};
    WsDiscoveryServer server(config);

    EXPECT_EQ(server.getMetadataVersion(), 1U);

    server.updateScopes({ "onvif://www.onvif.org/Profile/Streaming", "onvif://www.onvif.org/location/office" });
    EXPECT_EQ(server.getMetadataVersion(), 2U);

    const std::string pm = server.createProbeMatchesPayload("msg-1", "127.0.0.1");
    EXPECT_NE(pm.find("<d:MetadataVersion>2</d:MetadataVersion>"), std::string::npos);
}
