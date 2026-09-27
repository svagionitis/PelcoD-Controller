#include "LaserDesignatorCoordinator.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {
    inline constexpr double PI = 3.14159265358979323846;
    inline constexpr double DEG_TO_RAD = PI / 180.0;
    inline constexpr double RAD_TO_DEG = 180.0 / PI;

    [[nodiscard]] double normalizeHeadingDeg(double deg) noexcept
    {
        double h = std::fmod(deg, 360.0);
        if (h < 0.0) {
            h += 360.0;
        }
        return h;
    }
} // namespace

LaserDesignatorCoordinator::LaserDesignatorCoordinator(
    std::shared_ptr<IPanTiltUnit> ptu,
    std::shared_ptr<IDemProvider> dem,
    std::shared_ptr<GimbalSectorBlanking> blanking,
    DesignatorConfig config) noexcept
    : m_ptu(std::move(ptu))
    , m_dem(std::move(dem))
    , m_blanking(std::move(blanking))
    , m_config(std::move(config))
    , m_prfCode(m_config.defaultPrfCode)
    , m_lstTargetCode(m_config.defaultPrfCode)
{
    if (m_blanking) {
        m_blanking->registerInterlockCallback([this](bool allowed, const std::string& reason) {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if (!allowed && m_state == DesignatorState::Designating) {
                m_cooldownRemainingSec = m_burstDurationSec * m_config.enforcedCooldownRatio;
                transitionDesignatorState(DesignatorState::CoolingDown, "Designation inhibited by keep-out sector: " + reason);
            }
        });
    }
}

LaserDesignatorCoordinator::~LaserDesignatorCoordinator()
{
    if (isDesignating()) {
        stopDesignation();
    }
    triggerEmergencyDump();
}

void LaserDesignatorCoordinator::setConfig(const DesignatorConfig& config)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_config = config;
}

DesignatorConfig LaserDesignatorCoordinator::config() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_config;
}

bool LaserDesignatorCoordinator::setPrfCode(std::uint16_t code)
{
    if (!Stanag3733::isValidCode(code)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_prfCode = code;
    return true;
}

std::uint16_t LaserDesignatorCoordinator::prfCode() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_prfCode;
}

PrfBand LaserDesignatorCoordinator::prfBand() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return Stanag3733::bandOf(m_prfCode);
}

double LaserDesignatorCoordinator::pulseIntervalMicroseconds() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return Stanag3733::pulseIntervalUs(m_prfCode);
}

double LaserDesignatorCoordinator::pulseFrequencyHz() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return Stanag3733::pulseFrequencyHz(m_prfCode);
}

bool LaserDesignatorCoordinator::armDesignator()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_state == DesignatorState::ThermalCutoff) {
        return false;
    }
    if (m_state == DesignatorState::CoolingDown && m_cooldownRemainingSec > 0.0) {
        return false;
    }
    if (isKeepOutInhibited()) {
        return false;
    }

    m_capState = CapacitorState::Charging;
    transitionDesignatorState(DesignatorState::Armed, "Designator armed; charging capacitor bank");
    return true;
}

bool LaserDesignatorCoordinator::disarmDesignator()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == DesignatorState::Designating) {
        m_state = DesignatorState::Armed;
    }
    m_capState = CapacitorState::Dumping;
    transitionDesignatorState(DesignatorState::Disarmed, "Designator disarmed by operator");
    return true;
}

bool LaserDesignatorCoordinator::isArmed() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (m_state == DesignatorState::Armed ||
            m_state == DesignatorState::Charging ||
            m_state == DesignatorState::ReadyToFire ||
            m_state == DesignatorState::Designating);
}

bool LaserDesignatorCoordinator::startDesignation()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_state != DesignatorState::Armed && m_state != DesignatorState::ReadyToFire) {
        return false;
    }
    if (isKeepOutInhibited()) {
        return false;
    }
    if (m_diodeTempC >= m_config.cutoffTempThresholdC) {
        return false;
    }
    if (m_capVoltageVolts < m_config.firingVoltageVolts * 0.90) {
        return false; // Capacitor bank not sufficiently charged
    }

    m_burstDurationSec = 0.0;
    transitionDesignatorState(DesignatorState::Designating, "Laser target designation pulse firing started");
    return true;
}

