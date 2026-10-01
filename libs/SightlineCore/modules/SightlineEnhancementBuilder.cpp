/// @file SightlineEnhancementBuilder.cpp
/// @brief Implementation of Sightline enhancement command serializers.

#include "SightlineEnhancementBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetVideoEnhance(
    const MsgSetVideoEnhancement& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex,
        msg.contrast,
        msg.brightness,
        msg.sharpening,
        msg.claheEnable
    };
    return SightlineFraming::buildPacket(MessageId::SetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetVideoEnhanceFull(
    const MsgSetVideoEnhancementFull& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(28U + msg.customKernel.size() + 1U);

    // Byte 0: Bits 0-3 ContrastMode, Bits 4-7 Sharpening (0-15)
    const auto sharpenCapped = static_cast<std::uint8_t>(msg.sharpening & 0x0FU);
    const auto modeCapped = static_cast<std::uint8_t>(static_cast<std::uint8_t>(msg.mode) & 0x0FU);
    const auto sharpenEnhance = static_cast<std::uint8_t>((sharpenCapped << 4U) | modeCapped);
    payload.push_back(sharpenEnhance);

    // Byte 1: Alpha blend (0-255)
    payload.push_back(msg.alphaBlend);

    // Byte 2: EnhanceParam (0-127)
    payload.push_back(static_cast<std::uint8_t>(msg.enhanceParam & 0x7FU));

    // Byte 3: DenoiseRate (0-255)
    payload.push_back(msg.denoiseRate);

    // Byte 4: CameraIndex
    payload.push_back(msg.cameraIndex);

    // Byte 5: Flags (Bit 0: Aerial mask, Bit 1: Feature hist, Bit 2: Sqrt hist, Bit 4: Staring mask)
    payload.push_back(msg.flags);

    // Byte 6: HistAveRate (0-255)
    payload.push_back(msg.histAveRate);

    // Byte 7: HistMaxPctBin (0-255)
    payload.push_back(msg.histMaxPctBin);

    // Bytes 8-15: ROI coordinates (little endian)
    SightlineFraming::appendU16Le(payload, msg.roiRow);
    SightlineFraming::appendU16Le(payload, msg.roiCol);
    SightlineFraming::appendU16Le(payload, msg.roiHigh);
    SightlineFraming::appendU16Le(payload, msg.roiWide);

    // Byte 16: Deconvolution sigma (reserved, default 0)
    payload.push_back(msg.deconvSigma);

    // Byte 17: Gaussian blur (0-6)
    payload.push_back(static_cast<std::uint8_t>(msg.gaussianBlur > 6U ? 6U : msg.gaussianBlur));

    // Byte 18: LAP min diff (0-255)
    payload.push_back(msg.lapMinDiff);

    // Byte 19: Color enhance (0-255)
    payload.push_back(msg.colorEnhance);

    // Byte 20: Brightness (0-255, 128 = default)
    payload.push_back(msg.brightness);

    // Byte 21: Contrast (0-255, 128 = default)
    payload.push_back(msg.contrast);

    // Byte 22: Scintillation mode (0: Manual, 1: Low, 2: High, 3: IR)
    payload.push_back(static_cast<std::uint8_t>(msg.scintillation));

    // Byte 23: Sharpen radius (1, 2, or 3)
    const auto radius = static_cast<std::uint8_t>(
        (msg.sharpenRadius >= 1U && msg.sharpenRadius <= 3U) ? msg.sharpenRadius : 1U);
    payload.push_back(radius);

    // Bytes 24+: Custom convolution kernel & normalization
    if (!msg.customKernel.empty() || msg.normalizeKernel) {
        const auto len = static_cast<std::uint8_t>(msg.customKernel.size());
        payload.push_back(len);
        for (const auto val : msg.customKernel) {
            payload.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(val)));
        }
        payload.push_back(msg.normalizeKernel ? std::uint8_t { 1U } : std::uint8_t { 0U });
    }

    return SightlineFraming::buildPacket(MessageId::SetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetFalseColor(
    std::uint8_t cameraIndex, FalseColorPalette palette)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(17U);
    SightlineFraming::appendU16Le(payload, 0U); // rotationDegrees = 0
    SightlineFraming::appendU16Le(payload, 256U); // rotationLimit = 256
    payload.push_back(50U); // decayRate = 50
    payload.push_back(static_cast<std::uint8_t>(static_cast<std::uint8_t>(palette) & 0x7FU)); // falseColorZTT
    payload.push_back(64U); // zoom = 64 (1X)
    SightlineFraming::appendU16Le(payload, 0U); // panCol = 0
    SightlineFraming::appendU16Le(payload, 0U); // tiltRow = 0
    payload.push_back(cameraIndex); // cameraIndex
    SightlineFraming::appendU16Le(payload, 0U); // extendedZoom10 = 0
    SightlineFraming::appendU16Le(payload, 0U); // zoomRate = 0
    payload.push_back(0U); // flipMode = 0
    return SightlineFraming::buildPacket(MessageId::SetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetNoise3D(
    const MsgNoise3D& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex, msg.enable, msg.temporalStrength, msg.spatialStrength
    };
    return SightlineFraming::buildPacket(MessageId::Noise3D, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildGetVideoEnhance(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildGetNoise3D(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::Noise3D), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
