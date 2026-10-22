#pragma once

/// @file JpegGenerator.h
/// @brief Standards-compliant lightweight JFIF baseline JPEG image generator.
/// @details Produces valid baseline DCT JPEG images (ISO/IEC 10918-1 / ITU-T T.81)
///          for ONVIF Profile S snapshot delivery without external image dependencies.

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

namespace Onvif {

/// @class JpegGenerator
/// @brief Zero-dependency JFIF JPEG encoder and synthetic test card generator.
/// @details Encodes 24-bit RGB packed pixel buffers into baseline JFIF JPEG streams
///          with standard Huffman and quantization tables. Provides synthetic test card
///          frames with timestamp overlays for ONVIF snapshot delivery.
class JpegGenerator {
public:
    /// @brief Encodes a 24-bit RGB pixel buffer into a valid JFIF baseline JPEG byte stream.
    /// @details Encodes using 4:4:4 baseline DCT with standard Annex K tables and byte stuffing.
    /// @param[in] rgbData Pointer to raw 24-bit RGB packed pixel bytes (size >= width * height * 3).
    /// @param[in] width Image width in pixels (> 0).
    /// @param[in] height Image height in pixels (> 0).
    /// @param[in] quality Compression quality factor between 1 and 100 (default: 80).
    /// @return Byte vector containing standard JFIF JPEG binary payload (SOI to EOI).
    [[nodiscard]] static std::vector<std::uint8_t> encodeRgb(
        const std::uint8_t* rgbData, int width, int height, int quality = 80);

    /// @brief Generates a synthetic test card JPEG frame with timestamp overlay.
    /// @details Draws SMPTE-style color bars and a status banner with dynamic UTC timestamp.
    /// @param[in] width Image width in pixels (e.g. 640 or 1920).
    /// @param[in] height Image height in pixels (e.g. 480 or 1080).
    /// @param[in] timestamp Optional UTC timestamp string. If empty, current UTC time is formatted.
    /// @param[in] channelName Optional channel or stream label (e.g. "Profile S Snapshot").
    /// @return Byte vector containing valid JPEG image bytes.
    [[nodiscard]] static std::vector<std::uint8_t> createTestFrame(int width = 640, int height = 480,
        const std::string& timestamp = "", const std::string& channelName = "Profile S Snapshot");

    /// @brief Validates whether a byte buffer represents a valid JFIF JPEG stream.
    /// @details Checks for SOI (0xFFD8), JFIF APP0 (0xFFE0), and EOI (0xFFD9) markers.
    /// @param[in] data Byte buffer to inspect.
    /// @return True if SOI, JFIF APP0, and EOI are present and correctly formatted.
    [[nodiscard]] static bool isValidJpeg(const std::vector<std::uint8_t>& data);

    /// @brief Extracts image width and height from a JPEG byte stream.
    /// @details Parses SOF0/SOF2 baseline frame markers to determine image dimensions.
    /// @param[in] data Byte buffer containing JPEG stream.
    /// @param[out] width Extracted width in pixels.
    /// @param[out] height Extracted height in pixels.
    /// @return True if SOF marker was located and dimensions parsed successfully.
    [[nodiscard]] static bool getDimensions(const std::vector<std::uint8_t>& data, int& width, int& height);
};

} // namespace Onvif
