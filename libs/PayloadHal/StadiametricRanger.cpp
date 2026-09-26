/// @file StadiametricRanger.cpp
/// @brief Implementation of Passive Stadiametric & Kinematic Triangulation Range Estimator.

#include "StadiametricRanger.h"
#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kDefaultAngularNoiseRad = 0.0015; // ~1.5 mrad pointing/tracking jitter

inline double clampVal(double val, double minVal, double maxVal) noexcept
{
    return std::max(minVal, std::min(maxVal, val));
}

inline Vector3D vecSub(const Vector3D& a, const Vector3D& b) noexcept
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3D vecAdd(const Vector3D& a, const Vector3D& b) noexcept
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector3D vecScale(const Vector3D& a, double s) noexcept
{
    return { a.x * s, a.y * s, a.z * s };
}

inline double vecDot(const Vector3D& a, const Vector3D& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double vecNorm(const Vector3D& a) noexcept
{
    return std::sqrt(vecDot(a, a));
}

/// @brief Inverts a 3x3 symmetric matrix via adjugate matrix.
/// @return true if matrix was non-singular and inverted, false otherwise.
bool invert3x3(const double A[3][3], double inv[3][3]) noexcept
{
    const double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
                     - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
                     + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (std::abs(det) < 1e-11) {
        return false;
    }

    const double invDet = 1.0 / det;

    inv[0][0] =  (A[1][1] * A[2][2] - A[1][2] * A[2][1]) * invDet;
    inv[0][1] = -(A[0][1] * A[2][2] - A[0][2] * A[2][1]) * invDet;
    inv[0][2] =  (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invDet;

    inv[1][0] = -(A[1][0] * A[2][2] - A[1][2] * A[2][0]) * invDet;
    inv[1][1] =  (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invDet;
    inv[1][2] = -(A[0][0] * A[1][2] - A[0][2] * A[1][0]) * invDet;

    inv[2][0] =  (A[1][0] * A[2][1] - A[1][1] * A[2][0]) * invDet;
    inv[2][1] = -(A[0][0] * A[2][1] - A[0][1] * A[2][0]) * invDet;
    inv[2][2] =  (A[0][0] * A[1][1] - A[0][1] * A[1][0]) * invDet;

    return true;
}

} // namespace

StadiametricRanger::StadiametricRanger(std::size_t maxHistory)
    : m_maxHistory(std::max<std::size_t>(2, maxHistory))
{
    m_customDims = defaultDimensionsForProfile(TargetClassProfile::MainBattleTank);
}

void StadiametricRanger::setTargetProfile(TargetClassProfile profile)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile = profile;
    if (profile != TargetClassProfile::Custom) {
        m_customDims = defaultDimensionsForProfile(profile);
    }
}

TargetClassProfile StadiametricRanger::targetProfile() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile;
}

void StadiametricRanger::setCustomTargetDimensions(const TargetPhysicalDimensions& dims)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customDims = dims;
    m_profile = TargetClassProfile::Custom;
}

TargetPhysicalDimensions StadiametricRanger::targetDimensions() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_profile == TargetClassProfile::Custom) {
        return m_customDims;
    }
    return defaultDimensionsForProfile(m_profile);
}

void StadiametricRanger::setOpticalCalibration(const OpticalFrameCalibration& calib)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_calib = calib;
}

OpticalFrameCalibration StadiametricRanger::opticalCalibration() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_calib;
}

void StadiametricRanger::setMinimumConvergenceAngleDeg(double deg) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minConvergenceAngleDeg = std::max(0.1, deg);
}

double StadiametricRanger::minimumConvergenceAngleDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_minConvergenceAngleDeg;
}

TargetPhysicalDimensions StadiametricRanger::defaultDimensionsForProfile(TargetClassProfile profile) noexcept
{
    switch (profile) {
    case TargetClassProfile::MainBattleTank:
        return { 3.5, 2.4, 7.0 };
    case TargetClassProfile::ArmoredPersonnelCarrier:
        return { 2.8, 2.2, 6.0 };
    case TargetClassProfile::TacticalVehicle:
        return { 2.0, 1.8, 4.8 };
    case TargetClassProfile::HumanPersonnel:
        return { 0.5, 1.8, 0.3 };
    case TargetClassProfile::PatrolVessel:
        return { 6.0, 4.5, 25.0 };
    case TargetClassProfile::Helicopter:
        return { 3.5, 3.8, 15.0 };
    case TargetClassProfile::FixedWingUav:
        return { 2.5, 0.8, 2.0 };
    case TargetClassProfile::Custom:
        return { 2.0, 1.8, 4.8 };
    }
    return { 2.0, 1.8, 4.8 };
}

