#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/SoapFault.h>
#include <Onvif/XmlUtils.h>

#include "OnvifTestClient.h"

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;

namespace {

/// @brief Mock PTZ handler for security tests.
class SecurityMockPtzHandler final : public IPtzHandler {
public:
    void handleContinuousMove(float /*pan*/, float /*tilt*/, float /*zoom*/) override
    {
    }
    void handleAbsoluteMove(float /*pan*/, float /*tilt*/, float /*zoom*/) override
    {
    }
    void handleStop(bool /*stopPanTilt*/, bool /*stopZoom*/) override
    {
    }

    std::string handleSetPreset(const std::string& name, const std::string& token) override
    {
        std::string tok = token.empty() ? "1" : token;
        m_presets.push_back({ tok, name });
        return tok;
    }

    bool handleGotoPreset(const std::string& /*token*/) override
    {
        return true;
    }
    bool handleRemovePreset(const std::string& /*token*/) override
    {
        return true;
    }

    std::vector<PtzPreset> handleGetPresets() override
    {
        return m_presets;
    }

    PtzStatus handleGetStatus() override
    {
        PtzStatus st {};
        return st;
    }

    std::vector<PresetTour> handleGetPresetTours() override
    {
        return m_tours;
    }

    std::optional<PresetTour> handleGetPresetTour(const std::string& token) override
    {
        for (const auto& t : m_tours) {
            if (t.token == token) {
                return t;
            }
        }
        return std::nullopt;
    }

    std::string handleCreatePresetTour() override
    {
        PresetTour tour {};
        tour.token = "tour_1";
        m_tours.push_back(tour);
        return tour.token;
    }

    bool handleModifyPresetTour(const PresetTour& tour) override
    {
        for (auto& t : m_tours) {
            if (t.token == tour.token) {
                t = tour;
                return true;
            }
        }
        return false;
    }

    bool handleOperatePresetTour(const std::string& /*tourToken*/, PresetTourOperation /*op*/) override
    {
        return true;
    }

    bool handleRemovePresetTour(const std::string& /*tourToken*/) override
    {
        return true;
    }

    std::vector<PtzPreset> m_presets {};
    std::vector<PresetTour> m_tours {};
};

/// @brief Mock OSD handler for security tests.
class SecurityMockOsdHandler final : public IOsdHandler {
public:
    std::vector<OsdConfig> handleGetOSDs(const std::string& /*videoSourceToken*/) override
    {
        return m_osds;
    }

    std::optional<OsdConfig> handleGetOSD(const std::string& token) override
    {
        for (const auto& o : m_osds) {
            if (o.token == token) {
                return o;
            }
        }
        return std::nullopt;
    }

    std::string handleCreateOSD(const OsdConfig& osd) override
    {
        std::string tok = osd.token.empty() ? "osd_1" : osd.token;
        OsdConfig copy = osd;
        copy.token = tok;
        m_osds.push_back(copy);
        return tok;
    }

    bool handleSetOSD(const OsdConfig& osd) override
    {
        for (auto& o : m_osds) {
            if (o.token == osd.token) {
                o = osd;
                return true;
            }
        }
        return false;
    }

    bool handleDeleteOSD(const std::string& token) override
    {
        for (auto it = m_osds.begin(); it != m_osds.end(); ++it) {
            if (it->token == token) {
                m_osds.erase(it);
                return true;
            }
        }
        return false;
    }

    std::vector<OsdConfig> m_osds {};
};

} // namespace

class OnvifXmlSecurityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_config.bindAddress = "127.0.0.1";
        m_config.port = 18095;
        m_config.deviceName = "Security Test Camera";
        m_config.manufacturer = "PelcoD-Security";
        m_config.model = "Model-Secure";
        m_config.rtspStreamUri = "rtsp://127.0.0.1:8554/sec_stream";
        m_config.auth.enabled = false;
        m_config.defaultUsers = { OnvifUser { "admin", "P@ssw0rd!Secure", OnvifUserLevel::Administrator } };

        m_ptzHandler = std::make_shared<SecurityMockPtzHandler>();
        m_osdHandler = std::make_shared<SecurityMockOsdHandler>();

        m_server = std::make_unique<OnvifServer>(m_config, m_ptzHandler);
        m_server->setOsdHandler(m_osdHandler);

        ASSERT_TRUE(m_server->start());
        ASSERT_TRUE(m_server->isRunning());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        m_client = std::make_unique<OnvifTest::OnvifTestClient>("127.0.0.1", 18095);
        m_client->set_connection_timeout(std::chrono::seconds(2));
        m_client->set_read_timeout(std::chrono::seconds(2));
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
    }

    OnvifServerConfig m_config {};
    std::shared_ptr<SecurityMockPtzHandler> m_ptzHandler {};
    std::shared_ptr<SecurityMockOsdHandler> m_osdHandler {};
    std::unique_ptr<OnvifServer> m_server {};
    std::unique_ptr<OnvifTest::OnvifTestClient> m_client {};
};

TEST_F(OnvifXmlSecurityTest, PresetNameInjectionBlocked)
{
    const std::string maliciousName = "Preset1</tt:Name><tt:InjectedNode>pwnd</tt:InjectedNode><tt:Name>Preset1";
    m_ptzHandler->handleSetPreset(maliciousName, "1");

    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetPresets>\r\n"
                                "      <tptz:ProfileToken>Profile_1</tptz:ProfileToken>\r\n"
                                "    </tptz:GetPresets>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    auto res = m_client->Post("/onvif/ptz_service", soapReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(res->body.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    // Verify no injected node exists
    const pugi::xml_node injectedNode = doc.select_node("//*[local-name()='InjectedNode']").node();
    EXPECT_FALSE(injectedNode);

    // Verify preset name contains the unescaped literal content
    const pugi::xml_node nameNode = doc.select_node("//*[local-name()='Name']").node();
    ASSERT_TRUE(nameNode);
    EXPECT_EQ(nameNode.text().as_string(), maliciousName);
}

TEST_F(OnvifXmlSecurityTest, TourNameXmlEntitiesEscaped)
{
    PresetTour tour {};
    tour.token = "tour_inject";
    tour.name = "Tour\"&<>'Special";
    tour.status = PresetTourState::Idle;
    tour.autoStart = false;
    m_ptzHandler->m_tours.push_back(tour);

    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetPresetTours>\r\n"
                                "      <tptz:ProfileToken>Profile_1</tptz:ProfileToken>\r\n"
                                "    </tptz:GetPresetTours>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    auto res = m_client->Post("/onvif/ptz_service", soapReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(res->body.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    const pugi::xml_node tourNode = doc.select_node("//*[local-name()='PresetTour']").node();
    ASSERT_TRUE(tourNode);
    const pugi::xml_node nameNode = tourNode.child("tt:Name");
    ASSERT_TRUE(nameNode);
    EXPECT_EQ(nameNode.text().as_string(), tour.name);
}

TEST_F(OnvifXmlSecurityTest, OsdPlainTextEscaping)
{
    OsdConfig osd {};
    osd.token = "osd_inject";
    osd.videoSourceToken = "VideoSource_1";
    osd.type = OsdType::Text;
    osd.position = OsdPositionType::UpperLeft;
    osd.plainText = "Camera & <b>Front</b> \"Gate\"";
    m_osdHandler->handleCreateOSD(osd);

    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <trt:GetOSDs>\r\n"
                                "      <trt:ConfigurationToken>VideoSource_1</trt:ConfigurationToken>\r\n"
                                "    </trt:GetOSDs>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    auto res = m_client->Post("/onvif/media_service", soapReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(res->body.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    // Ensure no <b> element was parsed
    const pugi::xml_node bNode = doc.select_node("//b").node();
    EXPECT_FALSE(bNode);

    const pugi::xml_node plainTextNode = doc.select_node("//*[local-name()='PlainText']").node();
    ASSERT_TRUE(plainTextNode);
    EXPECT_EQ(plainTextNode.text().as_string(), osd.plainText);
}

