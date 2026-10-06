#pragma once

/// @file SightlineNuc.h
/// @brief Sightline SLA NUC Module (Non-Uniformity Correction and Dead Pixel Replacement).
/// @details Message layouts follow the Sightline IDD v3.11 (SLANucParameters_t 0x35,
///          SLAReadWriteNuc_t 0x36, SLADeadPixelStats_t 0xA1, SLADeadPixel_t 0xA8).
///          Operational guidance: docs/protocols/Sightline/EAN-NUC-and-DPR.pdf.
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__nuc.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

// =============================================================================
// NucParameters (0x35)
// =============================================================================

/// @enum NucShow
/// @brief Video display mode for NUC/DPR (0x35 byte 5, `nucShow`).
enum class NucShow : std::uint8_t {
    Uncorrected = 0U, ///< Show uncorrected captured video
    NucAndDpr = 1U, ///< Apply both NUC and DPR to video
    NucOnly = 2U, ///< Apply only NUC to video
    DprOnly = 3U, ///< Apply only DPR to video
    GainImage = 4U, ///< Display gain calculations as an image
    OffsetImage = 5U, ///< Display offset calculations as an image
    DeadImage = 6U ///< Display dead pixels as an image
};

/// @enum NucRunMode
/// @brief Calibration action to perform (0x35 byte 6, `nucRunMode`).
enum class NucRunMode : std::uint8_t {
    None = 0U, ///< Do nothing
    AddFrames = 1U, ///< Add `numFrames` frames to the calibration buffer
    ClearFrames = 2U, ///< Clear frames that have already been added
    ResetGain = 3U, ///< Reset gain calculations
    ResetOffset = 4U, ///< Reset offset calculations
    ResetDead = 5U, ///< Reset dead pixel calculation
    ResetAll = 6U, ///< Reset gain, offset, and dead pixel calculations
    Calc1Point = 7U, ///< Calculate 1-point (offset) correction
    Calc2Point = 8U, ///< Calculate 2-point correction (`numFrames` must be 0)
    CalcDead = 9U, ///< Calculate dead pixels and replacement pixels
    CalcReplace = 10U, ///< Calculate only replacement pixels for DPR
    AutoDead = 11U, ///< Automatic dead pixel detection on the current frame
    ShutterFlatten = 12U, ///< Run shutter flattening
    Noise3DStats = 13U ///< Calculate 3D noise statistics (reply: MsgNoise3D, 0xAF)
};

/// @enum DeadReplace
/// @brief Dead pixel replacement method (0x35 byte 32, `deadReplace`).
enum class DeadReplace : std::uint8_t {
    Nearest = 0U, ///< Nearest neighbour (firmware default)
    Average = 1U, ///< Average of `numReplace` nearest neighbours
    Median = 2U ///< Median of `numReplace` nearest neighbours
};

/// @enum DeadFilter
/// @brief Per-frame dynamic dead pixel filter (0x35 byte 34, `deadFilter`).
enum class DeadFilter : std::uint8_t {
    None = 0U, ///< No filtering
    MinMax = 1U, ///< Replace pixels beyond threshold above max / below min of 3x3 neighbours
    NearFar = 2U, ///< Replace 3x3 min/max pixels whose near values exceed far values by threshold
    Ignore = 255U ///< Ignore this parameter and `deadFilterThresh` (keep current setting)
};

