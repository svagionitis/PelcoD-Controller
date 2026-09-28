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
    [[nodiscard]] static ViscaFrame zoomStop(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomTele(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomWide(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame zoomTeleVariable(uint8_t cameraAddress, uint8_t speed);
    [[nodiscard]] static ViscaFrame zoomWideVariable(uint8_t cameraAddress, uint8_t speed);
    [[nodiscard]] static ViscaFrame zoomDirect(uint8_t cameraAddress, uint16_t position);

    [[nodiscard]] static ViscaFrame dzoomOn(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame dzoomMode(uint8_t cameraAddress, bool combine);

    [[nodiscard]] static ViscaFrame focusStop(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusFar(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusNear(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusFarVariable(uint8_t cameraAddress, uint8_t speed);
    [[nodiscard]] static ViscaFrame focusNearVariable(uint8_t cameraAddress, uint8_t speed);
    [[nodiscard]] static ViscaFrame focusDirect(uint8_t cameraAddress, uint16_t position);
    [[nodiscard]] static ViscaFrame focusAuto(uint8_t cameraAddress, bool autoMode);
    [[nodiscard]] static ViscaFrame focusOnePush(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame focusNearLimit(uint8_t cameraAddress, uint16_t limit);

    // --- Exposure Commands ---
    [[nodiscard]] static ViscaFrame exposureMode(uint8_t cameraAddress, SonyExposureMode mode);
    [[nodiscard]] static ViscaFrame shutterDirect(uint8_t cameraAddress, uint8_t position);
    [[nodiscard]] static ViscaFrame irisDirect(uint8_t cameraAddress, uint8_t position);
    [[nodiscard]] static ViscaFrame gainDirect(uint8_t cameraAddress, uint8_t position);
    [[nodiscard]] static ViscaFrame exposureComp(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame exposureCompDirect(uint8_t cameraAddress, uint8_t position);

    // --- White Balance Commands ---
    [[nodiscard]] static ViscaFrame wbMode(uint8_t cameraAddress, SonyWhiteBalanceMode mode);
    [[nodiscard]] static ViscaFrame wbOnePushTrigger(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame rGainDirect(uint8_t cameraAddress, uint8_t position);
    [[nodiscard]] static ViscaFrame bGainDirect(uint8_t cameraAddress, uint8_t position);

    // --- Enhancement Commands ---
    [[nodiscard]] static ViscaFrame stabilizer(uint8_t cameraAddress, SonyStabilizerMode mode);
    [[nodiscard]] static ViscaFrame defog(uint8_t cameraAddress, SonyDefogMode mode);
    [[nodiscard]] static ViscaFrame icr(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame autoIcr(uint8_t cameraAddress, bool on);

    // --- Picture & Sharpness Controls ---
    [[nodiscard]] static ViscaFrame apertureReset(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureUp(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureDown(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame apertureDirect(uint8_t cameraAddress, uint8_t level);
    [[nodiscard]] static ViscaFrame apertureInquiry(uint8_t cameraAddress = 1);

    // --- Exposure & Sensitivity Controls ---
    [[nodiscard]] static ViscaFrame backlight(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame backlightInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame autoSlowShutter(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame autoSlowShutterInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame highSensitivity(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame highSensitivityInquiry(uint8_t cameraAddress = 1);

    // --- Noise Reduction ---
    [[nodiscard]] static ViscaFrame noiseReduction2D(uint8_t cameraAddress, uint8_t level);
    [[nodiscard]] static ViscaFrame noiseReduction2DInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame noiseReduction3D(uint8_t cameraAddress, uint8_t level);
    [[nodiscard]] static ViscaFrame noiseReduction3DInquiry(uint8_t cameraAddress = 1);

    // --- Dynamic Range & Effects ---
    [[nodiscard]] static ViscaFrame wideD(uint8_t cameraAddress, SonyWideDMode mode);
    [[nodiscard]] static ViscaFrame wideDInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame freeze(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame freezeInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame pictureFlip(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame pictureFlipInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame lrReverse(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame lrReverseInquiry(uint8_t cameraAddress = 1);

    // --- Color Adjustments ---
    [[nodiscard]] static ViscaFrame colorGain(uint8_t cameraAddress, uint8_t gain);
    [[nodiscard]] static ViscaFrame colorGainInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame colorHue(uint8_t cameraAddress, uint8_t hue);
    [[nodiscard]] static ViscaFrame colorHueInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame chromaSuppress(uint8_t cameraAddress, uint8_t level);
    [[nodiscard]] static ViscaFrame chromaSuppressInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame gamma(uint8_t cameraAddress, uint8_t mode);
    [[nodiscard]] static ViscaFrame gammaInquiry(uint8_t cameraAddress = 1);

    // --- Preset Memory (CAM_Memory) ---
    [[nodiscard]] static ViscaFrame memory(uint8_t cameraAddress, SonyMemoryAction action, uint8_t channel);
    [[nodiscard]] static ViscaFrame memorySet(uint8_t cameraAddress, uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryRecall(uint8_t cameraAddress, uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryReset(uint8_t cameraAddress, uint8_t channel);
    [[nodiscard]] static ViscaFrame memoryInquiry(uint8_t cameraAddress = 1);

    // --- Spot Control (AE, Focus, AWB) ---
    [[nodiscard]] static ViscaFrame spotAe(uint8_t cameraAddress, bool on);
    [[nodiscard]] static ViscaFrame spotAePosition(uint8_t cameraAddress, uint8_t x, uint8_t y);
    [[nodiscard]] static ViscaFrame spotAeInquiry(uint8_t cameraAddress = 1);
    [[nodiscard]] static ViscaFrame spotAePositionInquiry(uint8_t cameraAddress = 1);

    [[nodiscard]] static ViscaFrame spotFocusPosition(uint8_t cameraAddress, uint8_t x, uint8_t y);
    [[nodiscard]] static ViscaFrame spotFocusPositionInquiry(uint8_t cameraAddress = 1);

    [[nodiscard]] static ViscaFrame spotAwbPosition(uint8_t cameraAddress, uint8_t x, uint8_t y);
    [[nodiscard]] static ViscaFrame spotAwbPositionInquiry(uint8_t cameraAddress = 1);

    // --- Registers & Configuration ---
    [[nodiscard]] static ViscaFrame writeRegister(uint8_t cameraAddress, uint8_t reg, uint8_t value);
    [[nodiscard]] static ViscaFrame registerInquiry(uint8_t cameraAddress, uint8_t reg);

    // --- Block Inquiries (00 to 05) ---
    [[nodiscard]] static ViscaFrame blockInquiry(uint8_t cameraAddress, uint8_t blockIndex);

private:
    [[nodiscard]] static constexpr uint8_t makeHeader(uint8_t destAddress) noexcept
    {
        return static_cast<uint8_t>(0x80 | (destAddress & 0x0F));
    }
};

} // namespace Visca::Sony
