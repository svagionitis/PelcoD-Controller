/// @file TestOnvifServicesConformance.cpp
/// @brief Conformance and regression tests for ONVIF GetServicesResponse (Finding C4).

#include <atomic>
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <string>
#include <thread>
#include <vector>

#include "OnvifServer.h"
#include "OnvifServerTypes.h"
#include "OnvifTestClient.h"
#include "ServiceRegistry.h"

namespace {

std::atomic<int> g_port { 19450 };

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

std::string wrapSoapReq(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
           "<s:Body>\r\n"
        + bodyXml + "\r\n</s:Body></s:Envelope>";
}

TEST(TestOnvifServicesConformance, IncludeCapabilityParsing)
{
    // Test true values
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability>true</tds:IncludeCapability></tds:GetServices>");
        EXPECT_TRUE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability>1</tds:IncludeCapability></tds:GetServices>");
        EXPECT_TRUE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }

    // Test false values
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability>false</tds:IncludeCapability></tds:GetServices>");
        EXPECT_FALSE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability>0</tds:IncludeCapability></tds:GetServices>");
        EXPECT_FALSE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }

    // Test omitted and empty
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices/>");
        EXPECT_FALSE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability/></tds:GetServices>");
        EXPECT_FALSE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }
    {
        pugi::xml_document doc;
        doc.load_string("<tds:GetServices><tds:IncludeCapability>invalid</tds:IncludeCapability></tds:GetServices>");
        EXPECT_FALSE(Onvif::ServiceRegistry::parseIncludeCap(doc.first_child()));
    }
}

TEST(TestOnvifServicesConformance, XmlWellFormednessAndTagPairing)
{
    const auto registry = Onvif::ServiceRegistry::createDefault(true, true);
    const std::string xml = registry.buildServicesXml("127.0.0.1", 8080, false);

    // C4 Regression check: no mismatched tds:XAddr closed with tt:XAddr
    EXPECT_EQ(xml.find("</tt:XAddr>"), std::string::npos);
    EXPECT_NE(xml.find("<tds:XAddr>"), std::string::npos);
    EXPECT_NE(xml.find("</tds:XAddr>"), std::string::npos);

    // Strict XML 1.0 well-formedness parsing
    pugi::xml_document doc;
    const auto parseResult = doc.load_string(xml.c_str());
    EXPECT_TRUE(parseResult) << "XML parse failed: " << parseResult.description();

    const auto root = doc.child("tds:GetServicesResponse");
    EXPECT_TRUE(root);

    size_t count = 0;
    for (const auto svc : root.children("tds:Service")) {
        ++count;
        EXPECT_TRUE(svc.child("tds:Namespace"));
        EXPECT_TRUE(svc.child("tds:XAddr"));
        EXPECT_TRUE(svc.child("tds:Version"));
    }
    EXPECT_GE(count, 8U);
}

TEST(TestOnvifServicesConformance, SchemaElementSequenceCompliance)
{
    const auto registry = Onvif::ServiceRegistry::createDefault(true, true);
    const std::string xml = registry.buildServicesXml("192.168.1.50", 8080, true);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    const auto root = doc.child("tds:GetServicesResponse");
    ASSERT_TRUE(root);

    for (const auto svc : root.children("tds:Service")) {
        // Enforce strict WSDL element sequence: Namespace -> XAddr -> [Capabilities] -> Version
        int state = 0;
        for (const auto child : svc.children()) {
            const std::string name = child.name();
            if (name == "tds:Namespace") {
                EXPECT_EQ(state, 0);
                state = 1;
            } else if (name == "tds:XAddr") {
                EXPECT_EQ(state, 1);
                state = 2;
            } else if (name == "tds:Capabilities") {
                EXPECT_EQ(state, 2);
                state = 3;
            } else if (name == "tds:Version") {
                EXPECT_TRUE(state == 2 || state == 3);
                state = 4;

                // Inside Version: tt:Major then tt:Minor
                const auto major = child.child("tt:Major");
                const auto minor = child.child("tt:Minor");
                EXPECT_TRUE(major);
                EXPECT_TRUE(minor);
            }
        }
        EXPECT_EQ(state, 4);
    }
}

TEST(TestOnvifServicesConformance, ValidStandardVersionNumbers)
{
    const auto registry = Onvif::ServiceRegistry::createDefault(true, true);
    const std::string xml = registry.buildServicesXml("127.0.0.1", 8080, false);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    for (const auto svc : doc.child("tds:GetServicesResponse").children("tds:Service")) {
        const std::string ns = svc.child("tds:Namespace").text().as_string();
        const int major = svc.child("tds:Version").child("tt:Major").text().as_int();
        const int minor = svc.child("tds:Version").child("tt:Minor").text().as_int();

        // Standard ONVIF service versions must NEVER be 10, 17, or 20
        EXPECT_LT(major, 10) << "Service " << ns << " has bogus major version " << major;

        if (ns == "http://www.onvif.org/ver10/device/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 5);
        } else if (ns == "http://www.onvif.org/ver10/media/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 6);
        } else if (ns == "http://www.onvif.org/ver20/media/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver20/ptz/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver20/imaging/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/deviceIO/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/events/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 5);
        } else if (ns == "http://www.onvif.org/ver20/analytics/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/recording/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/search/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/replay/wsdl") {
            EXPECT_EQ(major, 2);
            EXPECT_EQ(minor, 0);
        } else if (ns == "http://www.onvif.org/ver10/thermal/wsdl") {
            EXPECT_EQ(major, 1);
            EXPECT_EQ(minor, 0);
        }
    }
}

