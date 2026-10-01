#pragma once

/// @file SightlineDetectionBuilder.h
/// @brief Serializer for Sightline automatic object and MTI moving target detection commands (IDD Detection module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDetectionBuilder
/// @brief Encodes MTI moving target detection thresholds, modes, and target size bounds (Message ID 0x2D).
class SightlineDetectionBuilder {
public:
    /// @brief Encodes MTI moving target detection configuration (Message ID 0x2D).
    /// @param[in] msg Detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionParams(const MsgSetDetectionParameters& msg);

    /// @brief Encodes query for active detection parameters (Message ID 0x2E).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] detIdx Detection index (0 or 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDetectionParams(
        std::uint8_t cameraIndex = 0U, std::uint8_t detIdx = 0U);

    /// @brief Encodes Video Moving Target Indication (VMTI) configuration (Message ID 0x84).
    /// @param[in] msg VMTI parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVMTI(const MsgSetVMTI& msg);

    /// @brief Encodes query for active VMTI configuration (Message ID 0x28 query 0x84).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVMTI(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes detection region of interest (ROI) bounding box (Message ID 0x7C).
    /// @param[in] msg Detection ROI parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionROI(const MsgDetectionROI& msg);

    /// @brief Encodes query for active detection ROI (Message ID 0x28 query 0x7C).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] roiIndex ROI slot index (0..3).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDetectionROI(
        std::uint8_t cameraIndex = 0U, std::uint8_t roiIndex = 0U);

    /// @brief Encodes advanced detection velocity and persistence gating (Message ID 0x76).
    /// @param[in] msg Advanced detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAdvDetectionParams(
        const MsgAdvancedDetectionParameters& msg);

    /// @brief Encodes query for advanced detection parameters (Message ID 0x28 query 0x76).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAdvDetectionParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for tracking box pixel luminance statistics (Message ID 0x28 query 0x78).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] trackId Target track ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackingPixelStats(
        std::uint8_t cameraIndex = 0U, std::uint8_t trackId = 0U);

    /// @brief Encodes automated detection high-resolution snapshot trigger (Message ID 0xAB).
    /// @param[in] msg Snapshot trigger parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDoDetectSnapShot(const MsgDoDetectSnapShot& msg);
};

} // namespace Sightline
