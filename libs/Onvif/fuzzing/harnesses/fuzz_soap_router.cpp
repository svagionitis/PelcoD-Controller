/// @file fuzz_soap_router.cpp
/// @brief libFuzzer harness for ONVIF SOAP operation routing and XML document parsing.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>
#include <string_view>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"
#include "XmlUtils.h"

namespace {

constexpr std::size_t kMaxPayloadBytes { 65536U };

[[nodiscard]] Onvif::SoapRouter createFuzzRouter()
{
    Onvif::SoapRouter router;
    router.registerRoute("http://www.onvif.org/ver10/device/wsdl", "GetDeviceInformation",
        Onvif::AccessClass::ReadSystem,
        [](const Onvif::SoapContext&) { return Onvif::SoapHandlerResult::ok("<GetDeviceInformationResponse/>"); });
    router.registerRoute("http://www.onvif.org/ver10/media/wsdl", "GetProfiles", Onvif::AccessClass::ReadMedia,
        [](const Onvif::SoapContext&) { return Onvif::SoapHandlerResult::ok("<GetProfilesResponse/>"); });
    router.registerRoute("http://www.onvif.org/ver20/ptz/wsdl", "ContinuousMove", Onvif::AccessClass::Actuate,
        [](const Onvif::SoapContext&) { return Onvif::SoapHandlerResult::ok("<ContinuousMoveResponse/>"); });
    return router;
}

} // namespace

extern "C" int FuzzSoapRouterTest(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U || size > kMaxPayloadBytes) {
        return 0;
    }

    const std::string_view payload(reinterpret_cast<const char*>(data), size);

    // 1. Test XmlUtils sanitization
    static_cast<void>(Onvif::Xml::sanitizeXml(payload));

    // 2. Test pugixml load
    pugi::xml_document doc;
    const pugi::xml_parse_result result
        = doc.load_buffer(data, size, pugi::parse_default | pugi::parse_fragment, pugi::encoding_utf8);

    if (!result) {
        return 0;
    }

    // 3. Locate Body element and dispatch
    const pugi::xml_node envelope = doc.document_element();
    pugi::xml_node body = Onvif::Xml::findNodeWithSuffix(envelope, "Body");
    if (!body) {
        body = envelope;
    }

    const pugi::xml_node reqNode = body.first_child();
    if (!reqNode) {
        return 0;
    }

    static const Onvif::SoapRouter router = createFuzzRouter();

    Onvif::SoapContext ctx { };
    ctx.reqNode = reqNode;
    ctx.peerAddress = "127.0.0.1";
    ctx.requestUrl = "/onvif/device_service";
    ctx.principal.username = "admin";
    ctx.principal.level = Onvif::OnvifUserLevel::Administrator;

    static_cast<void>(router.dispatch(reqNode, ctx, "http://www.onvif.org/ver10/device/wsdl"));

    return 0;
}

#if !defined(FUZZ_STANDALONE_REGRESSION)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return FuzzSoapRouterTest(data, size);
}
#endif
