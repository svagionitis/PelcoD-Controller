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

} // namespace Sightline
