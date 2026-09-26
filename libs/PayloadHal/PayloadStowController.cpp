/// @file PayloadStowController.cpp
/// @brief Implementation of Payload Stow, De-Ice/Wiper Routine & Emergency Park Controller.

#include "PayloadStowController.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;

double normalizeAngleDeg(double angleDeg) noexcept
{
    while (angleDeg > 180.0) {
        angleDeg -= 360.0;
    }
    while (angleDeg <= -180.0) {
        angleDeg += 360.0;
    }
    return angleDeg;
}

double angleDifferenceDeg(double aDeg, double bDeg) noexcept
{
    return std::abs(normalizeAngleDeg(aDeg - bDeg));
}

} // namespace

PayloadStowController::PayloadStowController(
    std::shared_ptr<IPanTiltUnit> ptu,
    std::shared_ptr<IPtzPresetManager> presetMgr,
    std::shared_ptr<ICameraPayload> primaryCamera)
    : m_ptu(std::move(ptu))
    , m_presetMgr(std::move(presetMgr))
    , m_primaryCam(std::move(primaryCamera))
{
}

PayloadStowController::~PayloadStowController() = default;

void PayloadStowController::setStowOrientation(const StowOrientation& orientation) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stowPose = orientation;
}

StowOrientation PayloadStowController::stowOrientation() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stowPose;
}

void PayloadStowController::setDeployOrientation(const StowOrientation& orientation) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deployPose = orientation;
}

StowOrientation PayloadStowController::deployOrientation() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deployPose;
}

void PayloadStowController::setMaintenanceOrientation(const StowOrientation& orientation) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maintenancePose = orientation;
}

StowOrientation PayloadStowController::maintenanceOrientation() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maintenancePose;
}

bool PayloadStowController::stow()
{
    std::shared_ptr<IPanTiltUnit> ptu;
    std::shared_ptr<ICameraPayload> cam;
    StowOrientation pose;
    StowState prevState {};
    StateChangedCallback stateCb;
    bool alreadyArrived = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isZeroized) {
            return false;
        }

        m_mechanicalLockEngaged = false;
        ptu = m_ptu;
        cam = m_primaryCam;
        pose = m_stowPose;
        prevState = m_state;
        stateCb = m_stateCallback;

        if (checkArrivalLocked(pose)) {
            m_state = StowState::Stowed;
            alreadyArrived = true;
        } else {
            m_state = StowState::Stowing;
        }
    }

    if (cam) {
        cam->setZoomNormalized(0.0);
    }

    if (ptu) {
        if (ptu->hasRollAxis()) {
            ptu->setAbsoluteAngles3Axis(pose.panDeg, pose.tiltDeg, pose.rollDeg);
        } else {
            ptu->setAbsoluteAngles(pose.panDeg, pose.tiltDeg);
        }
    }

    if (stateCb) {
        stateCb(prevState, alreadyArrived ? StowState::Stowed : StowState::Stowing, "Stow command dispatched");
    }

    return true;
}

bool PayloadStowController::deploy()
{
    std::shared_ptr<IPanTiltUnit> ptu;
    StowOrientation pose;
    StowState prevState {};
    StateChangedCallback stateCb;
    InterlockAlertCallback interlockCb;
    bool alreadyArrived = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isZeroized) {
            return false;
        }

        if (m_vehicleMotionInterlockEnabled && m_vehicleMotionActive) {
            interlockCb = m_interlockCallback;
            if (interlockCb) {
                interlockCb(false, "Deploy blocked: vehicle motion active interlock");
            }
            return false;
        }

        m_mechanicalLockEngaged = false;
        ptu = m_ptu;
        pose = m_deployPose;
        prevState = m_state;
        stateCb = m_stateCallback;

        if (checkArrivalLocked(pose)) {
            m_state = StowState::Deployed;
            alreadyArrived = true;
        } else {
            m_state = StowState::Deploying;
        }
    }

    if (ptu) {
        if (ptu->hasRollAxis()) {
            ptu->setAbsoluteAngles3Axis(pose.panDeg, pose.tiltDeg, pose.rollDeg);
        } else {
            ptu->setAbsoluteAngles(pose.panDeg, pose.tiltDeg);
        }
    }

    if (stateCb) {
        stateCb(prevState, alreadyArrived ? StowState::Deployed : StowState::Deploying, "Deploy command dispatched");
    }

    return true;
}

