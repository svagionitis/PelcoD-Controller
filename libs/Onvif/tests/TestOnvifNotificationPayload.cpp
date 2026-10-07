/// @file TestOnvifNotificationPayload.cpp
/// @brief TDD test suite for ONVIF review finding H4 (Notification payload compliance).
/// @details Validates that PullMessages and push notifications comply with ONVIF Core §9.2,
///          Profile S §8, OASIS WS-BaseNotification §3.1-§3.2, and WS-Addressing 1.0.

#include "OnvifTestClient.h"

#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifTypes.h>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;
using OnvifTest::OnvifTestClient;

namespace {

/// @brief Atomic counter to assign unique port numbers for tests.
std::atomic<int> g_notifTestPort { 19400 };

/// @brief Wraps SOAP body in standard envelope.
/// @param[in] bodyXml Inner XML body.
/// @return Complete SOAP envelope.
[[nodiscard]] std::string wrapEnvelope(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
           "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
           "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
           "  <s:Body>\r\n"
        + bodyXml
        + "  </s:Body>\r\n"
          "</s:Envelope>";
}

/// @brief Builds a CreatePullPointSubscription SOAP body.
/// @return Request payload string.
[[nodiscard]] std::string makeCreatePullPointBody()
{
    return "    <tev:CreatePullPointSubscription>\r\n"
           "      <tev:InitialTerminationTime>PT60S</tev:InitialTerminationTime>\r\n"
           "    </tev:CreatePullPointSubscription>\r\n";
}

/// @brief Builds a PullMessages SOAP body.
/// @return Request payload string.
[[nodiscard]] std::string makePullMessagesBody()
{
    return "    <tev:PullMessages>\r\n"
           "      <tev:Timeout>PT5S</tev:Timeout>\r\n"
           "      <tev:MessageLimit>10</tev:MessageLimit>\r\n"
           "    </tev:PullMessages>\r\n";
}

/// @brief Builds a Subscribe request SOAP body for push notifications.
/// @param[in] consumerUrl Target push consumer URL.
/// @return Request payload string.
[[nodiscard]] std::string makeSubscribeBody(const std::string& consumerUrl)
{
    return "    <wsnt:Subscribe>\r\n"
           "      <wsnt:ConsumerReference>\r\n"
           "        <wsa:Address>"
        + consumerUrl
        + "</wsa:Address>\r\n"
          "      </wsnt:ConsumerReference>\r\n"
          "      <wsnt:InitialTerminationTime>PT60S</wsnt:InitialTerminationTime>\r\n"
          "    </wsnt:Subscribe>\r\n";
}

} // namespace

/// @class OnvifNotificationPayloadTest
/// @brief Integration fixture testing notification wire format and compliance.
class OnvifNotificationPayloadTest : public ::testing::Test {
protected:
    int m_serverPort { g_notifTestPort.fetch_add(1) };
    int m_consumerPort { g_notifTestPort.fetch_add(1) };
    OnvifServerConfig m_config {};
    std::unique_ptr<OnvifServer> m_server {};
    std::unique_ptr<OnvifTestClient> m_client {};

    void SetUp() override
    {
        m_config.bindAddress = "127.0.0.1";
        m_config.port = m_serverPort;
        m_config.auth.enabled = false;
        m_config.notification.allowLoopback = true; // Enable local consumer testing

        m_server = std::make_unique<OnvifServer>(m_config);
        ASSERT_TRUE(m_server->start());

        m_client = std::make_unique<OnvifTestClient>("127.0.0.1", m_serverPort);
        m_client->set_connection_timeout(2, 0);
        m_client->set_read_timeout(3, 0);
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
    }
};

/// @brief Verifies that SubscriptionReference uses WS-Addressing 1.0 (2005/08) namespace.
TEST_F(OnvifNotificationPayloadTest, SubscriptionRefUsesWsa200508)
{
    const std::string reqXml { wrapEnvelope(makeCreatePullPointBody()) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(res->body.c_str()));

    // Verify wsa:Address is present
    const auto addrNode { doc.select_node("//*[local-name()='Address']").node() };
    ASSERT_FALSE(addrNode.empty());

    // Verify that the SOAP Envelope declares WS-Addressing 2005/08, NOT 2004/08
    EXPECT_TRUE(res->body.find("http://www.w3.org/2005/08/addressing") != std::string::npos);
    EXPECT_TRUE(res->body.find("http://schemas.xmlsoap.org/ws/2004/08/addressing") == std::string::npos);
}