bool LaserDesignatorCoordinator::stopDesignation()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state != DesignatorState::Designating) {
        return false;
    }

    m_cooldownRemainingSec = m_burstDurationSec * m_config.enforcedCooldownRatio;
    if (m_cooldownRemainingSec > 0.0) {
        transitionDesignatorState(DesignatorState::CoolingDown, "Designation burst complete; cooling down");
    } else {
        transitionDesignatorState(DesignatorState::ReadyToFire, "Designation stopped; ready to fire");
    }
    return true;
}

bool LaserDesignatorCoordinator::isDesignating() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (m_state == DesignatorState::Designating);
}

DesignatorState LaserDesignatorCoordinator::designatorState() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_state;
}

void LaserDesignatorCoordinator::triggerEmergencyDump()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_state == DesignatorState::Designating) {
        m_state = DesignatorState::Armed;
    }
    m_capState = CapacitorState::Dumping;
    transitionDesignatorState(DesignatorState::Disarmed, "Emergency capacitor dump initiated");
}

void LaserDesignatorCoordinator::update(double deltaSeconds, double ambientTempC)
{
    const double dt = std::clamp(deltaSeconds, 0.0, 1.0);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // =========================================================================
    // 1. Capacitor Bank Physics
    // =========================================================================
    if (m_capState == CapacitorState::Charging) {
        const double dV = (m_config.chargingCurrentAmps / m_config.capacitorCapacitanceFarads) * dt;
        m_capVoltageVolts = std::min(m_config.firingVoltageVolts, m_capVoltageVolts + dV);
        if (m_capVoltageVolts >= m_config.firingVoltageVolts) {
            m_capState = CapacitorState::Ready;
            if (m_state == DesignatorState::Armed) {
                transitionDesignatorState(DesignatorState::ReadyToFire, "Capacitor bank charged to operational threshold");
            }
        }
    } else if (m_capState == CapacitorState::Dumping) {
        constexpr double bleedResistanceOhms = 50.0;
        const double tau = bleedResistanceOhms * m_config.capacitorCapacitanceFarads;
        const double dV = (m_capVoltageVolts / (tau > 0.0 ? tau : 0.02)) * dt;
        m_capVoltageVolts = std::max(0.0, m_capVoltageVolts - dV);
        if (m_capVoltageVolts < 5.0) {
            m_capVoltageVolts = 0.0;
            m_capState = CapacitorState::Discharged;
        }
    }

    // =========================================================================
    // 2. Diode & Rod Thermal Dissipation
    // =========================================================================
    if (m_state == DesignatorState::Designating) {
        if (isKeepOutInhibited()) {
            m_cooldownRemainingSec = m_burstDurationSec * m_config.enforcedCooldownRatio;
            transitionDesignatorState(DesignatorState::CoolingDown, "Designation inhibited by keep-out sector");
            return;
        }

        m_burstDurationSec += dt;

        const double prfHz = Stanag3733::pulseFrequencyHz(m_prfCode);
        const double pHeat = (1.0 - m_config.opticalEfficiency) * m_config.pulseEnergyJoules * prfHz;
        const double pDiss = m_config.thermalDissipationWPerC * (m_diodeTempC - m_heatSinkTempC);
        const double dT = ((pHeat - pDiss) / m_config.thermalCapacitanceJPerC) * dt;

        m_diodeTempC += dT;
        m_heatSinkTempC += 0.25 * dT;

        // Over-temperature critical cutoff check
        if (m_diodeTempC >= m_config.cutoffTempThresholdC) {
            m_thermalTripCount++;
            m_capState = CapacitorState::Dumping;
            m_cooldownRemainingSec = m_burstDurationSec * m_config.enforcedCooldownRatio * 1.5;
            transitionDesignatorState(DesignatorState::ThermalCutoff, "Critical over-temperature cutoff reached (>= 65°C)");
            return;
        }

        // Maximum continuous burst time check
        if (m_burstDurationSec >= m_config.maxContinuousBurstSec) {
            m_cooldownRemainingSec = m_burstDurationSec * m_config.enforcedCooldownRatio;
            transitionDesignatorState(DesignatorState::CoolingDown, "Max continuous designation burst time elapsed");
            return;
        }
    } else {
        // Natural convective cooling
        const double pDiss = m_config.thermalDissipationWPerC * (m_diodeTempC - ambientTempC);
        const double dT = (pDiss / m_config.thermalCapacitanceJPerC) * dt;
        m_diodeTempC = std::max(ambientTempC, m_diodeTempC - dT);
        m_heatSinkTempC = std::max(ambientTempC, m_heatSinkTempC - (0.5 * dT));

        if (m_cooldownRemainingSec > 0.0) {
            m_cooldownRemainingSec = std::max(0.0, m_cooldownRemainingSec - dt);
        }

        if (m_state == DesignatorState::CoolingDown) {
            if (m_cooldownRemainingSec <= 0.0 && m_diodeTempC <= m_config.safeRecoveryTempThresholdC) {
                transitionDesignatorState(DesignatorState::Armed, "Thermal cooldown complete; re-armed");
            }
        } else if (m_state == DesignatorState::ThermalCutoff) {
            if (m_diodeTempC <= m_config.safeRecoveryTempThresholdC && m_cooldownRemainingSec <= 0.0) {
                transitionDesignatorState(DesignatorState::Disarmed, "Safe recovery temperature restored following thermal cutoff");
            }
        }
    }

    // =========================================================================
    // 3. Laser Spot Tracker (LST) Seeker & Closed-Loop Auto-Cueing
    // =========================================================================
    if (m_lstState == LstState::Searching) {
        if (m_currentSpot.has_value() && m_currentSpot->valid) {
            m_currentSpot->prfMatched = (m_currentSpot->detectedPrfCode == m_lstTargetCode);
            if (m_currentSpot->prfMatched) {
                transitionLstState(LstState::Acquired);
            }
        }
    } else if (m_lstState == LstState::Acquired) {
        transitionLstState(LstState::Tracking);
    } else if (m_lstState == LstState::Tracking) {
        if (m_currentSpot.has_value() && m_currentSpot->valid && m_currentSpot->prfMatched) {
            m_coastTimerSec = 0.0;
            if (m_autoCueingEnabled && m_ptu) {
                const double ratePan = m_currentSpot->errorX * m_config.lstTrackingGain * 20.0;
                const double rateTilt = m_currentSpot->errorY * m_config.lstTrackingGain * 15.0;
                m_ptu->setRate(ratePan, rateTilt);
            }
        } else {
            transitionLstState(LstState::Coasting);
            m_coastTimerSec = 0.0;
        }
    } else if (m_lstState == LstState::Coasting) {
        m_coastTimerSec += dt;
        if (m_currentSpot.has_value() && m_currentSpot->valid && m_currentSpot->prfMatched) {
            transitionLstState(LstState::Tracking);
        } else if (m_coastTimerSec >= m_config.lstCoastTimeoutSec) {
            transitionLstState(LstState::Lost);
            if (m_autoCueingEnabled && m_ptu) {
                m_ptu->stopMotion();
            }
        }
    }
}

