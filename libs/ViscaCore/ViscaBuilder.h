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

    // --- Pan/Tilt Drive Commands (Opcode 0x06) ---

    /// @brief Builds a Pan_TiltDrive command with independent pan and tilt velocities and directions.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @param[in] panDir Pan motion direction (Left, Right, or Stop).
    /// @param[in] tiltDir Tilt motion direction (Up, Down, or Stop).
    /// @return Complete @ref ViscaFrame containing the drive packet (8x 01 06 01 VV WW 0p 0q FF).
    [[nodiscard]] static ViscaFrame panTiltDrive(uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed,
        ViscaPanDirection panDir, ViscaTiltDirection tiltDir);

    /// @brief Builds a Pan_TiltDrive stop command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan deceleration speed. Defaults to 0.
    /// @param[in] tiltSpeed Tilt deceleration speed. Defaults to 0.
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV WW 03 03 FF).
    [[nodiscard]] static ViscaFrame panTiltStop(uint8_t cameraAddress = 1, uint8_t panSpeed = 0, uint8_t tiltSpeed = 0);

    /// @brief Builds a Pan_TiltDrive Up command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 00 WW 03 01 FF).
    [[nodiscard]] static ViscaFrame panTiltUp(uint8_t cameraAddress, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltDrive Down command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 00 WW 03 02 FF).
    [[nodiscard]] static ViscaFrame panTiltDown(uint8_t cameraAddress, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltDrive Left command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV 00 01 03 FF).
    [[nodiscard]] static ViscaFrame panTiltLeft(uint8_t cameraAddress, uint8_t panSpeed);

    /// @brief Builds a Pan_TiltDrive Right command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV 00 02 03 FF).
    [[nodiscard]] static ViscaFrame panTiltRight(uint8_t cameraAddress, uint8_t panSpeed);

    /// @brief Builds a Pan_TiltDrive Up-Left command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV WW 01 01 FF).
    [[nodiscard]] static ViscaFrame panTiltUpLeft(uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltDrive Up-Right command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV WW 02 01 FF).
    [[nodiscard]] static ViscaFrame panTiltUpRight(uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltDrive Down-Left command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV WW 01 02 FF).
    [[nodiscard]] static ViscaFrame panTiltDownLeft(uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltDrive Down-Right command.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return Complete @ref ViscaFrame (8x 01 06 01 VV WW 02 02 FF).
    [[nodiscard]] static ViscaFrame panTiltDownRight(uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed);

    /// @brief Builds a Pan_TiltAbsolutePos command to slew to specific coordinates.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan motion speed (0x01..0x18).
    /// @param[in] tiltSpeed Tilt motion speed (0x01..0x14).
    /// @param[in] panPos Signed 16-bit target Pan step position.
    /// @param[in] tiltPos Signed 16-bit target Tilt step position.
    /// @return Complete @ref ViscaFrame (8x 01 06 02 VV WW 0Y 0Y 0Y 0Y 0Z 0Z 0Z 0Z FF).
    [[nodiscard]] static ViscaFrame panTiltAbsolute(
        uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed, int16_t panPos, int16_t tiltPos);

    /// @brief Builds a Pan_TiltRelativePos command to apply relative offset displacement.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] panSpeed Pan motion speed (0x01..0x18).
    /// @param[in] tiltSpeed Tilt motion speed (0x01..0x14).
    /// @param[in] deltaPan Signed 16-bit Pan step offset.
    /// @param[in] deltaTilt Signed 16-bit Tilt step offset.
    /// @return Complete @ref ViscaFrame (8x 01 06 03 VV WW 0Y 0Y 0Y 0Y 0Z 0Z 0Z 0Z FF).
    [[nodiscard]] static ViscaFrame panTiltRelative(
        uint8_t cameraAddress, uint8_t panSpeed, uint8_t tiltSpeed, int16_t deltaPan, int16_t deltaTilt);

    /// @brief Builds a Pan_TiltHome command to return mechanism to origin (0, 0).
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame (8x 01 06 04 FF).
    [[nodiscard]] static ViscaFrame panTiltHome(uint8_t cameraAddress = 1);

    /// @brief Builds a Pan_TiltReset command to re-initialize Pan/Tilt drive motors.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame (8x 01 06 05 FF).
    [[nodiscard]] static ViscaFrame panTiltReset(uint8_t cameraAddress = 1);

    /// @brief Builds a Pan_TiltLimitSet command to configure mechanical boundary limits.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] corner Limit boundary corner (DownLeft or UpRight).
    /// @param[in] panPos Signed 16-bit Pan limit coordinate.
    /// @param[in] tiltPos Signed 16-bit Tilt limit coordinate.
    /// @return Complete @ref ViscaFrame (8x 01 06 07 00 0W 0Y 0Y 0Y 0Y 0Z 0Z 0Z 0Z FF).
    [[nodiscard]] static ViscaFrame panTiltLimitSet(
        uint8_t cameraAddress, ViscaPanTiltCorner corner, int16_t panPos, int16_t tiltPos);

    /// @brief Builds a Pan_TiltLimitSet clear command to remove software boundary limits.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @param[in] corner Limit boundary corner to clear.
    /// @return Complete @ref ViscaFrame (8x 01 06 07 01 0W 07 0F 0F 0F 07 0F 0F 0F FF).
    [[nodiscard]] static ViscaFrame panTiltLimitClear(uint8_t cameraAddress, ViscaPanTiltCorner corner);

    /// @brief Builds a Pan_TiltPosInq inquiry packet.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame inquiry packet (8x 09 06 12 FF).
    [[nodiscard]] static ViscaFrame panTiltPositionInquiry(uint8_t cameraAddress = 1);

    /// @brief Builds a Pan_TiltStatusInq inquiry packet.
    /// @param[in] cameraAddress Target camera address (1..7).
    /// @return Complete @ref ViscaFrame inquiry packet (8x 09 06 10 FF).
    [[nodiscard]] static ViscaFrame panTiltStatusInquiry(uint8_t cameraAddress = 1);

    /// @brief Generates the standard VISCA header byte.
    /// @param[in] destAddress Destination camera ID (1..7) or 8 for broadcast.
    /// @return Byte with bit 7 set and destination encoded in lower nibble.
    [[nodiscard]] static constexpr uint8_t makeHeader(uint8_t destAddress) noexcept
    {
        return static_cast<uint8_t>(0x80 | (destAddress & 0x0F));
    }
};

} // namespace Visca
