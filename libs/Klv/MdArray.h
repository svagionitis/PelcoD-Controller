#pragma once

/// @file MdArray.h
/// @brief MISB ST 1303 Multi-Dimensional Array Pack (MDARRAY) encoder and decoder.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @enum MdArrayApa
/// @brief MISB ST 1303.2 Table 3 Array Processing Algorithm (APA) identifiers.
enum class MdArrayApa : std::uint8_t {
    Unused = 0x00U,
    NaturalFormat = 0x01U,
    ST1201 = 0x02U,
    BooleanArray = 0x03U,
    UnsignedInteger = 0x04U,
    RunLengthEncoding = 0x05U
};

/// @class MdArray
/// @brief Multi-Dimensional Array Pack encoder and decoder conforming to MISB ST 1303.2.
class MdArray {
public:
    /// @brief Encodes a 2D floating-point matrix using ST 1201 IMAPB.
    /// @param[in] matrix 2D vector [rows][cols] of floating-point values.
    /// @param[in] minVal Minimum range boundary (a parameter in IMAPB).
    /// @param[in] maxVal Maximum range boundary (b parameter in IMAPB).
    /// @param[in] ebytes Number of bytes per encoded element (1..8).
    /// @param[out] out Destination byte vector to append encoded MDARRAY pack.
    /// @return True on success, false if matrix is empty or dimensions are inconsistent.
    [[nodiscard]] static bool encodeFloat2D(const std::vector<std::vector<double>>& matrix,
                                            double minVal,
                                            double maxVal,
                                            std::size_t ebytes,
                                            std::vector<std::uint8_t>& out);

    /// @brief Encodes a 1D floating-point vector using ST 1201 IMAPB.
    /// @param[in] vec 1D vector of floating-point values.
    /// @param[in] minVal Minimum range boundary.
    /// @param[in] maxVal Maximum range boundary.
    /// @param[in] ebytes Number of bytes per encoded element (1..8).
    /// @param[out] out Destination byte vector to append encoded MDARRAY pack.
    /// @return True on success, false if vector is empty.
    [[nodiscard]] static bool encodeFloat1D(const std::vector<double>& vec,
                                            double minVal,
                                            double maxVal,
                                            std::size_t ebytes,
                                            std::vector<std::uint8_t>& out);

    /// @brief Encodes a 2D unsigned integer matrix using NaturalFormat (big-endian).
    /// @param[in] matrix 2D vector [rows][cols] of unsigned integer values.
    /// @param[in] ebytes Number of bytes per integer element (1, 2, or 4).
    /// @param[out] out Destination byte vector to append encoded MDARRAY pack.
    /// @return True on success, false if matrix is empty or jagged.
    [[nodiscard]] static bool encodeUInt2D(const std::vector<std::vector<std::uint32_t>>& matrix,
                                           std::size_t ebytes,
                                           std::vector<std::uint8_t>& out);

    /// @brief Encodes a 1D unsigned integer vector using NaturalFormat (big-endian).
    /// @param[in] vec 1D vector of unsigned integer values.
    /// @param[in] ebytes Number of bytes per integer element (1, 2, or 4).
    /// @param[out] out Destination byte vector to append encoded MDARRAY pack.
    /// @return True on success, false if vector is empty.
    [[nodiscard]] static bool encodeUInt1D(const std::vector<std::uint32_t>& vec,
                                           std::size_t ebytes,
                                           std::vector<std::uint8_t>& out);

    /// @brief Decodes a 2D floating-point matrix from an MDARRAY pack.
    /// @details Supports both ST 1201 (APA=2) and NaturalFormat (APA=1) encoding.
    /// @param[in] data Pointer to the MDARRAY pack bytes.
    /// @param[in] size Length of the MDARRAY pack buffer.
    /// @param[out] outMatrix Output 2D matrix [rows][cols].
    /// @return True on success, false if parsing fails.
    [[nodiscard]] static bool decodeFloat2D(const std::uint8_t* data,
                                            std::size_t size,
                                            std::vector<std::vector<double>>& outMatrix);

    /// @brief Decodes a 1D floating-point vector from an MDARRAY pack.
    /// @details Supports both ST 1201 (APA=2) and NaturalFormat (APA=1) encoding.
    /// @param[in] data Pointer to the MDARRAY pack bytes.
    /// @param[in] size Length of the MDARRAY pack buffer.
    /// @param[out] outVec Output 1D vector.
    /// @return True on success, false if parsing fails.
    [[nodiscard]] static bool decodeFloat1D(const std::uint8_t* data,
                                            std::size_t size,
                                            std::vector<double>& outVec);

    /// @brief Decodes a 2D unsigned integer matrix from an MDARRAY pack.
    /// @details Supports NaturalFormat (APA=1) and UnsignedInteger (APA=4) encoding.
    /// @param[in] data Pointer to the MDARRAY pack bytes.
    /// @param[in] size Length of the MDARRAY pack buffer.
    /// @param[out] outMatrix Output 2D matrix [rows][cols].
    /// @return True on success, false if parsing fails.
    [[nodiscard]] static bool decodeUInt2D(const std::uint8_t* data,
                                           std::size_t size,
                                           std::vector<std::vector<std::uint32_t>>& outMatrix);

    /// @brief Decodes a 1D unsigned integer vector from an MDARRAY pack.
    /// @details Supports NaturalFormat (APA=1) and UnsignedInteger (APA=4) encoding.
    /// @param[in] data Pointer to the MDARRAY pack bytes.
    /// @param[in] size Length of the MDARRAY pack buffer.
    /// @param[out] outVec Output 1D vector.
    /// @return True on success, false if parsing fails.
    [[nodiscard]] static bool decodeUInt1D(const std::uint8_t* data,
                                           std::size_t size,
                                           std::vector<std::uint32_t>& outVec);
};

} // namespace Klv