LtdThermalTelemetry LaserDesignatorCoordinator::thermalTelemetry() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LtdThermalTelemetry t {};
    t.diodeTempC = m_diodeTempC;
    t.heatSinkTempC = m_heatSinkTempC;
    t.capacityUtilization01 = std::clamp((m_diodeTempC - 25.0) / (m_config.cutoffTempThresholdC - 25.0), 0.0, 1.0);
    t.burstTimeRemainingSec = std::max(0.0, m_config.maxContinuousBurstSec - m_burstDurationSec);
    t.coolingTimeRemainingSec = m_cooldownRemainingSec;
    t.thermalTripCount = m_thermalTripCount;

    if (m_diodeTempC >= m_config.cutoffTempThresholdC || m_state == DesignatorState::ThermalCutoff) {
        t.zone = LtdThermalZone::Critical;
    } else if (m_diodeTempC >= m_config.warningTempThresholdC) {
        t.zone = LtdThermalZone::Warning;
    } else {
        t.zone = LtdThermalZone::Nominal;
    }
    return t;
}

CapacitorTelemetry LaserDesignatorCoordinator::capacitorTelemetry() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    CapacitorTelemetry c {};
    c.voltageVolts = m_capVoltageVolts;
    c.targetVoltageVolts = m_config.firingVoltageVolts;
    c.energyJoules = 0.5 * m_config.capacitorCapacitanceFarads * (m_capVoltageVolts * m_capVoltageVolts);
    c.chargePercent = std::clamp((m_capVoltageVolts / (m_config.firingVoltageVolts > 0.0 ? m_config.firingVoltageVolts : 1.0)) * 100.0, 0.0, 100.0);
    c.state = m_capState;
    return c;
}

