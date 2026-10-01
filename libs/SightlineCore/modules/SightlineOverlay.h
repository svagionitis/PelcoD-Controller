#pragma once

/// @file SightlineOverlay.h
/// @brief Sightline SLA Overlay Module (Reticles, dynamic graphic primitives, and overlays).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @struct MsgSetOverlayMode
/// @brief Configures overlay graphics rendering options (Message ID 0x06).
struct MsgSetOverlayMode {
    std::uint8_t displayIndex { 0U };
    std::uint8_t reticleMode { 1U }; // 0: None, 1: Crosshair, 2: Box, 3: Custom
    std::uint8_t trackingBoxMode { 1U }; // 0: Off, 1: Bounding Box, 2: Centroid
    std::uint8_t telemetryTextMode { 1U };
};

/// @struct MsgDrawObject
/// @brief Dynamic custom graphics object rendering on video overlay (Message ID 0x3B).
struct MsgDrawObject {
    std::uint8_t displayIndex { 0U };
    std::uint8_t objectId { 0U };
    std::uint8_t shapeType { 0U }; // 0: Line, 1: Rectangle, 2: Circle, 3: Text
    std::uint16_t x { 0U };
    std::uint16_t y { 0U };
    std::uint16_t width { 0U };
    std::uint16_t height { 0U };
    std::uint32_t colorRgba { 0x00FF00FFU }; // Green opaque default
    std::string text {};
};

/// @struct MsgDrawOverlay
/// @brief Advanced multi-primitive overlay graphics update (Message ID 0x9C).
struct MsgDrawOverlay {
    std::uint8_t displayIndex { 0U };
    std::uint8_t clearDisplay { 0U };
    std::vector<MsgDrawObject> objects {};
};

/// @struct MsgLogoParameters
/// @brief Configures on-screen logo watermark display and alpha blending (Message ID 0x9B).
/// @details Conforms to official Sightline SLALogoParameters_t struct layout.
struct MsgLogoParameters {
    std::uint8_t displayIndex { 0U }; ///< Target display channel index (0..3)
    std::uint8_t logoIndex { 0U }; ///< Stored logo image slot index (0..7)
    std::uint8_t enable { 0U }; ///< 0: Disable, 1: Enable
    std::uint16_t x { 0U }; ///< Top-left pixel X coordinate
    std::uint16_t y { 0U }; ///< Top-left pixel Y coordinate
    std::uint8_t opacity { 255U }; ///< Alpha blending opacity (0..255)
    std::uint8_t scale { 1U }; ///< Uniform scaling factor
};

/// @struct MsgAncillaryTextMetadata
/// @brief Injects dynamic tactical textual overlays and subtitles (Message ID 0xAC).
/// @details Conforms to official Sightline SLAAncillaryTextMetadata_t struct layout.
struct MsgAncillaryTextMetadata {
    std::uint8_t displayIndex { 0U }; ///< Display channel index (0..3)
    std::uint8_t lineIndex { 0U }; ///< Text line slot index (0..15)
    std::uint16_t x { 0U }; ///< Target X pixel position
    std::uint16_t y { 0U }; ///< Target Y pixel position
    std::uint8_t fontId { 0U }; ///< Font index (0: Default, 1..3: UserFont)
    std::uint32_t colorRgba { 0x00FF00FFU }; ///< Text RGBA color
    std::string text {}; ///< Dynamic text string
};

/// @struct MsgUserFont
/// @brief Loads custom raster font glyph tables into overlay generator (Message ID 0xAE).
/// @details Conforms to official Sightline SLAUserFont_t struct layout.
struct MsgUserFont {
    std::uint8_t fontId { 0U }; ///< Target font slot (0..3)
    std::uint8_t charWidth { 8U }; ///< Glyph cell width in pixels
    std::uint8_t charHeight { 16U }; ///< Glyph cell height in pixels
    std::uint8_t firstChar { 0x20U }; ///< First ASCII character represented
    std::uint8_t numChars { 96U }; ///< Number of consecutive glyphs
    std::vector<std::uint8_t> glyphData {}; ///< 1-bpp packed raster bitmap data
};

} // namespace Sightline