Vector3D StadiametricRanger::anglesToUnitVectorNed(double azDeg, double elDeg) noexcept
{
    const double azRad = azDeg * kDegToRad;
    const double elRad = elDeg * kDegToRad;
    const double cosEl = std::cos(elRad);

    // North = +X, East = +Y, Down = +Z (el positive up -> Z is -sin(el))
    return {
        cosEl * std::cos(azRad),
        cosEl * std::sin(azRad),
        -std::sin(elRad)
    };
}

StadiametricEstimate StadiametricRanger::estimateStadiametricRange(
    const BoundingBoxDetection& bbox,
    StadiametricDimensionMode mode,
    double aspectAngleDeg) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StadiametricEstimate est {};

    if (bbox.normWidth <= 1e-4 || bbox.normHeight <= 1e-4) {
        return est;
    }
    if (m_calib.horizontalFovDeg <= 0.1 || m_calib.verticalFovDeg <= 0.1) {
        return est;
    }

    const auto dims = (m_profile == TargetClassProfile::Custom)
                          ? m_customDims
                          : defaultDimensionsForProfile(m_profile);

    // Decide active dimension mode
    auto activeMode = mode;
    if (activeMode == StadiametricDimensionMode::AutomaticBest) {
        // Height is typically far less aspect-dependent than width/length
        activeMode = StadiametricDimensionMode::Height;
    }

    if (activeMode == StadiametricDimensionMode::Height) {
        const double halfVfovRad = (m_calib.verticalFovDeg * 0.5) * kDegToRad;
        const double tanHalfVfov = std::tan(halfVfovRad);
        const double thetaVRad = 2.0 * std::atan(bbox.normHeight * tanHalfVfov);

        if (thetaVRad <= 1e-6) {
            return est;
        }

        est.subtendedAngleMrad = thetaVRad * 1000.0;
        est.slantRangeMeters = dims.heightMeters / (2.0 * std::tan(thetaVRad * 0.5));
        est.usedDimension = StadiametricDimensionMode::Height;

        // Uncertainty based on bounding box pixel jitter
        const double pxHeight = bbox.normHeight * static_cast<double>(m_calib.frameHeightPx);
        if (pxHeight > 1e-3) {
            const double jitterRatio = bbox.pixelJitter1Sigma / pxHeight;
            est.rangeUncertaintyMeters = est.slantRangeMeters * jitterRatio;
        }
        est.valid = (est.slantRangeMeters > 0.0);
    } else {
        // Width / Length with aspect angle projection
        const double aspectRad = aspectAngleDeg * kDegToRad;
        const double projWidth = std::abs(dims.widthMeters * std::cos(aspectRad))
                               + std::abs(dims.lengthMeters * std::sin(aspectRad));

        const double halfHfovRad = (m_calib.horizontalFovDeg * 0.5) * kDegToRad;
        const double tanHalfHfov = std::tan(halfHfovRad);
        const double thetaHRad = 2.0 * std::atan(bbox.normWidth * tanHalfHfov);

        if (thetaHRad <= 1e-6) {
            return est;
        }

        est.subtendedAngleMrad = thetaHRad * 1000.0;
        est.slantRangeMeters = projWidth / (2.0 * std::tan(thetaHRad * 0.5));
        est.usedDimension = StadiametricDimensionMode::Width;

        const double pxWidth = bbox.normWidth * static_cast<double>(m_calib.frameWidthPx);
        if (pxWidth > 1e-3) {
            const double jitterRatio = bbox.pixelJitter1Sigma / pxWidth;
            est.rangeUncertaintyMeters = est.slantRangeMeters * jitterRatio;
        }
        est.valid = (est.slantRangeMeters > 0.0);
    }

    return est;
}

