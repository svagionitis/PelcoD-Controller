#include "modules/SightlineRadiometry.h"
#include "modules/SightlineDetection.h"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

using namespace Sightline;

TEST(TestSightlineRadiometry, UnitConversions)
{
    // Absolute Zero
    EXPECT_NEAR(SightlineRadiometry::kelvinToCelsius(0.0F), -273.15F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::celsiusToKelvin(-273.15F), 0.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::kelvinToFahrenheit(0.0F), -459.67F, 1e-3F);

    // Freezing point of water
    EXPECT_NEAR(SightlineRadiometry::celsiusToKelvin(0.0F), 273.15F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::celsiusToFahrenheit(0.0F), 32.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::fahrenheitToCelsius(32.0F), 0.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::fahrenheitToKelvin(32.0F), 273.15F, 1e-4F);

    // Boiling point of water
    EXPECT_NEAR(SightlineRadiometry::celsiusToKelvin(100.0F), 373.15F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::celsiusToFahrenheit(100.0F), 212.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::fahrenheitToCelsius(212.0F), 100.0F, 1e-4F);

    // Parity point (-40 C == -40 F)
    EXPECT_NEAR(SightlineRadiometry::celsiusToFahrenheit(-40.0F), -40.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::fahrenheitToCelsius(-40.0F), -40.0F, 1e-4F);

    // Compound struct builders
    const auto readingK = SightlineRadiometry::fromKelvin(300.0F);
    EXPECT_NEAR(readingK.kelvin, 300.0F, 1e-4F);
    EXPECT_NEAR(readingK.celsius, 26.85F, 1e-4F);
    EXPECT_NEAR(readingK.fahrenheit, 80.33F, 1e-2F);

    const auto readingC = SightlineRadiometry::fromCelsius(25.0F);
    EXPECT_NEAR(readingC.kelvin, 298.15F, 1e-4F);
    EXPECT_NEAR(readingC.celsius, 25.0F, 1e-4F);
    EXPECT_NEAR(readingC.fahrenheit, 77.0F, 1e-4F);

    const auto readingF = SightlineRadiometry::fromFahrenheit(77.0F);
    EXPECT_NEAR(readingF.celsius, 25.0F, 1e-4F);
}

TEST(TestSightlineRadiometry, FlirTau2TransferFunctions)
{
    // Low Resolution Mode (0.4 K / count)
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(0U, RadiometricSensor::FlirTau2LowRes), 0.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(750U, RadiometricSensor::FlirTau2LowRes), 300.0F, 1e-4F);
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(16383U, RadiometricSensor::FlirTau2LowRes), 6553.2F, 1e-2F);

    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(300.0F, RadiometricSensor::FlirTau2LowRes), 750U);
    // Clamping to 14-bit max count (16383)
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(7000.0F, RadiometricSensor::FlirTau2LowRes), 16383U);

    // High Resolution Mode (0.04 K / count) - Reference values from EAN-Infrared-Temperature Section 8.1
    // 300 K = 7500 counts
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(7500U, RadiometricSensor::FlirTau2HighRes), 300.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(300.0F, RadiometricSensor::FlirTau2HighRes), 7500U);

    // 478 K = 11950 counts
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(11950U, RadiometricSensor::FlirTau2HighRes), 478.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(478.0F, RadiometricSensor::FlirTau2HighRes), 11950U);

    // Clamping to 14-bit
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(1000.0F, RadiometricSensor::FlirTau2HighRes), 16383U);
}

TEST(TestSightlineRadiometry, FlirBosonTransferFunctions)
{
    // High Gain Mode (raw / 100 => 0.01 K / count)
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(30000U, RadiometricSensor::FlirBosonHighGain), 300.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(300.0F, RadiometricSensor::FlirBosonHighGain), 30000U);

    // Low Gain Mode (raw / 50 => 0.02 K / count)
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(15000U, RadiometricSensor::FlirBosonLowGain), 300.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(300.0F, RadiometricSensor::FlirBosonLowGain), 15000U);

    // 16-bit max clamping
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(1000.0F, RadiometricSensor::FlirBosonHighGain), 65535U);
}

