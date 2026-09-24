#pragma once

/// @file ICameraPayload.h
/// @brief Polymorphic interface for optical daylight and thermal camera payloads.

#include "IDevice.h"
#include "PayloadTypes.h"

#include <functional>

namespace PayloadHal {

/// @class ICameraPayload
/// @brief Unified optical abstraction governing continuous/direct zoom, autofocus,
///        electronic/optical image stabilization, defogging, and thermal palette control.
class ICameraPayload : public virtual IDevice {
public:
    ~ICameraPayload() override = default;

    /// @brief Queries the primary spectral band of the sensor.
    [[nodiscard]] virtual CameraSpectrum spectrum() const noexcept = 0;

    // --- Optical Zoom Controls ---

    /// @brief Drives optical zoom to a normalized position between 0.0 and 1.0.
    /// @param[in] zoom01 Normalized zoom coordinate (0.0 = Full Wide, 1.0 = Max Optical Tele).
    /// @return True if direct zoom command was accepted.
    virtual bool setZoomNormalized(double zoom01) = 0;

    /// @brief Commands continuous optical zooming at designated velocity.
    /// @param[in] velocity Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Wide, +1.0 = Max Tele).
    /// @return True if command was dispatched.
    virtual bool zoomContinuous(float velocity) = 0;

    /// @brief Halts active continuous optical zoom motion immediately.
    /// @return True if zoom motor was stopped.
    virtual bool zoomStop() = 0;

    // --- Focus Controls ---

    /// @brief Toggles continuous automatic focus algorithm.
    /// @param[in] enable True for auto-focus, false for manual focus hold.
    /// @return True if mode was updated.
    virtual bool setFocusAuto(bool enable) = 0;

    /// @brief Drives lens focus group to a normalized position.
    /// @param[in] focus01 Normalized focus position (0.0 = Near Limit, 1.0 = Infinity).
    /// @return True if direct focus command was accepted.
    virtual bool setFocusNormalized(double focus01) = 0;

    /// @brief Triggers a one-push automatic focus convergence cycle.
    /// @return True if one-push focus cycle was initiated.
    virtual bool triggerOnePushFocus() = 0;

    // --- Sensor Processing & Enhancement ---

    /// @brief Controls mechanical Infrared Cut (ICR) filter engagement.
    /// @param[in] nightMode True to retract IR filter for night vision / IR illuminator pass.
    /// @return True if filter solenoid was actuated.
    virtual bool setDayNightIcr(bool nightMode) = 0;

    /// @brief Toggles optical or digital atmospheric defogging.
    /// @param[in] enable True to engage defogging processing.
    /// @return True if defog mode was updated.
    virtual bool setDefog(bool enable) = 0;

    /// @brief Toggles optical (OIS) or electronic (EIS) image stabilization.
    /// @param[in] enable True to engage image stabilizer.
    /// @return True if stabilizer was actuated.
    virtual bool setStabilizer(bool enable) = 0;

    // --- Thermal Sensor Enhancements (Optional / Gated) ---

    /// @brief Configures false-color palette / polarity for thermal sensors.
    /// @param[in] polarity Target palette (WhiteHot, BlackHot, FusionColor, Rainbow).
    /// @return True if applied, false if unsupported or daylight sensor.
    virtual bool setThermalPolarity(ThermalPolarity polarity) { (void)polarity; return false; }

    /// @brief Actuates mechanical calibration flag for Non-Uniformity Correction (NUC).
    /// @return True if calibration cycle was initiated, false if unsupported.
    virtual bool triggerNucCalibration() { return false; }

    // --- Telemetry Callback ---

    /// @brief Callback signature for periodic optical telemetry and field-of-view updates.
    using TelemetryCallback = std::function<void(const CameraTelemetry& telemetry)>;

    /// @brief Registers an observer callback for live optical telemetry updates.
    /// @param[in] cb Callable receiving CameraTelemetry records.
    virtual void registerTelemetryCallback(TelemetryCallback cb) = 0;
};

} // namespace PayloadHal
