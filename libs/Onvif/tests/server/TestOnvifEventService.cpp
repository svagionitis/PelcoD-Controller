/// @file TestOnvifEventService.cpp
/// @brief Modular unit tests for OnvifServer Event service and PullPoint lifecycle.

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"

using namespace Onvif;
using namespace Onvif::Test;

TEST_F(OnvifServerFixture, EventPullPointLifecycle)
{
    // 1. CreatePullPointSubscription
    const std::string subReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
                               "  <SOAP-ENV:Body><tev:CreatePullPointSubscription/></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    pugi::xml_document docSub { };
    int status = sendSoapReq("/onvif/event_service", subReq, docSub);
    EXPECT_EQ(status, 200);

    const auto addrNode = docSub.select_node("//*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string subUrl = addrNode.text().as_string();
    const auto slashPos = subUrl.find("/onvif/events/subscription/");
    ASSERT_NE(slashPos, std::string::npos);
    const std::string subPath = subUrl.substr(slashPos);

    // 2. Publish mock motion detection event
    OnvifEvent motionEv { };
    motionEv.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
    motionEv.sourceName = "VideoSourceToken";
    motionEv.sourceValue = "VideoSource_1";
    motionEv.dataName = "IsMotion";
    motionEv.dataValue = "true";
    m_server->publishEvent(motionEv);

    // 3. PullMessages from subscription endpoint
    const std::string pullReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tev:PullMessages>\r\n"
                                "      <tev:Timeout>PT1S</tev:Timeout>\r\n"
                                "      <tev:MessageLimit>5</tev:MessageLimit>\r\n"
                                "    </tev:PullMessages>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document pullDoc { };
    status = sendSoapReq(subPath, pullReq, pullDoc);
    EXPECT_EQ(status, 200);

    const auto topicNode = pullDoc.select_node("//*[local-name()='Topic']").node();
    EXPECT_TRUE(topicNode && std::string(topicNode.text().as_string()) == "tns1:RuleEngine/CellMotionDetector/Motion");

    const auto dataNode = pullDoc.select_node("//*[local-name()='Data']/*[local-name()='SimpleItem']").node();
    EXPECT_TRUE(dataNode && std::string(dataNode.attribute("Value").as_string()) == "true");

    // 4. Unsubscribe
    const std::string unsubReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n"
                                 "  <SOAP-ENV:Body><wsnt:Unsubscribe/></SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";

    pugi::xml_document unsubDoc { };
    status = sendSoapReq(subPath, unsubReq, unsubDoc);
    EXPECT_EQ(status, 200);
}

TEST_F(OnvifServerFixture, EventPushSubscription)
{
    const std::string reqSub = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
                               "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <wsnt:Subscribe>\r\n"
                               "      "
                               "<wsnt:ConsumerReference><wsa:Address>http://127.0.0.1:18589/notify</wsa:Address></"
                               "wsnt:ConsumerReference>\r\n"
                               "    </wsnt:Subscribe>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/event_service", reqSub, doc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(doc.select_node("//*[local-name()='SubscribeResponse']"));
    EXPECT_TRUE(doc.select_node("//*[local-name()='SubscriptionReference']"));

    // Publish event
    OnvifEvent ev { };
    ev.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
    ev.dataName = "IsMotion";
    ev.dataValue = "true";
    m_server->publishEvent(ev);
}
