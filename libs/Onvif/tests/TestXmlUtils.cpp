#include <Onvif/XmlUtils.h>
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <sstream>
#include <string>

using namespace Onvif::Xml;

TEST(TestXmlUtils, PlainStringUnchanged)
{
    EXPECT_EQ(escapeXml(""), "");
    EXPECT_EQ(escapeXml("Hello World"), "Hello World");
    EXPECT_EQ(escapeXml("Camera_01-Profile.100"), "Camera_01-Profile.100");
}

TEST(TestXmlUtils, EscapePredefinedEntities)
{
    EXPECT_EQ(escapeXml("&"), "&amp;");
    EXPECT_EQ(escapeXml("<"), "&lt;");
    EXPECT_EQ(escapeXml(">"), "&gt;");
    EXPECT_EQ(escapeXml("\""), "&quot;");
    EXPECT_EQ(escapeXml("'"), "&apos;");
    EXPECT_EQ(escapeXml("AT&T <Devices> \"HQ\" 'North'"), "AT&amp;T &lt;Devices&gt; &quot;HQ&quot; &apos;North&apos;");
}

TEST(TestXmlUtils, SanitizeControlCharacters)
{
    // XML 1.0 illegal control characters: 0x00-0x08, 0x0B, 0x0C, 0x0E-0x1F
    const std::string badChars = std::string("A\x00"
                                             "B\x01"
                                             "C\x08"
                                             "D\x0B"
                                             "E\x0C"
                                             "F\x0E"
                                             "G\x1F"
                                             "H",
        16);
    const std::string sanitized = sanitizeXml(badChars);
    EXPECT_EQ(sanitized, "ABCDEFGH");

    // Allowed whitespace control characters: \t (0x09), \n (0x0A), \r (0x0D)
    const std::string allowedWs = "Line1\r\n\tLine2\n";
    EXPECT_EQ(sanitizeXml(allowedWs), "Line1\r\n\tLine2\n");
    EXPECT_EQ(escapeXml(allowedWs), "Line1\r\n\tLine2\n");
}

TEST(TestXmlUtils, Utf8Preservation)
{
    const std::string utf8Text = "Κάμερα & Εστίαση > 50°C \"Βορράς\"";
    const std::string expected = "Κάμερα &amp; Εστίαση &gt; 50°C &quot;Βορράς&quot;";
    EXPECT_EQ(escapeXml(utf8Text), expected);
}

TEST(TestXmlUtils, EscapeXmlTextVsAttr)
{
    const std::string sample = "A & B < C > D \"quote\" 'apostrophe'";
    // escapeXmlText keeps quotes intact
    EXPECT_EQ(escapeXmlText(sample), "A &amp; B &lt; C &gt; D \"quote\" 'apostrophe'");
    // escapeXmlAttr escapes quotes
    EXPECT_EQ(escapeXmlAttr(sample), "A &amp; B &lt; C &gt; D &quot;quote&quot; &apos;apostrophe&apos;");
}

TEST(TestXmlUtils, WriteXmlTag)
{
    std::ostringstream ss {};
    writeXmlTag(ss, "tt:Name", "Preset <1> & \"Home\"");
    EXPECT_EQ(ss.str(), "<tt:Name>Preset &lt;1&gt; &amp; &quot;Home&quot;</tt:Name>");
}

TEST(TestXmlUtils, PugiXmlRoundTripAndInjectionDefense)
{
    const std::vector<std::string> payloads = { "SimpleName", "Name with & and < and > and \" and '",
        "\"><evil_element>injected</evil_element><foo attr=\"", "</tt:Name><ter:NotAuthorized/><tt:Name>",
        "Control \x01\x02\x03\x04 stripped", "Temperature > 40°C & Humidity < 80%",
        "Special: &amp; &lt; &gt; &quot; &apos;" };

    for (const auto& payload : payloads) {
        const std::string escapedAttr = escapeXmlAttr(payload);
        const std::string escapedText = escapeXml(payload);

        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        xml << "<root attr=\"" << escapedAttr << "\">";
        xml << "<content>" << escapedText << "</content>";
        xml << "</root>";

        pugi::xml_document doc;
        const pugi::xml_parse_result result = doc.load_string(xml.str().c_str());
        ASSERT_TRUE(result) << "Failed to parse XML: " << result.description() << "\nRaw XML was:\n" << xml.str();

        // Ensure no evil injected element exists
        EXPECT_TRUE(doc.select_node("//evil_element").node().empty());
        EXPECT_TRUE(doc.select_node("//ter:NotAuthorized").node().empty());

        // Verify roundtrip value equals sanitized payload
        const std::string expectedValue = sanitizeXml(payload);
        const std::string actualAttr = doc.child("root").attribute("attr").value();
        const std::string actualText = doc.child("root").child("content").text().as_string();

        EXPECT_EQ(actualAttr, expectedValue);
        EXPECT_EQ(actualText, expectedValue);
    }
}
