#pragma once

/// @file SightlineFraming.h
/// @brief Core packet framing, CRC calculation, and endian read/append utilities for Sightline SLA protocol.

#include "SightlineCrc8.h"
#include "SightlineTypes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Sightline {

/// @class SightlineFraming
/// @brief Static utilities for packet framing, deframing, and byte-level endian conversions.
class SightlineFraming {
public:
    static constexpr std::uint8_t HeaderByte1 { 0x51U };
    static constexpr std::uint8_t HeaderByte2 { 0xACU };

    /// @brief Wraps a payload and message ID into a framed, CRC8-protected SLA packet.
    /// @details Encodes header (0x51 0xAC), 1-byte or 2-byte length, message ID, payload, and computed CRC-8 checksum.
    /// @param[in] id SLA Message ID.
    /// @param[in] payload Byte buffer containing message payload.
    /// @return Complete framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildPacket(MessageId id, ByteView payload);

    /// @brief Determines length of the packet header (3 bytes for short, 4 for extended length).
    /// @details Checks sync bytes and high bit of length byte to distinguish between standard (3-byte)
    ///          and extended (4-byte) SLA framing headers.
    /// @param[in] packet Raw packet buffer or view.
    /// @return Number of header bytes (0 if incomplete/invalid).
    [[nodiscard]] static std::size_t getHeaderLength(ByteView packet) noexcept;

    /// @brief Extracts the MessageId from a framed packet.
    /// @details Reads byte located immediately following header bytes without copying the frame.
    /// @param[in] packet Validated framed packet buffer or view.
    /// @return Extracted MessageId enum or MessageId::Unknown.
    [[nodiscard]] static MessageId identifyMessage(ByteView packet) noexcept;

    /// @brief Extracts payload bytes excluding header, Message ID, and CRC without heap allocation.
    /// @details Computes zero-copy ByteView slice between Message ID and CRC checksum.
    /// @param[in] packet Validated framed packet buffer or view.
    /// @return ByteView of payload bytes.
    [[nodiscard]] static ByteView extractPayload(ByteView packet) noexcept;

    // --- Endian Serialization Helpers (Little-Endian) ---

    /// @brief Appends a 16-bit unsigned integer in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendU16Le(std::vector<std::uint8_t>& buf, std::uint16_t val);

    /// @brief Appends a 16-bit signed integer in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendS16Le(std::vector<std::uint8_t>& buf, std::int16_t val);

    /// @brief Appends a 32-bit unsigned integer in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendU32Le(std::vector<std::uint8_t>& buf, std::uint32_t val);

    /// @brief Appends a 32-bit signed integer in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendS32Le(std::vector<std::uint8_t>& buf, std::int32_t val);

    /// @brief Appends a 64-bit unsigned integer in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendU64Le(std::vector<std::uint8_t>& buf, std::uint64_t val);

    /// @brief Appends a 32-bit IEEE float in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendFloat32Le(std::vector<std::uint8_t>& buf, float val);

    /// @brief Appends a 64-bit IEEE double in little-endian order.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] val Value to append.
    static void appendDouble64Le(std::vector<std::uint8_t>& buf, double val);

    /// @brief Appends a null-terminated ASCII string.
    /// @param[in,out] buf Target byte buffer.
    /// @param[in] str String to append.
    static void appendString(std::vector<std::uint8_t>& buf, const std::string& str);

    /// @brief Appends an IDD SVPLenString_t (u8 length prefix followed by raw characters, no terminator).
    /// @details The length prefix is a single byte, so strings longer than 255 characters cannot be
    ///          represented. In that case nothing is appended (no silent truncation, CERT STR50-CPP).
    /// @param[in,out] buf Target byte buffer; left unchanged on failure.
    /// @param[in] str String to append.
    /// @return True if the string was appended, false if it exceeds 255 characters.
    [[nodiscard]] static bool appendLenString(std::vector<std::uint8_t>& buf, const std::string& str);

    // --- Endian Deserialization Helpers (Little-Endian) ---

    /// @brief Reads a 16-bit unsigned integer in little-endian order.
    /// @param[in] ptr Pointer to at least 2 readable bytes.
    /// @return 16-bit value.
    [[nodiscard]] static std::uint16_t readU16Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 16-bit signed integer in little-endian order.
    /// @param[in] ptr Pointer to at least 2 readable bytes.
    /// @return 16-bit signed value.
    [[nodiscard]] static std::int16_t readS16Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 32-bit unsigned integer in little-endian order.
    /// @param[in] ptr Pointer to at least 4 readable bytes.
    /// @return 32-bit value.
    [[nodiscard]] static std::uint32_t readU32Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 32-bit signed integer in little-endian order.
    /// @param[in] ptr Pointer to at least 4 readable bytes.
    /// @return 32-bit signed value.
    [[nodiscard]] static std::int32_t readS32Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 64-bit unsigned integer in little-endian order.
    /// @param[in] ptr Pointer to at least 8 readable bytes.
    /// @return 64-bit value.
    [[nodiscard]] static std::uint64_t readU64Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 32-bit IEEE float in little-endian order.
    /// @details Deserializes little-endian float using std::memcpy to avoid strict aliasing violation.
    /// @param[in] ptr Pointer to at least 4 readable bytes.
    /// @return 32-bit float value.
    [[nodiscard]] static float readFloat32Le(const std::uint8_t* ptr) noexcept;

    /// @brief Reads a 64-bit IEEE double in little-endian order.
    /// @param[in] ptr Pointer to at least 8 readable bytes.
    /// @return 64-bit double value.
    [[nodiscard]] static double readDouble64Le(const std::uint8_t* ptr) noexcept;
};

} // namespace Sightline
