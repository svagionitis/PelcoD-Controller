/// @file SoapFault.cpp
/// @brief Implementation of SOAP 1.2 fault builders.

#include "SoapFault.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {

    [[nodiscard]] std::string formatFault(
        std::string_view codeValue, std::string_view subcode, std::string_view secondSubcode, std::string_view reason)
    {
        std::string out {};
        out.append("    <SOAP-ENV:Fault>\r\n")
            .append("      <SOAP-ENV:Code>\r\n")
            .append("        <SOAP-ENV:Value>")
            .append(Xml::escapeXml(codeValue))
            .append("</SOAP-ENV:Value>\r\n")
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

} // namespace

std::string SoapFault::sender(std::string_view subcode, std::string_view secondSubcode, std::string_view reason)
{
    return formatFault("SOAP-ENV:Sender", subcode, secondSubcode, reason);
}

std::string SoapFault::receiver(std::string_view subcode, std::string_view secondSubcode, std::string_view reason)
{
    return formatFault("SOAP-ENV:Receiver", subcode, secondSubcode, reason);
}

std::string SoapFault::actionNotSupported(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "Action Not Supported" : reason };
    return receiver("ter:ActionNotSupported", "", r);
}

std::string SoapFault::notAuthorized()
{
    return sender("ter:NotAuthorized", "", "Sender not Authorized");
}

std::string SoapFault::invalidConsumerRef(std::string_view reason)
{
    return sender("ter:InvalidArgVal", "wsnt:InvalidConsumerReferenceFault", reason);
}

std::string SoapFault::subscribeCreationFailed(std::string_view reason)
{
    return sender("wsnt:SubscribeCreationFailedFault", "", reason);
}

std::string SoapFault::resourceUnknown(std::string_view reason)
{
    return sender("wsrf-rw:ResourceUnknownFault", "", reason);
}

std::string SoapFault::deviceUnprovisioned()
{
    return sender("ter:OperationProhibited", "ter:DeviceUnprovisioned",
        "Device is unprovisioned. Initial administrator credentials must be set before use.");
}

std::string SoapFault::passwordTooWeak(std::string_view reason)
{
    return sender("ter:InvalidArgVal", "ter:PasswordTooWeak", reason);
}

} // namespace Onvif
