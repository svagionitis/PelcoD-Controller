#pragma once

/// @file IPayload.h
/// @brief Polymorphic composite interface integrating gimbal, daylight, thermal, and LRF subsystems.

#include "ICameraPayload.h"
#include "IDevice.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "PayloadTypes.h"
#include "Klv/KlvTypes.h"

#include <memory>
#include <optional>

namespace PayloadHal {

/// @class IPayload
/// @brief Composite payload station binding Pan-Tilt Unit, Primary/Secondary cameras,
///        and Laser Range Finder into a unified synchronized station.
class IPayload : public virtual IDevice {
public:
    ~IPayload() override = default;

    /// @brief Accesses the Pan-Tilt Unit / gimbal subsystem.
    /// @return Shared pointer to pan-tilt interface, or nullptr if fixed mount.
    [[nodiscard]] virtual std::shared_ptr<IPanTiltUnit> panTilt() const noexcept = 0;

    /// @brief Accesses the primary optical camera payload (e.g. Daylight Visible EO).
    /// @return Shared pointer to camera interface, or nullptr if none installed.
    [[nodiscard]] virtual std::shared_ptr<ICameraPayload> primaryCamera() const noexcept = 0;

    /// @brief Accesses the secondary optical camera payload (e.g. Thermal LWIR/MWIR).
    /// @return Shared pointer to secondary camera interface, or nullptr if single-sensor.
    [[nodiscard]] virtual std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept = 0;

    /// @brief Accesses the Laser Range Finder sensor subsystem.
    /// @return Shared pointer to LRF interface, or nullptr if not equipped.
    [[nodiscard]] virtual std::shared_ptr<ILaserRangeFinder> lrf() const noexcept = 0;

    /// @brief Calculates geodetic 2D target coordinate on the Earth surface using platform GPS, heading,
    ///        gimbal orientation, and LRF range or ground projection.
    /// @param[in] platformGps Host platform latitude/longitude in degrees.
    /// @param[in] platformHeadingDeg Platform true heading in degrees [0, 360).
    /// @param[in] platformAltMeters Platform altitude above Mean Sea Level (MSL) in meters.
    /// @return Geodetic 2D coordinates of target if calculation succeeds, std::nullopt otherwise.
    [[nodiscard]] virtual std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const = 0;
};

} // namespace PayloadHal
