#pragma once

/// @file libs/Klv/Misb1201.h
/// @brief MISB ST 1201.5 Floating Point to Integer Mapping implementation.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @enum Misb1201SpecialValue
/// @brief Special indicator values defined by MISB ST 1201.
enum class Misb1201SpecialValue : std::uint8_t {
    Valid = 0U,
    UserDefined,
    BelowMinimum,
    AboveMaximum,
    PositiveInfinity,
    NegativeInfinity,
    PositiveQuietNan,
    NegativeQuietNan,
    PositiveSignalingNan,
    NegativeSignalingNan,
    Reserved
};

/// @struct Misb1201Result
/// @brief Decoded floating-point result with special value indicator.
struct Misb1201Result {
    double value { 0.0 };
    Misb1201SpecialValue status { Misb1201SpecialValue::Valid };

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return status == Misb1201SpecialValue::Valid;
    }
};

/// @struct Misb1201Params
/// @brief Precomputed scaling and offset parameters for MISB ST 1201 mapping.
struct Misb1201Params {
    double minVal { 0.0 };
    double maxVal { 0.0 };
    std::size_t lengthBytes { 0U };
    double forwardScale { 1.0 };
    double reverseScale { 1.0 };
    double zeroOffset { 0.0 };
    double actualPrecision { 0.0 };
};

/// @class Misb1201
/// @brief Implements MISB ST 1201.5 Floating Point to Integer Mapping.
/// @details Supports Starting Point A (IMAPA) and Starting Point B (IMAPB)
///          as well as reverse mapping (RIMAPB) and IEEE-754 special values.
class Misb1201 {
public:
    /// @brief Computes recommended field length in bytes (IMAPA).
    /// @details Evaluates Lbits = ceil(log2(b - a)) - floor(log2(g)) + 1 and L = ceil(Lbits / 8).
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @param[in] precision Desired float precision (g).
    /// @return Recommended field length in bytes (1 to 8).
    [[nodiscard]] static std::size_t computeLength(double minVal, double maxVal, double precision) noexcept;

    /// @brief Computes mapping parameters (IMAPB constants).
    /// @details Precalculates forward scaling factor sF, reverse scaling factor sR,
    ///          and zero-point offset Zoffset as defined in MISB ST 1201 Section 7.1.2.
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @param[in] lengthBytes Field length in bytes (1 to 8).
    /// @return Precomputed parameters structure.
    [[nodiscard]] static Misb1201Params computeParams(double minVal, double maxVal, std::size_t lengthBytes) noexcept;

    /// @brief Forward maps a floating-point value to integer (IMAPB).
    /// @param[in] val Value to map.
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @param[in] lengthBytes Target field length in bytes (1 to 8).
    /// @return Unsigned integer bit representation.
    [[nodiscard]] static std::uint64_t encode(double val, double minVal, double maxVal, std::size_t lengthBytes) noexcept;

    /// @brief Forward maps using precomputed parameters.
    /// @param[in] val Value to map.
    /// @param[in] params Precomputed mapping parameters.
    /// @return Unsigned integer bit representation.
    [[nodiscard]] static std::uint64_t encode(double val, const Misb1201Params& params) noexcept;

    /// @brief Forward maps and appends big-endian bytes to buffer.
    /// @param[in] val Value to map.
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @param[in] lengthBytes Target field length in bytes (1 to 8).
    /// @param[out] out Destination byte vector.
    static void encodeBytes(double val, double minVal, double maxVal, std::size_t lengthBytes, std::vector<std::uint8_t>& out);

    /// @brief Reverse maps an integer value to floating-point (RIMAPB).
    /// @param[in] rawInt Unsigned integer value.
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @param[in] lengthBytes Field length in bytes (1 to 8).
    /// @return Decoded floating-point result with status indicator.
    [[nodiscard]] static Misb1201Result decode(std::uint64_t rawInt, double minVal, double maxVal, std::size_t lengthBytes) noexcept;

    /// @brief Reverse maps using precomputed parameters.
    /// @param[in] rawInt Unsigned integer value.
    /// @param[in] params Precomputed mapping parameters.
    /// @return Decoded floating-point result with status indicator.
    [[nodiscard]] static Misb1201Result decode(std::uint64_t rawInt, const Misb1201Params& params) noexcept;

    /// @brief Reverse maps from big-endian raw bytes.
    /// @param[in] data Pointer to raw big-endian byte array.
    /// @param[in] lengthBytes Length of byte array (1 to 8).
    /// @param[in] minVal Minimum float value (a).
    /// @param[in] maxVal Maximum float value (b).
    /// @return Decoded floating-point result with status indicator.
    [[nodiscard]] static Misb1201Result decodeBytes(const std::uint8_t* data, std::size_t lengthBytes, double minVal, double maxVal) noexcept;
};

} // namespace Klv
