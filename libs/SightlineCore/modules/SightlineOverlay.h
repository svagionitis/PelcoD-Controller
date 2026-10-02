#pragma once

/// @file SightlineOverlay.h
/// @brief Sightline SLA Overlay Module (Reticles, dynamic graphic primitives, KLV overlays, fonts).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-Overlay-Graphics.pdf

#include "../SightlineTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

// ==============================================================================
// 1. Enums and Bitmasks
// ==============================================================================

/// @enum TrackColorMode
/// @brief Primary, secondary, and MTI reticle track color modes (IDD Table 4-bit mode).
enum class TrackColorMode : std::uint8_t {
    Off = 0U,
    White = 1U,
    Black = 2U,
    Auto = 3U,
    Rainbow = 4U,
    Red = 5U,
    Orange = 6U,
    Yellow = 7U,
    Green = 8U,
    Blue = 9U,
    Violet = 10U,
    Cyan = 11U,
    BlackWhite = 12U
};

/// @enum TrackReticleType
/// @brief Tracking reticle shape styles (IDD Table 4-bit type).
enum class TrackReticleType : std::uint8_t {
    BoxCorners = 0U,
    Cross = 1U,
    Circle = 2U,
    DuplexCrosshair = 3U,
    ModernRange = 4U,
    TargetDot = 5U
};

/// @namespace OverlayGraphicsFlags
/// @brief 16-bit bitmask for graphic overlays selection in SLASetOverlayMode_t (0x06).
namespace OverlayGraphicsFlags {
    inline constexpr std::uint16_t None { 0x0000U };
    inline constexpr std::uint16_t TrackingBoxPixelStats { 1U << 2U }; ///< Bit 2: Pixel stats in track boxes
    inline constexpr std::uint16_t Histogram { 1U << 3U }; ///< Bit 3: Histogram display
    inline constexpr std::uint16_t TrackIndex { 1U << 4U }; ///< Bit 4: Track ID numbers
    inline constexpr std::uint16_t TrackMotionTrails { 1U << 5U }; ///< Bit 5: Target motion history trails
    inline constexpr std::uint16_t AutoFocusRoi { 1U << 6U }; ///< Bit 6: Autofocus metric and ROI box
    inline constexpr std::uint16_t RegistrationIgnoreEdges { 1U << 7U }; ///< Bit 7: Scene registration boundary mask
    inline constexpr std::uint16_t MtiTrackIndex { 1U << 8U }; ///< Bit 8: MTI track index badges
    inline constexpr std::uint16_t LogoWatermark { 1U << 9U }; ///< Bit 9: Enable userLogo.png watermark
    inline constexpr std::uint16_t LandingAid { 1U << 10U }; ///< Bit 10: Visual landing aid target overlays
    inline constexpr std::uint16_t LandingKeepOut { 1U << 11U }; ///< Bit 11: Landing keep-out zone boundary
    inline constexpr std::uint16_t UserOverlayObjects { 1U << 12U }; ///< Bit 12: SLADrawOverlay_t user objects
    inline constexpr std::uint16_t CoastingReticleDashed { 1U << 13U }; ///< Bit 13: Draw reticle dashed when coasting
}

/// @namespace ClassifierLabelsFlags
/// @brief Extra labels bitmask for AI Classifier in SLASetOverlayMode_t (0x06).
namespace ClassifierLabelsFlags {
    inline constexpr std::uint8_t None { 0x00U };
    inline constexpr std::uint8_t ClassifierTrackLabels { 1U << 0U }; ///< Bit 0: Track class labels & confidence
    inline constexpr std::uint8_t ClassifierMtiLabels { 1U << 1U }; ///< Bit 1: MTI class labels & confidence
    inline constexpr std::uint8_t TotalObjectsByClass { 1U << 2U }; ///< Bit 2: Detection count by model class
    inline constexpr std::uint8_t MtiLabelsOnly { 1U << 3U }; ///< Bit 3: Show MTI labels without confidence
    inline constexpr std::uint8_t TrackerReticleIndicators { 1U << 4U }; ///< Bit 4: Reticle AI assist / mode badge
    inline constexpr std::uint8_t ShortenLabelNames { 1U << 7U }; ///< Bit 7: Reduce label names to 10 chars
}