/// @brief Verifies that PullMessages outputs compliant tt:Message, PropertyOperation, and tns1 namespace.
TEST_F(OnvifNotificationPayloadTest, PullMessagesCompliantStructure)
{
    // 1. Create PullPoint subscription
    const std::string createXml { wrapEnvelope(makeCreatePullPointBody()) };
    const auto createRes { m_client->Post("/onvif/event_service", createXml, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(createRes);
    ASSERT_EQ(createRes->status, 200);

    pugi::xml_document createDoc {};
    ASSERT_TRUE(createDoc.load_string(createRes->body.c_str()));
    const std::string subAddr { createDoc.select_node("//*[local-name()='Address']").node().text().as_string() };
    ASSERT_FALSE(subAddr.empty());

    const auto slashPos { subAddr.find("/onvif/events/subscription/") };
    ASSERT_NE(slashPos, std::string::npos);
    const std::string subPath { subAddr.substr(slashPos) };

    // 2. Publish an event
    OnvifEvent ev {};
    ev.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
    ev.sourceName = "VideoSourceConfigurationToken";
    ev.sourceValue = "VideoSourceToken_1";
    ev.dataName = "IsMotion";
    ev.dataValue = "true";
    ev.propertyOperation = "Changed";
    m_server->publishEvent(ev);

    // 3. Pull messages
    const std::string pullXml { wrapEnvelope(makePullMessagesBody()) };
    const auto pullRes { m_client->Post(subPath.c_str(), pullXml, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(pullRes);
    ASSERT_EQ(pullRes->status, 200);

    pugi::xml_document pullDoc {};
    ASSERT_TRUE(pullDoc.load_string(pullRes->body.c_str()));

    // 4. Verify topic declares tns1 namespace prefix
    const auto topicNode { pullDoc.select_node("//*[local-name()='Topic']").node() };
    ASSERT_FALSE(topicNode.empty());
    EXPECT_STREQ(topicNode.text().as_string(), "tns1:RuleEngine/CellMotionDetector/Motion");

    // The topic node itself or Envelope MUST declare xmlns:tns1="http://www.onvif.org/ver10/topics"
    EXPECT_TRUE(pullRes->body.find("http://www.onvif.org/ver10/topics") != std::string::npos);

    // 5. Verify wsnt:Message wraps tt:Message
    const auto wsntMessageNode {
        pullDoc.select_node("//*[local-name()='NotificationMessage']/*[local-name()='Message']").node()
    };
    ASSERT_FALSE(wsntMessageNode.empty());

    // In compliant ONVIF, wsnt:Message has a child tt:Message
    const auto ttMessageNode { wsntMessageNode.child("tt:Message") };
    ASSERT_FALSE(ttMessageNode.empty());

    // 6. Verify PropertyOperation attribute on tt:Message
    const std::string propOp { ttMessageNode.attribute("PropertyOperation").as_string() };
    EXPECT_EQ(propOp, "Changed");

    // 7. Verify UtcTime attribute on tt:Message
    const std::string utcTime { ttMessageNode.attribute("UtcTime").as_string() };
    EXPECT_FALSE(utcTime.empty());

    // 8. Verify Source and Data are children of tt:Message
    const auto sourceItem {
        ttMessageNode.select_node(".//tt:SimpleItem[@Name='VideoSourceConfigurationToken']").node()
    };
    ASSERT_FALSE(sourceItem.empty());
    EXPECT_STREQ(sourceItem.attribute("Value").as_string(), "VideoSourceToken_1");

    const auto dataItem { ttMessageNode.select_node(".//tt:SimpleItem[@Name='IsMotion']").node() };
    ASSERT_FALSE(dataItem.empty());
    EXPECT_STREQ(dataItem.attribute("Value").as_string(), "true");
}

/// @brief Verifies that push notifications deliver a compliant wsnt:Notify SOAP envelope.
TEST_F(OnvifNotificationPayloadTest, PushDeliversWsntNotifyEnvelope)
{
    // 1. Start a local mock consumer HTTP server
    httplib::Server consumerServer {};
    std::mutex postMutex {};
    std::condition_variable postCv {};
    std::string receivedContentType {};
    std::string receivedBody {};
    bool receivedPost { false };

    consumerServer.Post("/consumer/notify", [&](const httplib::Request& req, httplib::Response& res) {
        std::scoped_lock lock(postMutex);
        receivedContentType = req.get_header_value("Content-Type");
        receivedBody = req.body;
        receivedPost = true;
        res.status = 200;
        postCv.notify_all();
    });

    std::thread consumerThread([&]() { consumerServer.listen("127.0.0.1", m_consumerPort); });

    // Wait until consumer server is actively listening
    for (int i { 0 }; i < 20; ++i) {
        if (consumerServer.is_running()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(consumerServer.is_running());

    const std::string consumerUrl { "http://127.0.0.1:" + std::to_string(m_consumerPort) + "/consumer/notify" };

    // 2. Subscribe to push notifications
    const std::string subXml { wrapEnvelope(makeSubscribeBody(consumerUrl)) };
    const auto subRes { m_client->Post("/onvif/event_service", subXml, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(subRes);
    ASSERT_EQ(subRes->status, 200);

    // 3. Publish an event
    OnvifEvent ev {};
    ev.topic = "tns1:RuleEngine/LineDetector/Crossed";
    ev.sourceName = "LineToken";
    ev.sourceValue = "Line_1";
    ev.dataName = "Crossed";
    ev.dataValue = "true";
    ev.propertyOperation = "Changed";
    m_server->publishEvent(ev);

    // 4. Wait for worker to deliver notification
    {
        std::unique_lock lock(postMutex);
        ASSERT_TRUE(postCv.wait_for(lock, std::chrono::seconds(5), [&]() { return receivedPost; }));
    }

    consumerServer.stop();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }

    // 5. Inspect the delivered payload
    ASSERT_FALSE(receivedBody.empty());
    // The previous buggy behavior sent literal "<NotificationMessage/>"
    EXPECT_NE(receivedBody, "<NotificationMessage/>");

    pugi::xml_document pushDoc {};
    ASSERT_TRUE(pushDoc.load_string(receivedBody.c_str()));

    // Verify root is SOAP Envelope
    const auto envNode { pushDoc.first_child() };
    EXPECT_TRUE(std::string(envNode.name()).find("Envelope") != std::string::npos);

    // Verify WS-Addressing Header
    const auto actionNode { pushDoc.select_node("//*[local-name()='Header']/*[local-name()='Action']").node() };
    ASSERT_FALSE(actionNode.empty());
    EXPECT_STREQ(actionNode.text().as_string(), "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify");

    const auto toNode { pushDoc.select_node("//*[local-name()='Header']/*[local-name()='To']").node() };
    ASSERT_FALSE(toNode.empty());
    EXPECT_STREQ(toNode.text().as_string(), consumerUrl.c_str());

    const auto msgIdNode { pushDoc.select_node("//*[local-name()='Header']/*[local-name()='MessageID']").node() };
    ASSERT_FALSE(msgIdNode.empty());
    EXPECT_TRUE(std::string(msgIdNode.text().as_string()).rfind("urn:uuid:", 0) == 0);

    // Verify Body contains wsnt:Notify wrapping wsnt:NotificationMessage
    const auto notifyNode { pushDoc.select_node("//*[local-name()='Body']/*[local-name()='Notify']").node() };
    ASSERT_FALSE(notifyNode.empty());

    const auto notifMsgNode { notifyNode.child("wsnt:NotificationMessage") };
    ASSERT_FALSE(notifMsgNode.empty());

    // Verify tt:Message inside wsnt:Message with PropertyOperation
    const auto ttMsgNode { notifMsgNode.select_node(".//tt:Message").node() };
    ASSERT_FALSE(ttMsgNode.empty());
    EXPECT_STREQ(ttMsgNode.attribute("PropertyOperation").as_string(), "Changed");

    // Verify Content-Type
    EXPECT_TRUE(receivedContentType.find("application/soap+xml") != std::string::npos);
}

/// @brief Verifies that OnvifClient parses both compliant nested tt:Message and legacy payloads.
TEST_F(OnvifNotificationPayloadTest, ClientParsesNestedTtMessageAndLegacy)
{
    // Compliant nested format
    const std::string compliantXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
          "xmlns:tns1=\"http://www.onvif.org/ver10/topics\" "
          "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tev:PullMessagesResponse>\n"
          "      <wsnt:NotificationMessage>\n"
          "        <wsnt:Topic "
          "xmlns:tns1=\"http://www.onvif.org/ver10/topics\">tns1:RuleEngine/CellMotionDetector/Motion</wsnt:Topic>\n"
          "        <wsnt:Message>\n"
          "          <tt:Message UtcTime=\"2026-10-07T14:40:05Z\" PropertyOperation=\"Initialized\">\n"
          "            <tt:Source>\n"
          "              <tt:SimpleItem Name=\"VideoSourceConfigurationToken\" Value=\"VideoSource_1\"/>\n"
          "            </tt:Source>\n"
          "            <tt:Data>\n"
          "              <tt:SimpleItem Name=\"IsMotion\" Value=\"true\"/>\n"
          "            </tt:Data>\n"
          "          </tt:Message>\n"
          "        </wsnt:Message>\n"
          "      </wsnt:NotificationMessage>\n"
          "    </tev:PullMessagesResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto compliantEvents { Onvif::OnvifClient::parsePullMessagesResponse(compliantXml) };
    ASSERT_EQ(compliantEvents.size(), 1U);
    EXPECT_EQ(compliantEvents[0].topic, "tns1:RuleEngine/CellMotionDetector/Motion");
    EXPECT_EQ(compliantEvents[0].utcTime, "2026-10-07T14:40:05Z");
    EXPECT_EQ(compliantEvents[0].propertyOperation, "Initialized");
    EXPECT_EQ(compliantEvents[0].sourceName, "VideoSourceConfigurationToken");
    EXPECT_EQ(compliantEvents[0].sourceValue, "VideoSource_1");
    EXPECT_EQ(compliantEvents[0].dataName, "IsMotion");
    EXPECT_EQ(compliantEvents[0].dataValue, "true");

    // Legacy flat format
    const std::string legacyXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
                                  "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
                                  "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
                                  "  <SOAP-ENV:Body>\n"
                                  "    <tev:PullMessagesResponse>\n"
                                  "      <wsnt:NotificationMessage>\n"
                                  "        <wsnt:Topic>tns1:PTZController/PTZPresets/Reached</wsnt:Topic>\n"
                                  "        <wsnt:Message UtcTime=\"2026-10-07T14:40:06Z\">\n"
                                  "          <tt:Source>\n"
                                  "            <tt:SimpleItem Name=\"PresetToken\" Value=\"Preset_1\"/>\n"
                                  "          </tt:Source>\n"
                                  "          <tt:Data>\n"
                                  "            <tt:SimpleItem Name=\"State\" Value=\"Reached\"/>\n"
                                  "          </tt:Data>\n"
                                  "        </wsnt:Message>\n"
                                  "      </wsnt:NotificationMessage>\n"
                                  "    </tev:PullMessagesResponse>\n"
                                  "  </SOAP-ENV:Body>\n"
                                  "</SOAP-ENV:Envelope>";

    const auto legacyEvents { Onvif::OnvifClient::parsePullMessagesResponse(legacyXml) };
    ASSERT_EQ(legacyEvents.size(), 1U);
    EXPECT_EQ(legacyEvents[0].topic, "tns1:PTZController/PTZPresets/Reached");
    EXPECT_EQ(legacyEvents[0].utcTime, "2026-10-07T14:40:06Z");
    EXPECT_EQ(legacyEvents[0].sourceName, "PresetToken");
    EXPECT_EQ(legacyEvents[0].sourceValue, "Preset_1");
    EXPECT_EQ(legacyEvents[0].dataName, "State");
    EXPECT_EQ(legacyEvents[0].dataValue, "Reached");
}
