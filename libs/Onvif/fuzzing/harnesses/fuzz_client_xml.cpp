/// @file fuzz_client_xml.cpp
/// @brief libFuzzer harness for OnvifClient XML response deserializers across all services.

#include <cstddef>
#include <cstdint>

#include <string>

#include "OnvifClient.h"

namespace {

constexpr std::size_t kMaxResponseBytes { 65536U };

void dispatchParse(std::uint8_t selector, const std::string& xml)
{
    switch (selector % 12U) {
    case 0U:
        static_cast<void>(Onvif::OnvifClient::parseCapabilitiesResponse(xml));
        break;
    case 1U:
        static_cast<void>(Onvif::OnvifClient::parseDeviceInformationResp(xml));
        break;
    case 2U:
        static_cast<void>(Onvif::OnvifClient::parseProfilesResponse(xml));
        break;
    case 3U:
        static_cast<void>(Onvif::OnvifClient::parseProfileResponse(xml));
        break;
    case 4U:
        static_cast<void>(Onvif::OnvifClient::parseVideoEncoderConfigResp(xml));
        break;
    case 5U:
        static_cast<void>(Onvif::OnvifClient::parseVideoEncoderOptionsResp(xml));
        break;
    case 6U:
        static_cast<void>(Onvif::OnvifClient::parseStreamUriResponse(xml));
        break;
    case 7U:
        static_cast<void>(Onvif::OnvifClient::parseSnapshotUriResponse(xml));
        break;
    case 8U:
        static_cast<void>(Onvif::OnvifClient::parsePtzStatusResponse(xml));
        break;
    case 9U:
        static_cast<void>(Onvif::OnvifClient::parsePresetsResponse(xml));
        break;
    case 10U:
        static_cast<void>(Onvif::OnvifClient::parsePresetToursResponse(xml));
        break;
    case 11U:
        static_cast<void>(Onvif::OnvifClient::parseImagingSettingsResponse(xml));
        break;
    default:
        break;
    }
}

} // namespace

extern "C" int FuzzClientXmlTest(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U || size > kMaxResponseBytes) {
        return 0;
    }

    if (data[0] != '<' && size > 1U) {
        const std::uint8_t selector = data[0];
        const std::string xml(reinterpret_cast<const char*>(data + 1U), size - 1U);
        dispatchParse(selector, xml);
    } else {
        const std::string xml(reinterpret_cast<const char*>(data), size);
        // Test primary parsers for XML inputs
        dispatchParse(0U, xml);
        dispatchParse(1U, xml);
        dispatchParse(2U, xml);
        dispatchParse(8U, xml);
    }

    return 0;
}

#if !defined(FUZZ_STANDALONE_REGRESSION)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    return FuzzClientXmlTest(data, size);
}
#endif
