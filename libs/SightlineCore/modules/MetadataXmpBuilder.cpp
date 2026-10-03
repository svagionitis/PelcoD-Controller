/// @file MetadataXmpBuilder.cpp
/// @brief Implementation of Adobe XMP and ISO/IEC 12234-2 metadata builder.

#include "MetadataXmpBuilder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace Sightline {

namespace {

constexpr std::string_view kXmpNamespace { "http://ns.adobe.com/xap/1.0/\0", 29 };
constexpr std::uint8_t kJpegMarkerPrefix { 0xFFU };
constexpr std::uint8_t kJpegSoi { 0xD8U };
constexpr std::uint8_t kJpegApp1 { 0xE1U };
constexpr std::uint8_t kJpegSos { 0xDAU };
constexpr std::uint8_t kJpegEoi { 0xD9U };

[[nodiscard]] std::string extractTag(std::string_view xml, std::string_view tag)
{
    const std::string openTag = "<" + std::string(tag) + ">";
    const std::string closeTag = "</" + std::string(tag) + ">";

    const auto startPos = xml.find(openTag);
    if (startPos == std::string_view::npos) {
        return {};
    }
    const auto contentStart = startPos + openTag.size();
    const auto endPos = xml.find(closeTag, contentStart);
    if (endPos == std::string_view::npos) {
        return {};
    }

    return std::string(xml.substr(contentStart, endPos - contentStart));
}

[[nodiscard]] double parseDouble(std::string_view xml, std::string_view tag, double defaultVal = 0.0)
{
    const auto valStr = extractTag(xml, tag);
    if (valStr.empty()) {
        return defaultVal;
    }
    char* endPtr { nullptr };
    const double res = std::strtod(valStr.c_str(), &endPtr);
    return (endPtr != valStr.c_str()) ? res : defaultVal;
}

[[nodiscard]] float parseFloat(std::string_view xml, std::string_view tag, float defaultVal = 0.0F)
{
    const auto valStr = extractTag(xml, tag);
    if (valStr.empty()) {
        return defaultVal;
    }
    char* endPtr { nullptr };
    const float res = std::strtof(valStr.c_str(), &endPtr);
    return (endPtr != valStr.c_str()) ? res : defaultVal;
}

} // namespace

std::string MetadataXmpBuilder::buildRdfXml(const XmpGeospatialMetadata& meta)
{
    std::ostringstream oss {};
    oss << std::fixed << std::setprecision(6);

    oss << "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
        << "  <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        << "    <rdf:Description rdf:about=\"\"\n"
        << "      xmlns:drone-dji=\"http://www.dji.com/drone-dji/1.0/\"\n"
        << "      xmlns:Camera=\"http://pix4d.com/camera/1.0/\"\n"
        << "      xmlns:tiff=\"http://ns.adobe.com/tiff/1.0/\"\n"
        << "      xmlns:exif=\"http://ns.adobe.com/exif/1.0/\">\n";

    oss << "      <drone-dji:GpsLatitude>" << meta.latitudeDeg << "</drone-dji:GpsLatitude>\n"
        << "      <drone-dji:GpsLongitude>" << meta.longitudeDeg << "</drone-dji:GpsLongitude>\n"
        << "      <drone-dji:AbsoluteAltitude>" << meta.absoluteAltitudeM << "</drone-dji:AbsoluteAltitude>\n"
        << "      <drone-dji:RelativeAltitude>" << meta.relativeAltitudeM << "</drone-dji:RelativeAltitude>\n"
        << "      <drone-dji:FlightRollDegree>" << meta.flightRollDeg << "</drone-dji:FlightRollDegree>\n"
        << "      <drone-dji:FlightPitchDegree>" << meta.flightPitchDeg << "</drone-dji:FlightPitchDegree>\n"
        << "      <drone-dji:FlightYawDegree>" << meta.flightYawDeg << "</drone-dji:FlightYawDegree>\n"
        << "      <drone-dji:GimbalRollDegree>" << meta.gimbalRollDeg << "</drone-dji:GimbalRollDegree>\n"
        << "      <drone-dji:GimbalPitchDegree>" << meta.gimbalPitchDeg << "</drone-dji:GimbalPitchDegree>\n"
        << "      <drone-dji:GimbalYawDegree>" << meta.gimbalYawDeg << "</drone-dji:GimbalYawDegree>\n"
        << "      <drone-dji:SlantRange>" << meta.slantRangeM << "</drone-dji:SlantRange>\n"
        << "      <Camera:HorizFOV>" << meta.horizontalFovDeg << "</Camera:HorizFOV>\n"
        << "      <Camera:VertFOV>" << meta.verticalFovDeg << "</Camera:VertFOV>\n";

    if (!meta.modelName.empty()) {
        oss << "      <tiff:Model>" << meta.modelName << "</tiff:Model>\n";
    }

    oss << "    </rdf:Description>\n"
        << "  </rdf:RDF>\n"
        << "</x:xmpmeta>\n"
        << "<?xpacket end=\"w\"?>";

    return oss.str();
}