bool PayloadStowController::moveToMaintenance()
{
    std::shared_ptr<IPanTiltUnit> ptu;
    StowOrientation pose;
    StowState prevState {};
    StateChangedCallback stateCb;
    InterlockAlertCallback interlockCb;
    bool alreadyArrived = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isZeroized) {
            return false;
        }

        if (m_vehicleMotionInterlockEnabled && m_vehicleMotionActive) {
            interlockCb = m_interlockCallback;
            if (interlockCb) {
                interlockCb(false, "Maintenance move blocked: vehicle motion active interlock");
            }
            return false;
        }

        m_mechanicalLockEngaged = false;
        ptu = m_ptu;
        pose = m_maintenancePose;
        prevState = m_state;
        stateCb = m_stateCallback;

        if (checkArrivalLocked(pose)) {
            m_state = StowState::Maintenance;
            alreadyArrived = true;
        } else {
            m_state = StowState::MovingToMaintenance;
        }
    }

    if (ptu) {
        if (ptu->hasRollAxis()) {
            ptu->setAbsoluteAngles3Axis(pose.panDeg, pose.tiltDeg, pose.rollDeg);
        } else {
            ptu->setAbsoluteAngles(pose.panDeg, pose.tiltDeg);
        }
    }

    if (stateCb) {
        stateCb(prevState, alreadyArrived ? StowState::Maintenance : StowState::MovingToMaintenance, "Moving to maintenance");
    }

    return true;
}

StowState PayloadStowController::state() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool PayloadStowController::isStowed() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == StowState::Stowed;
}

bool PayloadStowController::isDeployed() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == StowState::Deployed;
}

bool PayloadStowController::isSafeForVehicleMotion() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_state == StowState::Stowed || m_state == StowState::EmergencyParked);
}

void PayloadStowController::setVehicleMotionActive(bool moving) noexcept
{
    bool triggerAutoStow = false;
    InterlockAlertCallback alertCb;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_vehicleMotionActive = moving;

        if (moving && (m_state == StowState::Deployed || m_state == StowState::Deploying || m_state == StowState::Maintenance)) {
            alertCb = m_interlockCallback;
            if (m_autoStowOnVehicleMotion) {
                triggerAutoStow = true;
            }
        }
    }

    if (alertCb) {
        alertCb(false, "Warning: Host vehicle in motion while payload is unstowed");
    }

    if (triggerAutoStow) {
        stow();
    }
}

bool PayloadStowController::isVehicleMotionActive() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vehicleMotionActive;
}

void PayloadStowController::setAutoStowOnVehicleMotion(bool enable) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoStowOnVehicleMotion = enable;
}

bool PayloadStowController::isAutoStowOnVehicleMotionEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoStowOnVehicleMotion;
}

void PayloadStowController::setVehicleMotionInterlockEnabled(bool enable) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vehicleMotionInterlockEnabled = enable;
}

bool PayloadStowController::isVehicleMotionInterlockEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vehicleMotionInterlockEnabled;
}

bool PayloadStowController::engageMechanicalLock()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != StowState::Stowed && m_state != StowState::EmergencyParked) {
        return false;
    }
    m_mechanicalLockEngaged = true;
    return true;
}

bool PayloadStowController::releaseMechanicalLock()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mechanicalLockEngaged = false;
    return true;
}

bool PayloadStowController::isMechanicalLockEngaged() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mechanicalLockEngaged;
}

void PayloadStowController::setCleaningConfig(const WindowCleaningConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cleaningConfig = config;
}

WindowCleaningConfig PayloadStowController::cleaningConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cleaningConfig;
}