/// @enum MtiReticleType
/// @brief Reticle indicator shape for MTI detections.
enum class MtiReticleType : std::uint8_t {
    Ellipse = 0x01U,
    EllipseWithClass = 0x02U,
    InvertedTriangle = 0x04U,
    InvertedTriangleWithClass = 0x08U,
    Rectangle = 0x10U
};

/// @enum MtiLabelPosition
/// @brief Positioning of MTI detection labels to prevent overlap.
enum class MtiLabelPosition : std::uint8_t { Auto = 0U, TopRight = 1U, BottomRight = 2U, BottomLeft = 3U };

/// @enum OverlayObjectType
/// @brief Graphic object primitive types supported by SLADrawOverlay_t (0x9C).
enum class OverlayObjectType : std::uint8_t {
    Circle = 0U,
    Rectangle = 1U,
    Line = 2U,
    FilledCircle = 4U,
    FilledRectangle = 5U,
    TextExtended = 6U,
    KlvField = 8U,
    Cross = 9U,
    Square = 10U,
    FilledSquare = 11U,
    KlvWidget = 12U,
    ErrorNotFound = 255U
};

/// @namespace OverlayActionFlags
/// @brief Action byte definition for SLADrawOverlay_t (0x9C).
namespace OverlayActionFlags {
    inline constexpr std::uint8_t Destroy { 0x00U };
    inline constexpr std::uint8_t Create { 0x01U };
    inline constexpr std::uint8_t AlphaShift { 3U }; ///< Bits 3..7: alpha transparency (0..31)
}

/// @namespace OverlayPropertyFlags
/// @brief Coordinate space, scene steering, and persistence flags for SLADrawOverlay_t (0x9C).
namespace OverlayPropertyFlags {
    inline constexpr std::uint8_t CoordSourceSpace { 1U }; ///< Bits 0..3: Source coordinate space
    inline constexpr std::uint8_t CoordDisplaySceneSteered { 2U }; ///< Bits 0..3: Display space, moves with scene
    inline constexpr std::uint8_t CoordDisplayStatic { 4U }; ///< Bits 0..3: Display space static (default)
    inline constexpr std::uint8_t SaveToFlash { 1U << 6U }; ///< Bit 6: 1 = Save object to flash parameter file
    inline constexpr std::uint8_t OriginUpperLeft { 1U << 7U }; ///< Bit 7: 0 = center origin, 1 = upper-left origin
}

/// @enum OverlayPaletteColor
/// @brief 16-color palette nibble mapping for SLADrawOverlay_t (0x9C).
enum class OverlayPaletteColor : std::uint8_t {
    White = 0U,
    Black = 1U,
    LightGray = 2U,
    Gray = 3U,
    DarkGray = 4U,
    LightBlue = 5U,
    Blue = 6U,
    DarkBlue = 7U,
    LightGreen = 8U,
    Green = 9U,
    DarkGreen = 10U,
    Red = 11U,
    Orange = 12U,
    Yellow = 13U,
    TransparentBgOrTurquoiseFg = 14U,
    Automatic = 15U
};

/// @enum OverlayFontId
/// @brief Standard system font identifiers and UserFont slots (0..31).
enum class OverlayFontId : std::uint8_t {
    Courier = 0U,
    CourierBold = 1U,
    Arial = 5U,
    ArialBold = 6U,
    Verdana = 7U,
    VerdanaBold = 8U,
    Calibri = 9U,
    CalibriBold = 10U,
    UserFont0 = 16U,
    UserFont1 = 17U,
    UserFont2 = 18U,
    UserFont3 = 19U,
    UserFont4 = 20U,
    UserFont5 = 21U,
    UserFont6 = 22U,
    UserFont7 = 23U,
    UserFont8 = 24U,
    UserFont9 = 25U,
    UserFont10 = 26U,
    UserFont11 = 27U,
    UserFont12 = 28U,
    UserFont13 = 29U,
    UserFont14 = 30U,
    UserFont15 = 31U
};

