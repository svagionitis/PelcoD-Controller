/// @file TestNmeaDomainValueObjects.cpp
/// @brief Unit tests for strongly-typed domain value objects in libs/Nmea.

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "AisTypes.h"
#include "NmeaTypes.h"
#include "NmeaValueObjects.h"

using namespace Nmea;

TEST(TestNmeaDomainValueObjects, LatitudeClampingAndInvariants)
{
    // Default initialization
    constexpr Latitude latDefault {};
    EXPECT_DOUBLE_EQ(latDefault.degrees(), 0.0);
    EXPECT_DOUBLE_EQ(latDefault.value(), 0.0);
    EXPECT_TRUE(latDefault.isValid());

    // Valid values
    constexpr Latitude latNorth { 45.5 };
    EXPECT_DOUBLE_EQ(latNorth.degrees(), 45.5);

    constexpr Latitude latSouth { -60.25 };
    EXPECT_DOUBLE_EQ(latSouth.degrees(), -60.25);

    // Boundary conditions
    constexpr Latitude latMax { 90.0 };
    EXPECT_DOUBLE_EQ(latMax.degrees(), 90.0);
    EXPECT_TRUE(latMax.isValid());

    constexpr Latitude latMin { -90.0 };
    EXPECT_DOUBLE_EQ(latMin.degrees(), -90.0);
    EXPECT_TRUE(latMin.isValid());

    // Out-of-bounds clamping
    const Latitude latOver { 105.0 };
    EXPECT_DOUBLE_EQ(latOver.degrees(), 90.0);

    const Latitude latUnder { -120.0 };
    EXPECT_DOUBLE_EQ(latUnder.degrees(), -90.0);

    // NaN handling
    const Latitude latNan { std::numeric_limits<double>::quiet_NaN() };
    EXPECT_DOUBLE_EQ(latNan.degrees(), 0.0);

    // Relational operators
    EXPECT_TRUE(latSouth < latNorth);
    EXPECT_TRUE(latNorth > latSouth);
    EXPECT_TRUE(latNorth == Latitude { 45.5 });
    EXPECT_TRUE(latNorth != latSouth);
    EXPECT_TRUE(latSouth <= latNorth);
    EXPECT_TRUE(latNorth >= latSouth);
}

TEST(TestNmeaDomainValueObjects, LongitudeCircularNormalization)
{
    // Default initialization
    constexpr Longitude lonDefault {};
    EXPECT_DOUBLE_EQ(lonDefault.degrees(), 0.0);
    EXPECT_DOUBLE_EQ(lonDefault.value(), 0.0);
    EXPECT_TRUE(lonDefault.isValid());

    // Valid range
    constexpr Longitude lonEast { 120.0 };
    EXPECT_DOUBLE_EQ(lonEast.degrees(), 120.0);

    constexpr Longitude lonWest { -150.0 };
    EXPECT_DOUBLE_EQ(lonWest.degrees(), -150.0);

    // Boundary meridians
    constexpr Longitude lon180 { 180.0 };
    EXPECT_DOUBLE_EQ(lon180.degrees(), 180.0);

    constexpr Longitude lonNeg180 { -180.0 };
    EXPECT_DOUBLE_EQ(lonNeg180.degrees(), -180.0);

    // Circular normalization beyond +/- 180
    const Longitude lonOver180 { 190.0 }; // Should wrap to -170
    EXPECT_NEAR(lonOver180.degrees(), -170.0, 1e-9);

    const Longitude lonUnderNeg180 { -195.0 }; // Should wrap to +165
    EXPECT_NEAR(lonUnderNeg180.degrees(), 165.0, 1e-9);

    const Longitude lonFullWrap { 540.0 }; // 540 - 360 = 180
    EXPECT_NEAR(lonFullWrap.degrees(), 180.0, 1e-9);

    // Large angle normalization
    const Longitude lonLargeWrap { 730.0 }; // 730 - 720 = 10
    EXPECT_NEAR(lonLargeWrap.degrees(), 10.0, 1e-9);

    // NaN handling
    const Longitude lonNan { std::numeric_limits<double>::quiet_NaN() };
    EXPECT_DOUBLE_EQ(lonNan.degrees(), 0.0);

    // Relational operators
    EXPECT_TRUE(lonWest < lonEast);
    EXPECT_TRUE(lonEast > lonWest);
    EXPECT_TRUE(lonEast == Longitude { 120.0 });
    EXPECT_TRUE(lonEast != lonWest);
}

