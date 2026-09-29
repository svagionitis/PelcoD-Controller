#pragma once

/// @file AutoFramingController.h
/// @brief Range-adaptive optical field-of-view and zoom controller for automated target framing.

#include "ICameraPayload.h"
#include "Nmea/AisTypes.h"

#include <cmath>
#include <memory>
#include <mutex>

namespace PayloadHal {

/// @struct AutoFramingConfig
/// @brief Configuration parameters for range-adaptive target optical framing.
struct AutoFramingConfig {
    double targetFrameOccupancyRatio { 0.30 }; ///< Target occupies 30% of horizontal FOV
    double minTargetDimensionMeters { 5.0 }; ///< Minimum clamp for target size (e.g. small skiff/buoy)
    double maxTargetDimensionMeters { 300.0 }; ///< Maximum clamp for target size (e.g. supertanker)
    double defaultTargetLengthMeters { 20.0 }; ///< Default target length when dimensions unavailable
    double wideHfovDeg { 60.0 }; ///< Horizontal FOV at 1.0x (full wide) in degrees
    double teleHfovDeg { 2.0 }; ///< Horizontal FOV at maximum optical tele in degrees
    double minSlantRangeMeters { 10.0 }; ///< Minimum range clamp to prevent division by zero
    double maxSlantRangeMeters { 50000.0 }; ///< Maximum range clamp (50 km)
};

/// @class AutoFramingController
/// @brief Computes optimal camera optical field-of-view and commands normalized zoom based on target range and size.
/// @details Ensures that electro-optical sensors automatically zoom to display targets at consistent,
///          identifiable resolution regardless of whether the target is 200m or 5km away.
class AutoFramingController {
public:
    /// @brief Constructs an AutoFramingController with optional configuration.
    /// @param[in] config Framing ratios and optical boundaries.
    explicit AutoFramingController(const AutoFramingConfig& config = {});

    virtual ~AutoFramingController() = default;

    // Non-copyable, movable
    AutoFramingController(const AutoFramingController&) = delete;
    AutoFramingController& operator=(const AutoFramingController&) = delete;
    AutoFramingController(AutoFramingController&&) noexcept = default;
    AutoFramingController& operator=(AutoFramingController&&) noexcept = default;

    /// @brief Updates framing configuration parameters.
    /// @param[in] config New configuration.
    void setConfig(const AutoFramingConfig& config);

    /// @brief Retrieves the active configuration snapshot.
    /// @return Current AutoFramingConfig.
    [[nodiscard]] AutoFramingConfig config() const;

    /// @brief Calculates the ideal Horizontal Field of View (HFOV) for a target.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension (length or beam) in meters.
    /// @param[in] occupancyRatio Desired screen width ratio [0.05 .. 0.95].
    /// @return Desired HFOV in degrees.
    [[nodiscard]] static double calculateDesiredHfov(
        double slantRangeMeters, double targetDimensionMeters, double occupancyRatio = 0.30) noexcept;

    /// @brief Calculates normalized optical zoom position [0.0 .. 1.0] from target range and size.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension in meters.
    /// @param[in] occupancyRatio Desired screen occupancy ratio.
    /// @param[in] wideHfovDeg Lens horizontal FOV at full wide.
    /// @param[in] teleHfovDeg Lens horizontal FOV at full tele.
    /// @return Normalized zoom position [0.0 (wide) .. 1.0 (tele)].
    [[nodiscard]] static double calculateNormalizedZoom(double slantRangeMeters, double targetDimensionMeters,
        double occupancyRatio, double wideHfovDeg, double teleHfovDeg) noexcept;

    /// @brief Estimates target reference dimension in meters from AIS dimensions or fallback.
    /// @param[in] dimensions AIS physical dimensions struct.
    /// @return Estimated dimension in meters.
    [[nodiscard]] double estimateTargetDimension(const Nmea::AisDimensions& dimensions) const noexcept;

    /// @brief Computes normalized zoom and commands an optical camera payload.
    /// @param[in,out] camera Reference to ICameraPayload instance.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension in meters.
    /// @return True if zoom command was successfully accepted by camera.
    bool frameTarget(ICameraPayload& camera, double slantRangeMeters, double targetDimensionMeters);

private:
    mutable std::mutex m_mutex {};
    AutoFramingConfig m_config {};
};

} // namespace PayloadHal
