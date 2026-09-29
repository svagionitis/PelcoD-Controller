#pragma once

/// @file NmeaTypes.h
/// @brief Strongly-typed data models and enums for NMEA 0183 / IEC 61162-1 navigation and tracking sentences.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Nmea {

/// @enum NmeaSentenceId
/// @brief Supported standard and proprietary sentence identifiers.
enum class NmeaSentenceId : std::uint8_t {
    Unknown = 0,
    GGA, ///< GPS Fix Data (lat, lon, alt, quality)
    RMC, ///< Recommended Minimum Specific GNSS Data (lat, lon, sog, cog)
    HDT, ///< Heading - True (gyrocompass heading)
    THS, ///< True Heading and Status
    TTM, ///< Tracked Target Message (ARPA radar target)
    TLL, ///< Target Latitude and Longitude
    XDR, ///< Transducer Measurement (pitch, roll, environmental)
    VDM, ///< AIS VHF Data-link Message
    VDO, ///< AIS VHF Own-vessel Data-link Message
    PFEC, ///< FLIR Marine proprietary PTZ camera dialect
    RSD, ///< Radar System Data (radar cursor range & bearing)
    OSD, ///< Own Ship Data (heading, course, speed)
    APB, ///< Autopilot Sentence "B" (cross-track error, steer heading)
    BWC, ///< Bearing and Distance to Waypoint (Great Circle)
    BWR, ///< Bearing and Distance to Waypoint (Rhumb Line)
    MWV, ///< Wind Speed and Angle
    HDG, ///< Heading, Deviation & Variation
    RMB, ///< Recommended Minimum Navigation Information (XTE, steer, dest)
    RTE, ///< Routes (multi-sentence waypoint sequence)
    WPL, ///< Waypoint Location (lat, lon, waypoint ID)
    MTW, ///< Mean Water Temperature (Celsius)
    MDA, ///< Meteorological Composite Data (pressure, temp, humidity, wind)
    MMB, ///< Barometric Pressure (inHg and bar)
    PASHR, ///< Ashtech / Applanix Inertial Attitude (pitch, roll, heading, heave)
    GSA, ///< GNSS DOP and Active Satellites
    GSV, ///< GNSS Satellites in View
    ZDA, ///< UTC Time and Date
    VBW, ///< Dual Ground/Water Speed
    VHW, ///< Water Speed and Heading
    DPT, ///< Depth of Water
    DBT, ///< Depth Below Transducer
    ALF, ///< Alert Sentence (IEC 62923 BAM)
    ALC, ///< Alert Cyclic List (IEC 62923 BAM)
    ARC, ///< Alert Command Request (IEC 62923 BAM)
    HBT, ///< Heartbeat Supervision (IEC 62923 BAM)
    ALR, ///< Set Alarm State (Legacy IEC 61162-1)
    ACK ///< Acknowledge Alarm (Legacy IEC 61162-1)
};

/// @enum NmeaFixQuality
/// @brief GPS / GNSS operational fix quality reported in GGA sentences.
enum class NmeaFixQuality : std::uint8_t {
    Invalid = 0U,
    GpsFix = 1U,
    DgpsFix = 2U,
    PpsFix = 3U,
    RtkFixed = 4U,
    RtkFloat = 5U,
    Estimated = 6U,
    Manual = 7U,
    Simulation = 8U
};

/// @enum NmeaFaaMode
/// @brief FAA operational mode indicator defined in NMEA 2.3+.
enum class NmeaFaaMode : char {
    Autonomous = 'A',
    Differential = 'D',
    Estimated = 'E',
    Manual = 'M',
    Simulated = 'S',
    NotValid = 'N',
    Precise = 'P'
};

/// @enum TtmTargetStatus
/// @brief ARPA radar target tracking status reported in TTM sentences.
enum class TtmTargetStatus : char {
    Query = 'Q', ///< Target being initially evaluated / acquired
    Tracking = 'T', ///< Target actively tracked
    Lost = 'L' ///< Target lost / no radar echo
};

