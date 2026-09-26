#pragma once

/// @file IPtzPresetManager.h
/// @brief Polymorphic interface for spatial and optical PTZ preset management.

#include "PayloadTypes.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @struct PtzPreset
/// @brief Complete spatial and optical waypoint configuration.
struct PtzPreset {
    std::uint32_t id { 0U };                                   ///< Preset index (1 to 255)
    std::string name {};                                       ///< Human-readable label (e.g. "Main Gate North")
    double panAngleDeg { 0.0 };                                ///< Pan angle in degrees relative to mount [-180, +180]
    double tiltAngleDeg { 0.0 };                               ///< Tilt angle in degrees [-90, +90]
    double opticalZoomFactor { 1.0 };                          ///< Optical zoom magnification (e.g. 1.0x to 40.0x)
    double focusDistanceNormalized { 0.0 };                    ///< Normalized focus position [0.0 = Near, 1.0 = Inf]
    CameraSpectrum preferredSpectrum { CameraSpectrum::DaylightVisible }; ///< Preferred optical sensor
    std::chrono::system_clock::time_point timestamp {};        ///< Creation or last updated timestamp
};

/// @class IPtzPresetManager
/// @brief Abstract interface defining preset storage, current coordinate capture, recall, and Home position.
class IPtzPresetManager {
public:
    virtual ~IPtzPresetManager() = default;

    /// @brief Saves explicit preset parameters to storage.
    /// @param[in] preset Preset configuration structure.
    /// @return True if successfully stored.
    virtual bool savePreset(const PtzPreset& preset) = 0;

    /// @brief Captures current gimbal angles and camera optics into a new preset.
    /// @param[in] id Preset index.
    /// @param[in] name Optional human-readable name.
    /// @return True if successfully captured and stored.
    virtual bool saveCurrentPosition(std::uint32_t id, const std::string& name = "") = 0;

    /// @brief Slew gimbal and camera optics to a saved preset.
    /// @param[in] id Preset index to recall.
    /// @param[in] speedRatio Slew speed multiplier [0.1 to 1.0].
    /// @return True if preset exists and slew command was dispatched.
    virtual bool recallPreset(std::uint32_t id, float speedRatio = 1.0f) = 0;

    /// @brief Removes a saved preset by ID.
    /// @param[in] id Preset index to remove.
    /// @return True if preset was found and removed.
    virtual bool clearPreset(std::uint32_t id) = 0;

    /// @brief Queries a saved preset.
    /// @param[in] id Preset index.
    /// @return PtzPreset if found, std::nullopt otherwise.
    [[nodiscard]] virtual std::optional<PtzPreset> getPreset(std::uint32_t id) const = 0;

    /// @brief Lists all stored presets sorted by ID.
    /// @return Vector of all stored PtzPreset records.
    [[nodiscard]] virtual std::vector<PtzPreset> listPresets() const = 0;

    /// @brief Designates the default Home preset.
    /// @param[in] id Preset index to designate as Home.
    virtual void setHomePresetId(std::uint32_t id) = 0;

    /// @brief Retrieves the designated Home preset ID, if configured.
    /// @return Home preset index or std::nullopt.
    [[nodiscard]] virtual std::optional<std::uint32_t> getHomePresetId() const = 0;

    /// @brief Slews directly to the configured Home preset.
    /// @param[in] speedRatio Slew speed multiplier [0.1 to 1.0].
    /// @return True if Home preset is configured and recall dispatched.
    virtual bool goHome(float speedRatio = 1.0f) = 0;
};

} // namespace PayloadHal