/// @struct MsgNucParameters
/// @brief Set NUC and DPR parameters / trigger calibration actions (Message ID 0x35).
/// @details Payload layout (offsets relative to payload start; IDD offset = value + 4):
///          0 cameraIndex, 1 nucShow, 2 nucRunMode, 3 numFrames, 4 minDeadGain(u16),
///          6 maxDeadGain(u16), 8 minDeadVal(u16), 10 maxDeadVal(u16), 12 minDeadOff(s32),
///          16 maxDeadOff(s32), 20 maxStdDevDead(u32), 24 maxNumDead(s32), 28 deadReplace,
///          29 numReplace, 30 deadFilter, 31 deadFilterThresh(s16), 33 destripeAmount,
///          34 destripeSections, 35 nucName(SVPLenString_t).
///          Defaults are state-preserving: video keeps NUC+DPR applied, no calibration action runs,
///          and the dynamic dead filter is left unchanged (DeadFilter::Ignore).
struct MsgNucParameters {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    NucShow nucShow { NucShow::NucAndDpr }; ///< Display mode
    NucRunMode nucRunMode { NucRunMode::None }; ///< Calibration action
    std::uint8_t numFrames { 0U }; ///< Frames to add (0..255); 0 when unused
    std::uint16_t minDeadGain { 0U }; ///< Min gain limit for DPR, percent (0..999); CalcDead only
    std::uint16_t maxDeadGain { 0U }; ///< Max gain limit for DPR, percent (0..999); CalcDead only
    std::uint16_t minDeadVal { 0U }; ///< Min input pixel value for DPR (0..65535); CalcDead only
    std::uint16_t maxDeadVal { 0U }; ///< Max input pixel value for DPR (0..65535); CalcDead only
    std::int32_t minDeadOff { 0 }; ///< Min offset limit for DPR (-999999..999999); CalcDead only
    std::int32_t maxDeadOff { 0 }; ///< Max offset limit for DPR (-999999..999999); CalcDead only
    std::uint32_t maxStdDevDead { 0U }; ///< Max std deviation for DPR (firmware clamps to 65535)
    std::int32_t maxNumDead { 0 }; ///< Max number of dead pixels allowed; CalcDead only
    DeadReplace deadReplace { DeadReplace::Nearest }; ///< Replacement method
    std::uint8_t numReplace { 5U }; ///< Neighbours for average/median (1..8, default 5)
    DeadFilter deadFilter { DeadFilter::Ignore }; ///< Dynamic dead filter type
    std::int16_t deadFilterThresh { 64 }; ///< Dynamic dead filter threshold (0..255, default 64)
    std::uint8_t destripeAmount { 0U }; ///< Destripe strength (0 = none .. 255 = aggressive)
    std::uint8_t destripeSections { 1U }; ///< Vertical sections for destripe (default 1)
    std::string nucName {}; ///< Multi-NUC target table name (< 64 chars, no extension)
};

// =============================================================================
// ReadWriteNuc (0x36)
// =============================================================================

/// @enum NucFileOp
/// @brief File operation, low nibble (bits 0-3) of `nucReadWriteMode` (0x36 byte 6).
enum class NucFileOp : std::uint8_t {
    None = 0U, ///< Do nothing
    SaveNuc = 1U, ///< Save NUC table `<fileName>.nuc`
    LoadNuc = 2U, ///< Load NUC table
    SaveDead = 3U, ///< Save dead table `<fileName>.dead`
    LoadDead = 4U, ///< Load dead table
    LoadInterpolated = 5U, ///< Load NUC interpolated between fileName and secondaryFileName
    LoadShutterFlatten = 6U, ///< Load NUC with shutter flatten applied from secondary table
    SaveShutterFlatten = 7U ///< Save shutter flatten difference as a NUC table
};

/// @enum NucDefaultOp
/// @brief Startup default operation, high nibble (bits 4-7) of `nucReadWriteMode` (0x36 byte 6).
enum class NucDefaultOp : std::uint8_t {
    None = 0U, ///< Do nothing
    SetNuc = 1U, ///< Set NUC table to be loaded on startup
    SetDead = 2U, ///< Set dead table to be loaded on startup
    ClearNuc = 3U, ///< Clear startup NUC table (fileName ignored, may be blank)
    ClearDead = 4U ///< Clear startup dead table (fileName ignored, may be blank)
};

/// @enum NucTableQuery
/// @brief Table selector for GetParameters(0x36) `payload0` (IDD footnote 11).
enum class NucTableQuery : std::uint8_t {
    None = 0U, ///< No table
    NucTable = 1U, ///< Currently loaded NUC table
    DeadTable = 2U, ///< Currently loaded dead table
    DefaultNuc = 3U, ///< Startup default NUC table
    DefaultDead = 4U ///< Startup default dead table
};

/// @struct MsgReadWriteNuc
/// @brief Save/load NUC and dead tables on microSD and manage startup defaults (Message ID 0x36).
/// @details Payload: 0 cameraIndex, 1 reserved(0), 2 nucReadWriteMode = (defaultOp << 4) | fileOp,
///          3 fileName(SVPLenString_t), then secondaryFileName(SVPLenString_t), interpolationRatio(u8).
///          The firmware appends `.nuc` / `.dead`; a blank fileName is only accepted for 0x30 / 0x40.
struct MsgReadWriteNuc {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    NucFileOp fileOp { NucFileOp::None }; ///< File operation (low nibble)
    NucDefaultOp defaultOp { NucDefaultOp::None }; ///< Startup default operation (high nibble)
    std::string fileName {}; ///< Primary table filename (extension optional)
    std::string secondaryFileName {}; ///< Secondary table for interpolation / shutter flatten
    std::uint8_t interpolationRatio { 0U }; ///< 0 = fully primary .. 255 = fully secondary
};

