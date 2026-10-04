/// @file PtzSlewToCueBridge.cpp
/// @brief Implementation of geodetic cue ingestion, platform attitude compensation, and Pelco-D actuation.

#include "PtzSlewToCueBridge.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Tracking {

namespace {

constexpr double kPi { 3.14159265358979323846 };
constexpr double kEarthRadiusMeters { 6371008.8 };

inline double deg2rad(double deg) noexcept {
    return deg * (kPi / 180.0);
}

inline double rad2deg(double rad) noexcept {
    return rad * (180.0 / kPi);
}

inline double normalizeAngle360(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

struct Matrix3x3 {
    std::array<std::array<double, 3>, 3> m {};

    [[nodiscard]] static Matrix3x3 rotX(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = 1.0;
        r.m[1][1] = c;  r.m[1][2] = s;
        r.m[2][1] = -s; r.m[2][2] = c;
        return r;
    }

    [[nodiscard]] static Matrix3x3 rotY(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = c;  r.m[0][2] = -s;
        r.m[1][1] = 1.0;
        r.m[2][0] = s;  r.m[2][2] = c;
        return r;
    }

    [[nodiscard]] static Matrix3x3 rotZ(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = c;  r.m[0][1] = s;
        r.m[1][0] = -s; r.m[1][1] = c;
        r.m[2][2] = 1.0;
        return r;
    }

    [[nodiscard]] Matrix3x3 operator*(const Matrix3x3& o) const noexcept {
        Matrix3x3 r {};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < 3; ++k) {
                    sum += m[i][k] * o.m[k][j];
                }
                r.m[i][j] = sum;
            }
        }
        return r;
    }

    [[nodiscard]] Klv::Vector3D multVec(const Klv::Vector3D& v) const noexcept {
        return Klv::Vector3D {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }
};

} // namespace

PtzSlewToCueBridge::PtzSlewToCueBridge(PtzAutoTracker& tracker,
                                       const SlewBridgeConfig& config) noexcept
    : m_tracker(tracker)
    , m_config(config)
{
}

void PtzSlewToCueBridge::updateCue(const TargetCue& cue) noexcept
{
    m_activeCue = cue;
    if (m_state == SlewState::Idle || m_state == SlewState::TargetLost) {
        m_state = SlewState::CoarseAcquisition;
    }
}

void PtzSlewToCueBridge::clearCue() noexcept
{
    m_activeCue.reset();
    m_state = SlewState::Idle;
    m_tracker.reset();
}

void PtzSlewToCueBridge::updatePlatform(const PlatformNavState& platform) noexcept
{
    m_platform = platform;
}

double PtzSlewToCueBridge::shortestAngleDelta(double targetDeg, double currentDeg) noexcept
{
    double diff = std::fmod(targetDeg - currentDeg + 180.0, 360.0);
    if (diff < 0.0) {
        diff += 360.0;
    }
    return diff - 180.0;
}

