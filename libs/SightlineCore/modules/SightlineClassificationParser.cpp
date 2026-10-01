/// @file SightlineClassificationParser.cpp
/// @brief Implementation of Sightline AI classification frame deserializers.

#include "SightlineClassificationParser.h"

namespace Sightline {

bool SightlineClassificationParser::parseCustomAIDetect(ByteView packet, MsgCustomAIDetect& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CustomAIDetect) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.modelId = payload[1U];
    out.confidenceThreshold = payload[2U];
    out.nmsThreshold = payload[3U];
    return true;
}

bool SightlineClassificationParser::parseVMTIChips(ByteView packet, MsgVMTIChips& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::VMTIChips) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.trackId = payload[1U];
    out.chipIndex = payload[2U];
    out.totalChips = payload[3U];
    out.chipWidth = SightlineFraming::readU16Le(payload.data() + 4U);
    out.chipHeight = SightlineFraming::readU16Le(payload.data() + 6U);
    out.chipData.assign(payload.data() + 8U, payload.data() + payload.size());
    return true;
}

bool SightlineClassificationParser::parseVMTIFields(ByteView packet, MsgVMTIFields& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::VMTIFields) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.enabledFieldsMask = SightlineFraming::readU32Le(payload.data() + 1U);
    out.reportRate = payload[5U];
    return true;
}

bool SightlineClassificationParser::parseKlvClassFilters(ByteView packet, MsgKlvClassFilters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::KlvClassFilters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.classMask = SightlineFraming::readU16Le(payload.data() + 1U);
    out.minConfidence = payload[3U];
    return true;
}

bool SightlineClassificationParser::parseTrackingMultiClass(ByteView packet, MsgTrackingMultiClass& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackingMultiClass) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.trackId = payload[1U];
    out.primaryClass = payload[2U];
    out.confidence = payload[3U];
    out.flags = payload[4U];
    return true;
}

bool SightlineClassificationParser::parseCustomClassifier(ByteView packet, MsgCustomClassifier& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CustomClassifier) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.classifierType = payload[1U];
    out.enable = payload[2U];
    out.confidence = payload[3U];
    return true;
}

bool SightlineClassificationParser::parseClassifierParams(ByteView packet, MsgClassifierParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::ClassifierParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.modelIndex = payload[1U];
    out.nmsThreshold = payload[2U];
    out.maxDetections = SightlineFraming::readU16Le(payload.data() + 3U);
    return true;
}

} // namespace Sightline
