/// @file TestOnvifDeviceService.cpp
/// @brief Modular unit tests for OnvifServer Device Management service operations.

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"

using namespace Onvif;
using namespace Onvif::Test;

TEST_F(OnvifServerFixture, DeviceUsersLifecycle)
{
    // 1. GetUsers (defaults: admin)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetUsers/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        pugi::xml_document doc { };
        const int status = sendSoapReq("/onvif/device_service", req, doc);
        EXPECT_EQ(status, 200);
        const auto users = doc.select_nodes("//*[local-name()='User']");
        EXPECT_GE(users.size(), 1U);
    }

    // 2. CreateUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:CreateUsers>\r\n"
                                "      <tds:User>\r\n"
                                "        <tt:Username>guard1</tt:Username>\r\n"
                                "        <tt:Password>StR0ng_T3st_P@ss</tt:Password>\r\n"
                                "        <tt:UserLevel>User</tt:UserLevel>\r\n"
                                "      </tds:User>\r\n"
                                "    </tds:CreateUsers>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        pugi::xml_document doc { };
        const int status = sendSoapReq("/onvif/device_service", req, doc);
        EXPECT_EQ(status, 200);
        EXPECT_TRUE(doc.select_node("//*[local-name()='CreateUsersResponse']"));
    }

    // 3. Verify user created via GetUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetUsers/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        pugi::xml_document doc { };
        const int status = sendSoapReq("/onvif/device_service", req, doc);
        EXPECT_EQ(status, 200);
        EXPECT_TRUE(doc.select_node("//*[local-name()='Username'][text()='guard1']"));
    }

    // 4. SetUser (update role)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:SetUser>\r\n"
                                "      <tds:User>\r\n"
                                "        <tt:Username>guard1</tt:Username>\r\n"
                                "        <tt:Password>An0th3r_Str0ng_P@ss</tt:Password>\r\n"
                                "        <tt:UserLevel>Operator</tt:UserLevel>\r\n"
                                "      </tds:User>\r\n"
                                "    </tds:SetUser>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        pugi::xml_document doc { };
        const int status = sendSoapReq("/onvif/device_service", req, doc);
        EXPECT_EQ(status, 200);
        EXPECT_TRUE(doc.select_node("//*[local-name()='SetUserResponse']"));
    }

    // 5. DeleteUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:DeleteUsers>\r\n"
                                "      <tds:Username>guard1</tds:Username>\r\n"
                                "    </tds:DeleteUsers>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        pugi::xml_document doc { };
        const int status = sendSoapReq("/onvif/device_service", req, doc);
        EXPECT_EQ(status, 200);
        EXPECT_TRUE(doc.select_node("//*[local-name()='DeleteUsersResponse']"));
    }
}