void PayloadStowController::setHeaterMode(HeaterMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_heaterMode = mode;
    m_heaterTimerSec = 0.0;

    switch (mode) {
    case HeaterMode::Off:
        m_heaterActive = false;
        break;
    case HeaterMode::ManualOn:
        m_heaterActive = true;
        break;
    case HeaterMode::AutoThermostat:
        m_heaterActive = (m_currentWindowTempC <= m_cleaningConfig.autoDeIceOnTempC);
        break;
    }
}

HeaterMode PayloadStowController::heaterMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_heaterMode;
}

bool PayloadStowController::isHeaterActive() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_heaterActive;
}

void PayloadStowController::updateAmbientTemperature(double tempC) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentWindowTempC = tempC;

    if (m_heaterMode == HeaterMode::AutoThermostat) {
        if (!m_heaterActive && m_currentWindowTempC <= m_cleaningConfig.autoDeIceOnTempC) {
            m_heaterActive = true;
            m_heaterTimerSec = 0.0;
        } else if (m_heaterActive && m_currentWindowTempC >= m_cleaningConfig.autoDeIceOffTempC) {
            m_heaterActive = false;
            m_heaterTimerSec = 0.0;
        }
    }
}

void PayloadStowController::setWiperMode(WiperMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_wiperMode = mode;
    m_wiperTimerSec = 0.0;
    m_wiperIntervalTimerSec = 0.0;

    if (mode == WiperMode::Off) {
        m_wiperState = WiperState::Parked;
    } else {
        m_wiperState = WiperState::Wiping;
    }
}

WiperMode PayloadStowController::wiperMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_wiperMode;
}

WiperState PayloadStowController::wiperState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_wiperState;
}

bool PayloadStowController::triggerSingleWipe()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_wiperMode = WiperMode::SingleWipe;
    m_wiperState = WiperState::Wiping;
    m_wiperTimerSec = 0.0;
    return true;
}

bool PayloadStowController::startWasherRoutine()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_washerFluidLevel01 <= 0.001) {
        return false;
    }

    m_washerState = WasherState::Spraying;
    m_washerTimerSec = 0.0;
    m_currentWashWipeCycles = 0U;
    m_washerFluidLevel01 = std::max(0.0, m_washerFluidLevel01 - m_cleaningConfig.fluidConsumptionPerWash01);
    return true;
}

void PayloadStowController::cancelWasherRoutine()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_washerState = WasherState::Idle;
    m_washerTimerSec = 0.0;
    m_currentWashWipeCycles = 0U;
    m_wiperState = WiperState::Parked;
}

WasherState PayloadStowController::washerState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_washerState;
}

double PayloadStowController::washerFluidLevel() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_washerFluidLevel01;
}

void PayloadStowController::refillWasherFluid(double level01) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_washerFluidLevel01 = std::clamp(level01, 0.0, 1.0);
}

bool PayloadStowController::emergencyPark()
{
    std::shared_ptr<IPanTiltUnit> ptu;
    StowOrientation pose;
    StowState prevState {};
    StateChangedCallback stateCb;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_washerState = WasherState::Idle;
        m_wiperMode = WiperMode::Off;
        m_wiperState = WiperState::Parked;
        m_heaterActive = false;
        m_mechanicalLockEngaged = false;

        ptu = m_ptu;
        pose = m_stowPose;
        prevState = m_state;
        m_state = StowState::EmergencyParking;
        stateCb = m_stateCallback;
    }

    if (ptu) {
        if (ptu->hasRollAxis()) {
            ptu->setAbsoluteAngles3Axis(pose.panDeg, pose.tiltDeg, pose.rollDeg);
        } else {
            ptu->setAbsoluteAngles(pose.panDeg, pose.tiltDeg);
        }
    }

    if (stateCb) {
        stateCb(prevState, StowState::EmergencyParking, "Emergency park triggered");
    }

    return true;
}