TEST(TestNmeaDomainValueObjects, HeadingDegreesCircularWrapping)
{
    // Default initialization
    constexpr HeadingDegrees hdgDefault {};
    EXPECT_DOUBLE_EQ(hdgDefault.degrees(), 0.0);
    EXPECT_DOUBLE_EQ(hdgDefault.value(), 0.0);
    EXPECT_TRUE(hdgDefault.isValid());

    // Valid heading
    constexpr HeadingDegrees hdgEast { 90.0 };
    EXPECT_DOUBLE_EQ(hdgEast.degrees(), 90.0);

    // 360 degree wrapping to 0
    const HeadingDegrees hdg360 { 360.0 };
    EXPECT_DOUBLE_EQ(hdg360.degrees(), 0.0);

    // Multi-revolution wrapping
    const HeadingDegrees hdgOver { 725.0 }; // 725 - 720 = 5.0
    EXPECT_NEAR(hdgOver.degrees(), 5.0, 1e-9);

    // Negative heading wrapping
    const HeadingDegrees hdgNeg { -30.0 }; // 360 - 30 = 330.0
    EXPECT_NEAR(hdgNeg.degrees(), 330.0, 1e-9);

    // NaN handling
    const HeadingDegrees hdgNan { std::numeric_limits<double>::quiet_NaN() };
    EXPECT_DOUBLE_EQ(hdgNan.degrees(), 0.0);

    // Operators
    EXPECT_TRUE(hdgEast == HeadingDegrees { 90.0 });
    EXPECT_TRUE(hdgEast != hdgDefault);
    EXPECT_TRUE(hdgDefault < hdgEast);
}

TEST(TestNmeaDomainValueObjects, SpeedKnotsNonNegativeClamping)
{
    // Default initialization
    constexpr SpeedKnots sogDefault {};
    EXPECT_DOUBLE_EQ(sogDefault.knots(), 0.0);
    EXPECT_DOUBLE_EQ(sogDefault.value(), 0.0);

    // Valid speed
    constexpr SpeedKnots sogCruising { 24.5 };
    EXPECT_DOUBLE_EQ(sogCruising.knots(), 24.5);

    // Negative speed clamping
    const SpeedKnots sogNegative { -10.5 };
    EXPECT_DOUBLE_EQ(sogNegative.knots(), 0.0);

    // NaN handling
    const SpeedKnots sogNan { std::numeric_limits<double>::quiet_NaN() };
    EXPECT_DOUBLE_EQ(sogNan.knots(), 0.0);

    // Comparison operators
    EXPECT_TRUE(sogCruising > sogDefault);
    EXPECT_TRUE(sogDefault < sogCruising);
    EXPECT_TRUE(sogCruising == SpeedKnots { 24.5 });
    EXPECT_TRUE(sogCruising != sogDefault);
    EXPECT_TRUE(sogDefault <= sogCruising);
    EXPECT_TRUE(sogCruising >= sogDefault);
}

