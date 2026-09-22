#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Visca {

/// @brief VISCA packet delimiter byte terminating all commands, inquiries, and replies.
inline constexpr uint8_t kViscaTerminator { 0xFF };

/// @brief Minimum valid VISCA frame size in bytes (e.g., 88 30 01 FF is 4, but IF_Clear reply can be 3: 90 50 FF).
inline constexpr size_t kMinPacketLength { 3 };

/// @brief Maximum standard VISCA frame size in bytes (Block inquiries return 16 bytes).
inline constexpr size_t kMaxPacketLength { 16 };

/// @brief Maximum number of daisy-chained cameras addressable on a single VISCA bus.
inline constexpr uint8_t kMaxCamerasOnBus { 7 };

/// @brief Broadcast address identifier used in VISCA header nibbles.
inline constexpr uint8_t kBroadcastAddress { 8 };

/// @brief Controller device address (always 0 in standard VISCA topologies).
inline constexpr uint8_t kControllerAddress { 0 };

/// @brief Default baud rate for Sony FCB series cameras (9600 bps out of factory).
inline constexpr uint32_t kDefaultBaudRate { 9600 };

/// @brief Classification of VISCA packet types based on header and command semantics.
enum class ViscaMessageType : uint8_t {
    Command, ///< Normal command dispatched to camera
    Inquiry, ///< Telemetry or state inquiry
    Ack, ///< Socket ACK response (y0 4s FF)
    Completion, ///< Socket command execution complete (y0 5s FF) or inquiry reply (y0 50 ... FF)
    Error, ///< Error notification (y0 6s ee FF)
    NetworkChange, ///< Network configuration change notice (y0 38 FF)
    AddressSet, ///< Address assignment command or reply (88 30 0p FF)
    Unknown ///< Unrecognized or malformed packet
};

/// @brief Identifier of the camera command execution socket.
/// @details VISCA supports 2 concurrent execution sockets (Socket 1 and Socket 2).
enum class ViscaSocket : uint8_t {
    None = 0, ///< No socket associated (e.g. inquiries or broadcast commands)
    Socket1 = 1, ///< Execution socket 1
    Socket2 = 2 ///< Execution socket 2
};

/// @brief Standard VISCA error status codes returned in error responses (y0 6s ee FF).
enum class ViscaErrorCode : uint8_t {
    None = 0x00, ///< No error
    MessageLengthError = 0x01, ///< Message length exceeds 14 bytes or framing invalid
    SyntaxError = 0x02, ///< Command format or opcode unrecognized
    CommandBufferFull = 0x03, ///< Both execution sockets are currently busy
    CommandCanceled = 0x04, ///< Command execution was canceled by controller (8x 2y FF)
    NoSocket = 0x05, ///< Specified socket does not exist or cannot be canceled
    CommandNotExecutable = 0x41 ///< Camera condition prevents execution (e.g., manual focus command when in Auto)
};

/// @brief Converts a @ref ViscaErrorCode to a human-readable string representation.
/// @param[in] code The VISCA error code.
/// @return String view representing the error name.
[[nodiscard]] constexpr std::string_view errorCodeToString(ViscaErrorCode code) noexcept
{
    switch (code) {
    case ViscaErrorCode::None:
        return "No Error";
    case ViscaErrorCode::MessageLengthError:
        return "Message Length Error";
    case ViscaErrorCode::SyntaxError:
        return "Syntax Error";
    case ViscaErrorCode::CommandBufferFull:
        return "Command Buffer Full";
    case ViscaErrorCode::CommandCanceled:
        return "Command Canceled";
    case ViscaErrorCode::NoSocket:
        return "No Socket";
    case ViscaErrorCode::CommandNotExecutable:
        return "Command Not Executable";
    default:
        return "Unknown Error";
    }
}

/// @brief Converts a @ref ViscaMessageType to a human-readable string representation.
/// @param[in] type The VISCA message type.
/// @return String view representing the message type name.
[[nodiscard]] constexpr std::string_view messageTypeToString(ViscaMessageType type) noexcept
{
    switch (type) {
    case ViscaMessageType::Command:
        return "Command";
    case ViscaMessageType::Inquiry:
        return "Inquiry";
    case ViscaMessageType::Ack:
        return "Ack";
    case ViscaMessageType::Completion:
        return "Completion";
    case ViscaMessageType::Error:
        return "Error";
    case ViscaMessageType::NetworkChange:
        return "Network Change";
    case ViscaMessageType::AddressSet:
        return "Address Set";
    default:
        return "Unknown";
    }
}

} // namespace Visca