void StadiametricRanger::addBearingObservation(const BearingObservation& obs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back(obs);
    while (m_history.size() > m_maxHistory) {
        m_history.pop_front();
    }
}

void StadiametricRanger::addBearingObservation(
    double azimuthDeg, double elevationDeg,
    const Vector3D& platformPositionNed,
    std::chrono::steady_clock::time_point timestamp)
{
    BearingObservation obs {};
    obs.platformPositionNedMeters = platformPositionNed;
    obs.losUnitVectorNed = anglesToUnitVectorNed(azimuthDeg, elevationDeg);
    obs.azimuthDeg = azimuthDeg;
    obs.elevationDeg = elevationDeg;
    obs.timestamp = timestamp;
    addBearingObservation(obs);
}

TriangulationEstimate StadiametricRanger::computeTwoPointTriangulation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    TriangulationEstimate est {};

    if (m_history.size() < 2) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    const auto& obs1 = m_history.front();
    const auto& obs2 = m_history.back();

    const Vector3D& P1 = obs1.platformPositionNedMeters;
    const Vector3D& P2 = obs2.platformPositionNedMeters;
    const Vector3D& u1 = obs1.losUnitVectorNed;
    const Vector3D& u2 = obs2.losUnitVectorNed;

    const Vector3D w0 = vecSub(P1, P2);
    est.baselineTraversedMeters = vecNorm(w0);

    const double b = vecDot(u1, u2);
    const double cosGamma = clampVal(b, -1.0, 1.0);
    const double gammaRad = std::acos(cosGamma);
    est.convergenceAngleDeg = gammaRad * kRadToDeg;

    // Check minimum convergence angle
    if (est.convergenceAngleDeg < m_minConvergenceAngleDeg || est.baselineTraversedMeters < 0.5) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    const double D = 1.0 - (b * b);
    if (std::abs(D) < 1e-9) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    const double d = vecDot(u1, w0);
    const double e = vecDot(u2, w0);

    // Sc: range along u1 from P1; Tc: range along u2 from P2
    const double sc = (b * e - d) / D;
    const double tc = (e - b * d) / D;

    if (sc <= 0.0 || tc <= 0.0) {
        // Rays diverge behind platforms
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    const Vector3D Q1 = vecAdd(P1, vecScale(u1, sc));
    const Vector3D Q2 = vecAdd(P2, vecScale(u2, tc));

    est.targetPositionNedMeters = vecScale(vecAdd(Q1, Q2), 0.5);
    est.rayMissDistanceMeters = vecNorm(vecSub(Q1, Q2));
    est.slantRangeFromLatestMeters = tc;
    est.observationsUsed = 2;
    est.valid = true;

    // Range uncertainty estimation: sigma_R = R / sin(gamma) * sigma_theta
    const double sinGamma = std::sin(gammaRad);
    if (sinGamma > 1e-4) {
        est.rangeUncertaintyMeters = (tc / sinGamma) * kDefaultAngularNoiseRad;
    }

    if (est.convergenceAngleDeg < 5.0) {
        est.quality = TriangulationQuality::PoorGdop;
    } else if (est.convergenceAngleDeg < 15.0) {
        est.quality = TriangulationQuality::Acceptable;
    } else {
        est.quality = TriangulationQuality::Optimal;
    }

    return est;
}

