#pragma once

/// @file ILaserIlluminator.h
/// @brief Polymorphic interface for tactical laser pointers and near-infrared illuminators.

#include "IDevice.h"
#include "PayloadTypes.h"

#include <functional>

namespace PayloadHal {

/// @class ILaserIlluminator
/// @brief Abstract interface defining safety interlock controls, continuous/pulsed emission,
///        power modulation, and beam divergence zoom for tactical laser pointers and illuminators.
class ILaserIlluminator : public virtual IDevice {
public:
    ~ILaserIlluminator() override = default;

    // --- Safety Interlocks ---

    /// @brief Arms the laser transmitter diode for emission.
    /// @details In compliance with military/tactical eye-safety standards (Class 3B/4),
    ///          no emission can occur unless armed.
    /// @return True if arming succeeded and safety interlocks cleared.
    virtual bool armLaser() = 0;

    /// @brief Disarms the laser transmitter and safely cuts diode drive power.
    /// @return True if laser is safely disarmed.
    virtual bool disarmLaser() = 0;

    /// @brief Queries whether the laser transmitter is currently armed.
    [[nodiscard]] virtual bool isArmed() const noexcept = 0;

    // --- Emission Control ---

    /// @brief Activates laser diode emission in the currently configured mode.
    /// @details Fails immediately if the device is not armed.
    /// @return True if emission commenced.
    virtual bool startEmission() = 0;

    /// @brief Ceases active laser emission and returns diode to standby.
    /// @return True if emission ceased.
    virtual bool stopEmission() = 0;

    /// @brief Queries whether the laser is actively emitting radiation.
    [[nodiscard]] virtual bool isEmitting() const noexcept = 0;

    // --- Mode & Modulation Parameters ---

    /// @brief Selects emission profile (Standby, Continuous, Pulsed, Strobe).
    /// @param[in] mode Desired operational mode.
    /// @return True if mode was configured.
    virtual bool setMode(IlluminatorMode mode) = 0;

    /// @brief Queries the currently configured operational mode.
    [[nodiscard]] virtual IlluminatorMode mode() const noexcept = 0;

    /// @brief Sets normalized output radiant power level.
    /// @param[in] power01 Power ratio [0.0 (minimum threshold) to 1.0 (maximum rated power)].
    /// @return True if power level was updated.
    virtual bool setPowerNormalized(double power01) = 0;

    /// @brief Sets repetition pulse frequency for Pulsed and Strobe modes.
    /// @param[in] frequencyHz Modulation frequency in Hertz (e.g. 1.0 Hz to 30.0 Hz).
    /// @return True if frequency was accepted.
    virtual bool setPulseFrequency(double frequencyHz) = 0;

    /// @brief Adjusts motorized optical collimator / beam divergence zoom.
    /// @param[in] divergence01 Normalized beam spread (0.0 = Collimated spot pointer, 1.0 = Wide floodlight).
    /// @return True if divergence zoom command was accepted.
    virtual bool setBeamDivergenceNormalized(double divergence01) = 0;

    // --- Telemetry Feedback ---

    /// @brief Callback signature for live illuminator status updates.
    using TelemetryCallback = std::function<void(const IlluminatorTelemetry& telemetry)>;

    /// @brief Registers an observer callback for illuminator telemetry updates.
    /// @param[in] cb Callable receiving IlluminatorTelemetry records.
    virtual void registerTelemetryCallback(TelemetryCallback cb) = 0;

    /// @brief Retrieves the latest operational and safety telemetry synchronously.
    /// @return Current IlluminatorTelemetry snapshot.
    [[nodiscard]] virtual IlluminatorTelemetry currentTelemetry() const = 0;
};

} // namespace PayloadHal