/// @enum KlvFieldTag
/// @brief Telemetry fields for KLV Field graphic objects (SLADrawOverlay_t type 8).
enum class KlvFieldTag : std::uint8_t {
    UtcTime = 0U,
    MissionId = 1U,
    PlatformHeading = 2U,
    PlatformPitch = 3U,
    PlatformRoll = 4U,
    PlatformPitchFull = 5U,
    PlatformRollFull = 6U,
    SensorLatitude = 7U,
    SensorLongitude = 8U,
    SensorAltitude = 9U,
    SensorHfov = 10U,
    SensorVfov = 11U,
    SensorRelativeAzimuth = 12U,
    SensorRelativeElevation = 13U,
    SensorRelativeRoll = 14U,
    FrameCenterLatitude = 15U,
    FrameCenterLongitude = 16U,
    FrameCenterElevation = 17U,
    SlantRange = 19U,
    TargetLocationElevation = 20U,
    TargetLocationLatitude = 21U,
    TargetLocationLongitude = 22U
};

/// @enum KlvFormatType
/// @brief Formatting styles for KLV Field text values.
enum class KlvFormatType : std::uint8_t {
    TimeYmdHms = 0U,
    TimeDayMonthYear = 1U,
    TimeHms = 2U,
    TimeMonthDayYear = 3U,
    TimeMmDdYy = 4U,
    TimeMmDdYyyy = 5U,
    TimeDtg = 6U,
    TimeDtgNoSpaces = 7U,
    AngleDegrees = 0U,
    AngleRadians = 1U,
    AngleMilliRadians = 2U,
    DistanceMeters = 0U,
    DistanceFeet = 1U,
    PositionMgrs10 = 0U,
    PositionDecimalDegrees = 1U,
    PositionDms = 2U,
    PositionMgrs8 = 3U
};

// ==============================================================================
// 2. Protocol Command & Reply Structs
// ==============================================================================

/// @struct MsgSetOverlayMode
/// @brief Controls overlay graphics rendering options (Message ID 0x06 & 0x42).
/// @details Full 21-byte struct layout conforming to SLASetOverlayMode_t.
struct MsgSetOverlayMode {
    std::uint8_t primaryReticle { 0x01U }; ///< Bits 0..3: color (White), Bits 4..7: shape (Corners)
    std::uint8_t secondaryReticle { 0x01U }; ///< Bits 0..3: color (White), Bits 4..7: shape (Corners)
    std::uint16_t graphics { 0x1010U }; ///< Graphic overlay selection mask (OverlayGraphicsFlags)
    std::uint8_t mtiColor { 0x08U }; ///< Detected tracks color (Green)
    std::uint8_t mtiSelectableColor { 0x07U }; ///< Transitionable tracks color (Yellow)
    std::uint8_t cameraIndex { 0U }; ///< Target camera index (0..3, 255 = all)
    std::uint8_t selectedReticle { 0x01U }; ///< Selected track overlay
    std::uint8_t personReticle { 0x0BU }; ///< Person tracking mode (Cyan)
    std::uint8_t cursorReticle { 0x01U }; ///< Cursor reticle color (White)
    std::uint8_t lineThickness { 1U }; ///< Reticle line thickness in pixels
    std::uint8_t fontScale { 32U }; ///< Font scaling (scale / 32, default 32 = 100%)
    std::uint8_t fontId { 0U }; ///< Font ID for internal overlays
    std::uint8_t extraLabels { 0U }; ///< Extra labels bitmask (ClassifierLabelsFlags)
    std::uint8_t modernMode { 1U }; ///< Modern rounded HUD symbology (1 = enabled)
    std::uint8_t mtiReticle { 0x01U }; ///< MTI reticle shape (MtiReticleType)
    std::uint8_t mtiLabelAdv { 0U }; ///< MTI label position (MtiLabelPosition)
    std::uint8_t mtiMultiColor { 0U }; ///< Multi-color detection palette
    std::uint8_t detCounterColor { 0x01U }; ///< Detection counter color
    std::uint8_t mtiDisplayLabelLimit { 255U }; ///< Max detection labels (255 = no limit)
    std::uint8_t detCounterPos { 0U }; ///< Detection counter position
    std::uint8_t mtiLabelDeconflict { 1U }; ///< MTI label deconfliction (1 = enabled)
};

