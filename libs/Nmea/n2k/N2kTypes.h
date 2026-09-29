#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Nmea::N2k {

/// @brief CAN frame representation compatible with SocketCAN (struct can_frame) and binary CAN bus streams.
struct CanFrame {
    std::uint32_t id { 0U }; ///< 29-bit extended CAN identifier (EFF)
    std::uint8_t dlc { 0U }; ///< Data length code (0..8)
    std::array<std::uint8_t, 8> data {}; ///< Frame payload bytes
};

/// @brief NMEA 2000 (ISO 11783-3 / SAE J1939) 29-bit CAN Identifier Header.
/// @details Decomposes 29-bit extended CAN identifier into priority, PGN, source, and destination addresses.
struct N2kHeader {
    std::uint8_t priority { 6U }; ///< Priority (0-7, lower is higher priority)
    std::uint32_t pgn { 0U }; ///< Parameter Group Number (18 or 24 bits)
    std::uint8_t sourceAddress { 0U }; ///< Source CAN device address (0-253)
    std::uint8_t destinationAddress { 0xFF }; ///< Destination address (0xFF = broadcast / global)

    /// @brief Decomposes a 29-bit CAN ID into an N2kHeader.
    /// @param[in] canId 29-bit CAN identifier (bits 0..28).
    /// @return Parsed N2kHeader struct.
    [[nodiscard]] static N2kHeader fromCanId(std::uint32_t canId) noexcept
    {
        // Mask out CAN_EFF_FLAG (bit 31) if present in raw SocketCAN frames
        const std::uint32_t raw = canId & 0x1FFFFFFFU;
        N2kHeader hdr {};
        hdr.priority = static_cast<std::uint8_t>((raw >> 26) & 0x07U);
        const std::uint8_t dp = static_cast<std::uint8_t>((raw >> 24) & 0x01U);
        const std::uint8_t pf = static_cast<std::uint8_t>((raw >> 16) & 0xFFU);
        const std::uint8_t ps = static_cast<std::uint8_t>((raw >> 8) & 0xFFU);
        hdr.sourceAddress = static_cast<std::uint8_t>(raw & 0xFFU);

        if (pf < 240U) {
            // PDU1 (Addressable): PS is destination address
            hdr.destinationAddress = ps;
            hdr.pgn = (static_cast<std::uint32_t>(dp) << 16) | (static_cast<std::uint32_t>(pf) << 8);
        } else {
            // PDU2 (Broadcast): PS is Group Extension
            hdr.destinationAddress = 0xFFU;
            hdr.pgn = (static_cast<std::uint32_t>(dp) << 16) | (static_cast<std::uint32_t>(pf) << 8)
                | static_cast<std::uint32_t>(ps);
        }
        return hdr;
    }

    /// @brief Composes a 29-bit CAN identifier from the N2kHeader fields.
    /// @return 29-bit CAN ID.
    [[nodiscard]] std::uint32_t toCanId() const noexcept
    {
        const std::uint32_t prioBits = (static_cast<std::uint32_t>(priority) & 0x07U) << 26;
        const std::uint32_t dp = (pgn >> 16) & 0x01U;
        const std::uint32_t pf = (pgn >> 8) & 0xFFU;
        const std::uint32_t ps = (pf < 240U) ? static_cast<std::uint32_t>(destinationAddress) : (pgn & 0xFFU);

        return prioBits | (dp << 24) | (pf << 16) | (ps << 8) | static_cast<std::uint32_t>(sourceAddress);
    }
};

/// @brief Common NMEA 2000 Parameter Group Numbers (PGNs).
enum class Pgn : std::uint32_t {
    SystemTime = 126992U,
    ProductInformation = 126996U,
    VesselHeading = 127250U,
    RateOfTurn = 127251U,
    Attitude = 127257U,
    PositionRapidUpdate = 129025U,
    CogSogRapidUpdate = 129026U,
    GnssPositionData = 129029U,
    AisClassAPositionReport = 129038U,
    AisClassBPositionReport = 129039U,
    AisClassBExtendedPositionReport = 129040U,
    AisClassAStaticVoyageData = 129794U,
    WindData = 130306U
};

/// @brief Heading reference enum for N2K fields.
enum class HeadingReference : std::uint8_t { True = 0U, Magnetic = 1U, Error = 2U, Unavailable = 3U };

/// @brief Wind reference enum for N2K wind data.
enum class WindReference : std::uint8_t {
    TheoreticalGround = 0U,
    TheoreticalBoat = 1U,
    Apparent = 2U,
    ApparentBoat = 3U,
    Unavailable = 7U
};