bool PtzSlewToCueBridge::solveGimbalAngles(const PlatformNavState& platform,
                                          const Klv::GeoPoint3D& target,
                                          double& outGimbalPanDeg,
                                          double& outGimbalTiltDeg,
                                          double& outSlantRangeM) noexcept
{
    const Klv::GeoPoint2D pOrigin { platform.position.latitudeDeg, platform.position.longitudeDeg };
    const Klv::GeoPoint2D pTarget { target.latitudeDeg, target.longitudeDeg };

    const double horizDistanceM = Klv::KlvGeodesy::distanceMeters(pOrigin, pTarget);
    const double initialBearing = Klv::KlvGeodesy::bearingDeg(pOrigin, pTarget);

    const double deltaH = target.altitudeM - platform.position.altitudeM;

    // Vector in local topocentric NED frame
    const double bearingRad = deg2rad(initialBearing);
    const double xNorth = horizDistanceM * std::cos(bearingRad);
    const double yEast = horizDistanceM * std::sin(bearingRad);
    const double zDown = -deltaH;

    const double slantRange = std::sqrt(xNorth * xNorth + yEast * yEast + zDown * zDown);
    if (slantRange < 1e-3) {
        outGimbalPanDeg = 0.0;
        outGimbalTiltDeg = 0.0;
        outSlantRangeM = 0.0;
        return true;
    }

    outSlantRangeM = slantRange;
    const Klv::Vector3D rNed { xNorth, yEast, zDown };

    // Platform attitude rotation matrix from NED to Platform Coordinate System (PCS)
    // Yaw psi around Z (Heading), Pitch theta around Y, Roll phi around X
    const double psi = deg2rad(platform.headingDeg);
    const double theta = deg2rad(platform.pitchDeg);
    const double phi = deg2rad(platform.rollDeg);

    const Matrix3x3 rNedToPcs = Matrix3x3::rotX(phi) * (Matrix3x3::rotY(theta) * Matrix3x3::rotZ(psi));
    const Klv::Vector3D rPcs = rNedToPcs.multVec(rNed);

    // Compute Gimbal Pan and Tilt in Platform Body Frame:
    // +X is forward, +Y is starboard/right, +Z is down.
    // Pan = atan2(Y, X) [clockwise from body forward]
    // Tilt = asin(-Z / Range) [positive = looking upwards above body horizon]
    const double bodyPanRad = std::atan2(rPcs.y, rPcs.x);
    const double bodyTiltRad = std::asin(std::clamp(-rPcs.z / slantRange, -1.0, 1.0));

    outGimbalPanDeg = normalizeAngle360(rad2deg(bodyPanRad));
    outGimbalTiltDeg = rad2deg(bodyTiltRad);
    return true;
}

