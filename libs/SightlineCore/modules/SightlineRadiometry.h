#pragma once

/// @file SightlineRadiometry.h
/// @brief Radiometric temperature conversion algorithms and physical temperature modeling.
/// @details Implements temperature calibration models for FLIR Tau 2, FLIR Boson, and DRS Tamarisk
///          thermal imaging sensors conforming to EAN-Infrared-Temperature.
///          Maintains zero Qt dependencies and strict MISRA C++:2023 / SEI CERT C++ compliance.

#include "SightlineDetection.h"

#include <cstdint>

namespace Sightline {

/// @enum RadiometricSensor
/// @brief Supported radiometric infrared imager types and calibration models.
enum class RadiometricSensor : std::uint8_t {
    FlirTau2LowRes = 0U,    ///< FLIR Tau 2 TLinear Low Resolution (0.4 K/count, 14-bit)
    FlirTau2HighRes = 1U,   ///< FLIR Tau 2 TLinear High Resolution (0.04 K/count, 14-bit)
    FlirBosonHighGain = 2U, ///< FLIR Boson / Boson+ High Gain (0.01 K/count, 16-bit)
    FlirBosonLowGain = 3U,  ///< FLIR Boson / Boson+ Low Gain (0.02 K/count, 16-bit)
    DrsTamarisk = 4U,       ///< DRS Tamarisk Precision Superframe (11.5 fixed-point Kelvin, 16-bit)
    CustomLinear = 5U       ///< Custom linear calibration: Temp = raw * scaleA + offsetB
};

/// @enum TemperatureScale
/// @brief Physical temperature measurement units.
enum class TemperatureScale : std::uint8_t {
    Celsius    = 0U, ///< Degrees Celsius (°C)
    Fahrenheit = 1U, ///< Degrees Fahrenheit (°F)
    Kelvin     = 2U  ///< Kelvin (K)
};

/// @struct TemperatureReading
/// @brief Multi-unit calibrated physical temperature representation.
struct TemperatureReading {
    float kelvin { 0.0F };       ///< Temperature in Kelvin (absolute zero = 0.0 K)
    float celsius { -273.15F };  ///< Temperature in degrees Celsius
    float fahrenheit { -459.67F };///< Temperature in degrees Fahrenheit
};

/// @struct RadiometricSpotStats
/// @brief Calibrated physical temperature statistics for a tracking region of interest (ROI).
struct RadiometricSpotStats {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    TemperatureReading minTemp {};     ///< Minimum calibrated temperature in gate
    TemperatureReading maxTemp {};     ///< Maximum calibrated temperature in gate
    TemperatureReading meanTemp {};    ///< Mean calibrated temperature in gate
    float stdDevKelvin { 0.0F };       ///< Standard deviation in Kelvin
};

/// @struct IsothermAgcRange
/// @brief Digital count thresholds and scale factors for AGC freezing and false-color isotherm LUTs.
struct IsothermAgcRange {
    std::uint16_t agHoldmin { 0U };        ///< Raw digital count mapped to 8-bit 0
    std::uint16_t agHoldmax { 65535U };    ///< Raw digital count mapped to 8-bit 255
    float degreesPerCount { 0.0F };        ///< Thermal resolution per 8-bit output count
};

/// @class SightlineRadiometry
/// @brief Static conversion utilities and sensor transfer functions for IR radiometry.
class SightlineRadiometry final {
public:
    SightlineRadiometry() = delete;

    // --- Temperature Unit Conversions ---

    /// @brief Converts degrees Celsius to Kelvin.
    /// @param[in] celsius Temperature in degrees Celsius.
    /// @return Temperature in Kelvin.
    [[nodiscard]] static constexpr float celsiusToKelvin(float celsius) noexcept {
        return celsius + 273.15F;
    }

    /// @brief Converts Kelvin to degrees Celsius.
    /// @param[in] kelvin Temperature in Kelvin.
    /// @return Temperature in degrees Celsius.
    [[nodiscard]] static constexpr float kelvinToCelsius(float kelvin) noexcept {
        return kelvin - 273.15F;
    }

    /// @brief Converts degrees Celsius to degrees Fahrenheit.
    /// @param[in] celsius Temperature in degrees Celsius.
    /// @return Temperature in degrees Fahrenheit.
    [[nodiscard]] static constexpr float celsiusToFahrenheit(float celsius) noexcept {
        return (celsius * 1.8F) + 32.0F;
    }

    /// @brief Converts degrees Fahrenheit to degrees Celsius.
    /// @param[in] fahrenheit Temperature in degrees Fahrenheit.
    /// @return Temperature in degrees Celsius.
    [[nodiscard]] static constexpr float fahrenheitToCelsius(float fahrenheit) noexcept {
        return (fahrenheit - 32.0F) / 1.8F;
    }

    /// @brief Converts Kelvin to degrees Fahrenheit.
    /// @param[in] kelvin Temperature in Kelvin.
    /// @return Temperature in degrees Fahrenheit.
    [[nodiscard]] static constexpr float kelvinToFahrenheit(float kelvin) noexcept {
        return ((kelvin - 273.15F) * 1.8F) + 32.0F;
    }

    /// @brief Converts degrees Fahrenheit to Kelvin.
    /// @param[in] fahrenheit Temperature in degrees Fahrenheit.
    /// @return Temperature in Kelvin.
    [[nodiscard]] static constexpr float fahrenheitToKelvin(float fahrenheit) noexcept {
        return ((fahrenheit - 32.0F) / 1.8F) + 273.15F;
    }

