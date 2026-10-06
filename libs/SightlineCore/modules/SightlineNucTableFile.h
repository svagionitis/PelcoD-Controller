#pragma once

/// @file SightlineNucTableFile.h
/// @brief Codec for the on-card NUC (.nuc) and dead pixel (.dead) table files.
/// @details Layouts come from Appendix A2 (NUC table) and A3 (dead table) of
///          docs/protocols/Sightline/EAN-NUC-and-DPR.pdf.
///
///          Assumptions (not stated by the EAN, verify against a real card dump):
///          - Fields are packed with no padding.
///          - Multi-byte fields are little-endian, like the SLA serial protocol.
///
///          Only NUC table version 1 is documented, so other NUC versions are rejected.
///          Dead tables of versions 1..3 are accepted.
///
///          @code
///          NUC v1                      Dead v1 / v2 / v3
///          +-------------------+       +-------------------------+
///          | u32 version  (1)  |       | u32 version (1..3)      |
///          | u32 magic         |       | u32 magic               |
///          | s16 height        |       | s32 count               |
///          | s16 width         |       +-------------------------+
///          +-------------------+       | v2+: 28-byte params     |
///          | h*w x {u16 gain,  |       | v3+: s32 replaceMethod  |
///          |        s16 off}   |       |      u8  numReplace     |
///          +-------------------+       +-------------------------+
///                                      | count x {u16 row,       |
///                                      |   u16 col,              |
///                                      |   v1/v2: s8 rowOff,     |
///                                      |          s8 colOff}     |
///                                      +-------------------------+
///          @endcode
///
///          @verbatim
///          ```mermaid
///          flowchart LR
///              F[(file bytes)] -->|parseNuc / parseDead| T[NucTable / DeadTable]
///              T -->|writeNuc / writeDead| F
///              T -->|sortDead| T
///          ```
///          @endverbatim
///
///          The dead list must be strictly ascending by row (Y) then column (X). The codec
///          rejects unsorted or duplicate lists on both parse and write; use sortDead() first.

#include "../SightlineTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Sightline {

/// @brief Magic number shared by NUC and dead table files.
inline constexpr std::uint32_t kNucFileMagic { 0x51ACD00DU };

/// @brief Only documented NUC table version.
inline constexpr std::uint32_t kNucFileVersion { 1U };

/// @brief Newest documented dead table version.
inline constexpr std::uint32_t kDeadFileVersion { 3U };

/// @brief Largest pixel count accepted, checked before allocating (4096 x 4096).
inline constexpr std::size_t kMaxTablePixels { 4096U * 4096U };

/// @struct NucGainOffset
/// @brief Per-pixel NUC coefficients.
struct NucGainOffset {
    std::uint16_t gain { 8192U }; ///< Gain in Q3.13 fixed point (8192 = 1.0)
    std::int16_t offset { 0 }; ///< Offset in sensor counts
};

/// @struct NucTable
/// @brief Decoded NUC table file (EAN A2).
struct NucTable {
    std::uint32_t version { kNucFileVersion }; ///< Table version (must be 1)
    std::int16_t height { 0 }; ///< Image height in pixels (> 0)
    std::int16_t width { 0 }; ///< Image width in pixels (> 0)
    std::vector<NucGainOffset> pixels {}; ///< Row-major, height * width entries
};

/// @struct DeadCalcParams
/// @brief Dead pixel calculation parameters stored in dead tables of version 2 and later.
struct DeadCalcParams {
    float maxGain { 0.0F }; ///< Maximum gain
    float minGain { 0.0F }; ///< Minimum gain
    std::int32_t maxOff { 0 }; ///< Maximum offset
    std::int32_t minOff { 0 }; ///< Minimum offset
    std::uint16_t maxVal { 0U }; ///< Maximum pixel value
    std::uint16_t minVal { 0U }; ///< Minimum pixel value
    std::uint32_t maxStdDev { 0U }; ///< Maximum standard deviation
    std::int32_t maxNumDead { 0 }; ///< Maximum number of dead pixels
};

/// @struct DeadPixelEntry
/// @brief One dead pixel; (0,0) is the top-left pixel.
struct DeadPixelEntry {
    std::uint16_t row { 0U }; ///< Row (Y)
    std::uint16_t col { 0U }; ///< Column (X)
    std::int8_t rowOff { 0 }; ///< Row offset to the replacement pixel (v1/v2 only; 0 for v3)
    std::int8_t colOff { 0 }; ///< Column offset to the replacement pixel (v1/v2 only; 0 for v3)
};

