#pragma once

/// @file NmeaSlavingBridge.h
/// @brief Multi-sensor marine targeting coordinator slaving gimbal line-of-sight to ARPA radar and AIS vessels.

#include "GeoLockController.h"
#include "GeoreferenceUtils.h"
#include "Klv/KlvTypes.h"
#include "Nmea/AisTypes.h"
#include "Nmea/NmeaDevice.h"
#include "Nmea/NmeaTypes.h"
#include "PlatformLeverArmCompensator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {

/// @enum MarineTargetType
/// @brief Type of active target being tracked by the slaving bridge.
enum class MarineTargetType : std::uint8_t { None = 0, RadarArpa, AisVessel, GeodeticManual, RadarCursor, Waypoint };

/// @enum TargetLossPolicy
/// @brief Action taken when a tracked target ceases reporting telemetry beyond the timeout threshold.
enum class TargetLossPolicy : std::uint8_t {
    HoldPosition = 0, ///< Maintain line-of-sight pointing at last known target position
    CoastPredictive, ///< Dead-reckon target position forward along last SOG/COG vector
    Disengage ///< Disengage Geo-Lock immediately
};

/// @struct MarineSlavingStatus
/// @brief Comprehensive real-time status telemetry of the marine slaving bridge.
struct MarineSlavingStatus {
    bool active { false };
    MarineTargetType targetType { MarineTargetType::None };
    std::uint32_t targetId { 0U }; ///< Radar track number or AIS MMSI
    std::string targetName {};
    std::optional<Klv::GeoPoint3D> targetPosition {};
    double slantRangeMeters { 0.0 };
    double trueBearingDeg { 0.0 };
    bool isCoasting { false };
    std::chrono::steady_clock::time_point lastContact {};
};

/// @class NmeaSlavingBridge
/// @brief Tactical bridge coordinating ARPA radar contacts and AIS vessels to GeoLockController.
class NmeaSlavingBridge {
public:
    using StatusCallback = std::function<void(const MarineSlavingStatus&)>;
    using TargetLostCallback = std::function<void(MarineTargetType type, std::uint32_t targetId)>;

    /// @brief Constructs a slaving bridge connecting NmeaDevice to GeoLockController.
    /// @param[in] nmeaDevice Shared pointer to active NmeaDevice controller.
    /// @param[in] geoLockController Shared pointer to GeoLockController.
    /// @param[in] compensator Optional platform lever-arm compensator.
    explicit NmeaSlavingBridge(std::shared_ptr<Nmea::NmeaDevice> nmeaDevice,
        std::shared_ptr<GeoLockController> geoLockController,
        std::shared_ptr<PlatformLeverArmCompensator> compensator = nullptr);

    virtual ~NmeaSlavingBridge();

    // Non-copyable, non-movable
    NmeaSlavingBridge(const NmeaSlavingBridge&) = delete;
    NmeaSlavingBridge& operator=(const NmeaSlavingBridge&) = delete;
    NmeaSlavingBridge(NmeaSlavingBridge&&) = delete;
    NmeaSlavingBridge& operator=(NmeaSlavingBridge&&) = delete;

    /// @brief Slaves gimbal line-of-sight to an ARPA radar tracked target ($RATTM).
    /// @param[in] targetNumber Target tracking ID (00..99).
    /// @return True if target exists or is awaited.
    bool slaveToRadarTarget(std::uint32_t targetNumber);

    /// @brief Slaves gimbal line-of-sight to an AIS vessel target by MMSI (!AIVDM).
    /// @param[in] mmsi 9-digit Maritime Mobile Service Identity.
    /// @return True if target exists or is awaited.
    bool slaveToAisVessel(std::uint32_t mmsi);

    /// @brief Slaves gimbal line-of-sight to a fixed 3D geodetic target coordinate.
    /// @param[in] target 3D coordinate (latitude, longitude, altitude).
    /// @return True if target coordinate was accepted.
    bool slaveToGeodeticTarget(const Klv::GeoPoint3D& target);

    /// @brief Slaves gimbal line-of-sight to the active radar cursor ($--RSD).
    /// @param[in] rsd Radar System Data containing cursor range and bearing.
    /// @return True if cursor position was successfully projected and slaved.
    bool slaveToRadarCursor(const Nmea::RsdData& rsd);

