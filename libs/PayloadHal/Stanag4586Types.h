#pragma once

/// @file Stanag4586Types.h
/// @brief NATO STANAG 4586 Data Link Interface (DLI) payload message structures and constants.
/// @details Defines message definitions, enums, scaling parameters, and CRC-16 CCITT routines
///          for Messages #2000, #2001, #2002, #2003, and #2004.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace PayloadHal {

/// @brief Standard synchronization bytes for framed STANAG 4586 DLI packets.
inline constexpr std::uint8_t STANAG4586_SYNC_BYTE_0 = 0x45; // 'E'
inline constexpr std::uint8_t STANAG4586_SYNC_BYTE_1 = 0x86; // 0x86
inline constexpr std::uint16_t STANAG4586_SYNC_MARKER = 0x4586;

/// @brief Standard fixed DLI header length in bytes (excluding payload and CRC-16).
inline constexpr std::size_t STANAG4586_HEADER_SIZE = 26U;

/// @brief Minimum size of a complete framed STANAG 4586 packet (Header + 2-byte CRC).
inline constexpr std::size_t STANAG4586_MIN_PACKET_SIZE = STANAG4586_HEADER_SIZE + 2U;

/// @brief Payload byte sizes for STANAG 4586 DLI messages.
inline constexpr std::size_t STANAG_PAYLOAD_SIZE_2000 = 52U;
inline constexpr std::size_t STANAG_PAYLOAD_SIZE_2001 = 4U;
inline constexpr std::size_t STANAG_PAYLOAD_SIZE_2002 = 72U;
inline constexpr std::size_t STANAG_PAYLOAD_SIZE_2003 = 20U;
inline constexpr std::size_t STANAG_PAYLOAD_SIZE_2004 = 52U;

/// @brief NATO STANAG 4586 DLI Payload Message Identifiers (Annex B).
enum class StanagMessageId : std::uint16_t {
    PayloadConfiguration       = 2000U, ///< Message #2000: Payload Configuration Report
    PayloadConfigurationReq    = 2001U, ///< Message #2001: Payload Configuration Request Command
    PayloadOperatingState      = 2002U, ///< Message #2002: Payload Operating State Telemetry Report
    PayloadOperatingCommand    = 2003U, ///< Message #2003: Payload Operating Command
    PayloadSteeringCommand     = 2004U  ///< Message #2004: Payload Steering Command
};

/// @brief NATO STANAG 4586 Payload Type Classification.
enum class StanagPayloadType : std::uint8_t {
    Unknown         = 0U,
    FixedEo         = 1U,
    FixedIr         = 2U,
    MultiSensorEoIr = 3U,
    Radar           = 4U,
    LaserDesignator = 5U,
    Custom          = 6U
};

/// @brief Operating mode reported in Message #2002 and commanded in Message #2003.
enum class StanagOperatingMode : std::uint8_t {
    Off             = 0U,
    Standby         = 1U,
    Active          = 2U,
    Stowed          = 3U,
    Caged           = 4U,
    Fault           = 5U,
    EmergencyPark   = 6U
};

/// @brief Active sensor channel reported in Message #2002 and commanded in Message #2003.
enum class StanagSensorChannel : std::uint8_t {
    DaylightEo      = 0U,
    ThermalIr       = 1U,
    Fused           = 2U
};

/// @brief Commanded steering mode in Message #2004.
enum class StanagSteeringMode : std::uint8_t {
    AnglePosition   = 1U, ///< Commanded absolute azimuth and elevation angles
    RateVelocity    = 2U, ///< Commanded pan/tilt slewing angular rates
    GeoPointing     = 3U, ///< Commanded line-of-sight to target latitude/longitude/altitude
    SlavedTrack     = 4U, ///< Slaved to target kinematics or external track cue
    StowCage        = 5U  ///< Return to stow or cage position
};

/// @brief Focus control command in Message #2003.
enum class StanagFocusMode : std::uint8_t {
    Auto            = 0U,
    ManualNear      = 1U,
    ManualFar       = 2U,
    OnePush         = 3U,
    Hold            = 4U
};

/// @brief Iris/exposure control command in Message #2003.
enum class StanagIrisMode : std::uint8_t {
    Auto            = 0U,
    ManualOpen      = 1U,
    ManualClose     = 2U,
    Hold            = 3U
};

