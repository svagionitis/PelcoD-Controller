#pragma once

/// @file SightlineGeneralBuilder.h
/// @brief Serializer for Sightline general system configuration, resets, and status modes (IDD General module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineGeneralBuilder
/// @brief Encodes version queries, parameter queries, resets, flash saves, and status reporting modes.
class SightlineGeneralBuilder {
public:
    /// @brief Encodes get version number query (Message ID 0x00).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVersionNumber();

    /// @brief Encodes a generic parameter query (Message ID 0x28).
    /// @param[in] queryId Target setter Message ID to query.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetParameters(std::uint8_t queryId);

    /// @brief Encodes system reset command (Message ID 0x01).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetAllParameters(const MsgResetAllParameters& msg);

    /// @brief Encodes save to flash command (Message ID 0x25).
    /// @param[in] msg Save parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSaveParameters(const MsgSaveParameters& msg);

    /// @brief Encodes system status mode configuration (Message ID 0x80).
    /// @param[in] msg Status mode settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSystemStatusMode(const MsgSystemStatusMode& msg);

    /// @brief Encodes hardware ID query (Message ID 0x50).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetHardwareId();

    /// @brief Encodes query for system status mode (Message ID 0x28 query 0x80).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSystemStatusMode();

    /// @brief Encodes query for hardware configuration (Message ID 0x28 query 0x8E).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCurrentConfig();

    /// @brief Encodes set system value register (Message ID 0x92).
    /// @param[in] msg System value parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSystemValue(const MsgSystemValue& msg);

    /// @brief Encodes Linux Traffic Control (tc) bandwidth limiter (Message ID 0x92, Key 13).
    /// @param[in] rateKbps Maximum rate in kilobits per second.
    /// @param[in] burstBytes Burst in bytes (e.g. 3000).
    /// @param[in] mtuBytes MTU in bytes (e.g. 1500).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTrafficControl(
        std::uint32_t rateKbps, std::uint32_t burstBytes = 3000U, std::uint32_t mtuBytes = 1500U);

    /// @brief Encodes query for system value register (Message ID 0x28 query 0x92).
    /// @param[in] systemValueId Target system register identifier.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSystemValue(std::uint8_t systemValueId);

    /// @brief Encodes custom binary tag data frame (Message ID 0x96).
    /// @param[in] msg Tag data payload.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTagData(const MsgTagData& msg);

    /// @brief Encodes tag data broadcast decimation rate (Message ID 0x97).
    /// @param[in] msg Tag data rate settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagDataRate(const MsgTagDataRate& msg);

    /// @brief Encodes query for tag data rate (Message ID 0x28 query 0x97).
    /// @param[in] tagId Target tag identifier.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagDataRate(std::uint16_t tagId);

    /// @brief Encodes tag data source selector (Message ID 0x98).
    /// @param[in] msg Tag source selector settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagSourceSelector(const MsgTagSourceSelector& msg);

    /// @brief Encodes query for tag source selector (Message ID 0x28 query 0x98).
    /// @param[in] tagId Target tag identifier.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagSourceSelector(std::uint16_t tagId);

    /// @brief Encodes latency timing profiler packet (Message ID 0x88).
    /// @param[in] msg Detailed latency timing measurements.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDetailedTiming(const MsgDetailedTiming& msg);

    /// @brief Encodes appended metadata stream configuration (Message ID 0x89).
    /// @param[in] msg Appended metadata settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAppendedMetadata(const MsgAppendedMetadata& msg);

    /// @brief Encodes query for appended metadata configuration (Message ID 0x28 query 0x89).
    /// @param[in] cameraIndex Target camera channel index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAppendedMetadata(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes frame index and timestamp packet (Message ID 0x8A).
    /// @param[in] msg Frame index and timestamp.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFrameIndex(const MsgFrameIndex& msg);
};

} // namespace Sightline
