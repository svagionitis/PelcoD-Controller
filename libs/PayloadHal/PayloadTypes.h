#pragma once

/// @file PayloadTypes.h
/// @brief Common enumerations, telemetry structures, and metadata for the PayloadHal subsystem.

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum DeviceState
/// @brief Operational and connectivity lifecycle state of any payload hardware device.
enum class DeviceState : std::uint8_t {
    Disconnected, ///< Hardware transport is closed or unreachable
    Connecting, ///< Establishing physical/logical handshake
    Standby, ///< Connected and idle in low-power or stationary standby
    Ready, ///< Fully calibrated, operational, and accepting operational commands
    Degraded, ///< Partially operational (e.g. sensor overheating, gyro drifting)
    Fault ///< Hardware failure, limit switch lock, or communication timeout
};

/// @struct DeviceInfo
/// @brief Descriptive manufacturing and firmware metadata for a hardware unit.
struct DeviceInfo {
    std::string manufacturer {}; ///< Equipment manufacturer (e.g. "Sony", "Fujinon", "FLIR")
    std::string model {}; ///< Model descriptor (e.g. "FCB-EV9520L", "SX800")
    std::string serialNumber {}; ///< Unique factory serial number
    std::string firmwareVersion {}; ///< Firmware revision string
};

// =============================================================================
// Pan-Tilt & Gimbal Types
// =============================================================================

/// @enum PanTiltMode
/// @brief Operational motion control mode for a pan-tilt unit or gimbal.
enum class PanTiltMode : std::uint8_t {
    Rate, ///< Continuous angular rate / velocity control
    Angle, ///< Closed-loop absolute positioning
    Relative, ///< Relative angular offset nudge
    Stow, ///< Safe transit stow lock position
    Park ///< Park position (e.g., lens facing down)
};

/// @enum StabilizationMode
/// @brief Inertial gyro stabilization operational mode.
enum class StabilizationMode : std::uint8_t {
    Disabled,       ///< Open-loop / unstabilized pedestal mode
    RateStabilized, ///< Active inertial gyro rate stabilization (rejection of vehicle jitter)
    GeoHold,        ///< Inertial Line-of-Sight hold on geographic coordinate
    FollowPlatform, ///< Coordinated turn follow mode with vehicle heading
    HorizonLevel    ///< Active inertial stabilization + automatic horizon leveling roll compensation
};

/// @struct GimbalAttitude3D
/// @brief 3-axis Euler orientation angles for a pan-tilt-roll gimbal station.
struct GimbalAttitude3D {
    double panAngleDeg { 0.0 };  ///< Azimuth angle in degrees (-180.0° .. +180.0° or 0.0° .. 360.0°)
    double tiltAngleDeg { 0.0 }; ///< Elevation angle in degrees (-90.0° .. +90.0°)
    double rollAngleDeg { 0.0 }; ///< Roll angle in degrees (-180.0° .. +180.0°)
};

/// @struct GimbalAxisCapabilities
/// @brief Dynamic hardware capabilities of a pan-tilt or 3-axis gimbal mount.
struct GimbalAxisCapabilities {
    bool hasPan { true };                    ///< True if azimuth pan axis is equipped
    bool hasTilt { true };                   ///< True if elevation tilt axis is equipped
    bool hasRoll { false };                  ///< True if roll axis is equipped (3-axis gimbal)
    bool supportsHorizonLeveling { false };  ///< True if unit supports active horizon leveling
};

/// @struct GimbalTelemetry
/// @brief Live operational status and orientation telemetry from a pan-tilt or 3-axis unit.
struct GimbalTelemetry {
    double panAngleDeg { 0.0 };                          ///< Azimuth angle in degrees (-180.0° .. +180.0° or 0.0° .. 360.0°)
    double tiltAngleDeg { 0.0 };                         ///< Elevation angle in degrees (-90.0° .. +90.0°)
    double rollAngleDeg { 0.0 };                         ///< Roll angle in degrees (-180.0° .. +180.0°)
    double panRateDegPerSec { 0.0 };                     ///< Measured azimuth velocity in deg/sec
    double tiltRateDegPerSec { 0.0 };                    ///< Measured elevation velocity in deg/sec
    double rollRateDegPerSec { 0.0 };                    ///< Measured roll velocity in deg/sec
    bool isStabilized { false };                         ///< True if active gyro stabilization is actively holding LOS
    bool isHorizonLeveled { false };                     ///< True if active horizon leveling compensation is engaged
    bool isMoving { false };                             ///< True if actuators are in motion
    bool limitReached { false };                         ///< True if soft or hard mechanical limit switch is engaged
    std::chrono::system_clock::time_point timestamp {};  ///< Telemetry timestamp
};

// =============================================================================
// Laser Range Finder (LRF) Types
// =============================================================================