LaserHazardFan LaserDesignatorCoordinator::computeHazardFan(
    const Klv::GeoPoint3D& platformPos, double platformHeadingDeg) const
{
    LaserHazardFan fan {};
    fan.nohdMeters = 12000.0;
    fan.enohdMeters = 35000.0;

    double panDeg = 0.0;
    double tiltDeg = -10.0;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        fan.bufferHalfAngleDeg = m_config.safetyBufferAngleDeg;
        fan.isDesignatorInhibited = isKeepOutInhibited();
    }

    if (m_ptu) {
        const auto ptuTelem = m_ptu->currentTelemetry();
        panDeg = ptuTelem.panAngleDeg;
        tiltDeg = ptuTelem.tiltAngleDeg;
    }

    // Ground target intersection calculation
    const auto groundTarget = GeoreferenceUtils::computeTargetFromGroundIntersection(
        platformPos, platformHeadingDeg, panDeg, tiltDeg, 0.0);

    if (groundTarget.has_value()) {
        fan.targetCoordinate = *groundTarget;
    } else {
        // Fallback to NOHD projection along LOS
        const auto slantTarget = GeoreferenceUtils::computeTargetFromSlantRange(
            platformPos, platformHeadingDeg, panDeg, tiltDeg, fan.nohdMeters);
        if (slantTarget.has_value()) {
            fan.targetCoordinate = *slantTarget;
        }
    }

    const double losBearingDeg = normalizeHeadingDeg(platformHeadingDeg + panDeg);
    const double leftBearingDeg = normalizeHeadingDeg(losBearingDeg - fan.bufferHalfAngleDeg);
    const double rightBearingDeg = normalizeHeadingDeg(losBearingDeg + fan.bufferHalfAngleDeg);

    // Build 2D hazard fan footprint polygon on ground
    const Klv::GeoPoint2D apex { platformPos.latitudeDeg, platformPos.longitudeDeg };
    fan.hazardPolygon.push_back(apex);

    // Project Left and Right corridor boundary points
    const double projDistM = (groundTarget.has_value())
        ? std::min(fan.nohdMeters, 15000.0)
        : fan.nohdMeters;

    const double dLatDeg = (projDistM / 111320.0);
    const double dLonDeg = (projDistM / (111320.0 * std::cos(platformPos.latitudeDeg * DEG_TO_RAD)));

    const Klv::GeoPoint2D leftPt {
        platformPos.latitudeDeg + (dLatDeg * std::cos(leftBearingDeg * DEG_TO_RAD)),
        platformPos.longitudeDeg + (dLonDeg * std::sin(leftBearingDeg * DEG_TO_RAD))
    };
    const Klv::GeoPoint2D centerPt {
        fan.targetCoordinate.latitudeDeg,
        fan.targetCoordinate.longitudeDeg
    };
    const Klv::GeoPoint2D rightPt {
        platformPos.latitudeDeg + (dLatDeg * std::cos(rightBearingDeg * DEG_TO_RAD)),
        platformPos.longitudeDeg + (dLonDeg * std::sin(rightBearingDeg * DEG_TO_RAD))
    };

    fan.hazardPolygon.push_back(leftPt);
    fan.hazardPolygon.push_back(centerPt);
    fan.hazardPolygon.push_back(rightPt);

    // Recommended weapon delivery entry corridor (target bearing ± 60°, excluding ±10° direct beam)
    fan.minSafeAttackHeadingDeg = normalizeHeadingDeg(losBearingDeg - 60.0);
    fan.maxSafeAttackHeadingDeg = normalizeHeadingDeg(losBearingDeg + 60.0);

    return fan;
}