SlewCommandBatch PtzSlewToCueBridge::update(double currentPanDeg,
                                            double currentTiltDeg,
                                            double currentHfovDeg,
                                            double dt) noexcept
{
    SlewCommandBatch batch{};
    if (!m_activeCue.has_value()) {
        batch.state = SlewState::Idle;
        m_state = SlewState::Idle;
        return batch;
    }

    double targetPanDeg { 0.0 };
    double targetTiltDeg { 0.0 };
    double slantRangeM { 0.0 };

    if (!solveGimbalAngles(m_platform, m_activeCue->position, targetPanDeg, targetTiltDeg, slantRangeM)) {
        batch.state = SlewState::TargetLost;
        m_state = SlewState::TargetLost;
        return batch;
    }

    batch.targetGimbalAzDeg = targetPanDeg;
    batch.targetGimbalElDeg = targetTiltDeg;
    batch.slantRangeMeters = slantRangeM;

    const double azErrorDeg = shortestAngleDelta(targetPanDeg, currentPanDeg);
    const double elErrorDeg = targetTiltDeg - currentTiltDeg;

    batch.errorAzimuthDeg = azErrorDeg;
    batch.errorElevationDeg = elErrorDeg;

    const double absAzErr = std::abs(azErrorDeg);
    const double absElErr = std::abs(elErrorDeg);

    // Auto-Framing Zoom Computation
    if (m_config.enableAutoZoom && slantRangeM > 1.0 && m_activeCue->targetRadiusM > 0.0) {
        const double targetRadiusBounded = m_activeCue->targetRadiusM * m_config.zoomMarginFactor;
        const double optimalHfovRad = 2.0 * std::atan2(targetRadiusBounded, slantRangeM);
        const double optimalHfovDeg = std::clamp(rad2deg(optimalHfovRad), m_config.minHfovDeg, m_config.maxHfovDeg);
        batch.desiredHfovDeg = optimalHfovDeg;

        // Issue zoom commands if HFOV deviates by more than deadband
        const double hfovErr = currentHfovDeg - optimalHfovDeg;
        if (hfovErr > m_config.zoomChangeDeadbandDeg) {
            // Need narrower HFOV -> Zoom Tele (In)
            batch.zoomCmd = PelcoD::ProtocolBuilder::buildZoom(m_config.pelcoAddress, PelcoD::ZoomAction::Tele);
        } else if (hfovErr < -m_config.zoomChangeDeadbandDeg) {
            // Need wider HFOV -> Zoom Wide (Out)
            batch.zoomCmd = PelcoD::ProtocolBuilder::buildZoom(m_config.pelcoAddress, PelcoD::ZoomAction::Wide);
        } else {
            batch.zoomCmd = PelcoD::ProtocolBuilder::buildZoom(m_config.pelcoAddress, PelcoD::ZoomAction::Stop);
        }
    }

    // State machine logic
    if (absAzErr > m_config.coarseThresholdDeg || absElErr > m_config.coarseThresholdDeg) {
        m_state = SlewState::CoarseAcquisition;
        m_tracker.reset();

        // Convert target angles to Pelco-D centidegrees (1/100 of a degree)
        const auto panCentideg = static_cast<std::uint16_t>(
            std::clamp(std::round(targetPanDeg * 100.0), 0.0, 35999.0));

        // Pelco-D tilt is typically 0 to 35999 centidegrees or 0 to 9000 down / up
        // Normalize negative tilt to modulo 360 centidegrees
        const double normTiltDeg = (targetTiltDeg < 0.0) ? (360.0 + targetTiltDeg) : targetTiltDeg;
        const auto tiltCentideg = static_cast<std::uint16_t>(
            std::clamp(std::round(normTiltDeg * 100.0), 0.0, 35999.0));

        batch.coarsePanCmd = PelcoD::ProtocolBuilder::buildSetPan(m_config.pelcoAddress, panCentideg);
        batch.coarseTiltCmd = PelcoD::ProtocolBuilder::buildSetTilt(m_config.pelcoAddress, tiltCentideg);
    } else {
        // Within coarse acquisition threshold: switch to fine closed-loop rate tracking
        if (absAzErr <= m_config.settleToleranceDeg && absElErr <= m_config.settleToleranceDeg) {
            m_state = SlewState::Settled;
        } else {
            m_state = SlewState::FineTracking;
        }

        const auto trackingCmd = m_tracker.updateAngular(
            azErrorDeg,
            elErrorDeg,
            0.0, // target rate feedforward
            0.0,
            true, // isLocked
            false, // isCoasting
            dt
        );

        if (trackingCmd.shouldMove) {
            PelcoD::PanDirection panDir { PelcoD::PanDirection::Stop };
            if (trackingCmd.panDirection > 0) {
                panDir = PelcoD::PanDirection::Right;
            } else if (trackingCmd.panDirection < 0) {
                panDir = PelcoD::PanDirection::Left;
            }

            PelcoD::TiltDirection tiltDir { PelcoD::TiltDirection::Stop };
            if (trackingCmd.tiltDirection > 0) {
                tiltDir = PelcoD::TiltDirection::Up;
            } else if (trackingCmd.tiltDirection < 0) {
                tiltDir = PelcoD::TiltDirection::Down;
            }

            const auto pSpeed = static_cast<std::uint8_t>(std::clamp(trackingCmd.panSpeed, 0, 63));
            const auto tSpeed = static_cast<std::uint8_t>(std::clamp(trackingCmd.tiltSpeed, 0, 63));

            batch.rateMotionCmd = PelcoD::ProtocolBuilder::buildMotion(
                m_config.pelcoAddress,
                panDir,
                pSpeed,
                tiltDir,
                tSpeed
            );
        } else {
            batch.rateMotionCmd = PelcoD::ProtocolBuilder::buildStop(m_config.pelcoAddress);
        }
    }

    batch.state = m_state;
    return batch;
}

SlewState PtzSlewToCueBridge::getState() const noexcept
{
    return m_state;
}

bool PtzSlewToCueBridge::hasCue() const noexcept
{
    return m_activeCue.has_value();
}

const SlewBridgeConfig& PtzSlewToCueBridge::getConfig() const noexcept
{
    return m_config;
}

void PtzSlewToCueBridge::setConfig(const SlewBridgeConfig& config) noexcept
{
    m_config = config;
}

} // namespace Tracking
