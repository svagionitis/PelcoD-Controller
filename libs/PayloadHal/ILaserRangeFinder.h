#pragma once

/// @file ILaserRangeFinder.h
/// @brief Polymorphic interface for Laser Range Finder (LRF) sensors.

#include "IDevice.h"
#include "PayloadTypes.h"

#include <functional>

namespace PayloadHal {

/// @class ILaserRangeFinder
/// @brief Abstract interface defining safety interlock controls, single/continuous pulse
///        triggering, range gating, and distance measurement reporting for tactical LRFs.
class ILaserRangeFinder : public virtual IDevice {
public:
    ~ILaserRangeFinder() override = default;

    // --- Safety Interlocks ---

    /// @brief Arms the laser transmitter capacitor/diode for firing.
    /// @details In compliance with eye-safety standards, no pulse can be emitted unless armed.
    /// @return True if arming succeeded and safety interlocks cleared.
    virtual bool armLaser() = 0;

    /// @brief Disarms the laser transmitter and dumps capacitor charge to safe ground.
    /// @return True if laser is safely disarmed.
    virtual bool disarmLaser() = 0;

    /// @brief Queries whether the laser transmitter is currently armed.
    [[nodiscard]] virtual bool isArmed() const noexcept = 0;

    // --- Ranging Operations ---

    /// @brief Fires a single laser pulse and acquires target slant range.
    /// @return True if single shot was triggered (fails immediately if not armed).
    virtual bool triggerSingleMeasurement() = 0;

    /// @brief Engages repetitive continuous pulse ranging at designated frequency.
    /// @param[in] mode Repetition rate (1 Hz, 5 Hz, 10 Hz).
    /// @return True if continuous ranging was activated.
    virtual bool setContinuousMode(LrfMode mode) = 0;

    /// @brief Halts active continuous pulse emissions and returns diode to standby.
    /// @return True if emissions ceased.
    virtual bool stopRanging() = 0;

    // --- Range Gating ---

    /// @brief Configures minimum and maximum distance filters to reject atmospheric backscatter/clutter.
    /// @param[in] minRangeMeters Minimum distance threshold in meters.
    /// @param[in] maxRangeMeters Maximum distance threshold in meters.
    /// @return True if range gate was applied.
    virtual bool setRangeGating(double minRangeMeters, double maxRangeMeters) = 0;

    // --- Measurement Feedback ---

    /// @brief Callback signature for laser return echo measurements.
    using MeasurementCallback = std::function<void(const LrfTargetMeasurement& measurement)>;

    /// @brief Registers an observer callback for acquired range returns.
    /// @param[in] cb Callable receiving LrfTargetMeasurement records.
    virtual void registerMeasurementCallback(MeasurementCallback cb) = 0;

    /// @brief Retrieves the most recent laser range return measurement if one has been acquired.
    /// @return Acquired target measurement or std::nullopt if no valid pulse echo exists.
    [[nodiscard]] virtual std::optional<LrfTargetMeasurement> lastMeasurement() const = 0;
};

} // namespace PayloadHal
