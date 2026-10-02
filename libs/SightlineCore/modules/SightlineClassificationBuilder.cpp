/// @file SightlineClassificationBuilder.cpp
/// @brief Implementation of Sightline AI classification command serializers.

#include "SightlineClassificationBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineClassificationBuilder::buildCustomAIDetect(const MsgCustomAIDetect& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.modelId, msg.confidenceThreshold, msg.nmsThreshold };
    return SightlineFraming::buildPacket(MessageId::CustomAIDetect, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetCustomAIDetect()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CustomAIDetect) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildVMTIChips(const MsgVMTIChips& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(8U + msg.chipData.size());
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.trackId);
    payload.push_back(msg.chipIndex);
    payload.push_back(msg.totalChips);
    SightlineFraming::appendU16Le(payload, msg.chipWidth);
    SightlineFraming::appendU16Le(payload, msg.chipHeight);
    payload.insert(payload.end(), msg.chipData.begin(), msg.chipData.end());
    return SightlineFraming::buildPacket(MessageId::VMTIChips, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetVMTIFields(const MsgVMTIFields& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU32Le(payload, msg.enabledFieldsMask);
    payload.push_back(msg.reportRate);
    return SightlineFraming::buildPacket(MessageId::VMTIFields, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetVMTIFields(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::VMTIFields), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetKlvClassFilters(const MsgKlvClassFilters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.classMask);
    payload.push_back(msg.minConfidence);
    return SightlineFraming::buildPacket(MessageId::KlvClassFilters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetKlvClassFilters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::KlvClassFilters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildTrackingMultiClass(const MsgTrackingMultiClass& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId, msg.primaryClass, msg.confidence,
        msg.flags };
    return SightlineFraming::buildPacket(MessageId::TrackingMultiClass, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetCustomClassifier(const MsgCustomClassifier& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.classifierType, msg.enable, msg.confidence };
    return SightlineFraming::buildPacket(MessageId::CustomClassifier, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetCustomClassifier(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CustomClassifier), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetClassifierParams(const MsgClassifierParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(5U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.modelIndex);
    payload.push_back(msg.nmsThreshold);
    SightlineFraming::appendU16Le(payload, msg.maxDetections);
    return SightlineFraming::buildPacket(MessageId::ClassifierParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetClassifierParams(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::ClassifierParameters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetKlvMetricFilters(const MsgKlvMetricFilters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(50U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendFloat32Le(payload, msg.minTargetWidthM);
    SightlineFraming::appendFloat32Le(payload, msg.maxTargetWidthM);
    SightlineFraming::appendFloat32Le(payload, msg.minTargetHeightM);
    SightlineFraming::appendFloat32Le(payload, msg.maxTargetHeightM);
    const std::uint8_t horizonMask = static_cast<std::uint8_t>(
        (msg.filterAboveHorizon ? 0x01U : 0x00U) | (msg.filterBelowHorizon ? 0x02U : 0x00U));
    payload.push_back(horizonMask);
    SightlineFraming::appendDouble64Le(payload, msg.minLatitude);
    SightlineFraming::appendDouble64Le(payload, msg.maxLatitude);
    SightlineFraming::appendDouble64Le(payload, msg.minLongitude);
    SightlineFraming::appendDouble64Le(payload, msg.maxLongitude);
    return SightlineFraming::buildPacket(MessageId::KlvClassFilters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetKlvMetricFilters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::KlvClassFilters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildSetClassifierConfig(const MsgClassifierConfig& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(12U + msg.customModelName.size());
    payload.push_back(msg.cameraIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.model));
    payload.push_back(msg.maxPerFrame);
    SightlineFraming::appendU16Le(payload, msg.minDimensions);
    payload.push_back(static_cast<std::uint8_t>(msg.droneReporting));
    payload.push_back(msg.detectionPadding);
    payload.push_back(msg.updateRate);
    payload.push_back(static_cast<std::uint8_t>(msg.useNpu ? 1U : 0U));
    payload.push_back(static_cast<std::uint8_t>(msg.asyncExecution ? 1U : 0U));
    SightlineFraming::appendString(payload, msg.customModelName);
    return SightlineFraming::buildPacket(MessageId::ClassifierParameters, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetClassifierConfig(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::ClassifierParameters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
