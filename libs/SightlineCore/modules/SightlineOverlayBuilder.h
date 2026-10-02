#pragma once

/// @file SightlineOverlayBuilder.h
/// @brief Serializer for Sightline overlay modes, reticles, and dynamic graphic objects (IDD Overlays module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineOverlayBuilder
/// @brief Encodes video overlay modes, reticle crosshairs, dynamic graphic objects, and batch overlays.
class SightlineOverlayBuilder {
public:
    /// @brief Encodes overlay display mode configuration (Message ID 0x06).
    /// @param[in] msg Overlay mode settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOverlayMode(const MsgSetOverlayMode& msg);

    /// @brief Encodes query for active overlay mode (Message ID 0x07 / 0x28).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetOverlayMode(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes user specified graphic overlay object creation/deletion (Message ID 0x9C).
    /// @param[in] msg Draw overlay parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawOverlay(const MsgDrawOverlay& msg);

    /// @brief Encodes sequential batch of graphic overlay objects into concatenated packets (Message ID 0x9C).
    /// @param[in] objects Collection of graphic objects to create/modify.
    /// @return Concatenated framed binary packets.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawOverlayBatch(const std::vector<MsgDrawOverlay>& objects);

    /// @brief Factory helper to create a Cross overlay graphic primitive (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object ID (1..199).
    /// @param[in] centerX Center X coordinate in pixels.
    /// @param[in] centerY Center Y coordinate in pixels.
    /// @param[in] size Cross arm length in pixels.
    /// @param[in] fgColor Foreground palette color.
    /// @param[in] thickness Line thickness in pixels.
    /// @param[in] originUpperLeft True if coordinate system origin is upper-left.
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeCrossOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
        std::int16_t centerX, std::int16_t centerY, std::uint16_t size,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White, std::uint16_t thickness = 1U,
        bool originUpperLeft = false);

    /// @brief Factory helper to create an outlined or filled Rectangle overlay (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object ID (1..199).
    /// @param[in] x Top-left X coordinate in pixels.
    /// @param[in] y Top-left Y coordinate in pixels.
    /// @param[in] width Rectangle width in pixels.
    /// @param[in] height Rectangle height in pixels.
    /// @param[in] filled True for solid filled rectangle, false for outline.
    /// @param[in] fgColor Foreground/border palette color.
    /// @param[in] bgColor Background/fill palette color.
    /// @param[in] alpha Transparency level (0 = opaque, 1..31 = translucent).
    /// @param[in] thickness Border thickness in pixels (when not filled).
    /// @param[in] originUpperLeft True if coordinate system origin is upper-left.
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeRectangleOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
        std::int16_t x, std::int16_t y, std::uint16_t width, std::uint16_t height, bool filled = false,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        OverlayPaletteColor bgColor = OverlayPaletteColor::TransparentBgOrTurquoiseFg, std::uint8_t alpha = 0U,
        std::uint16_t thickness = 1U, bool originUpperLeft = true);

    /// @brief Factory helper to create a dynamic or static Text overlay primitive (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object ID (1..199).
    /// @param[in] x Starting X coordinate in pixels.
    /// @param[in] y Starting Y coordinate in pixels.
    /// @param[in] text ASCII/UTF-8 string content (up to 64 bytes).
    /// @param[in] fontId System font or user font slot.
    /// @param[in] fgColor Text font color.
    /// @param[in] bgColor Text background/outline color.
    /// @param[in] hScale Horizontal scale (32 = 100%).
    /// @param[in] vScale Vertical scale (32 = 100%).
    /// @param[in] originUpperLeft True if coordinate system origin is upper-left.
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeTextOverlay(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x,
        std::int16_t y, const std::string& text, OverlayFontId fontId = OverlayFontId::Courier,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White,
        OverlayPaletteColor bgColor = OverlayPaletteColor::TransparentBgOrTurquoiseFg, std::uint8_t hScale = 32U,
        std::uint8_t vScale = 32U, bool originUpperLeft = true);

    /// @brief Factory helper to create a live KLV telemetry field badge overlay (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object ID (1..199).
    /// @param[in] x Starting X coordinate in pixels.
    /// @param[in] y Starting Y coordinate in pixels.
    /// @param[in] fieldTag Telemetry field tag to bind.
    /// @param[in] formatType Formatting style for the telemetry value.
    /// @param[in] formatString C-style format template string (e.g. "%s" or "Slant: %f m").
    /// @param[in] fontId Font slot.
    /// @param[in] fgColor Text color.
    /// @param[in] originUpperLeft True if coordinate system origin is upper-left.
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeKlvFieldOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
        std::int16_t x, std::int16_t y, KlvFieldTag fieldTag, KlvFormatType formatType,
        const std::string& formatString = "%s", OverlayFontId fontId = OverlayFontId::Courier,
        OverlayPaletteColor fgColor = OverlayPaletteColor::White, bool originUpperLeft = true);

    /// @brief Factory helper to create an opaque blackout rectangle (Cooler Countdown EAN Sec 10).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Unique object ID (1..199).
    /// @param[in] width Display width in pixels (e.g. 640).
    /// @param[in] height Display height in pixels (e.g. 480).
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeBlackoutOverlay(
        std::uint8_t cameraIndex, std::uint8_t objectId, std::uint16_t width = 640U, std::uint16_t height = 480U);

    /// @brief Factory helper to destroy a specific overlay object or all objects (Message ID 0x9C).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] objectId Object ID to delete (0 to destroy all user objects).
    /// @return Configured MsgDrawOverlay struct.
    [[nodiscard]] static MsgDrawOverlay makeDestroyOverlay(std::uint8_t cameraIndex, std::uint8_t objectId = 0U);

    /// @brief Encodes legacy custom graphic object command (Message ID 0x3B).
    /// @param[in] msg Graphic object parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawObject(const MsgDrawObject& msg);

    /// @brief Encodes logo watermark configuration (Message ID 0x9B).
    /// @param[in] msg Logo display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetLogoParameters(const MsgLogoParameters& msg);

    /// @brief Encodes query for logo watermark configuration (Message ID 0x28 query 0x9B).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLogoParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes TrueType font assignment command (Message ID 0xAE).
    /// @param[in] msg User font parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildUserFont(const MsgUserFont& msg);

    /// @brief Encodes query for list of all active user overlay objects (Message ID 0x28 query 0x68).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetOverlayObjectsIds(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for parameters of an overlay object by ID (Message ID 0x28 query 0x6B).
    /// @param[in] objectId Target object ID (1..199).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetOverlayObjectParams(std::uint8_t objectId);

    /// @brief Encodes dynamic ancillary text metadata insertion into KLV stream (Message ID 0xAC).
    /// @param[in] msg Dynamic text overlay settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildAncillaryTextMetadata(const MsgAncillaryTextMetadata& msg);
};

} // namespace Sightline
