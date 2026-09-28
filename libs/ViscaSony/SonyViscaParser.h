#pragma once

#include "SonyViscaTypes.h"
#include <ViscaFrame.h>

#include <cstdint>
#include <optional>

namespace Visca::Sony {

/// @class SonyViscaParser
/// @brief Decoder for Sony FCB Block Inquiries (00 to 05) and register queries.
class SonyViscaParser {
public:
    /// @brief Decodes Lens Control System Inquiry (Block 00).
    /// @param[in] frame 16-byte inquiry reply (y0 50 ... FF).
    /// @param[in,out] status Camera status structure updated with lens telemetry.
    /// @return True on success, false if packet format/size is invalid.
    static bool parseBlock00(const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes Camera Control System Inquiry (Block 01).
    /// @param[in] frame 16-byte inquiry reply (y0 50 ... FF).
    /// @param[in,out] status Camera status structure updated with exposure/WB telemetry.
    /// @return True on success, false if packet format/size is invalid.
    static bool parseBlock01(const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes Other Inquiry (Block 02).
    /// @param[in] frame 16-byte inquiry reply (y0 50 ... FF).
    /// @param[in,out] status Camera status structure updated with power/stabilizer/ICR telemetry.
    /// @return True on success, false if packet format/size is invalid.
    static bool parseBlock02(const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes Extended Function 1 Inquiry (Block 03).
    /// @param[in] frame 16-byte inquiry reply (y0 50 ... FF).
    /// @param[in,out] status Camera status structure updated with image enhancement telemetry.
    /// @return True on success, false if packet format/size is invalid.
    static bool parseBlock03(const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes Extended Function 2 Inquiry (Block 04).
    /// @param[in] frame 16-byte inquiry reply (y0 50 ... FF).
    /// @param[in,out] status Camera status structure updated with Defog/Wide-D/VE telemetry.
    /// @return True on success, false if packet format/size is invalid.
    static bool parseBlock04(const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes any Block Inquiry (00 through 04) into status.
    /// @param[in] blockIndex Block index (0..4).
    /// @param[in] frame 16-byte inquiry reply.
    /// @param[in,out] status Status structure to update.
    /// @return True on success, false otherwise.
    static bool parseBlock(uint8_t blockIndex, const ViscaFrame& frame, SonyFCBStatus& status) noexcept;

    /// @brief Decodes a register inquiry response (y0 50 0p 0q FF).
    /// @param[in] frame 5-byte inquiry response frame.
    /// @param[out] outValue Reconstructed 8-bit register value.
    /// @return True if response is valid, false otherwise.
    static bool parseRegisterInquiry(const ViscaFrame& frame, uint8_t& outValue) noexcept;

    /// @brief Decodes a 16-bit word inquiry response (y0 50 0p 0q 0r 0s FF).
    /// @param[in] frame Inquiry response frame (7 bytes).
    /// @param[out] outValue Reconstructed 16-bit word.
    /// @return True on success, false if format is invalid.
    static bool parseWordInquiry(const ViscaFrame& frame, uint16_t& outValue) noexcept;

    /// @brief Decodes an 8-bit byte inquiry response (y0 50 00 00 0p 0q FF or y0 50 0p 0q FF).
    /// @param[in] frame Inquiry response frame.
    /// @param[out] outValue Reconstructed 8-bit byte.
    /// @return True on success, false if format is invalid.
    static bool parseByteInquiry(const ViscaFrame& frame, uint8_t& outValue) noexcept;

    /// @brief Decodes a boolean on/off inquiry response (y0 50 02/03 FF).
    /// @param[in] frame Inquiry response frame (4 bytes).
    /// @param[out] outState True for 0x02 (On), false for 0x03 (Off).
    /// @return True on success, false if format is invalid.
    static bool parseBoolInquiry(const ViscaFrame& frame, bool& outState) noexcept;

    /// @brief Decodes a 4-bit mode / channel inquiry response (y0 50 0p FF).
    /// @param[in] frame Inquiry response frame (4 bytes).
    /// @param[out] outMode Extracted nibble value.
    /// @return True on success, false if format is invalid.
    static bool parseModeInquiry(const ViscaFrame& frame, uint8_t& outMode) noexcept;

    /// @brief Decodes a 2D coordinate spot inquiry response (y0 50 00 0x 00 0y FF).
    /// @param[in] frame Inquiry response frame (7 bytes).
    /// @param[out] outX Extracted X coordinate.
    /// @param[out] outY Extracted Y coordinate.
    /// @return True on success, false if format is invalid.
    static bool parseSpotPosition(const ViscaFrame& frame, uint8_t& outX, uint8_t& outY) noexcept;
};

} // namespace Visca::Sony