TEST(TestOnvifServicesConformance, IncludeCapabilityConditioning)
{
    const auto registry = Onvif::ServiceRegistry::createDefault(true, true);

    // False case: Capabilities must be absent
    {
        const std::string xmlFalse = registry.buildServicesXml("127.0.0.1", 8080, false);
        pugi::xml_document docFalse;
        ASSERT_TRUE(docFalse.load_string(xmlFalse.c_str()));
        for (const auto svc : docFalse.child("tds:GetServicesResponse").children("tds:Service")) {
            EXPECT_FALSE(svc.child("tds:Capabilities"));
        }
    }

    // True case: Capabilities element must be present
    {
        const std::string xmlTrue = registry.buildServicesXml("127.0.0.1", 8080, true);
        pugi::xml_document docTrue;
        ASSERT_TRUE(docTrue.load_string(xmlTrue.c_str()));
        size_t capCount = 0;
        for (const auto svc : docTrue.child("tds:GetServicesResponse").children("tds:Service")) {
            if (svc.child("tds:Capabilities")) {
                ++capCount;
            }
        }
        EXPECT_GT(capCount, 0U);
    }
}

TEST(TestOnvifServicesConformance, ConditionalServiceAdvertisement)
{
    // Thermal disabled
    {
        const auto registry = Onvif::ServiceRegistry::createDefault(false, true);
        const std::string xml = registry.buildServicesXml("127.0.0.1", 8080, false);
        EXPECT_EQ(xml.find("http://www.onvif.org/ver10/thermal/wsdl"), std::string::npos);
        EXPECT_EQ(xml.find("/onvif/thermal_service"), std::string::npos);
    }

    // Profile G disabled
    {
        const auto registry = Onvif::ServiceRegistry::createDefault(true, false);
        const std::string xml = registry.buildServicesXml("127.0.0.1", 8080, false);
        EXPECT_EQ(xml.find("http://www.onvif.org/ver10/recording/wsdl"), std::string::npos);
        EXPECT_EQ(xml.find("http://www.onvif.org/ver10/search/wsdl"), std::string::npos);
        EXPECT_EQ(xml.find("http://www.onvif.org/ver10/replay/wsdl"), std::string::npos);
    }
}

TEST(TestOnvifServicesConformance, LiveServerEndToEndGetServices)
{
    const int port { g_port.fetch_add(1) };
    Onvif::OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    Onvif::OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTest::OnvifTestClient client { "127.0.0.1", port };

    // Request with IncludeCapability = true
    const std::string req
        = wrapSoapReq("<tds:GetServices><tds:IncludeCapability>true</tds:IncludeCapability></tds:GetServices>");
    const auto res = client.Post("/onvif/device_service", req, kSoapCt);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    // Verify raw XML has no tag mismatch
    EXPECT_EQ(res->body.find("</tt:XAddr>"), std::string::npos);

    // Verify full DOM parse
    pugi::xml_document doc;
    const auto parseRes = doc.load_string(res->body.c_str());
    ASSERT_TRUE(parseRes) << "XML load error: " << parseRes.description();

    const auto respNode = doc.select_node("//*[local-name()='GetServicesResponse']").node();
    ASSERT_TRUE(respNode);

    size_t serviceCount = 0;
    for (const auto svc : respNode.children()) {
        const std::string nodeName = svc.name();
        if (nodeName.find("Service") == std::string::npos) {
            continue;
        }
        ++serviceCount;

        const auto nsNode = svc.find_child(
            [](const pugi::xml_node& n) { return std::string(n.name()).find("Namespace") != std::string::npos; });
        const auto xAddrNode = svc.find_child(
            [](const pugi::xml_node& n) { return std::string(n.name()).find("XAddr") != std::string::npos; });
        const auto capNode = svc.find_child(
            [](const pugi::xml_node& n) { return std::string(n.name()).find("Capabilities") != std::string::npos; });
        const auto verNode = svc.find_child(
            [](const pugi::xml_node& n) { return std::string(n.name()).find("Version") != std::string::npos; });

        EXPECT_TRUE(nsNode);
        EXPECT_TRUE(xAddrNode);
        EXPECT_TRUE(capNode);
        EXPECT_TRUE(verNode);

        if (verNode) {
            const auto majNode = verNode.find_child(
                [](const pugi::xml_node& n) { return std::string(n.name()).find("Major") != std::string::npos; });
            const int major = majNode ? majNode.text().as_int() : -1;
            EXPECT_LT(major, 10);
            EXPECT_GT(major, 0);
        }
    }
    EXPECT_GE(serviceCount, 8U);

    server.stop();
}

} // namespace