/// @brief Laser Range Finder state reported in Message #2002.
enum class StanagLaserState : std::uint8_t {
    Disarmed        = 0U,
    Armed           = 1U,
    Firing          = 2U,
    Inhibited       = 3U
};

/// @brief Laser command in Message #2003.
enum class StanagLaserCommand : std::uint8_t {
    Disarm          = 0U,
    Arm             = 1U,
    FireSinglePulse = 2U,
    FireContinuous  = 3U,
    Inhibit         = 4U
};

/// @brief Laser pointer/illuminator command in Message #2003.
enum class StanagIlluminatorCommand : std::uint8_t {
    Off             = 0U,
    Continuous      = 1U,
    Strobe          = 2U
};

/// @brief Built-In-Test status reported in Message #2002.
enum class StanagBitStatus : std::uint8_t {
    Ok              = 0U,
    Degraded        = 1U,
    Fault           = 2U
};

/// @brief Sensor Station capability flags for Message #2000.
namespace StanagCapabilities {
    inline constexpr std::uint32_t PanTiltGimbal    = 1U << 0U;  ///< Pan/Tilt mechanical gimbal
    inline constexpr std::uint32_t DaylightEoCamera = 1U << 1U;  ///< Visible daylight EO camera
    inline constexpr std::uint32_t ThermalIrCamera  = 1U << 2U;  ///< Thermal LWIR/MWIR camera
    inline constexpr std::uint32_t LaserRangeFinder = 1U << 3U;  ///< Laser Range Finder
    inline constexpr std::uint32_t LaserIlluminator = 1U << 4U;  ///< Tactical Laser Pointer / Illuminator
    inline constexpr std::uint32_t OpticalZoom      = 1U << 5U;  ///< Continuous optical zoom
    inline constexpr std::uint32_t MotorizedFocus   = 1U << 6U;  ///< Motorized focus & one-push focus
    inline constexpr std::uint32_t MotorizedIris    = 1U << 7U;  ///< Auto / manual iris aperture
    inline constexpr std::uint32_t RollStabilization= 1U << 8U;  ///< 3-axis roll stabilization / horizon leveling
    inline constexpr std::uint32_t WindowDeIceWiper = 1U << 9U;  ///< Optical window de-ice heating & wiper
    inline constexpr std::uint32_t GeoLockTracking  = 1U << 10U; ///< Geodetic coordinate holding / Geo-Lock
    inline constexpr std::uint32_t MultiSensorFusion= 1U << 11U; ///< Multi-sensor fusion / match-zoom
} // namespace StanagCapabilities

/// @brief Auxiliary discrete controls for Message #2003.
namespace StanagAuxControls {
    inline constexpr std::uint16_t DeIceHeaterOn    = 1U << 0U;
    inline constexpr std::uint16_t DeIceHeaterOff   = 1U << 1U;
    inline constexpr std::uint16_t WiperSingleCycle = 1U << 2U;
    inline constexpr std::uint16_t WiperContinuous  = 1U << 3U;
    inline constexpr std::uint16_t WiperStop        = 1U << 4U;
    inline constexpr std::uint16_t WashCycleTrigger = 1U << 5U;
    inline constexpr std::uint16_t EmergencyPark    = 1U << 6U;
    inline constexpr std::uint16_t ZeroizePresets   = 1U << 7U;
} // namespace StanagAuxControls

/// @struct Stanag4586Header
/// @brief Standard NATO STANAG 4586 DLI packet header (26 bytes).
struct Stanag4586Header {
    std::uint16_t syncMarker { STANAG4586_SYNC_MARKER }; ///< 0x4586
    std::uint16_t messageId { 0U };                      ///< StanagMessageId
    std::uint16_t messageLength { 0U };                  ///< Length of message payload in bytes
    std::uint64_t timestampUs { 0U };                    ///< Time of data generation (UTC microsec)
    std::uint32_t sourceId { 0U };                       ///< Unique ID of transmitting node
    std::uint32_t destinationId { 0xFFFFFFFFU };         ///< Target node ID (0xFFFFFFFF = broadcast)
    std::uint32_t sequenceNumber { 0U };                 ///< Monotonically increasing sequence number
    std::uint8_t  stationId { 0U };                      ///< Payload mounting station index (0..15)
    std::uint8_t  flags { 0U };                          ///< Protocol flags / reserved
};