TEST(TestSightlineRadiometry, DrsTamariskTransferFunctions)
{
    // 11.5 fixed-point format (raw / 32.0f)
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(9600U, RadiometricSensor::DrsTamarisk), 300.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(300.0F, RadiometricSensor::DrsTamarisk), 9600U);

    // 1 count = 0.03125 K
    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(1U, RadiometricSensor::DrsTamarisk), 0.03125F, 1e-5F);

    // 16-bit max clamping
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(3000.0F, RadiometricSensor::DrsTamarisk), 65535U);
}

TEST(TestSightlineRadiometry, CustomLinearTransferFunctions)
{
    const float scaleA = 0.5F;
    const float offsetB = 20.0F;

    EXPECT_NEAR(SightlineRadiometry::rawToKelvin(200U, RadiometricSensor::CustomLinear, scaleA, offsetB), 120.0F, 1e-4F);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(120.0F, RadiometricSensor::CustomLinear, scaleA, offsetB), 200U);

    // Zero scale safety check (no division by zero)
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(100.0F, RadiometricSensor::CustomLinear, 0.0F, offsetB), 0U);
}

TEST(TestSightlineRadiometry, SpotStatisticsCalculation)
{
    MsgTrackingBoxPixelStats stats {};
    stats.cameraIndex = 2U;
    stats.trackId = 5U;
    stats.minIntensity = 18870U;
    stats.maxIntensity = 20271U;
    stats.meanIntensity = 19500U;
    stats.stdDevIntensity = 64U;

    // Evaluate on DRS Tamarisk
    const auto spot = SightlineRadiometry::calcSpotStats(stats, RadiometricSensor::DrsTamarisk);
    EXPECT_EQ(spot.cameraIndex, 2U);
    EXPECT_EQ(spot.trackId, 5U);

    EXPECT_NEAR(spot.minTemp.kelvin, 18870.0F / 32.0F, 1e-4F);
    EXPECT_NEAR(spot.maxTemp.kelvin, 20271.0F / 32.0F, 1e-4F);
    EXPECT_NEAR(spot.meanTemp.kelvin, 19500.0F / 32.0F, 1e-4F);
    EXPECT_NEAR(spot.stdDevKelvin, 64.0F / 32.0F, 1e-4F);
}

TEST(TestSightlineRadiometry, IsothermAgcRangeCalculation)
{
    // EAN-Infrared-Temperature Section 8.1 Example:
    // Range 80 F to 400 F (300 K to 478 K) on FLIR Tau 2 High Res
    const auto range = SightlineRadiometry::calcIsothermAgc(
        300.0F, 478.0F, RadiometricSensor::FlirTau2HighRes);

    EXPECT_EQ(range.agHoldmin, 7500U);
    EXPECT_EQ(range.agHoldmax, 11950U);
    EXPECT_NEAR(range.degreesPerCount, (478.0F - 300.0F) / 255.0F, 1e-4F);
}

TEST(TestSightlineRadiometry, EdgeCasesAndRobustness)
{
    // Negative Kelvin input to kelvinToRaw
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(-50.0F, RadiometricSensor::FlirTau2HighRes), 0U);

    // NaN and Inf handling
    const float nanVal = std::numeric_limits<float>::quiet_NaN();
    const float infVal = std::numeric_limits<float>::infinity();
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(nanVal, RadiometricSensor::FlirBosonHighGain), 0U);
    EXPECT_EQ(SightlineRadiometry::kelvinToRaw(infVal, RadiometricSensor::FlirBosonHighGain), 0U);

    // Inverted bounds in calcIsothermAgc
    const auto inverted = SightlineRadiometry::calcIsothermAgc(
        400.0F, 300.0F, RadiometricSensor::FlirTau2LowRes);
    EXPECT_EQ(inverted.agHoldmin, 1000U);
    EXPECT_EQ(inverted.agHoldmax, 1000U); // Clamped to min
}