/// @enum TtmReference
/// @brief Reference frame for radar target bearing and speed.
enum class TtmReference : char {
    True = 'T', ///< Referenced to True North
    Relative = 'R' ///< Referenced to Own Ship's Head
};

/// @struct NmeaUtcTime
/// @brief Represents time of day in UTC parsed from hhmmss.ss format.
struct NmeaUtcTime {
    std::uint8_t hour { 0U };
    std::uint8_t minute { 0U };
    std::uint8_t second { 0U };
    std::uint16_t millisecond { 0U };
};

/// @struct NmeaDate
/// @brief Represents calendar date parsed from ddmmyy format.
struct NmeaDate {
    std::uint8_t day { 0U };
    std::uint8_t month { 0U };
    std::uint16_t year { 0U }; ///< 4-digit year (e.g. 2026)
};

/// @struct NmeaCoordinates
/// @brief Geodetic latitude and longitude coordinates in decimal degrees.
struct NmeaCoordinates {
    double latitudeDeg { 0.0 }; ///< Latitude [-90.0 .. +90.0], North positive, South negative
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0 .. +180.0], East positive, West negative
};

/// @struct GgaData
/// @brief Telemetry unpacked from $--GGA Global Positioning System Fix Data sentence.
struct GgaData {
    NmeaUtcTime utcTime {};
    NmeaCoordinates coordinates {};
    NmeaFixQuality fixQuality { NmeaFixQuality::Invalid };
    std::uint8_t numSatellites { 0U };
    double hdop { 99.9 };
    double altitudeMeters { 0.0 };
    double geoidalSeparationMeters { 0.0 };
    double dgpsAgeSeconds { 0.0 };
    std::uint16_t dgpsStationId { 0U };
    bool valid { false };
};

/// @struct RmcData
/// @brief Telemetry unpacked from $--RMC Recommended Minimum Specific GNSS Data sentence.
struct RmcData {
    NmeaUtcTime utcTime {};
    bool statusActive { false }; ///< True if 'A' (Active/Valid), false if 'V' (Void/Warning)
    NmeaCoordinates coordinates {};
    double speedOverGroundKnots { 0.0 };
    double courseOverGroundDegrees { 0.0 }; ///< [0.0 .. 360.0)
    NmeaDate date {};
    double magneticVariationDegrees { 0.0 }; ///< Positive East, Negative West
    NmeaFaaMode faaMode { NmeaFaaMode::NotValid };
    bool valid { false };
};

/// @struct HdtData
/// @brief Telemetry unpacked from $--HDT Heading - True sentence.
struct HdtData {
    double headingDegrees { 0.0 }; ///< Gyrocompass true heading [0.0 .. 360.0)
    bool valid { false };
};

/// @struct ThsData
/// @brief Telemetry unpacked from $--THS True Heading and Status sentence.
struct ThsData {
    double headingDegrees { 0.0 }; ///< True heading [0.0 .. 360.0)
    NmeaFaaMode mode { NmeaFaaMode::NotValid };
    bool valid { false };
};

/// @struct TtmData
/// @brief Radar track telemetry unpacked from $--TTM Tracked Target Message sentence.
struct TtmData {
    std::uint32_t targetNumber { 0U }; ///< Target tracking number (00 to 99)
    double targetDistanceNmi { 0.0 }; ///< Distance to target in nautical miles (or km if unit specified)
    double bearingDegrees { 0.0 }; ///< Bearing to target [0.0 .. 360.0)
    TtmReference bearingReference { TtmReference::True };
    double targetSpeedKnots { 0.0 }; ///< Target speed in knots
    double targetCourseDegrees { 0.0 }; ///< Target course [0.0 .. 360.0)
    TtmReference courseReference { TtmReference::True };
    double distanceCpaNmi { 0.0 }; ///< Distance at closest point of approach
    double timeCpaMinutes { 0.0 }; ///< Time to CPA (minutes, negative indicates past)
    char speedDistanceUnits { 'K' }; ///< 'K'=Knots/NM, 'S'=Statute Miles, 'M'=km
    std::string targetName {}; ///< Target identifier name or label
    TtmTargetStatus status { TtmTargetStatus::Lost };
    bool referenceTarget { false }; ///< 'R' if reference target, 'T' otherwise
    NmeaUtcTime utcTimeTag {};
    char acquisitionType { 'A' }; ///< 'A'=Auto, 'M'=Manual
    bool valid { false };
};

