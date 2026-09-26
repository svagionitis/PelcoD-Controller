#pragma once

/// @file PayloadKlvGenerator.h
/// @brief STANAG 4609 / MISB ST 0601 KLV telemetry metadata generator for PayloadHal.

#include "IDemProvider.h"
#include "ICameraPayload.h"
#include "IPayload.h"
#include "Klv/KlvEncoder.h"
#include "Klv/KlvTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @struct PlatformNavData
/// @brief Navigation state vector supplied by host aircraft / vehicle / vessel.
struct PlatformNavData {
    Klv::GeoPoint3D position {};     ///< Latitude, Longitude, Altitude MSL in meters
    double headingDeg { 0.0 };       ///< True compass heading in degrees [0.0 .. 360.0)
    double pitchDeg { 0.0 };         ///< Vehicle pitch angle in degrees [-20.0 .. +20.0]
    double rollDeg { 0.0 };          ///< Vehicle roll angle in degrees [-50.0 .. +50.0]
};

/// @struct PayloadKlvConfig
/// @brief Static and administrative metadata configuration for KLV packets.
struct PayloadKlvConfig {
    std::string missionId { "MISSION_01" };                    ///< Tag 3: Mission Identifier
    std::string platformTailNumber { "UAS_01" };              ///< Tag 4: Platform Tail Number / Callsign
    std::string platformDesignation { "TACTICAL_SURVEILLANCE" };///< Tag 10: Platform model
    std::string imageSourceSensor { "DAYLIGHT_EO" };           ///< Tag 11: Sensor payload model
    std::string imageCoordinateSystem { "Geodetic WGS84" };    ///< Tag 12: Image coordinate reference
    Klv::SecurityMetadata security {};                         ///< Tag 48: MISB ST 0102 Security Classification
    std::uint8_t uasLsVersion { 12U };                         ///< Tag 65: MISB ST 0601 version number
    bool enableFrustumCorners { true };                        ///< True to calculate 4-corner footprint (Tags 26-33)
    double fallbackGroundElevationM { 0.0 };                   ///< Fallback ground plane elevation MSL in meters
    std::shared_ptr<IDemProvider> demProvider {};              ///< Optional Digital Elevation Model provider
};

/// @class PayloadKlvGenerator
/// @brief Generates standard MISB ST 0601 UAS Datalink messages and encoded byte packets from IPayload telemetry.
/// @details Gathers live orientation from IPanTiltUnit, optical field-of-view from ICameraPayload,
///          laser ranging from ILaserRangeFinder, and performs geodetic ground target and frustum footprint
///          calculations before serializing into STANAG 4609 compliant KLV byte streams.
class PayloadKlvGenerator {
public:
    /// @brief Callback invoked when a new encoded KLV packet is generated.
    using PacketCallback = std::function<void(const std::vector<std::uint8_t>& packet,
                                              const Klv::UasDatalinkMessage& message)>;

    /// @brief Constructs a generator bound to an IPayload instance.
    /// @param[in] payload Shared pointer to IPayload composite station.
    /// @param[in] config Static metadata configuration.
    explicit PayloadKlvGenerator(std::shared_ptr<IPayload> payload,
                                 PayloadKlvConfig config = {}) noexcept;

    virtual ~PayloadKlvGenerator() = default;

    // Configuration
    void setConfig(const PayloadKlvConfig& config);
    [[nodiscard]] PayloadKlvConfig config() const;

    /// @brief Selects which camera payload to query for optical FOV telemetry.
    /// @param[in] camera Camera payload pointer, or nullptr to use payload->primaryCamera().
    void setActiveCamera(std::shared_ptr<ICameraPayload> camera);

    /// @brief Retrieves the currently selected active camera payload.
    /// @return Shared pointer to active camera, or nullptr if none set.
    [[nodiscard]] std::shared_ptr<ICameraPayload> activeCamera() const;

    /// @brief Configures the Digital Elevation Model (DEM) provider for terrain ray intersection.
    /// @param[in] dem Shared pointer to DEM provider.
    void setDemProvider(std::shared_ptr<IDemProvider> dem);

    /// @brief Accesses the active DEM provider, checking config and IPayload fallback.
    /// @return Shared pointer to DEM provider, or nullptr.
    [[nodiscard]] std::shared_ptr<IDemProvider> demProvider() const;

    /// @brief Builds a strongly typed UasDatalinkMessage snapshot without serializing to bytes.
    /// @param[in] nav Platform navigation state.
    /// @param[in] timestampUs Microseconds since epoch (defaults to system_clock::now).
    /// @return Populated UasDatalinkMessage.
    [[nodiscard]] Klv::UasDatalinkMessage buildMessage(
        const PlatformNavData& nav,
        std::optional<std::uint64_t> timestampUs = std::nullopt) const;

    /// @brief Generates a complete serialized and checksummed STANAG 4609 KLV byte packet.
    /// @param[in] nav Platform navigation state.
    /// @param[in] timestampUs Microseconds since epoch (defaults to system_clock::now).
    /// @return Serialized byte vector including 16-byte UL, BER length, tags, and CRC-16.
    [[nodiscard]] std::vector<std::uint8_t> generatePacket(
        const PlatformNavData& nav,
        std::optional<std::uint64_t> timestampUs = std::nullopt) const;

private:
    std::shared_ptr<IPayload> m_payload {};
    std::shared_ptr<ICameraPayload> m_activeCamera {};
    mutable std::mutex m_mutex;
    PayloadKlvConfig m_config {};
};

} // namespace PayloadHal