std::vector<std::uint8_t> MetadataXmpBuilder::buildApp1Packet(const XmpGeospatialMetadata& meta)
{
    const std::string xml = buildRdfXml(meta);
    const std::size_t payloadSize = kXmpNamespace.size() + xml.size();
    const std::size_t markerTotalSize = 2U + 2U + payloadSize; // 0xFFE1 + Length(2) + Payload

    if (markerTotalSize > 65535U) {
        return {};
    }

    std::vector<std::uint8_t> packet {};
    packet.reserve(markerTotalSize);

    // Marker prefix
    packet.push_back(kJpegMarkerPrefix);
    packet.push_back(kJpegApp1);

    // Length (includes 2 bytes of length field itself + payload)
    const auto length = static_cast<std::uint16_t>(payloadSize + 2U);
    packet.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>(length & 0xFFU));

    // XMP namespace identifier (29 bytes with null terminator)
    for (const char ch : kXmpNamespace) {
        packet.push_back(static_cast<std::uint8_t>(ch));
    }

    // XML UTF-8 payload
    for (const char ch : xml) {
        packet.push_back(static_cast<std::uint8_t>(ch));
    }

    return packet;
}

bool MetadataXmpBuilder::injectXmpIntoJpeg(
    std::vector<std::uint8_t>& jpegStream, const XmpGeospatialMetadata& meta)
{
    if (jpegStream.size() < 4U) {
        return false;
    }

    // Must start with SOI: 0xFF 0xD8
    if (jpegStream[0] != kJpegMarkerPrefix || jpegStream[1] != kJpegSoi) {
        return false;
    }

    const auto app1Pkt = buildApp1Packet(meta);
    if (app1Pkt.empty()) {
        return false;
    }

    // Default insertion point is immediately after SOI (offset 2)
    std::size_t insertOffset { 2U };

    // If an existing Exif APP1 is present at offset 2, insert after it
    if (jpegStream.size() >= 6U &&
        jpegStream[2] == kJpegMarkerPrefix &&
        jpegStream[3] == kJpegApp1)
    {
        const std::size_t exifLen =
            (static_cast<std::size_t>(jpegStream[4]) << 8U) |
            static_cast<std::size_t>(jpegStream[5]);
        const std::size_t nextMarker = 4U + exifLen;
        if (nextMarker <= jpegStream.size()) {
            insertOffset = nextMarker;
        }
    }

    jpegStream.insert(
        jpegStream.begin() + static_cast<std::ptrdiff_t>(insertOffset),
        app1Pkt.begin(),
        app1Pkt.end());

    return true;
}

bool MetadataXmpBuilder::extractXmpFromJpeg(
    const std::vector<std::uint8_t>& jpegStream, std::string& outRdfXml)
{
    outRdfXml.clear();
    if (jpegStream.size() < 4U) {
        return false;
    }

    if (jpegStream[0] != kJpegMarkerPrefix || jpegStream[1] != kJpegSoi) {
        return false;
    }

    std::size_t offset { 2U };
    while (offset + 4U <= jpegStream.size()) {
        if (jpegStream[offset] != kJpegMarkerPrefix) {
            break;
        }

        const std::uint8_t marker = jpegStream[offset + 1U];
        if (marker == kJpegSos || marker == kJpegEoi) {
            break; // Stop at Start of Scan or End of Image
        }

        const std::size_t length =
            (static_cast<std::size_t>(jpegStream[offset + 2U]) << 8U) |
            static_cast<std::size_t>(jpegStream[offset + 3U]);

        if (offset + 2U + length > jpegStream.size()) {
            break; // Corrupted length
        }

        if (marker == kJpegApp1 && length >= (2U + kXmpNamespace.size())) {
            const std::size_t nsStart = offset + 4U;
            if (std::memcmp(&jpegStream[nsStart], kXmpNamespace.data(), kXmpNamespace.size()) == 0) {
                const std::size_t xmlStart = nsStart + kXmpNamespace.size();
                const std::size_t xmlLen = length - 2U - kXmpNamespace.size();
                outRdfXml.assign(
                    reinterpret_cast<const char*>(&jpegStream[xmlStart]), xmlLen);
                return true;
            }
        }

        offset += 2U + length;
    }

    return false;
}

bool MetadataXmpBuilder::parseRdfXml(
    std::string_view rdfXml, XmpGeospatialMetadata& outMeta)
{
    if (rdfXml.empty()) {
        return false;
    }

    outMeta.latitudeDeg = parseDouble(rdfXml, "drone-dji:GpsLatitude");
    outMeta.longitudeDeg = parseDouble(rdfXml, "drone-dji:GpsLongitude");
    outMeta.absoluteAltitudeM = parseDouble(rdfXml, "drone-dji:AbsoluteAltitude");
    outMeta.relativeAltitudeM = parseDouble(rdfXml, "drone-dji:RelativeAltitude");
    outMeta.flightRollDeg = parseFloat(rdfXml, "drone-dji:FlightRollDegree");
    outMeta.flightPitchDeg = parseFloat(rdfXml, "drone-dji:FlightPitchDegree");
    outMeta.flightYawDeg = parseFloat(rdfXml, "drone-dji:FlightYawDegree");
    outMeta.gimbalRollDeg = parseFloat(rdfXml, "drone-dji:GimbalRollDegree");
    outMeta.gimbalPitchDeg = parseFloat(rdfXml, "drone-dji:GimbalPitchDegree");
    outMeta.gimbalYawDeg = parseFloat(rdfXml, "drone-dji:GimbalYawDegree");
    outMeta.slantRangeM = parseDouble(rdfXml, "drone-dji:SlantRange");
    outMeta.horizontalFovDeg = parseFloat(rdfXml, "Camera:HorizFOV");
    outMeta.verticalFovDeg = parseFloat(rdfXml, "Camera:VertFOV");
    outMeta.modelName = extractTag(rdfXml, "tiff:Model");

    return true;
}

} // namespace Sightline