/// @struct TllData
/// @brief Target geographic coordinate telemetry unpacked from $--TLL sentence.
struct TllData {
    std::uint32_t targetNumber { 0U };
    NmeaCoordinates coordinates {};
    std::string targetName {};
    NmeaUtcTime utcTimeTag {};
    TtmTargetStatus status { TtmTargetStatus::Lost };
    bool referenceTarget { false };
    bool valid { false };
};

/// @struct XdrTransducer
/// @brief Single sensor transducer entry in an $--XDR sentence.
struct XdrTransducer {
    char type { '\0' }; ///< 'A'=Angular, 'C'=Temperature, 'P'=Pressure, etc.
    double measurement { 0.0 }; ///< Sensor value
    char units { '\0' }; ///< 'D'=Degrees, 'C'=Celsius, 'B'=Bar, etc.
    std::string id {}; ///< Transducer name (e.g. "PITCH", "ROLL")
};

/// @struct XdrData
/// @brief Transducer telemetry unpacked from $--XDR sentence.
struct XdrData {
    std::vector<XdrTransducer> transducers {};
    bool valid { false };
};

/// @struct PashrData
/// @brief RT300 / Applanix / Ashtech inertial attitude telemetry unpacked from $PASHR sentence.
struct PashrData {
    NmeaUtcTime utcTime {};
    double headingDegrees { 0.0 };     ///< Vessel heading in degrees [0.0 .. 360.0)
    bool isTrueHeading { true };        ///< True if 'T', False if 'M'
    double rollDegrees { 0.0 };         ///< Vessel roll in degrees (starboard down positive)
    double pitchDegrees { 0.0 };        ///< Vessel pitch in degrees (bow up positive)
    double heaveMeters { 0.0 };         ///< Vessel heave in meters
    double rollAccuracyDeg { 0.0 };     ///< Roll standard deviation / accuracy
    double pitchAccuracyDeg { 0.0 };    ///< Pitch standard deviation / accuracy
    double headingAccuracyDeg { 0.0 };  ///< Heading standard deviation / accuracy
    std::uint8_t gpsQualityFlag { 0U }; ///< GPS fix quality flag
    std::uint8_t imuStatusFlag { 0U };  ///< Inertial / IMU status flag
    bool valid { false };
};

/// @struct PfecAttitudeData
/// @brief Vessel attitude (yaw, pitch, roll) unpacked from FLIR $PFEC,GPatt sentence.
struct PfecAttitudeData {
    double yawDegrees { 0.0 };   ///< Vessel yaw / heading in degrees [0.0 .. 360.0)
    double pitchDegrees { 0.0 }; ///< Vessel pitch in degrees (bow up positive)
    double rollDegrees { 0.0 };  ///< Vessel roll in degrees (starboard down positive)
    bool valid { false };
};

/// @struct AttitudeData
/// @brief Unified vessel attitude report containing pitch, roll, heading, and heave.
struct AttitudeData {
    double pitchDegrees { 0.0 };   ///< Pitch in degrees (bow up positive)
    double rollDegrees { 0.0 };    ///< Roll in degrees (starboard down positive)
    double headingDegrees { 0.0 }; ///< Heading / yaw in degrees [0.0 .. 360.0)
    double heaveMeters { 0.0 };    ///< Heave in meters
    bool hasHeading { false };
    bool valid { false };
};

