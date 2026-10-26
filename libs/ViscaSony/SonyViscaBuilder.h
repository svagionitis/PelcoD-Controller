#pragma once

#include "SonyViscaTypes.h"
#include <ViscaFrame.h>
#include <ViscaTypes.h>

#include <cstdint>

namespace Visca::Sony {

/// @class SonyViscaBuilder
/// @brief Command and inquiry generator for Sony FCB-EV9520L and FCB-EW9500H cameras.
class SonyViscaBuilder {
public:
    // --- Lens Commands ---
    [[nodiscard]] static ViscaFrame zoomStop(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomTele(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomWide(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomTeleVariable(std::uint8_t cameraAddress, std::uint8_t speed);
    [[nodiscard]] static ViscaFrame zoomWideVariable(std::uint8_t cameraAddress, std::uint8_t speed);
    [[nodiscard]] static ViscaFrame zoomDirect(std::uint8_t cameraAddress, std::uint16_t position);

    [[nodiscard]] static ViscaFrame dzoomOn(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame dzoomMode(std::uint8_t cameraAddress, bool combine);
    [[nodiscard]] static ViscaFrame zoomPositionInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame dzoomModeInquiry(std::uint8_t cameraAddress = 1);

    [[nodiscard]] static ViscaFrame focusStop(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusFar(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusNear(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusFarVariable(std::uint8_t cameraAddress, std::uint8_t speed);
    [[nodiscard]] static ViscaFrame focusNearVariable(std::uint8_t cameraAddress, std::uint8_t speed);
    [[nodiscard]] static ViscaFrame focusDirect(std::uint8_t cameraAddress, std::uint16_t position);
    [[nodiscard]] static ViscaFrame focusAuto(std::uint8_t cameraAddress, bool autoMode);
    [[nodiscard]] static ViscaFrame focusOnePush(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusNearLimit(std::uint8_t cameraAddress, std::uint16_t limit);
    [[nodiscard]] static ViscaFrame focusPositionInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusNearLimitInquiry(std::uint8_t cameraAddress = 1);

    // --- Exposure Commands ---
    [[nodiscard]] static ViscaFrame exposureMode(std::uint8_t cameraAddress, SonyExposureMode mode);
    [[nodiscard]] static ViscaFrame shutterDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame irisDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame gainDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame exposureComp(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame exposureCompDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame exposureModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame shutterPositionInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame irisPositionInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame gainPositionInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame exposureCompModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame exposureCompPositionInquiry(std::uint8_t cameraAddress = 1);

    // --- White Balance Commands ---
    [[nodiscard]] static ViscaFrame wbMode(std::uint8_t cameraAddress, SonyWhiteBalanceMode mode);
    [[nodiscard]] static ViscaFrame wbOnePushTrigger(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame rGainDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame bGainDirect(std::uint8_t cameraAddress, std::uint8_t position);
    [[nodiscard]] static ViscaFrame wbModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame rGainInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame bGainInquiry(std::uint8_t cameraAddress = 1);

    // --- Enhancement Commands ---
    [[nodiscard]] static ViscaFrame stabilizer(std::uint8_t cameraAddress, SonyStabilizerMode mode);
    [[nodiscard]] static ViscaFrame defog(std::uint8_t cameraAddress, SonyDefogMode mode);
    [[nodiscard]] static ViscaFrame icr(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame autoIcr(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame stabilizerModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame defogModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame icrModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame autoIcrModeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame cameraIdInquiry(std::uint8_t cameraAddress = 1);

    // --- Picture & Sharpness Controls ---
    [[nodiscard]] static ViscaFrame apertureReset(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureUp(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureDown(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureDirect(std::uint8_t cameraAddress, std::uint8_t level);
    [[nodiscard]] static ViscaFrame apertureInquiry(std::uint8_t cameraAddress = 1);

    // --- Exposure & Sensitivity Controls ---
    [[nodiscard]] static ViscaFrame backlight(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame backlightInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame autoSlowShutter(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame autoSlowShutterInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame highSensitivity(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame highSensitivityInquiry(std::uint8_t cameraAddress = 1);

    // --- Noise Reduction ---
    [[nodiscard]] static ViscaFrame noiseReduction2D(std::uint8_t cameraAddress, std::uint8_t level);
    [[nodiscard]] static ViscaFrame noiseReduction2DInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame noiseReduction3D(std::uint8_t cameraAddress, std::uint8_t level);
    [[nodiscard]] static ViscaFrame noiseReduction3DInquiry(std::uint8_t cameraAddress = 1);

    // --- Dynamic Range & Effects ---
    [[nodiscard]] static ViscaFrame wideD(std::uint8_t cameraAddress, SonyWideDMode mode);
    [[nodiscard]] static ViscaFrame wideDInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame freeze(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame freezeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame pictureFlip(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame pictureFlipInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame lrReverse(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame lrReverseInquiry(std::uint8_t cameraAddress = 1);

    // --- Color Adjustments ---
    [[nodiscard]] static ViscaFrame colorGain(std::uint8_t cameraAddress, std::uint8_t gain);
    [[nodiscard]] static ViscaFrame colorGainInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame colorHue(std::uint8_t cameraAddress, std::uint8_t hue);
    [[nodiscard]] static ViscaFrame colorHueInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame chromaSuppress(std::uint8_t cameraAddress, std::uint8_t level);
    [[nodiscard]] static ViscaFrame chromaSuppressInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame gamma(std::uint8_t cameraAddress, std::uint8_t mode);
    [[nodiscard]] static ViscaFrame gammaInquiry(std::uint8_t cameraAddress = 1);

    // --- Preset Memory (CAM_Memory) ---
    [[nodiscard]] static ViscaFrame memory(std::uint8_t cameraAddress, SonyMemoryAction action, std::uint8_t channel);
    [[nodiscard]] static ViscaFrame memorySet(std::uint8_t cameraAddress, std::uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryRecall(std::uint8_t cameraAddress, std::uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryReset(std::uint8_t cameraAddress, std::uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryInquiry(std::uint8_t cameraAddress = 1);

    // --- Spot Control (AE, Focus, AWB) ---
    [[nodiscard]] static ViscaFrame spotAe(std::uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame spotAePosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y);
    [[nodiscard]] static ViscaFrame spotAeInquiry(std::uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame spotAePositionInquiry(std::uint8_t cameraAddress = 1);

    [[nodiscard]] static ViscaFrame spotFocusPosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y);
    [[nodiscard]] static ViscaFrame spotFocusPositionInquiry(std::uint8_t cameraAddress = 1);

    [[nodiscard]] static ViscaFrame spotAwbPosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y);
    [[nodiscard]] static ViscaFrame spotAwbPositionInquiry(std::uint8_t cameraAddress = 1);

    // --- Registers & Configuration ---
    [[nodiscard]] static ViscaFrame writeRegister(std::uint8_t cameraAddress, std::uint8_t reg, std::uint8_t value);
    [[nodiscard]] static ViscaFrame registerInquiry(std::uint8_t cameraAddress, std::uint8_t reg);

    // --- Block Inquiries (00 to 05) ---
    [[nodiscard]] static ViscaFrame blockInquiry(std::uint8_t cameraAddress, std::uint8_t blockIndex);

private:
    [[nodiscard]] static constexpr std::uint8_t makeHeader(std::uint8_t destAddress) noexcept
    {
        return static_cast<std::uint8_t>(0x80 | (destAddress & 0x0F));
    }
};

} // namespace Visca::Sony
