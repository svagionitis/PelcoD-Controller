#pragma once

/// @file PelcoDFrame.h
/// @brief Pelco-D frame serialization, checksum calculation, and stream parsing.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PelcoD {

/// @class PelcoDFrame
/// @brief Static utilities for constructing and validating Pelco-D protocol frames.
class PelcoDFrame {
public:
    static constexpr std::uint8_t SyncByte { 0xFFU };
    static constexpr std::size_t GeneralResponseSize { 4U };
    static constexpr std::size_t StandardFrameSize { 7U };
    static constexpr std::size_t QueryResponseSize { 18U };

    /// @brief Calculates 8-bit modulo 256 checksum over byte vector.
    /// @param[in] bytes Payload bytes excluding sync byte.
    /// @return Computed 8-bit checksum value.
    [[nodiscard]] static std::uint8_t calculateChecksum(const std::vector<std::uint8_t>& bytes) noexcept;

    /// @brief Calculates 8-bit modulo 256 checksum over raw memory buffer.
    /// @param[in] data Pointer to payload start.
    /// @param[in] length Number of payload bytes.
    /// @return Computed 8-bit checksum value.
    [[nodiscard]] static std::uint8_t calculateChecksum(const std::uint8_t* data, std::size_t length) noexcept;

    /// @brief Creates a 7-byte command frame.
    /// @param[in] address Device RS-485 bus address (1 - 255).
    /// @param[in] cmd1 Command byte 1.
    /// @param[in] cmd2 Command byte 2.
    /// @param[in] data1 Data byte 1.
    /// @param[in] data2 Data byte 2.
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> createFrame(
        std::uint8_t address, std::uint8_t cmd1, std::uint8_t cmd2, std::uint8_t data1, std::uint8_t data2);

    /// @brief Validates frame structure and checksum.
    /// @param[in] frame Raw frame bytes starting with SyncByte.
    /// @return True if length and checksum conform to protocol.
    [[nodiscard]] static bool isValidFrame(const std::vector<std::uint8_t>& frame) noexcept;

    /// @brief Extracts verified frames from continuous byte stream.
    /// @param[in] stream Input byte buffer.
    /// @return List of isolated valid protocol frames.
    [[nodiscard]] static std::vector<std::vector<std::uint8_t>> splitStream(const std::vector<std::uint8_t>& stream);
};

} // namespace PelcoD