/// @struct NmeaNavSnapshot
/// @brief Unified own-ship navigation snapshot synthesized from GPS, gyro, and transducer sentences.
struct NmeaNavSnapshot {
    NmeaCoordinates position {};
    double altitudeMeters { 0.0 };
    double trueHeadingDegrees { 0.0 };
    double sogKnots { 0.0 };
    double cogDegrees { 0.0 };
    double pitchDegrees { 0.0 };
    double rollDegrees { 0.0 };
    NmeaFixQuality fixQuality { NmeaFixQuality::Invalid };
    std::chrono::steady_clock::time_point timestamp {};
    bool hasPosition { false };
    bool hasHeading { false };
    bool hasAttitude { false };
};

/// @struct NmeaTagBlock
/// @brief Metadata parsed from NMEA 0183 v4.00+ / IEC 61162-1 tag blocks (\s:...,c:...*hh\).
struct NmeaTagBlock {
    std::string sourceId {}; ///< Station / talker identifier ('s:')
    std::uint64_t timestampEpochSec { 0ULL }; ///< UNIX epoch timestamp in seconds ('c:')
    std::string grouping {}; ///< Multi-sentence grouping ('g:', e.g. "1-2-1234")
    std::uint32_t lineCount { 0U }; ///< Line sequence number ('n:')
    std::string destinationId {}; ///< Destination system identifier ('d:')
    std::string text {}; ///< Free text / remark ('t:')
    bool valid { false };
};

/// @struct RsdData
/// @brief Radar cursor and system telemetry unpacked from $--RSD sentence.
struct RsdData {
    double cursorRangeNmi { 0.0 }; ///< Distance from own ship to active cursor in nautical miles
    double cursorBearingDeg { 0.0 }; ///< Bearing from own ship to active cursor [0.0 .. 360.0)
    double rangeScaleNmi { 0.0 }; ///< Display range scale in nautical miles
    char displayRotation { 'N' }; ///< 'C'=Course-up, 'H'=Head-up, 'N'=North-up
    bool valid { false };
};

/// @struct OsdData
/// @brief Own ship navigation data unpacked from $--OSD sentence.
struct OsdData {
    double headingDegrees { 0.0 }; ///< Heading in degrees [0.0 .. 360.0)
    bool headingValid { false }; ///< True if 'A', False if 'V'
    double courseDegrees { 0.0 }; ///< Course in degrees [0.0 .. 360.0)
    char courseReference { 'B' }; ///< 'B'=Bottom, 'M'=Manually, 'W'=Water, 'R'=Radar, 'P'=Positioning
    double vesselSpeed { 0.0 }; ///< Vessel speed
    char speedReference { 'B' };
    double vesselSetDeg { 0.0 }; ///< Set direction
    double vesselDriftSpeed { 0.0 }; ///< Drift speed
    char speedUnits { 'N' }; ///< 'K'=km/h, 'N'=knots, 'S'=statute miles/h
    bool valid { false };
};

/// @struct ApbData
/// @brief Autopilot sentence "B" unpacked from $--APB sentence.
struct ApbData {
    bool generalWarning { false }; ///< True if status1 is 'V'
    bool cycleLockWarning { false }; ///< True if status2 is 'V'
    double crossTrackErrorNmi { 0.0 }; ///< Magnitude of cross track error
    char directionToSteer { 'L' }; ///< 'L' or 'R'
    char xteUnits { 'N' }; ///< 'N'=Nautical Miles, 'K'=Kilometers
    bool arrivalCircleEntered { false }; ///< True if 'A'
    bool perpendicularPassed { false }; ///< True if 'A'
    double bearingOriginToDestDeg { 0.0 }; ///< Bearing origin to destination [0.0 .. 360.0)
    char bearingOriginRef { 'T' }; ///< 'M'=Magnetic, 'T'=True
    std::string destWaypointId {}; ///< Destination waypoint name/identifier
    double bearingPresentToDestDeg { 0.0 }; ///< Bearing present position to destination
    char bearingPresentRef { 'T' };
    double headingToSteerDeg { 0.0 }; ///< Commanded heading to steer to waypoint
    char headingToSteerRef { 'T' };
    NmeaFaaMode faaMode { NmeaFaaMode::Autonomous };
    bool valid { false };
};