/// @struct Stanag4586Message2000
/// @brief NATO STANAG 4586 Message #2000: Payload Configuration Report.
struct Stanag4586Message2000 {
    std::uint8_t  stationId { 0U };
    StanagPayloadType payloadType { StanagPayloadType::MultiSensorEoIr };
    std::uint32_t capabilities { 0U };
    float minAzimuthDeg { -180.0f };
    float maxAzimuthDeg { 180.0f };
    float minElevationDeg { -90.0f };
    float maxElevationDeg { 20.0f };
    float minRollDeg { -60.0f };
    float maxRollDeg { 60.0f };
    float maxPanRateDegPerSec { 60.0f };
    float maxTiltRateDegPerSec { 60.0f };
    float maxRollRateDegPerSec { 30.0f };
    float minHorizontalFovDeg { 1.5f };
    float maxHorizontalFovDeg { 60.0f };
};

/// @struct Stanag4586Message2001
/// @brief NATO STANAG 4586 Message #2001: Payload Configuration Request Command.
struct Stanag4586Message2001 {
    std::uint8_t stationId { 0xFFU }; ///< Station to query, or 0xFF for all stations
};

/// @struct Stanag4586Message2002
/// @brief NATO STANAG 4586 Message #2002: Payload Operating State Telemetry Report.
struct Stanag4586Message2002 {
    std::uint8_t  stationId { 0U };
    StanagOperatingMode operatingMode { StanagOperatingMode::Standby };
    StanagSensorChannel activeSensor { StanagSensorChannel::DaylightEo };
    float azimuthDeg { 0.0f };
    float elevationDeg { 0.0f };
    float rollDeg { 0.0f };
    float azimuthRateDegPerSec { 0.0f };
    float elevationRateDegPerSec { 0.0f };
    float rollRateDegPerSec { 0.0f };
    float horizontalFovDeg { 0.0f };
    float verticalFovDeg { 0.0f };
    float zoomMagnification { 1.0f };
    StanagLaserState laserState { StanagLaserState::Disarmed };
    float slantRangeMeters { 0.0f };
    std::uint8_t illuminatorState { 0U };
    StanagBitStatus bitStatus { StanagBitStatus::Ok };
    std::uint8_t targetLocationValid { 0U };
    double targetLatitudeDeg { 0.0 };
    double targetLongitudeDeg { 0.0 };
    float targetAltitudeMslMeters { 0.0f };
};

/// @struct Stanag4586Message2003
/// @brief NATO STANAG 4586 Message #2003: Payload Operating Command.
struct Stanag4586Message2003 {
    std::uint8_t  stationId { 0U };
    StanagOperatingMode commandedMode { StanagOperatingMode::Active };
    StanagSensorChannel sensorSelect { StanagSensorChannel::DaylightEo };
    StanagFocusMode focusMode { StanagFocusMode::Auto };
    float manualFocusVelocity { 0.0f };
    StanagIrisMode irisMode { StanagIrisMode::Auto };
    float manualIrisNormalized { 0.5f };
    StanagLaserCommand lrfCommand { StanagLaserCommand::Disarm };
    StanagIlluminatorCommand illuminatorCommand { StanagIlluminatorCommand::Off };
    std::uint16_t auxiliaryControls { 0U };
};

/// @struct Stanag4586Message2004
/// @brief NATO STANAG 4586 Message #2004: Payload Steering Command.
struct Stanag4586Message2004 {
    std::uint8_t  stationId { 0U };
    StanagSteeringMode steeringMode { StanagSteeringMode::AnglePosition };
    // Mode 1: Angle / Position Mode
    float commandedAzimuthDeg { 0.0f };
    float commandedElevationDeg { 0.0f };
    float commandedRollDeg { 0.0f };
    float maxSlewRateDegPerSec { 0.0f };
    // Mode 2: Rate / Velocity Mode
    float commandedAzimuthRateDegPerSec { 0.0f };
    float commandedElevationRateDegPerSec { 0.0f };
    // Mode 3: Geo-Pointing / Geo-Lock Mode
    double targetLatitudeDeg { 0.0 };
    double targetLongitudeDeg { 0.0 };
    float targetAltitudeMslMeters { 0.0f };
};

// ============================================================================
// Bitwise Big-Endian Wire Marshalling & CRC-16 Utilities
// ============================================================================