TriangulationEstimate StadiametricRanger::computeBatchTriangulation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    TriangulationEstimate est {};

    const std::size_t N = m_history.size();
    if (N < 2) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    // Compute baseline traversed from first to last observation
    const auto& obsFirst = m_history.front();
    const auto& obsLast = m_history.back();
    est.baselineTraversedMeters = vecNorm(vecSub(obsLast.platformPositionNedMeters, obsFirst.platformPositionNedMeters));

    // Form normal equations: A * P_T = b
    // where A = sum(I - u_k * u_k^T), b = sum((I - u_k * u_k^T) * P_k)
    double A[3][3] = { {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0} };
    double bVec[3] = { 0.0, 0.0, 0.0 };

    for (const auto& obs : m_history) {
        const double ux = obs.losUnitVectorNed.x;
        const double uy = obs.losUnitVectorNed.y;
        const double uz = obs.losUnitVectorNed.z;

        // M_k = I - u * u^T
        const double M[3][3] = {
            { 1.0 - ux * ux,      -ux * uy,      -ux * uz },
            {     -uy * ux,  1.0 - uy * uy,      -uy * uz },
            {     -uz * ux,      -uz * uy,  1.0 - uz * uz }
        };

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                A[r][c] += M[r][c];
            }
        }

        const double Px = obs.platformPositionNedMeters.x;
        const double Py = obs.platformPositionNedMeters.y;
        const double Pz = obs.platformPositionNedMeters.z;

        bVec[0] += M[0][0] * Px + M[0][1] * Py + M[0][2] * Pz;
        bVec[1] += M[1][0] * Px + M[1][1] * Py + M[1][2] * Pz;
        bVec[2] += M[2][0] * Px + M[2][1] * Py + M[2][2] * Pz;
    }

    double invA[3][3];
    if (!invert3x3(A, invA)) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    // P_T = invA * b
    est.targetPositionNedMeters.x = invA[0][0] * bVec[0] + invA[0][1] * bVec[1] + invA[0][2] * bVec[2];
    est.targetPositionNedMeters.y = invA[1][0] * bVec[0] + invA[1][1] * bVec[1] + invA[1][2] * bVec[2];
    est.targetPositionNedMeters.z = invA[2][0] * bVec[0] + invA[2][1] * bVec[1] + invA[2][2] * bVec[2];

    // Compute RMS ray miss distance
    double sumSqResiduals = 0.0;
    for (const auto& obs : m_history) {
        const Vector3D diff = vecSub(est.targetPositionNedMeters, obs.platformPositionNedMeters);
        const double proj = vecDot(diff, obs.losUnitVectorNed);
        const Vector3D perp = vecSub(diff, vecScale(obs.losUnitVectorNed, proj));
        sumSqResiduals += vecDot(perp, perp);
    }
    est.rayMissDistanceMeters = std::sqrt(sumSqResiduals / static_cast<double>(N));

    // Slant range from latest platform position
    const Vector3D relTarget = vecSub(est.targetPositionNedMeters, obsLast.platformPositionNedMeters);
    est.slantRangeFromLatestMeters = vecNorm(relTarget);
    est.observationsUsed = N;

    // Total baseline angular span between first and last observation
    const double dotFl = clampVal(vecDot(obsFirst.losUnitVectorNed, obsLast.losUnitVectorNed), -1.0, 1.0);
    est.convergenceAngleDeg = std::acos(dotFl) * kRadToDeg;

    if (est.convergenceAngleDeg < m_minConvergenceAngleDeg || est.baselineTraversedMeters < 0.5) {
        est.quality = TriangulationQuality::InsufficientBaseline;
        return est;
    }

    // Covariance estimation: sigma_R along latest LOS
    const Vector3D& uN = obsLast.losUnitVectorNed;
    // Var(R) = uN^T * Cov(P_T) * uN, where Cov(P_T) = sigma_theta^2 * invA
    const double invAu0 = invA[0][0] * uN.x + invA[0][1] * uN.y + invA[0][2] * uN.z;
    const double invAu1 = invA[1][0] * uN.x + invA[1][1] * uN.y + invA[1][2] * uN.z;
    const double invAu2 = invA[2][0] * uN.x + invA[2][1] * uN.y + invA[2][2] * uN.z;
    const double quadForm = uN.x * invAu0 + uN.y * invAu1 + uN.z * invAu2;

    if (quadForm > 0.0) {
        est.rangeUncertaintyMeters = std::sqrt(quadForm) * kDefaultAngularNoiseRad * est.slantRangeFromLatestMeters;
    } else {
        est.rangeUncertaintyMeters = (est.slantRangeFromLatestMeters / std::max(0.01, std::sin(est.convergenceAngleDeg * kDegToRad))) * kDefaultAngularNoiseRad;
    }

    est.valid = (est.slantRangeFromLatestMeters > 0.0);

    if (est.convergenceAngleDeg < 5.0) {
        est.quality = TriangulationQuality::PoorGdop;
    } else if (est.convergenceAngleDeg < 15.0) {
        est.quality = TriangulationQuality::Acceptable;
    } else {
        est.quality = TriangulationQuality::Optimal;
    }

    return est;
}