/// @struct BwcData
/// @brief Bearing and distance to waypoint unpacked from $--BWC sentence.
struct BwcData {
    NmeaUtcTime utcTime {};
    NmeaCoordinates waypointCoordinates {};
    double bearingTrueDeg { 0.0 };
    double bearingMagneticDeg { 0.0 };
    double distanceNmi { 0.0 };
    std::string waypointId {};
    NmeaFaaMode faaMode { NmeaFaaMode::Autonomous };
    bool valid { false };
};

/// @struct MwvData
/// @brief Wind speed and angle telemetry unpacked from $--MWV sentence.
struct MwvData {
    double windAngleDeg { 0.0 }; ///< Wind angle [0.0 .. 359.0]
    char reference { 'R' }; ///< 'R'=Relative/Apparent, 'T'=Theoretical/True
    double windSpeed { 0.0 }; ///< Wind speed magnitude
    char speedUnits { 'N' }; ///< 'K'=km/h, 'M'=m/s, 'N'=knots
    bool valid { false };
};

/// @struct HdgData
/// @brief Heading, deviation and variation unpacked from $--HDG sentence.
struct HdgData {
    double magneticHeadingDeg { 0.0 }; ///< Sensor magnetic heading [0.0 .. 360.0)
    double magneticDeviationDeg { 0.0 }; ///< Magnetic deviation (positive East, negative West)
    double magneticVariationDeg { 0.0 }; ///< Magnetic variation (positive East, negative West)
    bool hasDeviation { false };
    bool hasVariation { false };
    bool valid { false };
};

/// @struct RmbData
/// @brief Navigation and cross-track error telemetry unpacked from $--RMB sentence.
struct RmbData {
    bool statusActive { false }; ///< True if 'A' (Active/OK), False if 'V' (Warning)
    double crossTrackErrorNmi { 0.0 }; ///< Magnitude of cross track error in nautical miles
    char directionToSteer { 'L' }; ///< 'L'=Steer Left, 'R'=Steer Right
    std::string destWaypointId {}; ///< TO waypoint identifier
    std::string originWaypointId {}; ///< FROM waypoint identifier
    NmeaCoordinates destCoordinates {}; ///< Destination waypoint coordinates
    double rangeToDestNmi { 0.0 }; ///< Range to destination in nautical miles
    double bearingToDestTrueDeg { 0.0 }; ///< Bearing to destination in degrees True [0.0 .. 360.0)
    double closingVelocityKnots { 0.0 }; ///< Destination closing velocity (VMG) in knots
    bool arrivalAlarm { false }; ///< True if 'A' (arrival circle entered or perpendicular passed)
    NmeaFaaMode faaMode { NmeaFaaMode::Autonomous };
    bool valid { false };
};

/// @struct RteData
/// @brief Route waypoint sequence unpacked from $--RTE sentence.
struct RteData {
    std::uint32_t totalSentences { 1U }; ///< Total sentences being transmitted for this route
    std::uint32_t sentenceNumber { 1U }; ///< Sequence number of this sentence (1-indexed)
    char routeType { 'c' }; ///< 'c'=Complete route, 'w'=Working route
    std::string routeName {}; ///< Route identifier
    std::vector<std::string> waypointIds {}; ///< Waypoints in this sentence slice
    bool valid { false };
};

/// @struct WplData
/// @brief Waypoint coordinates unpacked from $--WPL sentence.
struct WplData {
    NmeaCoordinates coordinates {}; ///< Geodetic latitude and longitude
    std::string waypointId {}; ///< Waypoint identifier / name
    bool valid { false };
};

/// @struct MtwData
/// @brief Water temperature unpacked from $--MTW sentence.
struct MtwData {
    double waterTemperatureCelsius { 0.0 }; ///< Water surface temperature in degrees Celsius
    bool valid { false };
};

