#pragma once

#include "N2kTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Nmea::N2k {

/// @class N2kEncoder
/// @brief High-performance binary CAN frame encoder for NMEA 2000 Parameter Groups.
/// @details Encodes marine navigation, attitude, wind, and AIS data into 29-bit extended
///          CAN frames with correct Little-Endian scaling and Fast Packet fragmentation.
class N2kEncoder {
public:
    /// @brief Encodes PGN 129025 (Position, Rapid Update) into a single 8-byte CAN frame.
    /// @param[in] pos PositionRapid telemetry (lat/lon).
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] priority CAN message priority (0-7, default 3).
    /// @return Formatted 8-byte CanFrame.
    [[nodiscard]] static CanFrame encodePositionRapid(
        const PositionRapid& pos, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 3U) noexcept;

    /// @brief Encodes PGN 129026 (COG & SOG, Rapid Update) into a single 8-byte CAN frame.
    /// @param[in] cogSog Course & Speed over ground telemetry.
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] priority CAN message priority (default 3).
    /// @return Formatted 8-byte CanFrame.
    [[nodiscard]] static CanFrame encodeCogSogRapid(
        const CogSogRapid& cogSog, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 3U) noexcept;

    /// @brief Encodes PGN 127250 (Vessel Heading) into a single 8-byte CAN frame.
    /// @param[in] hdg Vessel heading telemetry (heading, variation, deviation).
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] priority CAN message priority (default 2).
    /// @return Formatted 8-byte CanFrame.
    [[nodiscard]] static CanFrame encodeVesselHeading(
        const VesselHeading& hdg, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 2U) noexcept;

    /// @brief Encodes PGN 127257 (Attitude) into a single 8-byte CAN frame.
    /// @param[in] att Attitude telemetry (yaw, pitch, roll).
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] priority CAN message priority (default 3).
    /// @return Formatted 8-byte CanFrame.
    [[nodiscard]] static CanFrame encodeAttitude(
        const Attitude& att, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 3U) noexcept;

    /// @brief Encodes PGN 130306 (Wind Data) into a single 8-byte CAN frame.
    /// @param[in] wind Wind data telemetry (speed, angle, reference).
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] priority CAN message priority (default 3).
    /// @return Formatted 8-byte CanFrame.
    [[nodiscard]] static CanFrame encodeWindData(
        const WindData& wind, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 3U) noexcept;

    /// @brief Encodes PGN 127245 (Rudder) into a single 8-byte CAN frame.
    [[nodiscard]] static CanFrame encodeRudder(
        const RudderData& rudder, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 2U) noexcept;

    /// @brief Encodes PGN 127258 (Magnetic Variation) into a single 8-byte CAN frame.
    [[nodiscard]] static CanFrame encodeMagneticVariation(
        const MagneticVariation& var, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 6U) noexcept;

    /// @brief Encodes PGN 126992 (System Time) into a single 8-byte CAN frame.
    [[nodiscard]] static CanFrame encodeSystemTime(
        const SystemTimeData& time, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 3U) noexcept;

    /// @brief Encodes PGN 126993 (Heartbeat) into a single 8-byte CAN frame.
    [[nodiscard]] static CanFrame encodeHeartbeat(
        const HeartbeatData& hb, std::uint8_t srcAddr = 0x23U, std::uint8_t priority = 6U) noexcept;

    /// @brief Encodes PGN 126464 (PGN List) into Fast Packet CAN frames.
    [[nodiscard]] static std::vector<CanFrame> encodePgnList(
        const PgnListData& list, std::uint8_t srcAddr = 0x23U, std::uint8_t seqCounter = 0U);

    /// @brief Encodes PGN 129038 (AIS Class A Position Report) raw payload buffer (28 bytes).
    /// @param[in] ais AIS Class A position report.
    /// @return 28-byte raw payload vector.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn129038Payload(const AisClassAPosition& ais);

    /// @brief Encodes PGN 129038 (AIS Class A Position Report) into Fast Packet CAN frames.
    /// @param[in] ais AIS Class A position report.
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] seqCounter 5-bit Fast Packet sequence counter (0..31).
    /// @return Sequence of 8-byte Fast Packet CanFrames.
    [[nodiscard]] static std::vector<CanFrame> encodeAisClassAPosition(
        const AisClassAPosition& ais, std::uint8_t srcAddr = 0x23U, std::uint8_t seqCounter = 0U);

    /// @brief Encodes PGN 129039 (AIS Class B Position Report) raw payload buffer (26 bytes).
    /// @param[in] ais AIS Class B position report.
    /// @return 26-byte raw payload vector.
    [[nodiscard]] static std::vector<std::uint8_t> encodePgn129039Payload(const AisClassBPosition& ais);

    /// @brief Encodes PGN 129039 (AIS Class B Position Report) into Fast Packet CAN frames.
    /// @param[in] ais AIS Class B position report.
    /// @param[in] srcAddr Source CAN node address (default 0x23).
    /// @param[in] seqCounter 5-bit Fast Packet sequence counter (0..31).
    /// @return Sequence of 8-byte Fast Packet CanFrames.
    [[nodiscard]] static std::vector<CanFrame> encodeAisClassBPosition(
        const AisClassBPosition& ais, std::uint8_t srcAddr = 0x23U, std::uint8_t seqCounter = 0U);
};

} // namespace Nmea::N2k