bool PayloadStowController::zeroize(ZeroizeReason reason)
{
    std::shared_ptr<IPanTiltUnit> ptu;
    std::shared_ptr<IPtzPresetManager> presetMgr;
    StowOrientation pose;
    StowState prevState {};
    StateChangedCallback stateCb;
    ZeroizeCallback zeroizeCb;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_washerState = WasherState::Idle;
        m_wiperMode = WiperMode::Off;
        m_wiperState = WiperState::Parked;
        m_heaterActive = false;
        m_mechanicalLockEngaged = false;

        ptu = m_ptu;
        presetMgr = m_presetMgr;
        pose = m_stowPose;
        prevState = m_state;
        m_state = StowState::Zeroizing;
        m_isZeroized = true;

        stateCb = m_stateCallback;
        zeroizeCb = m_zeroizeCallback;
    }

    // Purge presets from preset manager
    if (presetMgr) {
        const auto presets = presetMgr->listPresets();
        for (const auto& p : presets) {
            presetMgr->clearPreset(p.id);
        }
    }

    // High-rate slew to emergency park
    if (ptu) {
        if (ptu->hasRollAxis()) {
            ptu->setAbsoluteAngles3Axis(pose.panDeg, pose.tiltDeg, pose.rollDeg);
        } else {
            ptu->setAbsoluteAngles(pose.panDeg, pose.tiltDeg);
        }
    }

    if (stateCb) {
        stateCb(prevState, StowState::Zeroizing, "Mission coordinates zeroization initiated");
    }

    if (zeroizeCb) {
        zeroizeCb(reason, "All mission waypoints and tactical coordinates purged from storage");
    }

    return true;
}

bool PayloadStowController::isZeroized() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isZeroized;
}

bool PayloadStowController::resetZeroizeLock()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isZeroized) {
        return false;
    }
    m_isZeroized = false;
    m_state = StowState::Stowed;
    return true;
}

void PayloadStowController::update(double dtSeconds)
{
    StateChangedCallback stateCb;
    StowState prevState {};
    StowState newState {};
    bool stateChanged = false;
    std::string transitionReason;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // --- Window Heater Timer ---
        if (m_heaterActive) {
            m_heaterTimerSec += dtSeconds;
            if (m_heaterTimerSec >= m_cleaningConfig.maxContinuousHeatSec) {
                m_heaterActive = false;
                m_heaterTimerSec = 0.0;
            }
        }

        // --- Windshield Wiper Logic ---
        if (m_wiperState == WiperState::Wiping) {
            m_wiperTimerSec += dtSeconds;
            if (m_wiperTimerSec >= m_cleaningConfig.wipeStrokeDurationSec) {
                m_wiperTimerSec = 0.0;
                if (m_wiperMode == WiperMode::SingleWipe) {
                    m_wiperMode = WiperMode::Off;
                    m_wiperState = WiperState::Parked;
                } else if (m_wiperMode == WiperMode::IntervalWipe) {
                    m_wiperState = WiperState::Parked;
                    m_wiperIntervalTimerSec = 0.0;
                }
            }
        } else if (m_wiperState == WiperState::Parked && m_wiperMode == WiperMode::IntervalWipe) {
            m_wiperIntervalTimerSec += dtSeconds;
            if (m_wiperIntervalTimerSec >= m_cleaningConfig.intervalWipeDelaySec) {
                m_wiperState = WiperState::Wiping;
                m_wiperTimerSec = 0.0;
                m_wiperIntervalTimerSec = 0.0;
            }
        }

        // --- Pressurized Washer Routine ---
        if (m_washerState == WasherState::Spraying) {
            m_washerTimerSec += dtSeconds;
            if (m_washerTimerSec >= m_cleaningConfig.washSprayDurationSec) {
                m_washerState = WasherState::Soaking;
                m_washerTimerSec = 0.0;
            }
        } else if (m_washerState == WasherState::Soaking) {
            m_washerTimerSec += dtSeconds;
            if (m_washerTimerSec >= m_cleaningConfig.washSoakDwellSec) {
                m_washerState = WasherState::ClearingWipes;
                m_washerTimerSec = 0.0;
                m_wiperState = WiperState::Wiping;
            }
        } else if (m_washerState == WasherState::ClearingWipes) {
            m_washerTimerSec += dtSeconds;
            if (m_washerTimerSec >= m_cleaningConfig.wipeStrokeDurationSec) {
                m_washerTimerSec = 0.0;
                m_currentWashWipeCycles++;
                if (m_currentWashWipeCycles >= m_cleaningConfig.washWipeCycleCount) {
                    m_washerState = WasherState::Complete;
                    m_wiperState = WiperState::Parked;
                }
            }
        } else if (m_washerState == WasherState::Complete) {
            m_washerState = WasherState::Idle;
        }

        // --- Gimbal Arrival State Machine ---
        prevState = m_state;
        switch (m_state) {
        case StowState::Stowing:
            if (checkArrivalLocked(m_stowPose)) {
                m_state = StowState::Stowed;
                stateChanged = true;
                newState = m_state;
                transitionReason = "Arrived at Stow stance";
            }
            break;
        case StowState::Deploying:
            if (checkArrivalLocked(m_deployPose)) {
                m_state = StowState::Deployed;
                stateChanged = true;
                newState = m_state;
                transitionReason = "Arrived at Deploy stance";
            }
            break;
        case StowState::MovingToMaintenance:
            if (checkArrivalLocked(m_maintenancePose)) {
                m_state = StowState::Maintenance;
                stateChanged = true;
                newState = m_state;
                transitionReason = "Arrived at Maintenance stance";
            }
            break;
        case StowState::EmergencyParking:
            if (checkArrivalLocked(m_stowPose)) {
                m_state = StowState::EmergencyParked;
                stateChanged = true;
                newState = m_state;
                transitionReason = "Emergency park completed";
            }
            break;
        case StowState::Zeroizing:
            if (checkArrivalLocked(m_stowPose)) {
                m_state = StowState::Zeroized;
                stateChanged = true;
                newState = m_state;
                transitionReason = "Zeroization completed";
            }
            break;
        default:
            break;
        }

        stateCb = m_stateCallback;
    }

    if (stateChanged && stateCb) {
        stateCb(prevState, newState, transitionReason);
    }
}