/// @struct MmbData
/// @brief Barometric pressure unpacked from $--MMB sentence.
struct MmbData {
    double pressureInHg { 0.0 }; ///< Barometric pressure in inches of mercury
    double pressureBars { 0.0 }; ///< Barometric pressure in bars
    bool valid { false };
};

/// @struct MdaData
/// @brief Meteorological composite telemetry unpacked from $--MDA sentence.
struct MdaData {
    std::optional<double> barometricPressureInHg {}; ///< Barometric pressure in inHg
    std::optional<double> barometricPressureBars {}; ///< Barometric pressure in bars
    std::optional<double> airTemperatureCelsius {}; ///< Air temperature in degrees Celsius
    std::optional<double> waterTemperatureCelsius {}; ///< Water surface temperature in degrees Celsius
    std::optional<double> relativeHumidityPercent {}; ///< Relative humidity [0.0 .. 100.0]
    std::optional<double> absoluteHumidityGPerM3 {}; ///< Absolute humidity
    std::optional<double> dewPointCelsius {}; ///< Dew point temperature in degrees Celsius
    std::optional<double> windDirectionTrueDeg {}; ///< Wind direction true [0.0 .. 360.0)
    std::optional<double> windDirectionMagneticDeg {}; ///< Wind direction magnetic [0.0 .. 360.0)
    std::optional<double> windSpeedKnots {}; ///< Wind speed in knots
    std::optional<double> windSpeedMps {}; ///< Wind speed in meters per second
    bool valid { false };
};

/// @struct NmeaEnvironmentSnapshot
/// @brief Unified environmental state synthesized from meteorological and sea sensors.
struct NmeaEnvironmentSnapshot {
    std::optional<double> waterTemperatureCelsius {};
    std::optional<double> airTemperatureCelsius {};
    std::optional<double> seaAirDeltaTCelsius {}; ///< T_air - T_water (positive: air warmer; negative: water warmer)
    std::optional<double> relativeHumidityPercent {};
    std::optional<double> dewPointCelsius {};
    std::optional<double> barometricPressureHpa {}; ///< Pressure in hectopascals / millibars (1 bar = 1000 hPa)
    std::optional<double> windSpeedKnots {};
    std::optional<double> windDirectionTrueDeg {};
    std::chrono::steady_clock::time_point timestamp {};
    bool hasWaterTemp { false };
    bool hasAirTemp { false };
    bool hasHumidity { false };
    bool hasDewPoint { false };
    bool hasPressure { false };
    bool hasWind { false };
};

/// @struct GsaData
/// @brief GNSS DOP and active satellites unpacked from $--GSA sentence.
struct GsaData {
    char selectionMode { 'M' }; ///< 'M' = Manual, 'A' = Automatic 2D/3D
    std::uint8_t fixMode { 1U }; ///< 1 = Fix not available, 2 = 2D, 3 = 3D
    std::vector<std::uint8_t> activeSatellitePrns {}; ///< Up to 12 PRNs tracking
    double pdop { 99.9 }; ///< Dilution of precision (positional)
    double hdop { 99.9 }; ///< Dilution of precision (horizontal)
    double vdop { 99.9 }; ///< Dilution of precision (vertical)
    std::optional<std::uint8_t> systemId {}; ///< GNSS System ID (NMEA 4.10+: 1=GPS, 2=GLONASS, 3=Galileo, 4=BeiDou)
    bool valid { false };
};

/// @struct GsvSatelliteInfo
/// @brief Individual space vehicle status reported in $--GSV sentences.
struct GsvSatelliteInfo {
    std::uint16_t prn { 0U }; ///< Satellite PRN / ID number
    double elevationDeg { 0.0 }; ///< Elevation angle [0.0 .. 90.0] degrees
    double azimuthDeg { 0.0 }; ///< Azimuth angle [0.0 .. 359.0] degrees
    std::optional<double> snrDb {}; ///< Carrier-to-noise ratio (SNR) in dB-Hz [0 .. 99]
};