// =============================================================================
// DeadPixel (0xA8)
// =============================================================================

/// @enum DeadPixelMode
/// @brief Dead pixel list operation (0xA8 byte 5, `mode`).
enum class DeadPixelMode : std::uint8_t {
    Add = 0U, ///< Add pixel (a = column x, b = row y, c = defer map update)
    Remove = 1U, ///< Remove pixel (a = column x, b = row y, c = defer map update)
    DynamicDetect = 2U ///< Dynamic detection (a = kernel size, b = max pixel difference)
};

/// @struct MsgDeadPixel
/// @brief Manually modify the dead pixel list (Message ID 0xA8). Not queryable via GetParameters.
/// @details Payload: 0 cameraIndex, 1 mode, 2 a(u16), 4 b(u16), 6 c(u8), 7 d(u8).
///          `c` = 1 defers the dead-map update while batching; send the last pixel with `c` = 0.
///          Only remove pixels that were added manually (EAN-NUC-and-DPR section 5.3).
///          Prefer the SightlineNucBuilder convenience builders over filling a/b/c/d by hand.
struct MsgDeadPixel {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    DeadPixelMode mode { DeadPixelMode::Add }; ///< Operation (never defaults to Remove)
    std::uint16_t a { 0U }; ///< Column (x) for Add/Remove, kernel size for DynamicDetect
    std::uint16_t b { 0U }; ///< Row (y) for Add/Remove, max pixel difference for DynamicDetect
    std::uint8_t c { 0U }; ///< 0 = update dead map now, 1 = more pixels follow (Add/Remove)
    std::uint8_t d { 0U }; ///< Reserved, 0
};

/// @struct MsgUserPalette
/// @brief Ingests or queries custom pseudo-color lookup table (LUT) for thermal sensors (Message ID 0x72 / 0x73).
struct MsgUserPalette {
    std::uint8_t paletteIndex { 0U }; ///< Palette slot index (0..3)
    std::vector<std::uint8_t> lutData {}; ///< Raw RGB or YUV palette table data
};

// =============================================================================
// DeadPixelStats (0xA1)
// =============================================================================

/// @struct MsgDeadPixelStats
/// @brief Statistics from the dead pixel replacement calibration (Message ID 0xA1).
/// @details Payload (33 bytes): 0 cameraIndex, 1 nDead(s32), 5 nGainLo, 9 nGainHi, 13 nAvgLo,
///          17 nAvgHi, 21 nOffLo, 25 nOffHi, 29 nDevHi (all u32).
struct MsgDeadPixelStats {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    std::int32_t nDead { 0 }; ///< Number of dead pixels detected
    std::uint32_t nGainLo { 0U }; ///< Pixels with gain below minimum dead pixel gain
    std::uint32_t nGainHi { 0U }; ///< Pixels with gain above maximum dead pixel gain
    std::uint32_t nAvgLo { 0U }; ///< Pixels with average below minimum input value
    std::uint32_t nAvgHi { 0U }; ///< Pixels with average above maximum input value
    std::uint32_t nOffLo { 0U }; ///< Pixels with offset below minimum dead pixel offset
    std::uint32_t nOffHi { 0U }; ///< Pixels with offset above maximum dead pixel offset
    std::uint32_t nDevHi { 0U }; ///< Pixels with std deviation above maximum
};

/// @struct MsgCameraCalibration
/// @brief Geometric intrinsic pinhole camera calibration parameters (Message ID 0xC0).
struct MsgCameraCalibration {
    std::uint8_t cameraIndex { 0U };
    float focalLengthX { 0.0F }; ///< fx in pixels
    float focalLengthY { 0.0F }; ///< fy in pixels
    float principalPointX { 0.0F }; ///< cx in pixels
    float principalPointY { 0.0F }; ///< cy in pixels
    float radialDistortionK1 { 0.0F }; ///< k1 coefficient
    float radialDistortionK2 { 0.0F }; ///< k2 coefficient
    float tangentialP1 { 0.0F }; ///< p1 coefficient
    float tangentialP2 { 0.0F }; ///< p2 coefficient
};

/// @struct MsgCameraParameterFile
/// @brief Loads or saves sensor parameter calibration file on local storage (Message ID 0xC2).
struct MsgCameraParameterFile {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t action { 0U }; ///< 0: Load file, 1: Save file, 2: Reset to default
    std::string filename {}; ///< Target parameter configuration filename
};

} // namespace Sightline