void LaserDesignatorCoordinator::setLstTargetPrfCode(std::uint16_t code)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lstTargetCode = code;
}

std::uint16_t LaserDesignatorCoordinator::lstTargetPrfCode() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lstTargetCode;
}

void LaserDesignatorCoordinator::startLstSearch()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    transitionLstState(LstState::Searching);
}

void LaserDesignatorCoordinator::stopLstSearch()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    transitionLstState(LstState::Off);
    if (m_ptu && m_autoCueingEnabled) {
        m_ptu->stopMotion();
    }
}

LstState LaserDesignatorCoordinator::lstState() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lstState;
}

std::optional<LstSpotMeasurement> LaserDesignatorCoordinator::currentSpotMeasurement() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_currentSpot;
}

void LaserDesignatorCoordinator::injectSimulatedSpot(
    double errorX, double errorY, std::uint16_t detectedCode, double irradianceWattsPerM2, double snrDb)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LstSpotMeasurement m {};
    m.valid = true;
    m.errorX = std::clamp(errorX, -1.0, 1.0);
    m.errorY = std::clamp(errorY, -1.0, 1.0);
    m.detectedPrfCode = detectedCode;
    m.prfMatched = (detectedCode == m_lstTargetCode);
    m.irradianceWattsPerM2 = irradianceWattsPerM2;
    m.snrDb = snrDb;
    m.timestamp = std::chrono::system_clock::now();
    m_currentSpot = m;
}

void LaserDesignatorCoordinator::inject4QuadrantEnergies(
    double a, double b, double c, double d, std::uint16_t detectedCode)
{
    const double total = a + b + c + d;
    if (total <= 1e-9) {
        clearSimulatedSpot();
        return;
    }

    const double errX = ((b + d) - (a + c)) / total;
    const double errY = ((a + b) - (c + d)) / total;
    injectSimulatedSpot(errX, errY, detectedCode, total * 1e-3, 20.0 * std::log10(total + 1.0));
}

void LaserDesignatorCoordinator::clearSimulatedSpot()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_currentSpot.reset();
}

void LaserDesignatorCoordinator::setAutoCueingEnabled(bool enable) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_autoCueingEnabled = enable;
    if (!enable && m_ptu) {
        m_ptu->stopMotion();
    }
}

bool LaserDesignatorCoordinator::isAutoCueingEnabled() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_autoCueingEnabled;
}

void LaserDesignatorCoordinator::setDesignatorStateCallback(DesignatorStateCallback cb)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

void LaserDesignatorCoordinator::setLstStateCallback(LstStateCallback cb)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lstCallback = std::move(cb);
}

void LaserDesignatorCoordinator::transitionDesignatorState(DesignatorState newState, const std::string& reason)
{
    m_state = newState;
    if (m_stateCallback) {
        m_stateCallback(newState, reason);
    }
}

void LaserDesignatorCoordinator::transitionLstState(LstState newState)
{
    m_lstState = newState;
    if (m_lstCallback) {
        LstSpotMeasurement empty {};
        m_lstCallback(newState, m_currentSpot.value_or(empty));
    }
}

bool LaserDesignatorCoordinator::isKeepOutInhibited() const noexcept
{
    if (!m_blanking || !m_ptu) {
        return false;
    }
    const auto ptuTelem = m_ptu->currentTelemetry();
    return !m_blanking->isLaserAllowed(ptuTelem.panAngleDeg, ptuTelem.tiltAngleDeg);
}

} // namespace PayloadHal