namespace StanagWire {

    /// @brief Computes standard CRC-16 CCITT (Polynomial 0x1021, initial value 0xFFFF).
    /// @param[in] data Pointer to contiguous memory buffer.
    /// @param[in] length Number of bytes to process.
    /// @return 16-bit CRC checksum.
    [[nodiscard]] inline std::uint16_t computeCrc16Ccitt(const std::uint8_t* data, std::size_t length) noexcept
    {
        std::uint16_t crc = 0xFFFFU;
        for (std::size_t i = 0; i < length; ++i) {
            crc ^= static_cast<std::uint16_t>(data[i]) << 8U;
            for (std::uint8_t bit = 0; bit < 8U; ++bit) {
                if ((crc & 0x8000U) != 0U) {
                    crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
                } else {
                    crc = static_cast<std::uint16_t>(crc << 1U);
                }
            }
        }
        return crc;
    }

    /// @brief Writes a 16-bit unsigned integer in Big-Endian format.
    inline void writeUint16Be(std::uint8_t* dest, std::uint16_t value) noexcept
    {
        dest[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        dest[1] = static_cast<std::uint8_t>(value & 0xFFU);
    }

    /// @brief Reads a 16-bit unsigned integer in Big-Endian format.
    [[nodiscard]] inline std::uint16_t readUint16Be(const std::uint8_t* src) noexcept
    {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(src[0]) << 8U) |
                                          static_cast<std::uint16_t>(src[1]));
    }

    /// @brief Writes a 32-bit unsigned integer in Big-Endian format.
    inline void writeUint32Be(std::uint8_t* dest, std::uint32_t value) noexcept
    {
        dest[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
        dest[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        dest[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        dest[3] = static_cast<std::uint8_t>(value & 0xFFU);
    }

    /// @brief Reads a 32-bit unsigned integer in Big-Endian format.
    [[nodiscard]] inline std::uint32_t readUint32Be(const std::uint8_t* src) noexcept
    {
        return (static_cast<std::uint32_t>(src[0]) << 24U) |
               (static_cast<std::uint32_t>(src[1]) << 16U) |
               (static_cast<std::uint32_t>(src[2]) << 8U)  |
               static_cast<std::uint32_t>(src[3]);
    }

    /// @brief Writes a 64-bit unsigned integer in Big-Endian format.
    inline void writeUint64Be(std::uint8_t* dest, std::uint64_t value) noexcept
    {
        for (std::size_t i = 0; i < 8U; ++i) {
            dest[i] = static_cast<std::uint8_t>((value >> (56U - (i * 8U))) & 0xFFU);
        }
    }

    /// @brief Reads a 64-bit unsigned integer in Big-Endian format.
    [[nodiscard]] inline std::uint64_t readUint64Be(const std::uint8_t* src) noexcept
    {
        std::uint64_t val = 0U;
        for (std::size_t i = 0; i < 8U; ++i) {
            val = (val << 8U) | static_cast<std::uint64_t>(src[i]);
        }
        return val;
    }

    /// @brief Writes a 32-bit float in IEEE 754 Big-Endian format.
    inline void writeFloatBe(std::uint8_t* dest, float value) noexcept
    {
        std::uint32_t raw = 0U;
        std::memcpy(&raw, &value, sizeof(float));
        writeUint32Be(dest, raw);
    }

    /// @brief Reads a 32-bit float in IEEE 754 Big-Endian format.
    [[nodiscard]] inline float readFloatBe(const std::uint8_t* src) noexcept
    {
        const std::uint32_t raw = readUint32Be(src);
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(float));
        return value;
    }

    /// @brief Writes a 64-bit double in IEEE 754 Big-Endian format.
    inline void writeDoubleBe(std::uint8_t* dest, double value) noexcept
    {
        std::uint64_t raw = 0U;
        std::memcpy(&raw, &value, sizeof(double));
        writeUint64Be(dest, raw);
    }

    /// @brief Reads a 64-bit double in IEEE 754 Big-Endian format.
    [[nodiscard]] inline double readDoubleBe(const std::uint8_t* src) noexcept
    {
        const std::uint64_t raw = readUint64Be(src);
        double value = 0.0;
        std::memcpy(&value, &raw, sizeof(double));
        return value;
    }

} // namespace StanagWire

} // namespace PayloadHal