    /// @brief Slaves gimbal line-of-sight to a navigation route waypoint ($--BWC).
    /// @param[in] bwc Bearing and Distance to Waypoint data.
    /// @return True if waypoint coordinates were valid and slaved.
    bool slaveToWaypoint(const Nmea::BwcData& bwc);

    /// @brief Disengages target slaving and releases GeoLockController.
    void disengage();

    /// @brief Configures policy when target telemetry is delayed or lost.
    void setTargetLossPolicy(TargetLossPolicy policy) noexcept;

    /// @brief Sets maximum elapsed time before target is considered lost.
    void setTargetTimeout(std::chrono::milliseconds timeout) noexcept;

    /// @brief Enables or disables predictive dead-reckoning coasting.
    void setPredictiveCoasting(bool enabled) noexcept;

    /// @brief Retrieves the active slaving status telemetry.
    [[nodiscard]] MarineSlavingStatus status() const;

    /// @brief Checks whether active target slaving is currently engaged.
    [[nodiscard]] bool isSlaving() const noexcept;

    /// @brief Executes a single update cycle (dead reckoning, timeout check, GeoLock update).
    void update();

    /// @brief Subscribes to periodic slaving status reports.
    std::size_t addStatusCallback(StatusCallback cb);

    /// @brief Unsubscribes a status callback.
    void removeStatusCallback(std::size_t id);

    /// @brief Subscribes to target loss notification events.
    std::size_t addTargetLostCallback(TargetLostCallback cb);

    /// @brief Unsubscribes a target lost callback.
    void removeTargetLostCallback(std::size_t id);

    /// @brief Utility to project a target 3D coordinate from platform position, slant range, and true bearing.
    /// @param[in] platformPos Platform 3D coordinate (lat, lon, alt).
    /// @param[in] rangeMeters Distance in meters.
    /// @param[in] trueBearingDeg True geographic bearing in degrees [0.0 .. 360.0).
    /// @return Target 3D geodetic position.
    [[nodiscard]] static Klv::GeoPoint3D projectTargetFromRadar(
        const Klv::GeoPoint3D& platformPos, double rangeMeters, double trueBearingDeg) noexcept;

private:
    void handleNavUpdate(const Nmea::NmeaNavSnapshot& snap);
    void handleRadarUpdate(const Nmea::TtmData& ttm);
    void handleAisUpdate(const Nmea::AisVesselTarget& target);
    void workerLoop();
    [[nodiscard]] MarineSlavingStatus statusLocked() const;

    std::shared_ptr<Nmea::NmeaDevice> m_nmeaDevice;
    std::shared_ptr<GeoLockController> m_geoLockController;
    std::shared_ptr<PlatformLeverArmCompensator> m_compensator;

    std::size_t m_navSubId { 0U };
    std::size_t m_radarSubId { 0U };
    std::size_t m_aisSubId { 0U };

    mutable std::mutex m_stateMutex;
    MarineTargetType m_targetType { MarineTargetType::None };
    std::uint32_t m_targetId { 0U };
    std::string m_targetName {};

    Klv::GeoPoint3D m_platformPos {};
    double m_platformHeadingDeg { 0.0 };
    bool m_hasPlatformNav { false };

    Klv::GeoPoint3D m_lastKnownTargetPos {};
    double m_targetSogKnots { 0.0 };
    double m_targetCogDegrees { 0.0 };
    std::chrono::steady_clock::time_point m_lastContactTime {};

    TargetLossPolicy m_lossPolicy { TargetLossPolicy::CoastPredictive };
    std::chrono::milliseconds m_targetTimeout { 15000 };
    bool m_predictiveCoasting { true };

    std::atomic<bool> m_running { false };
    std::thread m_workerThread {};
    std::condition_variable m_cv {};
    std::mutex m_workerMutex {};

    mutable std::mutex m_callbackMutex;
    std::size_t m_nextCallbackId { 1U };
    std::shared_ptr<const std::vector<std::pair<std::size_t, StatusCallback>>> m_statusCallbacks {
        std::make_shared<std::vector<std::pair<std::size_t, StatusCallback>>>()
    };
    std::shared_ptr<const std::vector<std::pair<std::size_t, TargetLostCallback>>> m_lostCallbacks {
        std::make_shared<std::vector<std::pair<std::size_t, TargetLostCallback>>>()
    };
};

} // namespace PayloadHal
