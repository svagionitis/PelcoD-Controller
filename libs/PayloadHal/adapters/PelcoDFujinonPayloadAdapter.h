#pragma once

/// @file PelcoDFujinonPayloadAdapter.h
/// @brief Multi-sensor composite payload adapter for Fujinon SX800 long-range cameras.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "PelcoDFujinon/FujinonSX800Device.h"

#include <memory>
#include <mutex>

namespace PayloadHal {

/// @class PelcoDFujinonPayloadAdapter
/// @brief Adapts a Fujinon SX800 long-range camera and associated Pelco-D PT head into
///        a unified composite IPayload station with full access to 40x optics, OIS, and defog.
class PelcoDFujinonPayloadAdapter : public IPayload {
public:
    /// @brief Constructs adapter for a Fujinon SX800 device.
    /// @param[in] device Shared pointer to FujinonSX800Device.
    explicit PelcoDFujinonPayloadAdapter(std::shared_ptr<PelcoD::FujinonSX800Device> device);
    ~PelcoDFujinonPayloadAdapter() override;

    // --- IDevice Lifecycle ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- IPayload Subsystem Accessors ---
    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override;
    void setLrf(std::shared_ptr<ILaserRangeFinder> lrf) noexcept;

    [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override;

    // --- Extended Fujinon-Specific Optics & Enhancement Controls ---
    bool setOISMode(PelcoD::FujinonOISMode mode);
    bool setDefogLevel(PelcoD::FujinonDefogLevel level);
    bool setHeatHaze(PelcoD::FujinonHeatHazeLevel level);
    bool setWDR(PelcoD::FujinonWDRLevel level);
    bool setVLCFilter(bool enable);

    [[nodiscard]] std::shared_ptr<PelcoD::FujinonSX800Device> underlyingDevice() const noexcept {
        return m_device;
    }

private:
    class FujinonPtuUnit;
    class FujinonCameraUnit;

    std::shared_ptr<PelcoD::FujinonSX800Device> m_device;
    std::shared_ptr<FujinonPtuUnit> m_ptu;
    std::shared_ptr<FujinonCameraUnit> m_camera;
    std::shared_ptr<ILaserRangeFinder> m_lrf {};
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
};

} // namespace PayloadHal
