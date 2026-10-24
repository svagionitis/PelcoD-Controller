/// @file TestOnvifImagingService.cpp
/// @brief Modular unit tests for OnvifServer Imaging service and optical focus control.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"

using namespace Onvif;
using namespace Onvif::Test;

TEST_F(OnvifServerFixture, ImagingSettingsAndOptions)
{
    m_imagingHandler->settings.brightness = 60.0f;
    m_imagingHandler->settings.contrast = 70.0f;

    // 1. GetImagingSettings
    const std::string getReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                               "  "
                               "<SOAP-ENV:Body><timg:GetImagingSettings><timg:VideoSourceToken>VideoSource_1</"
                               "timg:VideoSourceToken></timg:GetImagingSettings></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    pugi::xml_document getDoc { };
    int status = sendSoapReq("/onvif/imaging_service", getReq, getDoc);
    EXPECT_EQ(status, 200);
    const auto bNode = getDoc.select_node("//*[local-name()='Brightness']").node();
    EXPECT_TRUE(bNode);
    EXPECT_LT(std::abs(bNode.text().as_float() - 60.0f), 0.1f);

    // 2. SetImagingSettings
    const std::string setReq
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><timg:SetImagingSettings><timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>"
          "    "
          "<timg:ImagingSettings><tt:Brightness>85.0</tt:Brightness><tt:Contrast>45.0</tt:Contrast></"
          "timg:ImagingSettings>"
          "  </timg:SetImagingSettings></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    pugi::xml_document setDoc { };
    status = sendSoapReq("/onvif/imaging_service", setReq, setDoc);
    EXPECT_EQ(status, 200);
    EXPECT_EQ(m_imagingHandler->setSettingsCount, 1);
    EXPECT_LT(std::abs(m_imagingHandler->settings.brightness - 85.0f), 0.1f);
    EXPECT_LT(std::abs(m_imagingHandler->settings.contrast - 45.0f), 0.1f);
}

TEST_F(OnvifServerFixture, ImagingFocusMoveAndStop)
{
    // 1. Move Focus Continuous
    const std::string moveReq
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><timg:Move><timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>"
          "    <timg:Focus><tt:Continuous><tt:Speed>0.8</tt:Speed></tt:Continuous></timg:Focus>"
          "  </timg:Move></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    pugi::xml_document moveDoc { };
    int status = sendSoapReq("/onvif/imaging_service", moveReq, moveDoc);
    EXPECT_EQ(status, 200);
    EXPECT_EQ(m_imagingHandler->moveFocusCount, 1);
    EXPECT_LT(std::abs(m_imagingHandler->lastFocusSpeed - 0.8f), 0.01f);

    // 2. Stop Focus
    const std::string stopReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><timg:Stop><timg:VideoSourceToken>VideoSource_1</"
                                "timg:VideoSourceToken></timg:Stop></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document stopDoc { };
    status = sendSoapReq("/onvif/imaging_service", stopReq, stopDoc);
    EXPECT_EQ(status, 200);
    EXPECT_EQ(m_imagingHandler->stopFocusCount, 1);
}