/// @struct MsgDrawOverlay
/// @brief User specified graphic overlay object creation/deletion (Message ID 0x9C).
/// @details Full struct layout conforming to SLADrawOverlay_t.
struct MsgDrawOverlay {
    std::uint8_t cameraIndex { 0U }; ///< Target camera index (0..3, 255 = all)
    std::uint8_t objectId { 1U }; ///< 0 = destroy all, 1..199 = unique object ID
    std::uint8_t action { 0x01U }; ///< Bit 0: Create (1) / Destroy (0); Bits 3..7: Alpha (0..31)
    std::uint8_t propertyFlags { 0x04U }; ///< Bits 0..3: Display static (4); Bit 6: Save; Bit 7: Origin
    OverlayObjectType type { OverlayObjectType::Rectangle }; ///< Object geometry or text type
    std::uint16_t a { 0U }; ///< Type-specific parameter A
    std::uint16_t b { 0U }; ///< Type-specific parameter B
    std::uint16_t c { 0U }; ///< Type-specific parameter C
    std::uint16_t d { 0U }; ///< Type-specific parameter D
    std::uint8_t backgroundColor { 0x0EU }; ///< Foreground [7..4] | Background shadow [3..0]
    std::string text {}; ///< Text string (up to 64 chars)
    std::uint16_t e { 0U }; ///< Optional parameter E (e.g. line thickness)
    std::uint16_t f { 0U }; ///< Optional parameter F (e.g. angle)
    bool hasE { false }; ///< True if parameter E is encoded
    bool hasF { false }; ///< True if parameter F is encoded
};

/// @struct MsgLogoParameters
/// @brief Configures on-screen logo watermark display and alpha blending (Message ID 0x9B).
/// @details Conforms to official Sightline SLALogoParameters_t struct layout.
struct MsgLogoParameters {
    std::uint8_t cameraIndex { 0U }; ///< Target camera index (0..3, 255 = all)
    std::uint8_t logoOpacity { 255U }; ///< 0 = hidden, 255 = fully opaque
    std::uint16_t offsetX { 0U }; ///< Columns offset from lower right corner
    std::uint16_t offsetY { 0U }; ///< Rows offset from lower right corner
};

/// @struct MsgUserFont
/// @brief Assigns TrueType/OpenType font files to font slots UserFont0..UserFont15 (Message ID 0xAE).
/// @details Conforms to official Sightline SLAUserFont_t struct layout.
struct MsgUserFont {
    std::uint8_t userFontIndex { 0U }; ///< Font slot (0..15 -> UserFont0..UserFont15)
    std::string fontFileName {}; ///< TTF/OTF filename in /home/slroot/sl/bin/fonts/
};

/// @struct MsgCurrentOverlayObjectsIds
/// @brief Bitmask report of all active user overlay objects (Message ID 0x68).
/// @details Conforms to official Sightline SLACurrentOverlayObjectsIds_t struct layout.
struct MsgCurrentOverlayObjectsIds {
    std::array<std::uint64_t, 4> idMask {}; ///< 256 bits representing active object IDs