/// @brief PGN 129025: Position, Rapid Update (8 bytes single frame).
struct PositionRapid {
    double latitudeDeg { 0.0 }; ///< Latitude [-90.0, +90.0]
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0]
    bool isValid { false }; ///< False if data is unavailable sentinel
};

/// @brief PGN 129026: COG & SOG, Rapid Update (8 bytes single frame).
struct CogSogRapid {
    std::uint8_t sid { 0U }; ///< Sequence ID
    double cogDegrees { 0.0 }; ///< Course Over Ground [0.0, 360.0)
    double sogKnots { 0.0 }; ///< Speed Over Ground in knots
    HeadingReference cogReference { HeadingReference::True }; ///< True or Magnetic
    bool hasCog { false }; ///< False if COG is unavailable
    bool hasSog { false }; ///< False if SOG is unavailable
};

/// @brief PGN 127250: Vessel Heading (8 bytes single frame).
struct VesselHeading {
    std::uint8_t sid { 0U }; ///< Sequence ID
    double headingDegrees { 0.0 }; ///< Heading [0.0, 360.0)
    double deviationDegrees { 0.0 }; ///< Magnetic deviation
    double variationDegrees { 0.0 }; ///< Magnetic variation
    HeadingReference reference { HeadingReference::True }; ///< Heading sensor reference
    bool hasHeading { false };
    bool hasDeviation { false };
    bool hasVariation { false };
};

/// @brief PGN 127257: Attitude (8 bytes single frame).
struct Attitude {
    std::uint8_t sid { 0U }; ///< Sequence ID
    double yawDegrees { 0.0 }; ///< Yaw angle
    double pitchDegrees { 0.0 }; ///< Pitch angle: positive = bow up
    double rollDegrees { 0.0 }; ///< Roll angle: positive = starboard down
    bool hasYaw { false };
    bool hasPitch { false };
    bool hasRoll { false };
};

/// @brief PGN 129038: AIS Class A Position Report (28 bytes Fast Packet).
struct AisClassAPosition {
    std::uint8_t messageId { 0U }; ///< AIS Message ID (1, 2, or 3)
    std::uint8_t repeatIndicator { 0U };
    std::uint32_t mmsi { 0U }; ///< 9-digit MMSI
    double latitudeDeg { 0.0 };
    double longitudeDeg { 0.0 };
    double cogDegrees { 0.0 };
    double sogKnots { 0.0 };
    double headingDegrees { 0.0 };
    double rateOfTurnDegPerSec { 0.0 };
    std::uint8_t navStatus { 0U };
    bool positionValid { false };
    bool hasCog { false };
    bool hasSog { false };
    bool hasHeading { false };
    bool hasRateOfTurn { false };
};

/// @brief PGN 129039: AIS Class B Position Report (26 bytes Fast Packet).
struct AisClassBPosition {
    std::uint32_t mmsi { 0U }; ///< 9-digit MMSI
    double latitudeDeg { 0.0 };
    double longitudeDeg { 0.0 };
    double cogDegrees { 0.0 };
    double sogKnots { 0.0 };
    double headingDegrees { 0.0 };
    bool positionValid { false };
    bool hasCog { false };
    bool hasSog { false };
    bool hasHeading { false };
};

/// @brief PGN 130306: Wind Data (6-8 bytes single frame).
struct WindData {
    std::uint8_t sid { 0U }; ///< Sequence ID
    double windSpeedMps { 0.0 }; ///< Wind speed in m/s
    double windSpeedKnots { 0.0 }; ///< Wind speed in knots
    double windAngleDegrees { 0.0 }; ///< Wind angle [0.0, 360.0)
    WindReference reference { WindReference::Apparent };
    bool hasWindSpeed { false };
    bool hasWindAngle { false };
};

/// @brief Reassembled NMEA 2000 message containing complete PGN payload.
struct N2kMessage {
    N2kHeader header {};
    std::vector<std::uint8_t> payload {};
    std::chrono::steady_clock::time_point timestamp {};
};

/// @brief Sentinel constants indicating 'Data Not Available' per N2K specification.
namespace Sentinels {
    inline constexpr std::int32_t kUnavailableInt32 = 0x7FFFFFFF;
    inline constexpr std::uint32_t kUnavailableUInt32 = 0xFFFFFFFFU;
    inline constexpr std::int16_t kUnavailableInt16 = 0x7FFF;
    inline constexpr std::uint16_t kUnavailableUInt16 = 0xFFFFU;
    inline constexpr std::uint8_t kUnavailableUInt8 = 0xFFU;
} // namespace Sentinels

} // namespace Nmea::N2k
