/// @file TestOnvifEventsProfileS.cpp
/// @brief Conformance tests for ONVIF Profile S Event operations, SetSynchronizationPoint, and topic filtering.

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

/// @brief Wraps inner body XML with SOAP 1.2 envelope and Event namespaces.
/// @param[in] bodyXml Inner SOAP body payload.
/// @return Full SOAP envelope string.
std::string wrapSoapEvents(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
           "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
           "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
           "  <s:Body>\r\n"
        + bodyXml
        + "\r\n  </s:Body>\r\n"
          "</s:Envelope>";
}

/// @struct TestEventsServerContext
/// @brief Scoped test fixture holding a running OnvifServer instance.
struct TestEventsServerContext {
    OnvifServerConfig config {};
    int port { static_cast<int>(Onvif::Test::getFreeEphemeralPort()) };
    std::unique_ptr<OnvifServer> server {};

    TestEventsServerContext()
    {
        config.port = port;
        config.bindAddress = "127.0.0.1";
        config.defaultUsers = OnvifTest::defaultTestUsers();

        server = std::make_unique<OnvifServer>(config);
        EXPECT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestEventsServerContext()
    {
        if (server) {
            server->stop();
        }
    }
};

} // namespace

// ============================================================================
// 1. AccessPolicy Events Classifications
// ============================================================================

