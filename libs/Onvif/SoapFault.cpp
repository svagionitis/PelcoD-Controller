/// @file SoapFault.cpp
/// @brief Implementation of SOAP 1.2 fault builders.

#include "SoapFault.h"

#include <sstream>
#include <string>
#include <string_view>

#include <pugixml.hpp>

namespace Onvif {

namespace {

    [[nodiscard]] std::string formatFault(
        std::string_view codeValue, std::string_view subcode, std::string_view secondSubcode, std::string_view reason)
    {
        pugi::xml_document doc {};
        auto faultNode = doc.append_child("SOAP-ENV:Fault");

        auto codeNode = faultNode.append_child("SOAP-ENV:Code");
        codeNode.append_child("SOAP-ENV:Value").text().set(std::string(codeValue).c_str());

        auto subNode = codeNode.append_child("SOAP-ENV:Subcode");
        subNode.append_child("SOAP-ENV:Value").text().set(std::string(subcode).c_str());

        if (!secondSubcode.empty()) {
            auto secSubNode = subNode.append_child("SOAP-ENV:Subcode");
            secSubNode.append_child("SOAP-ENV:Value").text().set(std::string(secondSubcode).c_str());
        }

        auto reasonNode = faultNode.append_child("SOAP-ENV:Reason");
        auto textNode = reasonNode.append_child("SOAP-ENV:Text");
        textNode.append_attribute("xml:lang").set_value("en");
        textNode.text().set(std::string(reason).c_str());

        std::ostringstream ss {};
        doc.print(ss, "  ", pugi::format_indent | pugi::format_no_declaration);
        return ss.str();
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

std::string SoapFault::noProfile(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "The requested profile token does not exist." : reason };
    return sender("ter:InvalidArgVal", "ter:NoProfile", r);
}

std::string SoapFault::invalidArgVal(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "Invalid argument value" : reason };
    return sender("ter:InvalidArgVal", "", r);
}

std::string SoapFault::noToken(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "The requested token does not exist." : reason };
    return sender("ter:InvalidArgVal", "ter:NoToken", r);
}

std::string SoapFault::outOfRange(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "Argument value is out of range" : reason };
    return sender("ter:InvalidArgVal", "ter:OutofRange", r);
}

std::string SoapFault::usernameClash(std::string_view reason)
{
    const std::string_view r { reason.empty() ? "The requested username already exists" : reason };
    return sender("ter:InvalidArgVal", "ter:UsernameClash", r);
}

} // namespace Onvif
