/// @file SightlineStabilizationBuilder.cpp
/// @brief Implementation of Sightline video stabilization command serializers.

#include "SightlineStabilizationBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetStabilization(
    const MsgSetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.mode,
        msg.rate,
        msg.translationLimit,
        msg.angleLimit,
        msg.cameraIndex,
        msg.maxStabOff,
        msg.edgeY,
        msg.edgeU,
        msg.edgeV
    };
    return SightlineFraming::buildPacket(MessageId::SetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildResetStabilization(
    const MsgResetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.resetType, msg.cameraIndex };
    return SightlineFraming::buildPacket(MessageId::ResetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetStabilizationBias(
    const MsgSetStabilizationBias& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendS16Le(payload, msg.biasCol);
    SightlineFraming::appendS16Le(payload, msg.biasRow);
    SightlineFraming::appendS16Le(payload, msg.biasRotation);
    return SightlineFraming::buildPacket(MessageId::StabilizationBias, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetRegistration(
    const MsgSetRegistrationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.searchRange, msg.pyramidLevels, msg.flags };
    return SightlineFraming::buildPacket(MessageId::SetRegistrationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetBlendParameters(
    const MsgSetBlendParameters& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.absOffZoom,
        static_cast<std::uint8_t>(msg.vertical),
        static_cast<std::uint8_t>(msg.horizontal),
        msg.rotation,
        msg.zoom,
        msg.mode,
        msg.amt,
        msg.hue,
        msg.flags,
        msg.reset,
        msg.reserved,
        msg.warpIndex,
        msg.fixedIndex,
        msg.usePresetAlign,
        msg.presetAlignIndex,
        msg.hzoom,
        msg.hotStart,
        msg.coldEnd
    };
    return SightlineFraming::buildPacket(MessageId::SetBlendParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetNoise3D(
    const MsgNoise3D& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.enable, msg.temporalStrength, msg.spatialStrength };
    return SightlineFraming::buildPacket(MessageId::Noise3D, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetStabilization(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetRegistration(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetRegistrationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetStabilizationBias(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetStabilizationBias), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
