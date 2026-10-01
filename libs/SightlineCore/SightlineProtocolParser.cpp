/// @file SightlineProtocolParser.cpp
/// @brief Facade implementation delegating to modular domain parsers.

#include "SightlineProtocolParser.h"
#include "SightlineFraming.h"
#include "modules/SightlineCaptureParser.h"
#include "modules/SightlineCompressionParser.h"
#include "modules/SightlineFocusParser.h"
#include "modules/SightlineGeneralParser.h"
#include "modules/SightlineKlvParser.h"
#include "modules/SightlineNetworkParser.h"
#include "modules/SightlineRecordingParser.h"
#include "modules/SightlineStabilizationParser.h"
#include "modules/SightlineTelemetryParser.h"
#include "modules/SightlineTrackingParser.h"

namespace Sightline {

std::size_t SightlineProtocolParser::getHeaderLength(ByteView packet) noexcept
{
    return SightlineFraming::getHeaderLength(packet);
}

MessageId SightlineProtocolParser::identifyMessage(ByteView packet) noexcept
{
    return SightlineFraming::identifyMessage(packet);
}

ByteView SightlineProtocolParser::extractPayload(ByteView packet) noexcept
{
    return SightlineFraming::extractPayload(packet);
}

// ==============================================================================
// 1. System, Diagnostic & Configuration Deserializers (General module)
// ==============================================================================

bool SightlineProtocolParser::parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out)
{
    return SightlineGeneralParser::parseVersionNumber(packet, out);
}

bool SightlineProtocolParser::parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out)
{
    return SightlineGeneralParser::parseUserWarning(packet, out);
}

bool SightlineProtocolParser::parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out)
{
    return SightlineGeneralParser::parseSystemStatus(packet, out);
}

bool SightlineProtocolParser::parseCurrentConfiguration(
    const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out)
{
    return SightlineGeneralParser::parseCurrentConfiguration(packet, out);
}

// ==============================================================================
// 2. Video Tracking & Motion Telemetry Deserializers (Tracking module)
// ==============================================================================

bool SightlineProtocolParser::parseTrackingPosition(const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out)
{
    return SightlineTrackingParser::parseTrackingPosition(packet, out);
}

bool SightlineProtocolParser::parseTrackingPositions(const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out)
{
    return SightlineTrackingParser::parseTrackingPositions(packet, out);
}

bool SightlineProtocolParser::parsePositionsExtended(
    const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out)
{
    return SightlineTrackingParser::parsePositionsExtended(packet, out);
}

bool SightlineProtocolParser::parseTrackTrails(const std::vector<std::uint8_t>& packet, MsgTrackTrails& out)
{
    return SightlineTrackingParser::parseTrackTrails(packet, out);
}

// ==============================================================================
// 3. Configuration State Deserializers (Stabilization, Capture, Compression, Focus, KLV)
// ==============================================================================

bool SightlineProtocolParser::parseStabilizationParams(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out)
{
    return SightlineStabilizationParser::parseStabilizationParams(packet, out);
}

bool SightlineProtocolParser::parseVideoParameters(const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out)
{
    return SightlineCaptureParser::parseVideoParameters(packet, out);
}

bool SightlineProtocolParser::parseH264Parameters(const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out)
{
    return SightlineCompressionParser::parseH264Parameters(packet, out);
}

bool SightlineProtocolParser::parseFocusStats(const std::vector<std::uint8_t>& packet, MsgFocusParameters& out)
{
    return SightlineFocusParser::parseFocusStats(packet, out);
}

bool SightlineProtocolParser::parseMetadataValues(const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    return SightlineKlvParser::parseMetadataValues(packet, out);
}

bool SightlineProtocolParser::parseTrackingParameters(
    const std::vector<std::uint8_t>& packet, MsgSetTrackingParameters& out)
{
    return SightlineTrackingParser::parseTrackingParameters(packet, out);
}

bool SightlineProtocolParser::parseLensParameters(const std::vector<std::uint8_t>& packet, MsgSetLensParameters& out)
{
    return SightlineFocusParser::parseLensParameters(packet, out);
}

bool SightlineProtocolParser::parseSystemStatusMode(const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out)
{
    return SightlineGeneralParser::parseSystemStatusMode(packet, out);
}

bool SightlineProtocolParser::parseMetadataStaticValues(
    const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out)
{
    return SightlineKlvParser::parseMetadataStaticValues(packet, out);
}

bool SightlineProtocolParser::parseMetadataRate(const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out)
{
    return SightlineKlvParser::parseMetadataRate(packet, out);
}

bool SightlineProtocolParser::parseNetworkList(const std::vector<std::uint8_t>& packet, MsgCurrentNetworkList& out)
{
    return SightlineNetworkParser::parseNetworkList(packet, out);
}

bool SightlineProtocolParser::parseSnapShot(const std::vector<std::uint8_t>& packet, MsgCurrentSnapShot& out)
{
    return SightlineRecordingParser::parseSnapShot(packet, out);
}

} // namespace Sightline