/// @struct GsvData
/// @brief GNSS satellites in view unpacked from $--GSV sentence sequence.
struct GsvData {
    std::uint8_t totalSentences { 1U }; ///< Total sentences in sequence [1 .. 9]
    std::uint8_t sentenceNumber { 1U }; ///< Sentence sequence number [1 .. 9]
    std::uint16_t totalSatellitesInView { 0U }; ///< Total satellites in view across constellation
    std::vector<GsvSatelliteInfo> satellites {}; ///< Up to 4 satellites in this sentence
    std::optional<std::uint8_t> signalId {}; ///< Signal ID (NMEA 4.10+)
    bool valid { false };
};

/// @struct ZdaData
/// @brief UTC time and calendar date unpacked from $--ZDA sentence.
struct ZdaData {
    NmeaUtcTime utcTime {}; ///< Universal Time Coordinated
    std::uint8_t day { 0U }; ///< Day of month [1 .. 31]
    std::uint8_t month { 0U }; ///< Month [1 .. 12]
    std::uint16_t year { 0U }; ///< 4-digit year (e.g. 2026)
    std::int8_t localZoneHours { 0 }; ///< Local zone description hours [-13 .. +13]
    std::uint8_t localZoneMinutes { 0U }; ///< Local zone description minutes [0 .. 59]
    bool valid { false };
};

/// @struct VbwData
/// @brief Dual ground and water speed unpacked from $--VBW sentence.
struct VbwData {
    double longitudinalWaterSpeedKnots { 0.0 }; ///< Fore/Aft water speed (+ fore, - aft)
    double transverseWaterSpeedKnots { 0.0 }; ///< Port/Starboard water speed (+ stbd, - port)
    char waterSpeedStatus { 'V' }; ///< 'A' = Valid, 'V' = Invalid
    double longitudinalGroundSpeedKnots { 0.0 }; ///< Fore/Aft ground speed (+ fore, - aft)
    double transverseGroundSpeedKnots { 0.0 }; ///< Port/Starboard ground speed (+ stbd, - port)
    char groundSpeedStatus { 'V' }; ///< 'A' = Valid, 'V' = Invalid
    std::optional<double> sternWaterSpeedKnots {}; ///< Stern transverse water speed (NMEA 3.0+)
    std::optional<char> sternWaterStatus {}; ///< 'A' = Valid, 'V' = Invalid
    std::optional<double> sternGroundSpeedKnots {}; ///< Stern transverse ground speed (NMEA 3.0+)
    std::optional<char> sternGroundStatus {}; ///< 'A' = Valid, 'V' = Invalid
    bool valid { false };
};

/// @struct VhwData
/// @brief Water speed and heading unpacked from $--VHW sentence.
struct VhwData {
    std::optional<double> headingDegreesTrue {}; ///< Heading true [0.0 .. 360.0)
    std::optional<double> headingDegreesMagnetic {}; ///< Heading magnetic [0.0 .. 360.0)
    std::optional<double> speedWaterKnots {}; ///< Speed through water in knots
    std::optional<double> speedWaterKmh {}; ///< Speed through water in km/h
    bool valid { false };
};

/// @struct DptData
/// @brief Water depth and transducer keel/waterline offset unpacked from $--DPT sentence.
struct DptData {
    double waterDepthMeters { 0.0 }; ///< Water depth relative to transducer in meters
    double offsetMeters { 0.0 }; ///< Offset (+ distance to waterline = depth, - distance to keel = under-keel clearance)
    std::optional<double> maximumRangeScaleMeters {}; ///< Maximum range scale in meters
    bool valid { false };
};

/// @struct DbtData
/// @brief Depth below transducer in feet, meters, and fathoms unpacked from $--DBT sentence.
struct DbtData {
    double depthFeet { 0.0 }; ///< Water depth below transducer in feet
    double depthMeters { 0.0 }; ///< Water depth below transducer in meters
    double depthFathoms { 0.0 }; ///< Water depth below transducer in fathoms
    bool valid { false };
};

} // namespace Nmea
