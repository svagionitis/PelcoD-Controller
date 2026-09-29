#pragma once

#include "N2kTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Nmea::N2k {

/// @class N2kDecoder
/// @brief High-performance, zero-allocation binary decoders and encoders for NMEA 2000 PGNs.
/// @details Provides strict validation against out-of-range sentinels and endianness-safe bit unpacking.
class N2kDecoder {
public:
    // --- Decoders ---

    /// @brief Decodes PGN 129025 (Position, Rapid Update).
    /// @param[in] data Payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 8).
    /// @param[out] out Decoded PositionRapid structure.
    /// @return True if parsing succeeded and coordinates were extracted, false if corrupted or sentinel.
    [[nodiscard]] static bool parsePgn129025(const std::uint8_t* data, std::size_t len, PositionRapid& out) noexcept;

    /// @brief Decodes PGN 129026 (COG & SOG, Rapid Update).
    /// @param[in] data Payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 8).
    /// @param[out] out Decoded CogSogRapid structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn129026(const std::uint8_t* data, std::size_t len, CogSogRapid& out) noexcept;

    /// @brief Decodes PGN 127250 (Vessel Heading).
    /// @param[in] data Payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 8).
    /// @param[out] out Decoded VesselHeading structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn127250(const std::uint8_t* data, std::size_t len, VesselHeading& out) noexcept;

    /// @brief Decodes PGN 127257 (Attitude).
    /// @param[in] data Payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 7).
    /// @param[out] out Decoded Attitude structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn127257(const std::uint8_t* data, std::size_t len, Attitude& out) noexcept;

    /// @brief Decodes PGN 129038 (AIS Class A Position Report).
    /// @param[in] data Fast Packet payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 27).
    /// @param[out] out Decoded AisClassAPosition structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn129038(
        const std::uint8_t* data, std::size_t len, AisClassAPosition& out) noexcept;

    /// @brief Decodes PGN 129039 (AIS Class B Position Report).
    /// @param[in] data Fast Packet payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 25).
    /// @param[out] out Decoded AisClassBPosition structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn129039(
        const std::uint8_t* data, std::size_t len, AisClassBPosition& out) noexcept;

    /// @brief Decodes PGN 130306 (Wind Data).
    /// @param[in] data Payload byte buffer.
    /// @param[in] len Length of payload in bytes (must be >= 6).
    /// @param[out] out Decoded WindData structure.
    /// @return True if parsing succeeded, false if buffer truncated.
    [[nodiscard]] static bool parsePgn130306(const std::uint8_t* data, std::size_t len, WindData& out) noexcept;

    // --- Encoders ---

    /// @brief Encodes PGN 129025 (Position, Rapid Update) into 8 bytes.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn129025(const PositionRapid& pos);

    /// @brief Encodes PGN 129026 (COG & SOG, Rapid Update) into 8 bytes.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn129026(const CogSogRapid& cogSog);

    /// @brief Encodes PGN 127250 (Vessel Heading) into 8 bytes.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn127250(const VesselHeading& hdg);

    /// @brief Encodes PGN 127257 (Attitude) into 8 bytes.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn127257(const Attitude& att);

    /// @brief Encodes PGN 130306 (Wind Data) into 6 bytes.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn130306(const WindData& wind);

    /// @brief Splits an arbitrary multi-frame payload into a sequence of Fast Packet CAN frames.
    /// @param[in] header N2K header for the frames.
    /// @param[in] data Raw payload buffer.
    /// @param[in] len Size of payload in bytes.
    /// @param[in] seqCounter 5-bit sequence counter (0..31).
    /// @return Vector of 8-byte CAN frames ready for transmission.
    [[nodiscard]] static std::vector<CanFrame> splitFastPacket(
        const N2kHeader& header, const std::uint8_t* data, std::size_t len, std::uint8_t seqCounter = 0U);
};

} // namespace Nmea::N2k