TEST(TestOnvifEventsProfileS, AccessPolicyClassifications)
{
    EXPECT_EQ(AccessPolicy::classify("Events", "tev:GetServiceCapabilities"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("Events", "tev:SetSynchronizationPoint"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("PullPoint", "tev:SetSynchronizationPoint"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Events", "tev:CreatePullPointSubscription"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Events", "tev:GetEventProperties"), AccessClass::ReadMedia);
}

// ============================================================================
// 2. Event Service Capabilities (Task 5.3)
// ============================================================================

TEST(TestOnvifEventsProfileS, GetServiceCapabilities)
{
    TestEventsServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    const std::string req = wrapSoapEvents("<tev:GetServiceCapabilities/>");
    const auto res = client.Post("/onvif/event_service", req, kSoapCt);
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    const auto respNode = doc.select_node(".//*[local-name()='GetServiceCapabilitiesResponse']").node();
    ASSERT_TRUE(respNode);

    const auto capsNode = respNode.select_node(".//*[local-name()='Capabilities']").node();
    ASSERT_TRUE(capsNode);

    EXPECT_STREQ(capsNode.attribute("WSSubscriptionPolicySupport").as_string(), "true");
    EXPECT_STREQ(capsNode.attribute("WSPullPointSupport").as_string(), "true");
    EXPECT_STREQ(capsNode.attribute("MaxPullPoints").as_string(), "10");
    EXPECT_STREQ(capsNode.attribute("PersistentNotificationStorage").as_string(), "false");
}

// ============================================================================
// 3. SetSynchronizationPoint & State Initialization (Task 5.2)
// ============================================================================

TEST(TestOnvifEventsProfileS, SetSyncPointEnqueuesInit)
{
    TestEventsServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Create a PullPoint subscription
    const std::string createSubReq = wrapSoapEvents("<tev:CreatePullPointSubscription>"
                                                    "  <tev:InitialTerminationTime>PT300S</tev:InitialTerminationTime>"
                                                    "</tev:CreatePullPointSubscription>");
    const auto createRes = client.Post("/onvif/event_service", createSubReq, kSoapCt);
    ASSERT_TRUE(createRes && createRes->status == 200);

    pugi::xml_document createDoc {};
    ASSERT_TRUE(createDoc.load_string(createRes->body.c_str()));

    const auto addrNode
        = createDoc.select_node(".//*[local-name()='SubscriptionReference']/*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string subAddr = addrNode.text().as_string();
    ASSERT_FALSE(subAddr.empty());

    // Extract subId
    const std::string prefix = "/onvif/events/subscription/";
    const auto pos = subAddr.rfind(prefix);
    ASSERT_NE(pos, std::string::npos);
    const std::string subId = subAddr.substr(pos + prefix.length());
    ASSERT_FALSE(subId.empty());

    const std::string subPath = prefix + subId;

    // 2. Call SetSynchronizationPoint on the subscription endpoint
    const std::string syncReq = wrapSoapEvents("<tev:SetSynchronizationPoint/>");
    const auto syncRes = client.Post(subPath, syncReq, kSoapCt);
    ASSERT_TRUE(syncRes && syncRes->status == 200);
    EXPECT_NE(syncRes->body.find("SetSynchronizationPointResponse"), std::string::npos);

    // 3. PullMessages from subscription
    const std::string pullReq = wrapSoapEvents("<tev:PullMessages>"
                                               "  <tev:Timeout>PT1S</tev:Timeout>"
                                               "  <tev:MessageLimit>10</tev:MessageLimit>"
                                               "</tev:PullMessages>");
    const auto pullRes = client.Post(subPath, pullReq, kSoapCt);
    ASSERT_TRUE(pullRes && pullRes->status == 200);

    pugi::xml_document pullDoc {};
    ASSERT_TRUE(pullDoc.load_string(pullRes->body.c_str()));

    // Verify Initialized events are present
    const auto msgs = pullDoc.select_nodes(".//*[local-name()='NotificationMessage']");
    ASSERT_GE(msgs.size(), 3U);

    bool foundRelay { false };
    bool foundInput { false };
    bool foundMotion { false };

    for (const auto& item : msgs) {
        const auto node = item.node();
        const auto topicNode = node.select_node(".//*[local-name()='Topic']").node();
        const std::string topic = topicNode ? topicNode.text().as_string() : "";

        const auto msgNode = node.select_node(".//*[local-name()='Message']").node();
        if (msgNode) {
            const std::string propOp = msgNode.attribute("PropertyOperation").as_string();
            EXPECT_EQ(propOp, "Initialized");
        }

        if (topic.find("Device/Trigger/Relay") != std::string::npos) {
            foundRelay = true;
        } else if (topic.find("Device/Trigger/DigitalInput") != std::string::npos) {
            foundInput = true;
        } else if (topic.find("CellMotionDetector/Motion") != std::string::npos) {
            foundMotion = true;
        }
    }

    EXPECT_TRUE(foundRelay);
    EXPECT_TRUE(foundInput);
    EXPECT_TRUE(foundMotion);
}

// ============================================================================
// 4. SetSynchronizationPoint on Event Service and Invalid Token Handling
// ============================================================================

TEST(TestOnvifEventsProfileS, SetSyncPointVariants)
{
    TestEventsServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. SetSynchronizationPoint on /onvif/event_service
    {
        const std::string syncReq = wrapSoapEvents("<tev:SetSynchronizationPoint/>");
        const auto syncRes = client.Post("/onvif/event_service", syncReq, kSoapCt);
        ASSERT_TRUE(syncRes && syncRes->status == 200);
        EXPECT_NE(syncRes->body.find("SetSynchronizationPointResponse"), std::string::npos);
    }

    // 2. SetSynchronizationPoint on invalid subscription URI -> ResourceUnknownFault
    {
        const std::string syncReq = wrapSoapEvents("<tev:SetSynchronizationPoint/>");
        const auto syncRes = client.Post("/onvif/events/subscription/NonExistentSub_999", syncReq, kSoapCt);
        ASSERT_TRUE(syncRes && syncRes->status == 400);
        EXPECT_NE(syncRes->body.find("ResourceUnknownFault"), std::string::npos);
    }
}

// ============================================================================
// 5. Topic Filter Parsing & Dispatch Matching (Task 5.3)
// ============================================================================

TEST(TestOnvifEventsProfileS, TopicFilterMatching)
{
    TestEventsServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Create PullPoint subscription with exact topic filter for Motion
    const std::string createSubReq = wrapSoapEvents(
        "<tev:CreatePullPointSubscription>"
        "  <tev:Filter>"
        "    <wsnt:TopicExpression Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
        "      tns1:RuleEngine/CellMotionDetector/Motion"
        "    </wsnt:TopicExpression>"
        "  </tev:Filter>"
        "  <tev:InitialTerminationTime>PT300S</tev:InitialTerminationTime>"
        "</tev:CreatePullPointSubscription>");
    const auto createRes = client.Post("/onvif/event_service", createSubReq, kSoapCt);
    ASSERT_TRUE(createRes && createRes->status == 200);

    pugi::xml_document createDoc {};
    ASSERT_TRUE(createDoc.load_string(createRes->body.c_str()));

    const auto addrNode
        = createDoc.select_node(".//*[local-name()='SubscriptionReference']/*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string subAddr = addrNode.text().as_string();
    const std::string prefix = "/onvif/events/subscription/";
    const auto pos = subAddr.rfind(prefix);
    ASSERT_NE(pos, std::string::npos);
    const std::string subPath = prefix + subAddr.substr(pos + prefix.length());

    // 2. Publish two events: one matching (Motion), one non-matching (Relay)
    OnvifEvent motionEv {};
    motionEv.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
    motionEv.sourceName = "VideoSourceConfigurationToken";
    motionEv.sourceValue = "VideoSourceToken_1";
    motionEv.dataName = "IsMotion";
    motionEv.dataValue = "true";
    motionEv.utcTime = "2026-10-22T12:00:00Z";
    motionEv.propertyOperation = "Changed";
    ctx.server->publishEvent(motionEv);

    OnvifEvent relayEv {};
    relayEv.topic = "tns1:Device/Trigger/Relay";
    relayEv.sourceName = "RelayToken";
    relayEv.sourceValue = "Relay_1";
    relayEv.dataName = "LogicalState";
    relayEv.dataValue = "active";
    relayEv.utcTime = "2026-10-22T12:00:01Z";
    relayEv.propertyOperation = "Changed";
    ctx.server->publishEvent(relayEv);

    // 3. PullMessages: verify ONLY motion event was enqueued
    const std::string pullReq = wrapSoapEvents("<tev:PullMessages>"
                                               "  <tev:Timeout>PT1S</tev:Timeout>"
                                               "  <tev:MessageLimit>10</tev:MessageLimit>"
                                               "</tev:PullMessages>");
    const auto pullRes = client.Post(subPath, pullReq, kSoapCt);
    ASSERT_TRUE(pullRes && pullRes->status == 200);

    pugi::xml_document pullDoc {};
    ASSERT_TRUE(pullDoc.load_string(pullRes->body.c_str()));

    const auto msgs = pullDoc.select_nodes(".//*[local-name()='NotificationMessage']");
    ASSERT_EQ(msgs.size(), 1U);

    const auto topicNode = msgs[0].node().select_node(".//*[local-name()='Topic']").node();
    ASSERT_TRUE(topicNode);
    EXPECT_EQ(topicNode.text().as_string(), std::string("tns1:RuleEngine/CellMotionDetector/Motion"));
}

// ============================================================================
// 6. Topic Filter Wildcard Matching
// ============================================================================

TEST(TestOnvifEventsProfileS, TopicFilterWildcardMatching)
{
    TestEventsServerContext ctx {};
    OnvifTest::OnvifTestClient client("127.0.0.1", ctx.port, OnvifTest::defaultAdmin());

    // 1. Create PullPoint subscription with wildcard filter for Device
    const std::string createSubReq = wrapSoapEvents(
        "<tev:CreatePullPointSubscription>"
        "  <tev:Filter>"
        "    <wsnt:TopicExpression Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
        "      tns1:Device/*"
        "    </wsnt:TopicExpression>"
        "  </tev:Filter>"
        "  <tev:InitialTerminationTime>PT300S</tev:InitialTerminationTime>"
        "</tev:CreatePullPointSubscription>");
    const auto createRes = client.Post("/onvif/event_service", createSubReq, kSoapCt);
    ASSERT_TRUE(createRes && createRes->status == 200);

    pugi::xml_document createDoc {};
    ASSERT_TRUE(createDoc.load_string(createRes->body.c_str()));

    const auto addrNode
        = createDoc.select_node(".//*[local-name()='SubscriptionReference']/*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string subAddr = addrNode.text().as_string();
    const std::string prefix = "/onvif/events/subscription/";
    const auto pos = subAddr.rfind(prefix);
    ASSERT_NE(pos, std::string::npos);
    const std::string subPath = prefix + subAddr.substr(pos + prefix.length());

    // 2. Publish two events: matching Device/Trigger/Relay, and non-matching RuleEngine
    OnvifEvent relayEv {};
    relayEv.topic = "tns1:Device/Trigger/Relay";
    relayEv.sourceName = "RelayToken";
    relayEv.sourceValue = "Relay_1";
    relayEv.dataName = "LogicalState";
    relayEv.dataValue = "active";
    relayEv.utcTime = "2026-10-22T12:00:00Z";
    relayEv.propertyOperation = "Changed";
    ctx.server->publishEvent(relayEv);

    OnvifEvent motionEv {};
    motionEv.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
    motionEv.sourceName = "VideoSourceConfigurationToken";
    motionEv.sourceValue = "VideoSourceToken_1";
    motionEv.dataName = "IsMotion";
    motionEv.dataValue = "true";
    motionEv.utcTime = "2026-10-22T12:00:01Z";
    motionEv.propertyOperation = "Changed";
    ctx.server->publishEvent(motionEv);

    // 3. PullMessages: verify ONLY relay event was received
    const std::string pullReq = wrapSoapEvents("<tev:PullMessages>"
                                               "  <tev:Timeout>PT1S</tev:Timeout>"
                                               "  <tev:MessageLimit>10</tev:MessageLimit>"
                                               "</tev:PullMessages>");
    const auto pullRes = client.Post(subPath, pullReq, kSoapCt);
    ASSERT_TRUE(pullRes && pullRes->status == 200);

    pugi::xml_document pullDoc {};
    ASSERT_TRUE(pullDoc.load_string(pullRes->body.c_str()));

    const auto msgs = pullDoc.select_nodes(".//*[local-name()='NotificationMessage']");
    ASSERT_EQ(msgs.size(), 1U);

    const auto topicNode = msgs[0].node().select_node(".//*[local-name()='Topic']").node();
    ASSERT_TRUE(topicNode);
    EXPECT_EQ(topicNode.text().as_string(), std::string("tns1:Device/Trigger/Relay"));
}
