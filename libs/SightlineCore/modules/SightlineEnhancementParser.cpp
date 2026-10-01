/// @file SightlineEnhancementParser.cpp
/// @brief Implementation of Sightline enhancement frame deserializers.

#include "SightlineEnhancementParser.h"

namespace Sightline {

bool SightlineEnhancementParser::parseVideoEnhance(
    const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancement& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentVideoEnhancementParameters && id != MessageId::SetVideoEnhancementParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    if (payload.size() >= 24U) {
        const std::uint8_t sharpenEnhance { payload[0U] };
        out.cameraIndex = payload[4U];
        out.contrast = payload[21U];
        out.brightness = payload[20U];
        out.sharpening = static_cast<std::uint8_t>((sharpenEnhance >> 4U) & 0x0FU);
        out.claheEnable = ((sharpenEnhance & 0x0FU) == 1U) ? 1U : 0U;
        return true;
    }

    out.cameraIndex = payload[0U];
    out.contrast = payload[1U];
    out.brightness = payload[2U];
    out.sharpening = payload[3U];
    out.claheEnable = payload[4U];
    return true;
}

bool SightlineEnhancementParser::parseVideoEnhanceFull(
    const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancementFull& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentVideoEnhancementParameters && id != MessageId::SetVideoEnhancementParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 24U) {
        return false;
    }

    const std::uint8_t sharpenEnhance { payload[0U] };
    out.mode = static_cast<ContrastMode>(sharpenEnhance & 0x0FU);
    out.sharpening = static_cast<std::uint8_t>((sharpenEnhance >> 4U) & 0x0FU);
    out.alphaBlend = payload[1U];
    out.enhanceParam = payload[2U];
    out.denoiseRate = payload[3U];
    out.cameraIndex = payload[4U];
    out.flags = payload[5U];
    out.histAveRate = payload[6U];
    out.histMaxPctBin = payload[7U];
    out.roiRow = static_cast<std::uint16_t>(payload[8U] | (static_cast<std::uint16_t>(payload[9U]) << 8U));
    out.roiCol = static_cast<std::uint16_t>(payload[10U] | (static_cast<std::uint16_t>(payload[11U]) << 8U));
    out.roiHigh = static_cast<std::uint16_t>(payload[12U] | (static_cast<std::uint16_t>(payload[13U]) << 8U));
    out.roiWide = static_cast<std::uint16_t>(payload[14U] | (static_cast<std::uint16_t>(payload[15U]) << 8U));
    out.deconvSigma = payload[16U];
    out.gaussianBlur = payload[17U];
    out.lapMinDiff = payload[18U];
    out.colorEnhance = payload[19U];
    out.brightness = payload[20U];
    out.contrast = payload[21U];
    out.scintillation = static_cast<ScintillationPreset>(payload[22U]);
    out.sharpenRadius = payload[23U];

    out.customKernel.clear();
    out.normalizeKernel = false;
    if (payload.size() > 24U) {
        const std::uint8_t kernelLen { payload[24U] };
        if (payload.size() >= static_cast<std::size_t>(25U + kernelLen)) {
            out.customKernel.reserve(kernelLen);
            for (std::size_t i { 0U }; i < kernelLen; ++i) {
                out.customKernel.push_back(static_cast<std::int8_t>(payload[25U + i]));
            }
            if (payload.size() > static_cast<std::size_t>(25U + kernelLen)) {
                out.normalizeKernel = (payload[25U + kernelLen] != 0U);
            }
        }
    }
    return true;
}

bool SightlineEnhancementParser::parseNoise3D(
    const std::vector<std::uint8_t>& packet, MsgNoise3D& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::Noise3D) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.enable = payload[1U];
    out.temporalStrength = payload[2U];
    out.spatialStrength = payload[3U];
    return true;
}

} // namespace Sightline