TEST_F(OnvifXmlSecurityTest, HostHeaderInjectionSanitized)
{
    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetCapabilities/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    httplib::Client rawClient("127.0.0.1", 18095);
    httplib::Headers headers;
    headers.emplace("Host", "attacker.com:8080</XAddr><InjectedTag>hacked</InjectedTag><XAddr>");

    auto res = rawClient.Post("/onvif/device_service", headers, soapReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(res->body.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    // Verify injected tag was not parsed as XML element
    const pugi::xml_node injectedNode = doc.select_node("//*[local-name()='InjectedTag']").node();
    EXPECT_FALSE(injectedNode);
}

TEST_F(OnvifXmlSecurityTest, ControlCharacterSanitizationInPresets)
{
    // Preset name with non-printable control characters (\x01, \x02, \x1f)
    const std::string rawName = std::string("SafeName\x01\x02\x1F_End");
    m_ptzHandler->handleSetPreset(rawName, "ctrl_tok");

    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetPresets>\r\n"
                                "      <tptz:ProfileToken>Profile_1</tptz:ProfileToken>\r\n"
                                "    </tptz:GetPresets>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    auto res = m_client->Post("/onvif/ptz_service", soapReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res && res->status == 200);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(res->body.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    const pugi::xml_node nameNode = doc.select_node("//*[local-name()='Name']").node();
    ASSERT_TRUE(nameNode);
    EXPECT_EQ(std::string(nameNode.text().as_string()), "SafeName_End");
}

TEST(XmlSecurityStandaloneTest, UsernameTokenInjectionPrevented)
{
    SecurityCredentials creds {};
    creds.username = "admin</wsse:Username><wsse:Password>fake</wsse:Password>";
    creds.password = "realpassword";
    const std::string headerXml = OnvifSecurity::buildSoapSecurityHeader(creds);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(headerXml.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    // Verify there is only 1 Username element
    pugi::xpath_node_set xpNodes = doc.select_nodes("//*[local-name()='Username']");
    EXPECT_EQ(xpNodes.size(), 1u);
    if (!xpNodes.empty()) {
        EXPECT_EQ(xpNodes[0].node().text().as_string(), creds.username);
    }
}

TEST(XmlSecurityStandaloneTest, SoapFaultInjectionPrevented)
{
    const std::string attackReason = "Error: </env:Text><env:Injected>bad</env:Injected>";
    const std::string faultXml = SoapFault::sender("ter:InvalidArgVal", "", attackReason);

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(faultXml.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    const pugi::xml_node injectedNode = doc.select_node("//*[local-name()='Injected']").node();
    EXPECT_FALSE(injectedNode);

    const pugi::xml_node textNode = doc.select_node("//*[local-name()='Text']").node();
    ASSERT_TRUE(textNode);
    EXPECT_EQ(std::string(textNode.text().as_string()), attackReason);
}

TEST(XmlSecurityStandaloneTest, WriteXmlTagEscaping)
{
    const std::string maliciousContent = "<hack>value</hack> & 'quote'";
    std::ostringstream ss {};
    Xml::writeXmlTag(ss, "tt:CustomTag", maliciousContent);
    const std::string tag = ss.str();

    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(tag.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    const pugi::xml_node hackNode = doc.select_node("//hack").node();
    EXPECT_FALSE(hackNode);

    const pugi::xml_node tagNode = doc.select_node("//*[local-name()='CustomTag']").node();
    ASSERT_TRUE(tagNode);
    EXPECT_EQ(std::string(tagNode.text().as_string()), maliciousContent);
}

TEST(XmlSecurityStandaloneTest, AttributeInjectionBlocked)
{
    const std::string evilAttr = "val\" injected=\"true\" other=\"";
    const std::string escaped = Xml::escapeXmlAttr(evilAttr);

    std::string element = "<tt:Item attr=\"" + escaped + "\"/>";
    pugi::xml_document doc {};
    pugi::xml_parse_result result = doc.load_string(element.c_str());
    EXPECT_EQ(result.status, pugi::status_ok);

    const pugi::xml_node item = doc.child("tt:Item");
    ASSERT_TRUE(item);
    EXPECT_FALSE(item.attribute("injected"));
    EXPECT_EQ(std::string(item.attribute("attr").as_string()), evilAttr);
}