TEST_F(OnvifServerFixture, DeviceNetworkSettings)
{
    // Network Interfaces: Get and Set
    const std::string getReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetNetworkInterfaces/>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document getDoc { };
    int status = sendSoapReq("/onvif/device_service", getReq, getDoc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(getDoc.select_node("//*[local-name()='NetworkInterfaces'][@token='eth0']"));

    const std::string setReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                               "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:SetNetworkInterfaces>\r\n"
                               "      <tds:InterfaceToken>eth0</tds:InterfaceToken>\r\n"
                               "      <tds:NetworkInterface>\r\n"
                               "        <tt:Enabled>true</tt:Enabled>\r\n"
                               "        <tt:MTU>1400</tt:MTU>\r\n"
                               "        <tt:IPv4>\r\n"
                               "          <tt:Enabled>true</tt:Enabled>\r\n"
                               "          <tt:Manual>\r\n"
                               "            <tt:Address>10.0.0.50</tt:Address>\r\n"
                               "            <tt:PrefixLength>16</tt:PrefixLength>\r\n"
                               "          </tt:Manual>\r\n"
                               "          <tt:DHCP>false</tt:DHCP>\r\n"
                               "        </tt:IPv4>\r\n"
                               "      </tds:NetworkInterface>\r\n"
                               "    </tds:SetNetworkInterfaces>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document setDoc { };
    status = sendSoapReq("/onvif/device_service", setReq, setDoc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(setDoc.select_node("//*[local-name()='SetNetworkInterfacesResponse']"));

    pugi::xml_document verifyDoc { };
    status = sendSoapReq("/onvif/device_service", getReq, verifyDoc);
    EXPECT_EQ(status, 200);
    const auto mtuNode = verifyDoc.select_node("//*[local-name()='MTU']").node();
    EXPECT_TRUE(mtuNode && std::string(mtuNode.text().as_string()) == "1400");
    const auto addrNode = verifyDoc.select_node("//*[local-name()='Address']").node();
    EXPECT_TRUE(addrNode && std::string(addrNode.text().as_string()) == "10.0.0.50");
}

TEST_F(OnvifServerFixture, DeviceGatewayDnsNtp)
{
    // Default Gateway: Set and Get
    const std::string setGwReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                 "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                 "  <SOAP-ENV:Body>\r\n"
                                 "    <tds:SetNetworkDefaultGateway>\r\n"
                                 "      <tds:IPv4Address>10.0.0.1</tds:IPv4Address>\r\n"
                                 "    </tds:SetNetworkDefaultGateway>\r\n"
                                 "  </SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";
    pugi::xml_document gwDoc { };
    int status = sendSoapReq("/onvif/device_service", setGwReq, gwDoc);
    EXPECT_EQ(status, 200);

    const std::string getGwReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                 "  <SOAP-ENV:Body>\r\n"
                                 "    <tds:GetNetworkDefaultGateway/>\r\n"
                                 "  </SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";
    pugi::xml_document getGwDoc { };
    status = sendSoapReq("/onvif/device_service", getGwReq, getGwDoc);
    EXPECT_EQ(status, 200);
    const auto gwNode = getGwDoc.select_node("//*[local-name()='IPv4Address']").node();
    EXPECT_TRUE(gwNode && std::string(gwNode.text().as_string()) == "10.0.0.1");

    // DNS: Set and Get
    const std::string setDns = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:SetDNS>\r\n"
                               "      <tds:FromDHCP>false</tds:FromDHCP>\r\n"
                               "      <tds:DNSManual><tt:IPv4Address>9.9.9.9</tt:IPv4Address></tds:DNSManual>\r\n"
                               "    </tds:SetDNS>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document dnsDoc { };
    status = sendSoapReq("/onvif/device_service", setDns, dnsDoc);
    EXPECT_EQ(status, 200);

    const std::string getDns = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetDNS/>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document getDnsDoc { };
    status = sendSoapReq("/onvif/device_service", getDns, getDnsDoc);
    EXPECT_EQ(status, 200);
    const auto dnsNode = getDnsDoc.select_node("//*[local-name()='IPv4Address']").node();
    EXPECT_TRUE(dnsNode && std::string(dnsNode.text().as_string()) == "9.9.9.9");

    // NTP: Set and Get
    const std::string setNtp = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:SetNTP>\r\n"
                               "      <tds:FromDHCP>false</tds:FromDHCP>\r\n"
                               "      <tds:NTPManual><tt:DNSname>time.cloudflare.com</tt:DNSname></tds:NTPManual>\r\n"
                               "    </tds:SetNTP>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document ntpDoc { };
    status = sendSoapReq("/onvif/device_service", setNtp, ntpDoc);
    EXPECT_EQ(status, 200);

    const std::string getNtp = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetNTP/>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document getNtpDoc { };
    status = sendSoapReq("/onvif/device_service", getNtp, getNtpDoc);
    EXPECT_EQ(status, 200);
    const auto ntpNode = getNtpDoc.select_node("//*[local-name()='DNSname']").node();
    EXPECT_TRUE(ntpNode && std::string(ntpNode.text().as_string()) == "time.cloudflare.com");
}

TEST_F(OnvifServerFixture, DeviceHostnameAndScopes)
{
    // Hostname: Set and Get
    const std::string setHn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                              "  <SOAP-ENV:Body>\r\n"
                              "    <tds:SetHostname><tds:Name>PTZ-Camera-West</tds:Name></tds:SetHostname>\r\n"
                              "  </SOAP-ENV:Body>\r\n"
                              "</SOAP-ENV:Envelope>";
    pugi::xml_document hnDoc { };
    int status = sendSoapReq("/onvif/device_service", setHn, hnDoc);
    EXPECT_EQ(status, 200);

    const std::string getHn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                              "  <SOAP-ENV:Body>\r\n"
                              "    <tds:GetHostname/>\r\n"
                              "  </SOAP-ENV:Body>\r\n"
                              "</SOAP-ENV:Envelope>";
    pugi::xml_document getHnDoc { };
    status = sendSoapReq("/onvif/device_service", getHn, getHnDoc);
    EXPECT_EQ(status, 200);
    const auto nameNode = getHnDoc.select_node("//*[local-name()='Name']").node();
    EXPECT_TRUE(nameNode && std::string(nameNode.text().as_string()) == "PTZ-Camera-West");

    // Scopes: Add, Get, Remove
    const std::string addSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                              "  <SOAP-ENV:Body>\r\n"
                              "    <tds:AddScopes>\r\n"
                              "      <tds:ScopeItem>onvif://www.onvif.org/location/Sector4</tds:ScopeItem>\r\n"
                              "    </tds:AddScopes>\r\n"
                              "  </SOAP-ENV:Body>\r\n"
                              "</SOAP-ENV:Envelope>";
    pugi::xml_document addScDoc { };
    status = sendSoapReq("/onvif/device_service", addSc, addScDoc);
    EXPECT_EQ(status, 200);

    const std::string getSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                              "  <SOAP-ENV:Body>\r\n"
                              "    <tds:GetScopes/>\r\n"
                              "  </SOAP-ENV:Body>\r\n"
                              "</SOAP-ENV:Envelope>";
    pugi::xml_document getScDoc { };
    status = sendSoapReq("/onvif/device_service", getSc, getScDoc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(getScDoc.select_node("//*[local-name()='ScopeItem'][contains(text(), 'Sector4')]"));

    const std::string remSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                              "  <SOAP-ENV:Body>\r\n"
                              "    <tds:RemoveScopes>\r\n"
                              "      <tds:ScopeItem>onvif://www.onvif.org/location/Sector4</tds:ScopeItem>\r\n"
                              "    </tds:RemoveScopes>\r\n"
                              "  </SOAP-ENV:Body>\r\n"
                              "</SOAP-ENV:Envelope>";
    pugi::xml_document remScDoc { };
    status = sendSoapReq("/onvif/device_service", remSc, remScDoc);
    EXPECT_EQ(status, 200);
}

TEST_F(OnvifServerFixture, DeviceRebootAndFactoryDefault)
{
    const std::string reboot = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:SystemReboot/>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document rebootDoc { };
    int status = sendSoapReq("/onvif/device_service", reboot, rebootDoc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(rebootDoc.select_node("//*[local-name()='SystemRebootResponse']"));

    const std::string factory = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:SetSystemFactoryDefault>"
                                "      <tds:FactoryDefault>Soft</tds:FactoryDefault>"
                                "    </tds:SetSystemFactoryDefault>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
    pugi::xml_document factoryDoc { };
    status = sendSoapReq("/onvif/device_service", factory, factoryDoc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(factoryDoc.select_node("//*[local-name()='SetSystemFactoryDefaultResponse']"));
}
