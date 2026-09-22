#pragma once

#include "ViscaTypes.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace Visca {

/// @class ViscaFrame
/// @brief Encapsulates a raw VISCA protocol packet.
/// @details A VISCA frame consists of 3 to 16 bytes terminated by 0xFF.
/// It provides utility methods to parse headers, extract sockets, identify response types,
/// and encode/decode 4-bit nibbles used by VISCA commands.
class ViscaFrame {
public:
    /// @brief Default constructor creating an empty frame.
    ViscaFrame() = default;

    /// @brief Constructs a ViscaFrame from a vector of raw bytes.
    /// @param[in] bytes Raw VISCA packet bytes including the 0xFF delimiter.
    explicit ViscaFrame(std::vector<uint8_t> bytes);

    /// @brief Constructs a ViscaFrame from an initializer list of bytes.
    /// @param[in] init Initializer list of packet bytes.
    ViscaFrame(std::initializer_list<uint8_t> init);

    /// @brief Constructs a ViscaFrame from a raw buffer pointer and length.
    /// @param[in] data Pointer to raw byte data.
    /// @param[in] length Number of bytes in packet.
    ViscaFrame(const uint8_t* data, size_t length);

    /// @brief Checks whether the frame contains a syntactically valid VISCA packet.
    /// @details A valid frame must be between 3 and 16 bytes long and terminate with 0xFF.
    /// @return True if frame size and terminator are valid, false otherwise.
    [[nodiscard]] bool isValid() const noexcept;

    /// @brief Checks if the frame is empty.
    /// @return True if no bytes are contained, false otherwise.
    [[nodiscard]] bool empty() const noexcept
    {
        return m_bytes.empty();
    }

    /// @brief Returns the number of bytes in the packet.
    /// @return Packet size in bytes.
    [[nodiscard]] size_t size() const noexcept
    {
        return m_bytes.size();
    }

    /// @brief Accesses raw packet byte array.
    /// @return Const pointer to underlying byte array.
    [[nodiscard]] const uint8_t* data() const noexcept
    {
        return m_bytes.data();
    }

    /// @brief Retrieves the packet bytes vector.
    /// @return Const reference to internal byte vector.
    [[nodiscard]] const std::vector<uint8_t>& bytes() const noexcept
    {
        return m_bytes;
    }

    /// @brief Subscript operator to access individual bytes.
    /// @param[in] index Byte index (0-based).
    /// @return Byte value at given index.
    [[nodiscard]] uint8_t operator[](size_t index) const
    {
        return m_bytes[index];
    }

    /// @brief Extracts the source address of the packet.
    /// @details For controller commands (0x8y), source is 0. For camera replies (0x90..0xF0),
    /// the source is the camera ID (1..7).
    /// @return Source address (0 for controller, 1..7 for cameras).
    [[nodiscard]] uint8_t sourceAddress() const noexcept;

    /// @brief Extracts the destination address of the packet.
    /// @details For controller commands (0x8y), destination is y (1..7) or 8 for broadcast.
    /// For camera replies (0x90..0xF0), destination is 0.
    /// @return Destination address.
    [[nodiscard]] uint8_t destinationAddress() const noexcept;

    /// @brief Checks if the frame is a broadcast message.
    /// @return True if addressed to broadcast (address 8), false otherwise.
    [[nodiscard]] bool isBroadcast() const noexcept;

    /// @brief Classifies the VISCA packet type.
    /// @return The detected @ref ViscaMessageType.
    [[nodiscard]] ViscaMessageType messageType() const noexcept;

    /// @brief Checks if this packet is a socket ACK response (y0 4s FF).
    /// @return True if packet is an ACK, false otherwise.
    [[nodiscard]] bool isAck() const noexcept;

    /// @brief Checks if this packet is a command completion response (y0 5s FF) or inquiry completion (y0 50 ... FF).
    /// @return True if packet is a completion, false otherwise.
    [[nodiscard]] bool isCompletion() const noexcept;

    /// @brief Checks if this packet is an error notification (y0 6s ee FF).
    /// @return True if packet is an error, false otherwise.
    [[nodiscard]] bool isError() const noexcept;

    /// @brief Checks if this packet is an inquiry response.
    /// @return True if packet is a multi-byte inquiry completion (y0 50 ... FF with size >= 4).
    [[nodiscard]] bool isInquiryResponse() const noexcept;

    /// @brief Checks if this packet is a network change notification (y0 38 FF).
    /// @return True if packet is network change, false otherwise.
    [[nodiscard]] bool isNetworkChange() const noexcept;

    /// @brief Extracts the associated execution socket from an ACK, Completion, or Error response.
    /// @return Socket 1 or 2, or ViscaSocket::None if not applicable.
    [[nodiscard]] ViscaSocket socket() const noexcept;

    /// @brief Extracts the VISCA error code if this packet is an error response.
    /// @return The error code, or ViscaErrorCode::None if not an error packet.
    [[nodiscard]] ViscaErrorCode errorCode() const noexcept;

    /// @brief Packs a 16-bit word into 4 4-bit VISCA nibbles (0p 0q 0r 0s).
    /// @param[in] value 16-bit integer value.
    /// @return Vector of 4 bytes containing the high-to-low nibbles.
    [[nodiscard]] static std::vector<uint8_t> packWordNibbles(uint16_t value);

    /// @brief Unpacks 4 consecutive 4-bit VISCA nibbles into a 16-bit word.
    /// @param[in] data Pointer to at least 4 bytes of nibble data (0p 0q 0r 0s).
    /// @return Reconstructed 16-bit unsigned integer.
    [[nodiscard]] static uint16_t unpackWordNibbles(const uint8_t* data) noexcept;

    /// @brief Packs an 8-bit byte into 2 4-bit VISCA nibbles (0p 0q).
    /// @param[in] value 8-bit integer value.
    /// @return Vector of 2 bytes containing high-to-low nibbles.
    [[nodiscard]] static std::vector<uint8_t> packByteNibbles(uint8_t value);

    /// @brief Unpacks 2 consecutive 4-bit VISCA nibbles into an 8-bit byte.
    /// @param[in] data Pointer to at least 2 bytes of nibble data (0p 0q).
    /// @return Reconstructed 8-bit unsigned integer.
    [[nodiscard]] static uint8_t unpackByteNibbles(const uint8_t* data) noexcept;

    /// @brief Formats the packet as a spaced hexadecimal string (e.g. "81 01 04 00 02 FF").
    /// @return Formatted hex string.
    [[nodiscard]] std::string toHexString() const;

    /// @brief Equality comparison operator.
    [[nodiscard]] bool operator==(const ViscaFrame& other) const noexcept
    {
        return m_bytes == other.m_bytes;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] bool operator!=(const ViscaFrame& other) const noexcept
    {
        return m_bytes != other.m_bytes;
    }

private:
    std::vector<uint8_t> m_bytes {};
};

} // namespace Visca
