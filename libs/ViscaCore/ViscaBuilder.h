#pragma once

#include "ViscaFrame.h"
#include "ViscaTypes.h"

#include <cstdint>

namespace Visca {

/// @class ViscaBuilder
/// @brief Factory class to construct standard VISCA command and inquiry frames.
/// @details Implements packet builders for fundamental VISCA protocol operations
/// including address auto-configuration, interface reset, socket cancellation, and power control.
class ViscaBuilder {
public:
    /// @brief Builds an AddressSet broadcast command (88 30 01 FF).
    /// @details Automatically assigns sequential IDs (1..7) to daisy-chained VISCA cameras.
    /// @return Complete @ref ViscaFrame containing the AddressSet packet.
    [[nodiscard]] static ViscaFrame addressSet();

    /// @brief Builds an IF_Clear command to clear the command buffer of a camera (8x 01 00 01 FF).
    /// @param[in] cameraAddress Target camera address (1..7) or 8 for broadcast.
    /// @return Complete @ref ViscaFrame containing the IF_Clear packet.
    [[nodiscard]] static ViscaFrame ifClear(uint8_t cameraAddress = 1);

    /// @brief Builds an IF_Clear broadcast command (88 01 00 01 FF).
    /// @return Complete @ref ViscaFrame containing the broadcast IF_Clear packet.
    [[nodiscard]] static ViscaFrame ifClearBroadcast();

    /// @brief Builds a Command Cancel packet to abort an in-flight socket command (8x 2y FF).
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] socket Target socket to cancel (Socket1 or Socket2).
    /// @return Complete @ref ViscaFrame containing the cancel command.
    [[nodiscard]] static ViscaFrame commandCancel(uint8_t cameraAddress, ViscaSocket socket);

    /// @brief Builds a camera power on/off command (8x 01 04 00 02/03 FF).
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] on True for Power On (0x02), false for Standby/Power Off (0x03).
    /// @return Complete @ref ViscaFrame.
    [[nodiscard]] static ViscaFrame power(uint8_t cameraAddress, bool on);

    /// @brief Builds a CAM_VersionInq packet (8x 09 00 02 FF).
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame inquiry packet.
    [[nodiscard]] static ViscaFrame versionInquiry(uint8_t cameraAddress);

    /// @brief Builds a CAM_PowerInq packet (8x 09 04 00 FF).
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame inquiry packet.
    [[nodiscard]] static ViscaFrame powerInquiry(uint8_t cameraAddress);

    /// @brief Generates the standard VISCA header byte.
    /// @param[in] destAddress Destination camera ID (1..7) or 8 for broadcast.
    /// @return Byte with bit 7 set and destination encoded in lower nibble.
    [[nodiscard]] static constexpr uint8_t makeHeader(uint8_t destAddress) noexcept
    {
        return static_cast<uint8_t>(0x80 | (destAddress & 0x0F));
    }
};

} // namespace Visca