StowControllerStatus PayloadStowController::status() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StowControllerStatus s {};
    s.stowState = m_state;
    s.vehicleMotionSafe = (m_state == StowState::Stowed || m_state == StowState::EmergencyParked);
    s.vehicleMotionActive = m_vehicleMotionActive;
    s.mechanicalLockEngaged = m_mechanicalLockEngaged;

    s.heaterMode = m_heaterMode;
    s.heaterActive = m_heaterActive;
    s.currentWindowTempC = m_currentWindowTempC;

    s.wiperMode = m_wiperMode;
    s.wiperState = m_wiperState;

    s.washerState = m_washerState;
    s.washerFluidLevel01 = m_washerFluidLevel01;
    s.washerFluidLow = (m_washerFluidLevel01 <= 0.15);

    s.isZeroized = m_isZeroized;
    return s;
}

void PayloadStowController::setStateChangedCallback(StateChangedCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(cb);
}

void PayloadStowController::setInterlockAlertCallback(InterlockAlertCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interlockCallback = std::move(cb);
}

void PayloadStowController::setZeroizeCallback(ZeroizeCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_zeroizeCallback = std::move(cb);
}

bool PayloadStowController::checkArrivalLocked(const StowOrientation& targetPose) const noexcept
{
    if (!m_ptu) {
        return true;
    }

    const auto telem = m_ptu->currentTelemetry();
    const double dPan = angleDifferenceDeg(telem.panAngleDeg, targetPose.panDeg);
    const double dTilt = std::abs(telem.tiltAngleDeg - targetPose.tiltDeg);

    if (dPan > targetPose.arrivalToleranceDeg || dTilt > targetPose.arrivalToleranceDeg) {
        return false;
    }

    if (m_ptu->hasRollAxis()) {
        const double dRoll = std::abs(telem.rollAngleDeg - targetPose.rollDeg);
        if (dRoll > targetPose.arrivalToleranceDeg) {
            return false;
        }
    }

    return true;
}

} // namespace PayloadHal
