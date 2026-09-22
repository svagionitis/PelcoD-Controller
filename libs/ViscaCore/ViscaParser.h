#pragma once

#include "ViscaFrame.h"
#include "ViscaTypes.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Visca {

/// @struct ViscaAckResponse
/// @brief Represents a parsed socket ACK packet (y0 4s FF).
struct ViscaAckResponse {
    uint8_t cameraAddress { 0 }; ///< Address of responding camera (1..7)
    ViscaSocket socket { ViscaSocket::None }; ///< Acknowledged execution socket
};

/// @struct ViscaCompletionResponse
/// @brief Represents a parsed command completion packet (y0 5s FF).
struct ViscaCompletionResponse {
    uint8_t cameraAddress { 0 }; ///< Address of responding camera (1..7)
    ViscaSocket socket { ViscaSocket::None }; ///< Completed execution socket (or None for general completion)
};

/// @struct ViscaErrorResponse
/// @brief Represents a parsed error packet (y0 6s ee FF).
struct ViscaErrorResponse {
    uint8_t cameraAddress { 0 }; ///< Address of responding camera (1..7)
    ViscaSocket socket { ViscaSocket::None }; ///< Socket associated with error, if any
    ViscaErrorCode code { ViscaErrorCode::None }; ///< Parsed error code
};

/// @struct ViscaVersionInfo
/// @brief Parsed hardware and firmware version information from CAM_VersionInq (y0 50 gg gg hh hh jj jj kk FF).
struct ViscaVersionInfo {
    uint8_t cameraAddress { 0 }; ///< Responding camera ID
    uint16_t vendorId { 0 }; ///< Vendor ID (0x0020 for Sony)
    uint16_t modelId { 0 }; ///< Model ID (0x0711 = EV9520L, 0x070F = EW9500H)
    uint16_t romVersion { 0 }; ///< Firmware ROM version
    uint8_t maxSockets { 0 }; ///< Maximum supported concurrent sockets (usually 2)

    /// @brief Checks if the vendor ID matches Sony Corporation (0x0020).
    [[nodiscard]] bool isSony() const noexcept
    {
        return vendorId == 0x0020;
    }
};

/// @struct ViscaAddressSetResponse
/// @brief Parsed response from an AddressSet command (88 30 0p FF).
struct ViscaAddressSetResponse {
    uint8_t cameraCount { 0 }; ///< Number of daisy-chained cameras detected on bus (p - 1)
};

/// @class ViscaParser
/// @brief Static decoder for VISCA reply frames into strongly-typed structures.
class ViscaParser {
public:
    /// @brief Parses an ACK packet.
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional @ref ViscaAckResponse if frame is a valid ACK, std::nullopt otherwise.
    [[nodiscard]] static std::optional<ViscaAckResponse> parseAck(const ViscaFrame& frame) noexcept;

    /// @brief Parses a command completion packet.
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional @ref ViscaCompletionResponse if frame is a valid completion, std::nullopt otherwise.
    [[nodiscard]] static std::optional<ViscaCompletionResponse> parseCompletion(const ViscaFrame& frame) noexcept;

    /// @brief Parses an error packet.
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional @ref ViscaErrorResponse if frame is a valid error, std::nullopt otherwise.
    [[nodiscard]] static std::optional<ViscaErrorResponse> parseError(const ViscaFrame& frame) noexcept;

    /// @brief Parses a CAM_VersionInq response packet.
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional @ref ViscaVersionInfo if frame is a valid version reply, std::nullopt otherwise.
    [[nodiscard]] static std::optional<ViscaVersionInfo> parseVersionInquiry(const ViscaFrame& frame) noexcept;

    /// @brief Parses an AddressSet reply packet (88 30 0p FF).
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional @ref ViscaAddressSetResponse if frame is a valid address set reply, std::nullopt otherwise.
    [[nodiscard]] static std::optional<ViscaAddressSetResponse> parseAddressSet(const ViscaFrame& frame) noexcept;

    /// @brief Parses a CAM_PowerInq response packet (y0 50 02/03 FF).
    /// @param[in] frame Raw frame to inspect.
    /// @return Optional bool (true for power on, false for standby/off), std::nullopt if invalid.
    [[nodiscard]] static std::optional<bool> parsePowerInquiry(const ViscaFrame& frame) noexcept;
};

} // namespace Visca
