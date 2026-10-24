/// @file fuzz_ws_discovery.cpp
/// @brief libFuzzer harness for WS-Discovery UDP multicast datagram parsing and validation.

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include <pugixml.hpp>

#include "OnvifDiscovery.h"
#include "WsDiscoveryValidator.h"

namespace {

constexpr std::size_t kMaxDatagramBytes { 8192U };
const std::string kLocalServiceUuid { "urn:uuid:12345678-1234-1234-1234-123456789abc" };
const std::vector<std::string> kActiveScopes { "onvif://www.onvif.org/type/video_encoder",
    "onvif://www.onvif.org/type/ptz", "onvif://www.onvif.org/hardware/PelcoD" };

} // namespace

extern "C" int FuzzWsDiscoveryTest(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U || size > kMaxDatagramBytes) {
        return 0;
    }

    std::uint16_t senderPort = 8080U;
    const std::uint8_t* xmlData = data;
    std::size_t xmlSize = size;

    if (size >= 2U && data[0] != '<') {
        senderPort = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[0]) << 8U) | static_cast<std::uint16_t>(data[1]));
        xmlData += 2U;
        xmlSize -= 2U;
    }

    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_buffer(xmlData, xmlSize, pugi::parse_default, pugi::encoding_utf8);

    if (result) {
        // Exercise WsDiscoveryValidator (noexcept)
        static_cast<void>(
            Onvif::WsDiscoveryValidator::validateProbe(doc, senderPort, kLocalServiceUuid, true, kActiveScopes));

        static_cast<void>(Onvif::WsDiscoveryValidator::matchesScopes(doc, kActiveScopes));
        static_cast<void>(Onvif::WsDiscoveryValidator::matchesTypes(doc));
        static_cast<void>(Onvif::WsDiscoveryValidator::isSelfMessage(doc, kLocalServiceUuid));
    }

    // Exercise OnvifDiscovery::parseProbeMatches
    const std::string rawPayload(reinterpret_cast<const char*>(xmlData), xmlSize);
    static_cast<void>(Onvif::OnvifDiscovery::parseProbeMatches(rawPayload, "192.168.1.100"));

    return 0;
}

#if !defined(FUZZ_STANDALONE_REGRESSION)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return FuzzWsDiscoveryTest(data, size);
}
#endif
