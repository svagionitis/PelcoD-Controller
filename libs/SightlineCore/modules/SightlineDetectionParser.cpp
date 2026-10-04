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
    out.mode = static_cast<DetectionMode>(payload[1U]);
    out.threshold = payload[2U];
    out.minTargetSize = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxTargetSize = SightlineFraming::readU16Le(payload.data() + 5U);

    if (payload.size() >= 8U) {
        out.detectionIndex = payload[7U];
    }
    if (payload.size() >= 9U) {
        out.sensitivityMode = static_cast<SensitivityMode>(payload[8U]);
    }
    if (payload.size() >= 10U) {
        out.bkgdThreshold = payload[9U];
    }
    if (payload.size() >= 11U) {
        out.watchFrames = payload[10U];
    }
    if (payload.size() >= 12U) {
        out.suspiciousScore = payload[11U];
    }
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

    if (payload.size() >= 12U) {
        out.detectionIndex = payload[11U];
    }
    if (payload.size() >= 13U) {
        out.geometryMode = static_cast<RoiGeometryMode>(payload[12U]);
    }
    if (payload.size() >= 22U) {
        out.lineLeftX = SightlineFraming::readU16Le(payload.data() + 13U);
        out.lineLeftY = SightlineFraming::readU16Le(payload.data() + 15U);
        out.lineRightX = SightlineFraming::readU16Le(payload.data() + 17U);
        out.lineRightY = SightlineFraming::readU16Le(payload.data() + 19U);
        out.lineSide = static_cast<LineReportSide>(payload[21U]);
    }
    if (payload.size() >= 57U) {
        out.blocksWide = payload[22U];
        out.blocksHigh = payload[23U];
        for (std::size_t i { 0U }; i < 4U; ++i) {
            out.gridMasks[i] = SightlineFraming::readU64Le(payload.data() + 24U + (i * 8U));
        }
        out.showRegions = (payload[56U] != 0U);
    }
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

    if (payload.size() >= 24U) {
        out.detectionIndex = payload[8U];
        out.hideOverlapTracks = (payload[9U] != 0U);
        out.detectNearTrack = (payload[10U] != 0U);
        out.averageTimeConstant = payload[11U];
        out.edgePenalty = payload[12U];
        out.nFramesBack = payload[13U];
        out.useRegistration = (payload[14U] != 0U);
        out.updateRate = payload[15U];
        out.surroundSize = payload[16U];
        out.blobDirection = static_cast<BlobDirection>(payload[17U]);
        out.use8BitImages = (payload[18U] != 0U);
        out.gasAddOriginal = payload[19U];
        out.gasColor = payload[20U];
        out.aiIouThreshold = payload[21U];
        out.enableMtd = (payload[22U] != 0U);
        out.downsample = static_cast<DetectionDownsample>(payload[23U]);
    }

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

    if (payload.size() >= 10U) {
        out.minIntensity = SightlineFraming::readU16Le(payload.data() + 6U);
        out.maxIntensity = SightlineFraming::readU16Le(payload.data() + 8U);
    } else {
        out.minIntensity = static_cast<std::uint16_t>(payload[6U]);
        out.maxIntensity = static_cast<std::uint16_t>(payload[7U]);
    }

    return true;
}

} // namespace Sightline
