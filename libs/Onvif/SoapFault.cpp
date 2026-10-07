/// @file SoapFault.cpp
/// @brief Implementation of SOAP 1.2 fault builders.

#include "SoapFault.h"
#include "XmlUtils.h"

namespace Onvif {

std::string SoapFault::sender(std::string_view subcode, std::string_view secondSubcode, std::string_view reason)
{
    std::string out {};
    out.append("    <SOAP-ENV:Fault>\r\n")
        .append("      <SOAP-ENV:Code>\r\n")
        .append("        <SOAP-ENV:Value>SOAP-ENV:Sender</SOAP-ENV:Value>\r\n")
        .append("        <SOAP-ENV:Subcode>\r\n")
        .append("          <SOAP-ENV:Value>")
        .append(Xml::escapeXml(subcode))
        .append("</SOAP-ENV:Value>\r\n");
    if (!secondSubcode.empty()) {
        out.append("          <SOAP-ENV:Subcode><SOAP-ENV:Value>")
            .append(Xml::escapeXml(secondSubcode))
            .append("</SOAP-ENV:Value></SOAP-ENV:Subcode>\r\n");
    }
    out.append("        </SOAP-ENV:Subcode>\r\n")
        .append("      </SOAP-ENV:Code>\r\n")
        .append("      <SOAP-ENV:Reason><SOAP-ENV:Text xml:lang=\"en\">")
        .append(Xml::escapeXml(reason))
        .append("</SOAP-ENV:Text></SOAP-ENV:Reason>\r\n")
        .append("    </SOAP-ENV:Fault>\r\n");
    return out;
}

std::string SoapFault::notAuthorized()
{
    return sender("ter:NotAuthorized", "", "Sender not Authorized");
}

} // namespace Onvif
