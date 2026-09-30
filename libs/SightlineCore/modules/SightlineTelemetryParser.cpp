/// @file SightlineTelemetryParser.cpp
/// @brief Implementation of Sightline SLA platform telemetry and KLV metadata deserializers.

#include "SightlineTelemetryParser.h"
#include "SightlineKlvParser.h"

namespace Sightline {

bool SightlineTelemetryParser::parseMetadataValues(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    return SightlineKlvParser::parseMetadataValues(packet, out);
}

bool SightlineTelemetryParser::parseCoordReportingMode(
    const std::vector<std::uint8_t>& packet, MsgCoordinateReportingMode& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CoordinateReportingMode) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 4U) {
        return false;
    }

    out.framePeriod = payload[0U];
    out.flags = SightlineFraming::readU16Le(payload.data() + 1U);
    out.cameraIndex = payload[3U];
    out.reportingFlags = static_cast<std::uint8_t>(out.flags & 0xFFU);
    return true;
}

bool SightlineTelemetryParser::parseTelemetryDestination(
    const std::vector<std::uint8_t>& packet, MsgSetTelemetryDestination& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SetTelemetryDestination) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 8U) {
        return false;
    }

    out.clientIndex = payload[0U];
    out.clientIpAddress = SightlineFraming::readU32Le(payload.data() + 1U);
    out.clientPort = SightlineFraming::readU16Le(payload.data() + 5U);
    out.flags = payload[7U];
    return true;
}

} // namespace Sightline