/// @enum LrfMode
/// @brief Pulse emission profile for laser range finder sensors.
enum class LrfMode : std::uint8_t {
    Standby, ///< Laser diode uncharged, emissions disabled
    SingleShot, ///< Single pulse measurement on demand
    Continuous1Hz, ///< Repetitive 1 pulse-per-second ranging
    Continuous5Hz, ///< Repetitive 5 pulses-per-second ranging
    Continuous10Hz ///< Repetitive 10 pulses-per-second ranging
};

/// @struct LrfTargetMeasurement
/// @brief Laser ranging echo return data and safety telemetry.
struct LrfTargetMeasurement {
    bool valid { false }; ///< True if valid target echo was detected
    double slantRangeMeters { 0.0 }; ///< Measured line-of-sight distance in meters
    double signalQualityRatio { 0.0 }; ///< Return echo confidence score (0.0 to 1.0)
    double diodeTemperatureC { 0.0 }; ///< Laser transmitter diode temperature in Celsius
    std::uint32_t pulseCounter { 0U }; ///< Lifetime emitted pulse count
    std::chrono::system_clock::time_point timestamp {}; ///< Measurement acquisition time
};

// =============================================================================
// Camera & Optical Payload Types
// =============================================================================

/// @enum CameraSpectrum
/// @brief Sensor optical waveband category.
enum class CameraSpectrum : std::uint8_t {
    DaylightVisible, ///< Visible spectrum RGB / NIR sensor
    ThermalLWIR, ///< Long-Wave Infrared (8–14 µm) uncooled bolometer
    ThermalMWIR, ///< Mid-Wave Infrared (3–5 µm) cooled photon detector
    SWIR ///< Short-Wave Infrared (0.9–1.7 µm)
};

/// @enum ThermalPolarity
/// @brief False-color chromatic lookup table and polarity for thermal imagery.
enum class ThermalPolarity : std::uint8_t {
    WhiteHot, ///< Hotter targets render brighter white
    BlackHot, ///< Hotter targets render darker black
    FusionColor, ///< Pseudo-color thermal fusion
    Rainbow ///< High-contrast thermal rainbow
};

/// @struct CameraTelemetry
/// @brief Optical sensor parameters and calculated angular field-of-view.
struct CameraTelemetry {
    double opticalZoomFactor { 1.0 }; ///< Current physical optical magnification (e.g. 1.0x to 40.0x)
    double normalizedZoom { 0.0 }; ///< Normalized zoom range (0.0 = Wide, 1.0 = Tele)
    double focusDistanceNormalized { 0.0 }; ///< Normalized focus position (0.0 = Near, 1.0 = Infinity)
    bool autoFocusActive { true }; ///< True if continuous autofocus algorithm is engaged
    double irisNormalized { 0.0 }; ///< Normalized aperture (0.0 = Closed, 1.0 = Fully Open)
    bool autoIrisActive { true }; ///< True if auto-iris / auto-exposure is engaged
    bool dayNightIcrActive { false }; ///< True if mechanical IR-cut filter is retracted (night mode)
    double horizontalFovDeg { 60.0 }; ///< Calculated Horizontal Field-of-View in degrees
    double verticalFovDeg { 36.0 }; ///< Calculated Vertical Field-of-View in degrees
    std::chrono::system_clock::time_point timestamp {}; ///< Telemetry timestamp
};

// =============================================================================
// Laser Pointer & Illuminator Types
// =============================================================================

/// @enum IlluminatorMode
/// @brief Emission operation mode for laser illuminator / tactical pointer.
enum class IlluminatorMode : std::uint8_t {
    Standby, ///< Diode uncharged, emissions disabled
    Continuous, ///< Constant continuous-wave (CW) emission
    Pulsed, ///< Repetitive pulsing at designated frequency (target tagging / NVG)
    Strobe ///< Rapid strobe signaling / emergency flash
};

/// @struct IlluminatorTelemetry
/// @brief Live operational status and safety telemetry for laser illuminator.
struct IlluminatorTelemetry {
    bool isArmed { false }; ///< True if safety interlock is armed
    bool isEmitting { false }; ///< True if laser diode is actively firing
    IlluminatorMode mode { IlluminatorMode::Standby }; ///< Active emission profile
    double powerNormalized { 0.0 }; ///< Output power ratio [0.0 to 1.0]
    double pulseFrequencyHz { 0.0 }; ///< Pulse frequency in Hz (for Pulsed/Strobe modes)
    double beamDivergenceNormalized { 0.0 }; ///< Beam spread [0.0 = Collimated spot pointer, 1.0 = Wide flood]
    double diodeTemperatureC { 0.0 }; ///< Diode junction temperature in Celsius
    std::chrono::system_clock::time_point timestamp {}; ///< Telemetry acquisition time
};

} // namespace PayloadHal