    /// @brief Sets or clears the active flag for a specific object ID.
    /// @param[in] objId Object ID (0..255).
    /// @param[in] active True to mark active, false to clear.
    constexpr void setObjectActive(std::uint8_t objId, bool active = true) noexcept
    {
        const std::size_t wordIdx { static_cast<std::size_t>(objId / 64U) };
        const std::uint64_t bitIdx { 1ULL << (objId % 64U) };
        if (active) {
            idMask[wordIdx] |= bitIdx;
        } else {
            idMask[wordIdx] &= ~bitIdx;
        }
    }

    /// @brief Checks if a specific object ID is currently active.
    /// @param[in] objId Object ID to test (0..255).
    /// @return True if active.
    [[nodiscard]] constexpr bool isObjectActive(std::uint8_t objId) const noexcept
    {
        const std::size_t wordIdx { static_cast<std::size_t>(objId / 64U) };
        const std::uint64_t bitIdx { 1ULL << (objId % 64U) };
        return (idMask[wordIdx] & bitIdx) != 0ULL;
    }

    /// @brief Returns a list of all active user object IDs (1..199).
    /// @return Vector of active object IDs.
    [[nodiscard]] std::vector<std::uint8_t> getActiveObjectIds() const
    {
        std::vector<std::uint8_t> active {};
        for (std::size_t i { 1U }; i < 200U; ++i) {
            if (isObjectActive(static_cast<std::uint8_t>(i))) {
                active.push_back(static_cast<std::uint8_t>(i));
            }
        }
        return active;
    }
};

/// @struct MsgCurrentOverlayObjectParameters
/// @brief Details and geometry parameters of a single graphic overlay object (Message ID 0x6B).
/// @details Conforms to official Sightline SLACurrentOverlayObjectParameters_t struct layout.
struct MsgCurrentOverlayObjectParameters {
    std::uint8_t type { 0U }; ///< Object type (OverlayObjectType)
    std::uint8_t objectId { 0U }; ///< Object ID
    std::uint8_t flags { 0U }; ///< Property flags
    std::uint8_t staticObject { 0U }; ///< 1 if static overlay
    std::uint16_t a { 0U }; ///< Parameter A
    std::uint16_t b { 0U }; ///< Parameter B
    std::uint16_t c { 0U }; ///< Parameter C
    std::uint16_t d { 0U }; ///< Parameter D
    std::uint8_t color { 0U }; ///< Foreground / Background color
    std::string text {}; ///< Text string
};

/// @struct MsgDrawObject
/// @brief Legacy dynamic custom graphics object rendering (Message ID 0x3B).
/// @note Deprecated in Sightline firmware post-2.24 in favor of MsgDrawOverlay (0x9C).
struct MsgDrawObject {
    std::uint8_t objectId { 0U };
    std::uint8_t action { 0x01U };
    std::uint8_t propertyFlags { 0x04U };
    std::uint8_t shapeType { 0U };
    std::uint16_t a { 0U };
    std::uint16_t b { 0U };
    std::uint16_t c { 0U };
    std::uint16_t d { 0U };
    std::uint8_t color { 0x0EU };
    std::string text {};
};

/// @struct MsgAncillaryTextMetadata
/// @brief Injects MISB ST 0808 text metadata into the MPEG-TS KLV stream (Message ID 0xAC).
/// @details Conforms to official Sightline SLAAncillaryTextMetadata_t struct layout.
struct MsgAncillaryTextMetadata {
    std::uint64_t creationTime { 0ULL }; ///< UTC time in microseconds
    std::string source {}; ///< Originator type (e.g. "human", max 8 chars)
    std::string originator {}; ///< Originator ID (max 16 chars)
    std::string messageBody {}; ///< UTF-8 text string
    std::uint16_t displayId { 0x0002U }; ///< Network display ID (e.g. Net0 = 0x0002)
};

} // namespace Sightline
