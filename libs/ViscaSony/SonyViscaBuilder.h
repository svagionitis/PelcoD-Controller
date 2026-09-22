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
