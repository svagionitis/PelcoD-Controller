/// @file TestClientEventParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering Event and PullPoint responses.

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientEventParsingTest, PullPointEventsParsing)
{
    // 1. Parse subscription creation response
    const std::string subXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
          "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tev:CreatePullPointSubscriptionResponse>\n"
          "      <tev:SubscriptionReference>\n"
          "        <wsa:Address>http://192.168.1.100:8080/onvif/Subscription?idx=42</wsa:Address>\n"
          "      </tev:SubscriptionReference>\n"
          "      <wsnt:CurrentTime>2026-09-16T20:40:00Z</wsnt:CurrentTime>\n"
          "      <wsnt:TerminationTime>2026-09-16T20:41:00Z</wsnt:TerminationTime>\n"
          "    </tev:CreatePullPointSubscriptionResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto subUrl = Onvif::OnvifClient::parseCreatePullPointSubResp(subXml);
    ASSERT_TRUE(subUrl.has_value());
    EXPECT_EQ(*subUrl, "http://192.168.1.100:8080/onvif/Subscription?idx=42");

    // 2. Parse pull messages response with motion & tamper events
    const std::string pullXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
          "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tev:PullMessagesResponse>\n"
          "      <wsnt:NotificationMessage>\n"
          "        <wsnt:Topic>tns1:RuleEngine/CellMotionDetector/Motion</wsnt:Topic>\n"
          "        <wsnt:Message UtcTime=\"2026-09-16T20:40:05Z\">\n"
          "          <tt:Source>\n"
          "            <tt:SimpleItem Name=\"VideoSourceConfigurationToken\" Value=\"VideoSource_1\"/>\n"
          "          </tt:Source>\n"
          "          <tt:Data>\n"
          "            <tt:SimpleItem Name=\"IsMotion\" Value=\"true\"/>\n"
          "          </tt:Data>\n"
          "        </wsnt:Message>\n"
          "      </wsnt:NotificationMessage>\n"
          "    </tev:PullMessagesResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto events = Onvif::OnvifClient::parsePullMessagesResponse(pullXml);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].topic, "tns1:RuleEngine/CellMotionDetector/Motion");
    EXPECT_EQ(events[0].sourceName, "VideoSourceConfigurationToken");
    EXPECT_EQ(events[0].sourceValue, "VideoSource_1");
    EXPECT_EQ(events[0].dataName, "IsMotion");
    EXPECT_EQ(events[0].dataValue, "true");
    EXPECT_EQ(events[0].utcTime, "2026-09-16T20:40:05Z");
}
