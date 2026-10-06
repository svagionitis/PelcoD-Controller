/// @file SightlineStabilizationBuilder.cpp
/// @brief Implementation of Sightline video stabilization command serializers.

#include "SightlineStabilizationBuilder.h"
#include "SightlineBlendingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetStabilization(const MsgSetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.mode, msg.rate, msg.translationLimit, msg.angleLimit, msg.cameraIndex,
        msg.maxStabOff, msg.edgeY, msg.edgeU, msg.edgeV };
    return SightlineFraming::buildPacket(MessageId::SetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildResetStabilization(
    const MsgResetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.resetType, msg.cameraIndex };
    return SightlineFraming::buildPacket(MessageId::ResetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetStabilizationBias(const MsgSetStabilizationBias& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendS16Le(payload, msg.biasCol);
    SightlineFraming::appendS16Le(payload, msg.biasRow);
    payload.push_back(msg.autoBias);
    payload.push_back(msg.updateRate);
    return SightlineFraming::buildPacket(MessageId::StabilizationBias, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetRegistration(const MsgSetRegistrationParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(15U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.maxTranslation);
    payload.push_back(msg.maxRotation);
    payload.push_back(msg.zoomRange);
    SightlineFraming::appendU16Le(payload, msg.left);
    SightlineFraming::appendU16Le(payload, msg.right);
    SightlineFraming::appendU16Le(payload, msg.top);
    SightlineFraming::appendU16Le(payload, msg.bottom);
    payload.push_back(msg.updateRate);
    payload.push_back(msg.flags);
    return SightlineFraming::buildPacket(MessageId::RegistrationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildSetBlendParameters(const MsgSetBlendParameters& msg)
{
    return SightlineBlendingBuilder::buildSetBlendParameters(msg);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetStabilization(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetRegistration(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::RegistrationParameters),
        cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineStabilizationBuilder::buildGetStabilizationBias(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::StabilizationBias), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

MsgSetStabilizationBias SightlineStabilizationBuilder::calcGimbalBias(double panLeft, double tiltUp, std::uint16_t hRes,
    std::uint16_t vRes, double hFov, double vFov, double fps, std::uint8_t cameraIndex, std::uint8_t autoBias,
    std::uint8_t updateRate)
{
    MsgSetStabilizationBias out {};
    out.cameraIndex = cameraIndex;
    out.autoBias = autoBias;
    out.updateRate = updateRate;

    if (hFov > 0.001 && fps > 0.001) {
        const double col = (panLeft * static_cast<double>(hRes)) / (hFov * fps);
        out.biasCol = static_cast<std::int16_t>(std::round(col));
    }
    if (vFov > 0.001 && fps > 0.001) {
        const double row = (tiltUp * static_cast<double>(vRes)) / (vFov * fps);
        out.biasRow = static_cast<std::int16_t>(std::round(row));
    }
    return out;
}

MsgSetRegistrationParameters SightlineStabilizationBuilder::makeRegistrationPreset(
    StabilizationPreset preset, std::uint8_t cameraIndex)
{
    MsgSetRegistrationParameters params {};
    params.cameraIndex = cameraIndex;

    switch (preset) {
    case StabilizationPreset::AirborneGimbal:
        params.maxTranslation = 0U;
        params.maxRotation = 5U;
        params.zoomRange = 0U;
        params.left = 0U;
        params.right = 0U;
        params.top = 0U;
        params.bottom = 0U;
        params.updateRate = 100U; // Low drift unchecked
        params.flags = 0U;
        break;
    case StabilizationPreset::FixedMountPtz:
        params.maxTranslation = 50U;
        params.maxRotation = 0U;
        params.zoomRange = 0U;
        params.left = 0U;
        params.right = 0U;
        params.top = 0U;
        params.bottom = 0U;
        params.updateRate = 10U; // Low drift checked (scene lock)
        params.flags = 0U;
        break;
    case StabilizationPreset::MovingVehicle:
        params.maxTranslation = 0U;
        params.maxRotation = 0U;
        params.zoomRange = 0U;
        params.left = 100U;
        params.right = 100U;
        params.top = 50U;
        params.bottom = 0U;
        params.updateRate = 100U; // Low drift unchecked
        params.flags = 0U;
        break;
    }
    return params;
}

MsgSetStabilizationParameters SightlineStabilizationBuilder::makeStabilizationPreset(
    StabilizationPreset preset, std::uint8_t cameraIndex)
{
    MsgSetStabilizationParameters params {};
    params.cameraIndex = cameraIndex;
    params.mode = 1U; // Stabilization ON

    switch (preset) {
    case StabilizationPreset::AirborneGimbal:
        params.rate = 50U;
        params.translationLimit = 0U;
        params.angleLimit = 5U;
        params.maxStabOff = 0U;
        break;
    case StabilizationPreset::FixedMountPtz:
        params.rate = 20U;
        params.translationLimit = 0U;
        params.angleLimit = 0U;
        params.maxStabOff = 32U; // Prevents large borders during pan/tilt
        break;
    case StabilizationPreset::MovingVehicle:
        params.rate = 50U;
        params.translationLimit = 0U;
        params.angleLimit = 0U;
        params.maxStabOff = 50U; // Prevents large lags during rapid turns
        break;
    }
    return params;
}

MsgSetStabilizationBias SightlineStabilizationBuilder::makeBiasPreset(
    StabilizationPreset preset, std::uint8_t cameraIndex)
{
    MsgSetStabilizationBias bias {};
    bias.cameraIndex = cameraIndex;
    bias.biasCol = 0;
    bias.biasRow = 0;
    bias.updateRate = 50U;

    switch (preset) {
    case StabilizationPreset::AirborneGimbal:
        bias.autoBias = 1U;
        break;
    case StabilizationPreset::FixedMountPtz:
        bias.autoBias = 0U; // Unchecked for fixed staring camera
        break;
    case StabilizationPreset::MovingVehicle:
        bias.autoBias = 1U; // Keeps image centered through turns
        break;
    }
    return bias;
}

} // namespace Sightline