TEST(TestNmeaDomainValueObjects, MmsiValidationAndBeaconClassification)
{
    // Default initialization
    constexpr Mmsi mmsiDefault {};
    EXPECT_EQ(mmsiDefault.value(), 0U);
    EXPECT_EQ(mmsiDefault.raw(), 0U);
    EXPECT_FALSE(mmsiDefault.isValid());
    EXPECT_EQ(mmsiDefault.beaconType(), AisBeaconType::None);
    EXPECT_FALSE(mmsiDefault.isEmergencyBeacon());

    // Valid commercial ship MMSI
    constexpr Mmsi shipMmsi { 211234567U };
    EXPECT_TRUE(shipMmsi.isValid());
    EXPECT_EQ(shipMmsi.beaconType(), AisBeaconType::StandardVessel);
    EXPECT_FALSE(shipMmsi.isEmergencyBeacon());
    EXPECT_EQ(shipMmsi.value(), 211234567U);

    // SART emergency distress beacon (970 prefix)
    constexpr Mmsi sartMmsi { 970123456U };
    EXPECT_TRUE(sartMmsi.isValid());
    EXPECT_EQ(sartMmsi.beaconType(), AisBeaconType::AisSart);
    EXPECT_TRUE(sartMmsi.isEmergencyBeacon());

    // MOB emergency distress beacon (972 prefix)
    constexpr Mmsi mobMmsi { 972987654U };
    EXPECT_TRUE(mobMmsi.isValid());
    EXPECT_EQ(mobMmsi.beaconType(), AisBeaconType::AisMob);
    EXPECT_TRUE(mobMmsi.isEmergencyBeacon());

    // EPIRB emergency distress beacon (974 prefix)
    constexpr Mmsi epirbMmsi { 974112233U };
    EXPECT_TRUE(epirbMmsi.isValid());
    EXPECT_EQ(epirbMmsi.beaconType(), AisBeaconType::EpirbAis);
    EXPECT_TRUE(epirbMmsi.isEmergencyBeacon());

    // SAR Aircraft (111 prefix)
    constexpr Mmsi aircraftMmsi { 111222333U };
    EXPECT_TRUE(aircraftMmsi.isValid());
    EXPECT_EQ(aircraftMmsi.beaconType(), AisBeaconType::SarAircraft);
    EXPECT_TRUE(aircraftMmsi.isEmergencyBeacon());

    // Out-of-range MMSI (> 9 digits) rejected
    const Mmsi invalidMmsi { 1234567890U };
    EXPECT_EQ(invalidMmsi.value(), 0U);
    EXPECT_FALSE(invalidMmsi.isValid());

    // Assignment from uint32_t
    Mmsi assignedMmsi {};
    assignedMmsi = 970999888U;
    EXPECT_TRUE(assignedMmsi.isValid());
    EXPECT_EQ(assignedMmsi.beaconType(), AisBeaconType::AisSart);

    // Comparisons
    EXPECT_TRUE(shipMmsi == 211234567U);
    EXPECT_TRUE(shipMmsi != 0U);
    EXPECT_TRUE(shipMmsi == Mmsi { 211234567U });
    EXPECT_TRUE(shipMmsi != sartMmsi);
    EXPECT_TRUE(shipMmsi < sartMmsi);
}

TEST(TestNmeaDomainValueObjects, NmeaCoordinatesComposition)
{
    const NmeaCoordinates coords { 37.7749, -122.4194 };
    EXPECT_NEAR(coords.latitude.degrees(), 37.7749, 1e-4);
    EXPECT_NEAR(coords.longitude.degrees(), -122.4194, 1e-4);
    EXPECT_NEAR(coords.latitudeDeg(), 37.7749, 1e-4);
    EXPECT_NEAR(coords.longitudeDeg(), -122.4194, 1e-4);
    EXPECT_TRUE(coords.isValid());

    // Setters
    NmeaCoordinates modCoords {};
    modCoords.setLatitude(Latitude(48.1173));
    modCoords.setLongitude(Longitude(11.5167));
    EXPECT_NEAR(modCoords.latitudeDeg(), 48.1173, 1e-4);
    EXPECT_NEAR(modCoords.longitudeDeg(), 11.5167, 1e-4);

    modCoords.setLatitudeDeg(-33.8688);
    modCoords.setLongitudeDeg(151.2093);
    EXPECT_NEAR(modCoords.latitudeDeg(), -33.8688, 1e-4);
    EXPECT_NEAR(modCoords.longitudeDeg(), 151.2093, 1e-4);

    // Equality
    const NmeaCoordinates coordsCopy = coords;
    EXPECT_TRUE(coords == coordsCopy);
    EXPECT_TRUE(coords != modCoords);
}
