/// @file DemRayCaster.cpp
/// @brief Implementation of DemRayCaster numerical ray-marching engine for PayloadHal.

#include "DemRayCaster.h"
#include "Klv/KlvGeodesy.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {
    constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
} // namespace

Klv::GeoPoint3D DemRayCaster::evaluateRayPoint(
    const Klv::GeoPoint3D& platformPos,
    double trueBearingDeg,
    double tiltDeg,
    double slantDistanceM) noexcept
{
    const double R_E = Klv::KlvGeodesy::kEarthRadiusMeters;
    const double R0 = R_E + platformPos.altitudeM;
    const double sinTilt = std::sin(tiltDeg * DEG_TO_RAD);
    const double s = std::max(0.0, slantDistanceM);

    // Exact curved-Earth distance from Earth center to point along ray
    const double distFromCenter = std::sqrt(R0 * R0 + 2.0 * R0 * s * sinTilt + s * s);
    const double rayAltM = distFromCenter - R_E;

    // Great-circle arc angle subtended at Earth center
    const double cosGamma = std::clamp((R0 + s * sinTilt) / distFromCenter, -1.0, 1.0);
    const double gamma = std::acos(cosGamma);
    const double groundDistM = R_E * gamma;

    const auto pt2D = Klv::KlvGeodesy::directGeodetic(
        { platformPos.latitudeDeg, platformPos.longitudeDeg },
        trueBearingDeg,
        groundDistM);

    return Klv::GeoPoint3D { pt2D.latitudeDeg, pt2D.longitudeDeg, rayAltM };
}

std::optional<DemIntersectionResult> DemRayCaster::intersect(
    const IDemProvider& dem,
    const Klv::GeoPoint3D& platformPos,
    double platformHeadingDeg,
    double gimbalPanDeg,
    double gimbalTiltDeg,
    const DemRayConfig& config) noexcept
{
    // Boresight pointing direction
    double bearing = std::fmod(platformHeadingDeg + gimbalPanDeg, 360.0);
    if (bearing < 0.0) {
        bearing += 360.0;
    }

    // If ray points at or above horizon and platform is above maximum terrain height, no ground intersection
    if (gimbalTiltDeg >= 0.0 && platformPos.altitudeM >= dem.maxElevationM()) {
        return std::nullopt;
    }

    // Evaluate at platform origin (s = 0)
    const auto pt0 = evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, 0.0);
    const double hDem0 = dem.getElevationM(pt0.latitudeDeg, pt0.longitudeDeg)
                             .value_or(config.fallbackGroundElevationM);
    const double f0 = pt0.altitudeM - hDem0;

    if (f0 <= 0.0) {
        // Platform is at or below ground elevation
        return DemIntersectionResult { pt0, 0.0, pt0.altitudeM, hDem0, false, 1 };
    }

    // Phase 1: Forward Ray-Marching to bracket the first terrain collision
    double sPrev = 0.0;
    double fPrev = f0;
    double sCurr = std::max(config.minStepMeters, config.initialStepMeters);
    int steps = 1;
    bool bracketFound = false;

    while (sCurr <= config.maxSlantRangeMeters) {
        const auto pt = evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sCurr);
        steps++;

        const double hDem = dem.getElevationM(pt.latitudeDeg, pt.longitudeDeg)
                                .value_or(config.fallbackGroundElevationM);
        const double fCurr = pt.altitudeM - hDem;

        if (fCurr <= 0.0) {
            bracketFound = true;
            break;
        }

        sPrev = sCurr;
        fPrev = fCurr;

        // Adaptive step: smaller near terrain, larger when far above
        double step = config.initialStepMeters;
        if (fCurr < 50.0) {
            step = std::max(config.minStepMeters, config.initialStepMeters * 0.25);
        } else if (fCurr > 500.0) {
            step = config.initialStepMeters * 2.0;
        }
        sCurr += step;
    }

    if (!bracketFound) {
        return std::nullopt;
    }

    // Phase 2: Bounded False-Position (Illinois Secant / Bisection) Root Refinement
    double sA = sPrev;
    double fA = fPrev;
    double sB = sCurr;
    double fB = evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sB).altitudeM -
               dem.getElevationM(evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sB).latitudeDeg,
                                 evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sB).longitudeDeg)
                   .value_or(config.fallbackGroundElevationM);

    for (int iter = 0; iter < config.maxRootIterations; ++iter) {
        if (std::abs(sB - sA) <= config.toleranceMeters || std::abs(fB) <= config.toleranceMeters) {
            break;
        }

        // Illinois secant interpolation
        const double denom = fB - fA;
        double sMid = (std::abs(denom) > 1e-9) ? (sB - fB * (sB - sA) / denom)
                                               : (0.5 * (sA + sB));

        // Safeguard to bisection if secant step falls outside [sA, sB]
        if (sMid <= sA || sMid >= sB) {
            sMid = 0.5 * (sA + sB);
        }

        const auto ptMid = evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sMid);
        steps++;

        const double hDemMid = dem.getElevationM(ptMid.latitudeDeg, ptMid.longitudeDeg)
                                  .value_or(config.fallbackGroundElevationM);
        const double fMid = ptMid.altitudeM - hDemMid;

        if (std::abs(fMid) <= config.toleranceMeters) {
            sB = sMid;
            fB = fMid;
            break;
        }

        if (fMid <= 0.0) {
            sB = sMid;
            fB = fMid;
        } else {
            sA = sMid;
            fA = fMid;
        }
    }

    const auto finalPt = evaluateRayPoint(platformPos, bearing, gimbalTiltDeg, sB);
    const double finalDem = dem.getElevationM(finalPt.latitudeDeg, finalPt.longitudeDeg)
                                .value_or(config.fallbackGroundElevationM);

    Klv::GeoPoint3D targetGeo { finalPt.latitudeDeg, finalPt.longitudeDeg, finalDem };
    const bool occluded = (finalDem > (config.fallbackGroundElevationM + 25.0));

    return DemIntersectionResult {
        targetGeo,
        sB,
        finalPt.altitudeM,
        finalDem,
        occluded,
        steps
    };
}

} // namespace PayloadHal
