#pragma once

/// @file IPayload.h
/// @brief Polymorphic composite interface integrating gimbal, daylight, thermal, and LRF subsystems.

#include "GeoreferenceUtils.h"
#include "ICameraPayload.h"
#include "IDevice.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "Klv/KlvTypes.h"
#include "PayloadTypes.h"

#include <memory>
#include <mutex>
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

    /// @brief Computes the 4-corner ground projection footprint frustum polygon for the specified camera.
    /// @param[in] camera Camera payload interface to query FOV from.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] groundElevationM Target ground elevation MSL in meters (default 0.0).
    /// @return FrustumCorners containing 4 geodetic coordinates, or std::nullopt if looking at or above horizon.
    [[nodiscard]] virtual std::optional<Klv::FrustumCorners> computeFrustumCorners(
        const std::shared_ptr<ICameraPayload>& camera, const Klv::GeoPoint3D& platformPos, double platformHeadingDeg,
        double groundElevationM = 0.0) const
    {
        const auto ptu = panTilt();
        if (!ptu || !camera) {
            return std::nullopt;
        }
        const auto ptuTelem = ptu->currentTelemetry();
        const auto camTelem = camera->currentTelemetry();
        return GeoreferenceUtils::computeFrustumCorners(platformPos, platformHeadingDeg, ptuTelem.panAngleDeg,
            ptuTelem.tiltAngleDeg, camTelem.horizontalFovDeg, camTelem.verticalFovDeg, groundElevationM);
    }

    /// @brief Computes the 4-corner ground projection footprint frustum polygon for the primary camera.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] groundElevationM Target ground elevation MSL in meters (default 0.0).
    /// @return FrustumCorners containing 4 geodetic coordinates, or std::nullopt if looking at or above horizon.
    [[nodiscard]] virtual std::optional<Klv::FrustumCorners> computeFrustumCorners(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, double groundElevationM = 0.0) const
    {
        return computeFrustumCorners(primaryCamera(), platformPos, platformHeadingDeg, groundElevationM);
    }

    /// @brief Slews the Pan-Tilt Unit line-of-sight to point towards a geodetic 3D target coordinate.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] targetPos Target 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @return true if command was dispatched to the Pan-Tilt Unit, false otherwise.
    virtual bool slewToGeoTarget(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, const Klv::GeoPoint3D& targetPos)
    {
        const auto ptu = panTilt();
        if (!ptu) {
            return false;
        }
        const auto look = GeoreferenceUtils::computeLookAnglesToTarget(platformPos, platformHeadingDeg, targetPos);
        return ptu->setAbsoluteAngles(look.panAngleDeg, look.tiltAngleDeg);
    }

    /// @brief Engages Geo-Lock tracking mode on a target coordinate.
    /// @param[in] targetPos Target 3D coordinate to hold.
    /// @return true if target was locked, false if ptu is null.
    virtual bool engageGeoLock(const Klv::GeoPoint3D& targetPos)
    {
        if (!panTilt()) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_geoLockMutex);
        m_geoLockTarget = targetPos;
        m_geoLockActive = true;
        return true;
    }

    /// @brief Disengages Geo-Lock tracking mode.
    virtual void disengageGeoLock()
    {
        std::lock_guard<std::mutex> lock(m_geoLockMutex);
        m_geoLockActive = false;
        m_geoLockTarget.reset();
    }

    /// @brief Checks whether Geo-Lock tracking mode is currently engaged.
    /// @return true if Geo-Lock is engaged.
    [[nodiscard]] virtual bool isGeoLocked() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_geoLockMutex);
        return m_geoLockActive;
    }

    /// @brief Retrieves the currently locked geographic target, if any.
    /// @return Target 3D coordinate if Geo-Lock is engaged, std::nullopt otherwise.
    [[nodiscard]] virtual std::optional<Klv::GeoPoint3D> geoLockTarget() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_geoLockMutex);
        return m_geoLockTarget;
    }

    /// @brief Updates gimbal orientation to maintain line-of-sight on the locked target.
    /// @param[in] platformPos Updated platform 3D coordinate.
    /// @param[in] platformHeadingDeg Updated platform true compass heading in degrees.
    /// @return true if slew command was dispatched, false if not geo-locked or failed.
    virtual bool updateGeoLock(const Klv::GeoPoint3D& platformPos, double platformHeadingDeg)
    {
        std::optional<Klv::GeoPoint3D> target;
        {
            std::lock_guard<std::mutex> lock(m_geoLockMutex);
            if (!m_geoLockActive || !m_geoLockTarget) {
                return false;
            }
            target = m_geoLockTarget;
        }
        return slewToGeoTarget(platformPos, platformHeadingDeg, *target);
    }

protected:
    mutable std::mutex m_geoLockMutex;
    std::optional<Klv::GeoPoint3D> m_geoLockTarget {};
    bool m_geoLockActive { false };
};

} // namespace PayloadHal