/// @struct DeadTable
/// @brief Decoded dead pixel table file (EAN A3).
struct DeadTable {
    std::uint32_t version { kDeadFileVersion }; ///< Table version (1..3)
    DeadCalcParams params {}; ///< Calculation parameters (v2+; zero for v1)
    std::int32_t replaceMethod { 0 }; ///< 0x35 deadReplace: 0..2 (v3+)
    std::uint8_t numReplace { 5U }; ///< Neighbours for average/median, 1..8 (v3+)
    std::vector<DeadPixelEntry> pixels {}; ///< Strictly ascending by row, then column
};

/// @enum TableFileError
/// @brief Result of parsing or writing a table file.
enum class TableFileError : std::uint8_t {
    Ok = 0U, ///< Success
    Truncated = 1U, ///< Input shorter than the header or declared contents
    BadMagic = 2U, ///< Magic number is not 0x51ACD00D
    BadVersion = 3U, ///< Unsupported table version
    BadSize = 4U, ///< Non-positive dimensions, negative count, or pixel count mismatch
    TrailingData = 5U, ///< Bytes left over after the declared contents
    NotSorted = 6U, ///< Dead list is not ascending by row then column
    Duplicate = 7U, ///< Dead list contains the same pixel twice
    BadValue = 8U, ///< Field outside its permitted range
    TooLarge = 9U ///< Pixel count exceeds kMaxTablePixels
};

/// @class SightlineNucTableFile
/// @brief Stateless parser / writer for NUC and dead table files.
class SightlineNucTableFile {
public:
    /// @brief Decodes a NUC table file.
    /// @details Validates magic, version, dimensions and exact size before allocating.
    ///          @p out is only modified on success.
    /// @param[in] data File contents.
    /// @param[out] out Decoded table.
    /// @return TableFileError::Ok on success, otherwise the first problem found.
    [[nodiscard]] static TableFileError parseNuc(ByteView data, NucTable& out);

    /// @brief Encodes a NUC table file.
    /// @details @p out is only modified on success.
    /// @param[in] table Table to encode; version must be 1 and pixels.size() == height * width.
    /// @param[out] out File contents.
    /// @return TableFileError::Ok on success, otherwise the first problem found.
    [[nodiscard]] static TableFileError writeNuc(const NucTable& table, std::vector<std::uint8_t>& out);

    /// @brief Decodes a dead pixel table file (versions 1..3).
    /// @details Validates magic, version, count, exact size, v3 replacement settings and list
    ///          order before returning. @p out is only modified on success.
    /// @param[in] data File contents.
    /// @param[out] out Decoded table.
    /// @return TableFileError::Ok on success, otherwise the first problem found.
    [[nodiscard]] static TableFileError parseDead(ByteView data, DeadTable& out);

    /// @brief Encodes a dead pixel table file in the layout of @p table.version.
    /// @details For v3, replacement offsets must be zero. @p out is only modified on success.
    /// @param[in] table Table to encode.
    /// @param[out] out File contents.
    /// @return TableFileError::Ok on success, otherwise the first problem found.
    [[nodiscard]] static TableFileError writeDead(const DeadTable& table, std::vector<std::uint8_t>& out);

    /// @brief Tests whether a dead list is strictly ascending by row, then column.
    /// @param[in] pixels Dead list.
    /// @return True if sorted with no duplicates.
    [[nodiscard]] static bool isSorted(const std::vector<DeadPixelEntry>& pixels) noexcept;

    /// @brief Sorts a dead list by row, then column, and removes duplicate coordinates.
    /// @details When duplicates differ in replacement offsets the first one after sorting
    ///          (stable order) is kept.
    /// @param[in,out] pixels Dead list.
    /// @return Number of duplicates removed.
    [[nodiscard]] static std::size_t sortDead(std::vector<DeadPixelEntry>& pixels);

    /// @brief Converts a Q3.13 gain to a real value.
    /// @param[in] raw Fixed-point gain.
    /// @return raw / 8192.
    [[nodiscard]] static double gainToReal(std::uint16_t raw) noexcept;

    /// @brief Converts a real gain to Q3.13, rounding to nearest.
    /// @param[in] gain Real gain; must be in [0, 8).
    /// @param[out] raw Fixed-point gain; unchanged on failure.
    /// @return True on success, false if @p gain is out of range or not finite.
    [[nodiscard]] static bool gainFromReal(double gain, std::uint16_t& raw) noexcept;

    /// @brief Returns a short human-readable description of a result.
    /// @param[in] err Result code.
    /// @return Static, null-terminated string; never null.
    [[nodiscard]] static const char* errorText(TableFileError err) noexcept;
};

} // namespace Sightline
