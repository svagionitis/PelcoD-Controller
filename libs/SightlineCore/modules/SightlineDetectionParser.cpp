/// @file SightlineDetectionParser.cpp
/// @brief Implementation of Sightline detection frame deserializers.

#include "SightlineDetectionParser.h"

namespace Sightline {

bool SightlineDetectionParser::parseDetectionParams(ByteView packet, MsgSetDetectionParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentDetectionParameters && id != MessageId::SetDetectionParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.threshold = payload[2U];
    out.minTargetSize = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxTargetSize = SightlineFraming::readU16Le(payload.data() + 5U);
    return true;
}

bool SightlineDetectionParser::parseVMTI(ByteView packet, MsgSetVMTI& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SetVMTI) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.enable = payload[1U];
    out.sensitivity = payload[2U];
    out.minTargetArea = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxTargetArea = SightlineFraming::readU16Le(payload.data() + 5U);
    out.mode = payload[7U];
    return true;
}

bool SightlineDetectionParser::parseDetectionROI(ByteView packet, MsgDetectionROI& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::SetDetectionRegionOfInterestParameters
        && id != MessageId::CurrentDetectionRegionOfInterestParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.roiIndex = payload[1U];
    out.roiType = payload[2U];
    out.left = SightlineFraming::readU16Le(payload.data() + 3U);
    out.top = SightlineFraming::readU16Le(payload.data() + 5U);
    out.width = SightlineFraming::readU16Le(payload.data() + 7U);
    out.height = SightlineFraming::readU16Le(payload.data() + 9U);
    return true;
}

bool SightlineDetectionParser::parseAdvDetectionParams(ByteView packet, MsgAdvancedDetectionParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::SetAdvancedDetectionParameters && id != MessageId::CurrentAdvancedDetectionParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.minVelocity = SightlineFraming::readU16Le(payload.data() + 1U);
    out.maxVelocity = SightlineFraming::readU16Le(payload.data() + 3U);
    out.persistenceFrames = payload[5U];
    out.mergeDistance = SightlineFraming::readU16Le(payload.data() + 6U);
    return true;
}

bool SightlineDetectionParser::parseTrackingPixelStats(ByteView packet, MsgTrackingBoxPixelStats& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackingBoxPixelStats) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.trackId = payload[1U];
    out.meanIntensity = SightlineFraming::readU16Le(payload.data() + 2U);
    out.stdDevIntensity = SightlineFraming::readU16Le(payload.data() + 4U);
    out.minIntensity = payload[6U];
    out.maxIntensity = payload[7U];
    return true;
}

} // namespace Sightline