void StadiametricRanger::resetTriangulation()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

std::size_t StadiametricRanger::observationCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history.size();
}

PassiveRangeSolution StadiametricRanger::computeFusedRange(
    const std::optional<BoundingBoxDetection>& bbox,
    double aspectAngleDeg) const
{
    PassiveRangeSolution sol {};

    if (bbox.has_value()) {
        sol.stadiametric = estimateStadiametricRange(*bbox, StadiametricDimensionMode::AutomaticBest, aspectAngleDeg);
        sol.stadiametricAvailable = sol.stadiametric.valid;
    }

    sol.triangulation = computeBatchTriangulation();
    sol.triangulationAvailable = sol.triangulation.valid;

    if (sol.stadiametricAvailable && sol.triangulationAvailable) {
        // Optimal inverse-variance fusion
        const double varStad = std::max(1.0, sol.stadiametric.rangeUncertaintyMeters * sol.stadiametric.rangeUncertaintyMeters);
        const double varTri = std::max(1.0, sol.triangulation.rangeUncertaintyMeters * sol.triangulation.rangeUncertaintyMeters);

        const double wStad = 1.0 / varStad;
        const double wTri = 1.0 / varTri;
        const double sumW = wStad + wTri;

        sol.estimatedRangeMeters = (wStad * sol.stadiametric.slantRangeMeters + wTri * sol.triangulation.slantRangeFromLatestMeters) / sumW;
        sol.rangeUncertaintyMeters = std::sqrt(1.0 / sumW);
        sol.confidence01 = clampVal(1.0 - (sol.rangeUncertaintyMeters / sol.estimatedRangeMeters), 0.1, 0.99);
        sol.valid = true;
        sol.diagnosticText = "FUSED: STADIA+TRIANG";
    } else if (sol.triangulationAvailable) {
        sol.estimatedRangeMeters = sol.triangulation.slantRangeFromLatestMeters;
        sol.rangeUncertaintyMeters = sol.triangulation.rangeUncertaintyMeters;
        sol.confidence01 = clampVal(1.0 - (sol.rangeUncertaintyMeters / sol.estimatedRangeMeters), 0.1, 0.95);
        sol.valid = true;
        sol.diagnosticText = "KINEMATIC TRIANGULATION";
    } else if (sol.stadiametricAvailable) {
        sol.estimatedRangeMeters = sol.stadiametric.slantRangeMeters;
        sol.rangeUncertaintyMeters = sol.stadiametric.rangeUncertaintyMeters;
        sol.confidence01 = clampVal(1.0 - (sol.rangeUncertaintyMeters / sol.estimatedRangeMeters), 0.1, 0.90);
        sol.valid = true;
        sol.diagnosticText = "OPTICAL STADIAMETRIC";
    } else {
        sol.valid = false;
        sol.diagnosticText = "NO PASSIVE RANGE SOLUTION";
    }

    return sol;
}

void StadiametricRanger::updateFromPayload(
    const IPayload& payload,
    const Vector3D& platformPositionNed,
    const std::optional<BoundingBoxDetection>& bbox)
{
    // Update camera calibration if daylight or active camera is available
    if (auto cam = payload.primaryCamera()) {
        const auto camTelem = cam->currentTelemetry();
        OpticalFrameCalibration calib {};
        calib.horizontalFovDeg = camTelem.horizontalFovDeg;
        calib.verticalFovDeg = camTelem.verticalFovDeg;
        calib.frameWidthPx = 1920;
        calib.frameHeightPx = 1080;
        setOpticalCalibration(calib);
    }

    // Ingest current pan/tilt angles
    if (auto ptu = payload.panTilt()) {
        const auto ptuTelem = ptu->currentTelemetry();
        addBearingObservation(ptuTelem.panAngleDeg, ptuTelem.tiltAngleDeg, platformPositionNed);
    }

    // If bounding box was provided, fused range can immediately be calculated or logged
    if (bbox.has_value()) {
        (void)computeFusedRange(bbox);
    }
}

} // namespace PayloadHal