    /// @brief Converts a temperature value in a specified scale to Kelvin.
    /// @param[in] temp Temperature scalar.
    /// @param[in] scale Scale of input temperature.
    /// @return Temperature in Kelvin.
    [[nodiscard]] static constexpr float toKelvin(float temp, TemperatureScale scale) noexcept {
        switch (scale) {
        case TemperatureScale::Celsius:
            return celsiusToKelvin(temp);
        case TemperatureScale::Fahrenheit:
            return fahrenheitToKelvin(temp);
        case TemperatureScale::Kelvin:
        default:
            return temp;
        }
    }

    /// @brief Converts a temperature from Kelvin to a specified scale.
    /// @param[in] kelvin Absolute temperature in Kelvin.
    /// @param[in] scale Desired output temperature scale.
    /// @return Temperature in target scale.
    [[nodiscard]] static constexpr float fromKelvinToScale(float kelvin, TemperatureScale scale) noexcept {
        switch (scale) {
        case TemperatureScale::Celsius:
            return kelvinToCelsius(kelvin);
        case TemperatureScale::Fahrenheit:
            return kelvinToFahrenheit(kelvin);
        case TemperatureScale::Kelvin:
        default:
            return kelvin;
        }
    }

    /// @brief Creates a multi-unit TemperatureReading from Kelvin.
    /// @param[in] kelvin Absolute temperature in Kelvin.
    /// @return Fully populated TemperatureReading struct.
    [[nodiscard]] static constexpr TemperatureReading fromKelvin(float kelvin) noexcept {
        TemperatureReading r {};
        r.kelvin = kelvin;
        r.celsius = kelvinToCelsius(kelvin);
        r.fahrenheit = kelvinToFahrenheit(kelvin);
        return r;
    }

    /// @brief Creates a multi-unit TemperatureReading from degrees Celsius.
    /// @param[in] celsius Temperature in degrees Celsius.
    /// @return Fully populated TemperatureReading struct.
    [[nodiscard]] static constexpr TemperatureReading fromCelsius(float celsius) noexcept {
        return fromKelvin(celsiusToKelvin(celsius));
    }

    /// @brief Creates a multi-unit TemperatureReading from degrees Fahrenheit.
    /// @param[in] fahrenheit Temperature in degrees Fahrenheit.
    /// @return Fully populated TemperatureReading struct.
    [[nodiscard]] static constexpr TemperatureReading fromFahrenheit(float fahrenheit) noexcept {
        return fromKelvin(fahrenheitToKelvin(fahrenheit));
    }

    // --- Sensor Transfer Functions ---

    /// @brief Converts raw sensor count to Kelvin for a designated sensor model.
    /// @param[in] raw Raw unsigned 14-bit or 16-bit detector pixel count.
    /// @param[in] sensor Calibration model type.
    /// @param[in] scaleA Custom linear slope multiplier (used if sensor == CustomLinear).
    /// @param[in] offsetB Custom linear offset in Kelvin (used if sensor == CustomLinear).
    /// @return Temperature in Kelvin.
    [[nodiscard]] static float rawToKelvin(
        std::uint16_t raw, RadiometricSensor sensor, float scaleA = 1.0F, float offsetB = 0.0F) noexcept;

    /// @brief Converts Kelvin temperature to raw sensor counts for AGC clamping.
    /// @param[in] kelvin Target temperature in Kelvin.
    /// @param[in] sensor Calibration model type.
    /// @param[in] scaleA Custom linear slope multiplier (used if sensor == CustomLinear).
    /// @param[in] offsetB Custom linear offset in Kelvin (used if sensor == CustomLinear).
    /// @return Clamped raw sensor counts.
    [[nodiscard]] static std::uint16_t kelvinToRaw(
        float kelvin, RadiometricSensor sensor, float scaleA = 1.0F, float offsetB = 0.0F) noexcept;

    /// @brief Calculates calibrated physical temperature metrics from tracking box pixel stats.
    /// @param[in] stats Raw tracking gate pixel statistics (Message ID 0x78).
    /// @param[in] sensor Calibration model type.
    /// @param[in] scaleA Custom linear slope multiplier.
    /// @param[in] offsetB Custom linear offset in Kelvin.
    /// @return Calibrated spot statistics structure.
    [[nodiscard]] static RadiometricSpotStats calcSpotStats(
        const MsgTrackingBoxPixelStats& stats, RadiometricSensor sensor,
        float scaleA = 1.0F, float offsetB = 0.0F) noexcept;

    /// @brief Calculates manual AGC clamping limits and degrees/count for user-defined isotherm bands.
    /// @param[in] minKelvin Minimum temperature of interest in Kelvin.
    /// @param[in] maxKelvin Maximum temperature of interest in Kelvin.
    /// @param[in] sensor Calibration model type.
    /// @param[in] scaleA Custom linear slope multiplier.
    /// @param[in] offsetB Custom linear offset in Kelvin.
    /// @return Clamping parameters for MsgDigitalCameraParameters (0x70).
    [[nodiscard]] static IsothermAgcRange calcIsothermAgc(
        float minKelvin, float maxKelvin, RadiometricSensor sensor,
        float scaleA = 1.0F, float offsetB = 0.0F) noexcept;
};

} // namespace Sightline
