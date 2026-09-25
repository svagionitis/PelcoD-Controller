#pragma once

/// @file PelcoDViscaCompositePayload.h
/// @brief Multi-sensor composite payload adapter binding a Pelco-D PT head and Sony FCB VISCA camera.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "PelcoDCore/PelcoDDevice.h"
#include "SonyFCBDevice.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

namespace PayloadHal {

/// @class PelcoDViscaCompositePayload
/// @brief Composite IPayload station integrating an RS-485 / IP Pelco-D pan-tilt head
///        with a Sony FCB series optical block camera driven via VISCA.
/// @details Coordinates dual lifecycle states, aggregates device health telemetry,
///          and projects line-of-sight ground target georeferencing coordinates.
class PelcoDViscaCompositePayload : public IPayload {
public:
    /// @brief Constructs a composite payload by wrapping underlying device controllers.
    /// @details Automatically instantiates PelcoDPtzAdapter and ViscaSonyAdapter internally.
    /// @param[in] ptzDevice Shared pointer to the Pelco-D pan-tilt device driver.
    /// @param[in] cameraDevice Shared pointer to the Sony FCB VISCA camera driver.
    explicit PelcoDViscaCompositePayload(
        std::shared_ptr<PelcoD::PelcoDDevice> ptzDevice, std::shared_ptr<Visca::Sony::SonyFCBDevice> cameraDevice);

    /// @brief Constructs a composite payload wrapping existing HAL subsystem adapters.
    /// @param[in] ptu Shared pointer to an initialized IPanTiltUnit instance.
    /// @param[in] camera Shared pointer to an initialized ICameraPayload instance.
    explicit PelcoDViscaCompositePayload(std::shared_ptr<IPanTiltUnit> ptu, std::shared_ptr<ICameraPayload> camera);

    /// @brief Virtual destructor ensuring clean shutdown of subsystems.
    ~PelcoDViscaCompositePayload() override;

    // --- IDevice Lifecycle & Health ---

    /// @brief Connects and initializes both pan-tilt and camera subsystems.
    /// @return True if both subsystems connected successfully, false otherwise.
    bool connect() override;

    /// @brief Disconnects both pan-tilt and camera subsystems.
    void disconnect() override;

    /// @brief Checks whether both subsystems are currently connected.
    /// @return True if both pan-tilt and camera report connected status.
    [[nodiscard]] bool isConnected() const noexcept override;

    /// @brief Evaluates aggregated composite device health.
    /// @return DeviceState::Ready if both are ready; Degraded if one is operational; Disconnected or Error otherwise.
    [[nodiscard]] DeviceState state() const noexcept override;

    /// @brief Returns aggregated device manufacturer and hardware model descriptors.
    /// @return Combined DeviceInfo structure.
    [[nodiscard]] DeviceInfo info() const noexcept override;

    /// @brief Registers a callback invoked whenever the aggregate payload health state changes.
    /// @param[in] cb State transition callback.
    void registerStateCallback(StateCallback cb) override;

    // --- IPayload Subsystem Accessors ---

    /// @brief Retrieves the primary Pan-Tilt gimbal subsystem.
    /// @return Shared pointer to IPanTiltUnit interface.
    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override;

    /// @brief Retrieves the primary daylight visible camera subsystem.
    /// @return Shared pointer to ICameraPayload interface.
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override;

    /// @brief Retrieves the secondary camera subsystem (none in baseline composite).
    /// @return nullptr.
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override;

    /// @brief Retrieves the laser rangefinder subsystem (none in baseline composite).
    /// @return nullptr.
    [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override;

    // --- Target Georeferencing ---

    /// @brief Projects the camera line-of-sight vector onto the WGS-84 ellipsoid / ground plane.
    /// @details Combines current platform geodetic coordinates, platform heading, and gimbal pan/tilt telemetry.
    /// @param[in] platformGps Current geodetic location of the station platform.
    /// @param[in] platformHeadingDeg True heading of the platform in degrees [0, 360).
    /// @param[in] platformAltMeters Height Above Ellipsoid (HAE) or MSL altitude in meters.
    /// @return Projected target ground coordinate, or std::nullopt if aiming above horizon.
    [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override;

    /// @brief Provides access to the underlying PelcoDDevice instance, if available.
    /// @return Shared pointer to PelcoDDevice, or nullptr if initialized with generic IPanTiltUnit.
    [[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> underlyingPelcoDevice() const noexcept;

    /// @brief Provides access to the underlying SonyFCBDevice instance, if available.
    /// @return Shared pointer to SonyFCBDevice, or nullptr if initialized with generic ICameraPayload.
    [[nodiscard]] std::shared_ptr<Visca::Sony::SonyFCBDevice> underlyingSonyDevice() const noexcept;

private:
    void handleSubsystemStateChange();

    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<ICameraPayload> m_camera {};
    std::shared_ptr<PelcoD::PelcoDDevice> m_pelcoDevice {};
    std::shared_ptr<Visca::Sony::SonyFCBDevice> m_sonyDevice {};

    mutable std::mutex m_mutex {};
    StateCallback m_stateCallback {};
    DeviceState m_lastAggregatedState { DeviceState::Disconnected };
    std::atomic<DeviceState> m_ptuState { DeviceState::Disconnected };
    std::atomic<DeviceState> m_cameraState { DeviceState::Disconnected };
};

} // namespace PayloadHal
