#pragma once

/// @file SimulatedPayload.h
/// @brief Multi-sensor simulated payload station for testing and offline development.

#include "ICameraPayload.h"
#include "ILaserIlluminator.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace PayloadHal {

/// @class SimulatedPayload
/// @brief Fully software-simulated multi-sensor electro-optical/infrared payload station
///        integrating a gyro-stabilized gimbal, daylight EO, thermal LWIR, eye-safe LRF, and tactical illuminator.
class SimulatedPayload : public IPayload {
public:
    SimulatedPayload();
    ~SimulatedPayload() override;

    // --- IDevice Lifecycle ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- IPayload Accessors ---
    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override;
    [[nodiscard]] std::shared_ptr<ILaserIlluminator> illuminator() const noexcept override;
    [[nodiscard]] std::shared_ptr<IPtzPresetManager> presetManager() const noexcept override;
    [[nodiscard]] std::shared_ptr<TourEngine> tourEngine() const noexcept override;
    [[nodiscard]] std::shared_ptr<TacticalSearchEngine> tacticalSearch() const noexcept override;
    [[nodiscard]] std::shared_ptr<SensorParallaxCompensator> parallaxCompensator() const noexcept override;
    [[nodiscard]] std::shared_ptr<PlatformLeverArmCompensator> leverArmCompensator() const noexcept override;
    [[nodiscard]] std::shared_ptr<GimbalSectorBlanking> sectorBlanking() const noexcept override;
    [[nodiscard]] std::shared_ptr<PayloadHealthMonitor> healthMonitor() const noexcept override;

    [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override;

    // Simulation helpers
    void setSimulatedSlantRange(double rangeMeters);
    void setSimulatedGroundElevation(double groundElevationM);
    void setSimulatedPlatformAttitude(double pitchDeg, double rollDeg, double headingDeg = 0.0);

private:
    class SimPtu;
    class SimCamera;
    class SimLrf;
    class SimIlluminator;

    std::shared_ptr<SimPtu> m_ptu;
    std::shared_ptr<SimCamera> m_daylightCamera;
    std::shared_ptr<SimCamera> m_thermalCamera;
    std::shared_ptr<SimLrf> m_lrf;
    std::shared_ptr<SimIlluminator> m_illuminator;
    std::shared_ptr<IPtzPresetManager> m_presetMgr;
    std::shared_ptr<TourEngine> m_tourEngine;
    std::shared_ptr<TacticalSearchEngine> m_tacticalEngine;
    std::shared_ptr<SensorParallaxCompensator> m_parallaxCompensator;
    std::shared_ptr<PlatformLeverArmCompensator> m_leverArmCompensator;
    std::shared_ptr<GimbalSectorBlanking> m_sectorBlanking;
    std::shared_ptr<PayloadHealthMonitor> m_healthMonitor;

    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    std::atomic<bool> m_connected { false };
    double m_simGroundElevation { 0.0 };
};

} // namespace PayloadHal
