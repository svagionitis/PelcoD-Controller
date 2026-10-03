#pragma once

/// @file Misb0805.h
/// @brief MISB ST 0805.1 KLV to Cursor-on-Target (CoT) Conversions implementation.
/// @details Translates MISB ST 0601 (Platform Position, Sensor Point of Interest)
///          and MISB ST 0806 (RVT Points of Interest) into standard Cursor-on-Target XML events.
/// @see MISB ST 0805.1 "KLV to Cursor-on-Target (CoT) Conversions"

#include "KlvTypes.h"
#include "RvtTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Klv {

/// @struct CotPoint
/// @brief 3D geographic point and circular/linear error estimates in CoT schema.
struct CotPoint {
    double lat { 0.0 };         ///< WGS-84 latitude in decimal degrees [-90.0, +90.0]
    double lon { 0.0 };         ///< WGS-84 longitude in decimal degrees [-180.0, +180.0]
    double hae { 0.0 };         ///< Height Above Ellipsoid (HAE) in meters
    double ce { 9999999.0 };    ///< Circular 1-sigma error estimate in meters (9999999 = unknown)
    double le { 9999999.0 };    ///< Linear 1-sigma error estimate in meters (9999999 = unknown)
};

/// @struct CotEvent
/// @brief Strongly-typed representation of a Cursor-on-Target (CoT) XML event.
struct CotEvent {
    std::string version { "2.0" };     ///< CoT schema version
    std::string uid {};                ///< Unique event identifier
    std::string type {};               ///< MIL-STD-2525 / CoT hierarchical entity type
    std::string time {};               ///< ISO-8601 UTC event generation time
    std::string start {};              ///< ISO-8601 UTC validity start time
    std::string stale {};              ///< ISO-8601 UTC validity expiration time
    std::string how { "m-p" };         ///< Acquisition method ("m-p" = machine-passed)
    CotPoint point {};                 ///< 3D location and error estimates
    std::string detailXml {};          ///< Inner XML contents of the <detail> tag

    /// @brief Serializes the CoT event into an XML string.
    /// @return XML formatted Cursor-on-Target message string.
    [[nodiscard]] std::string toXml() const;
};

/// @class Misb0805
/// @brief Translates MISB ST 0601 and ST 0806 telemetry into Cursor-on-Target (CoT) events.
class Misb0805 {
public:
    /// @brief Converts MISB ST 0601 telemetry into a CoT Platform Position message.
    /// @details Conforms to MISB ST 0805.1 Table 1.
    /// @param[in] msg Source MISB ST 0601 UAS Datalink message.
    /// @param[in] staleSec Duration in seconds until event is marked stale (default 10.0).
    /// @param[in] platformType CoT entity type (default "a-f-A-M-F" for friendly military fixed-wing).
    /// @return CotEvent representing platform position.
    [[nodiscard]] static CotEvent toPlatformPosition(
        const UasDatalinkMessage& msg,
        double staleSec = 10.0,
        const std::string& platformType = "a-f-A-M-F");

    /// @brief Converts MISB ST 0601 telemetry into a CoT Sensor Point of Interest (SPI) message.
    /// @details Conforms to MISB ST 0805.1 Table 2.
    /// @param[in] msg Source MISB ST 0601 UAS Datalink message.
    /// @param[in] staleSec Duration in seconds until event is marked stale (default 10.0).
    /// @param[in] platformType CoT parent entity type (default "a-f-A-M-F").
    /// @return CotEvent representing Sensor Point of Interest (b-m-p-s-p-i).
    [[nodiscard]] static CotEvent toSensorPointOfInterest(
        const UasDatalinkMessage& msg,
        double staleSec = 10.0,
        const std::string& platformType = "a-f-A-M-F");

    /// @brief Converts a MISB ST 0806 Point of Interest (POI) Pack into a CoT event.
    /// @param[in] poi Point of Interest pack to translate.
    /// @param[in] parentUid Platform UID to link the POI to.
    /// @param[in] timestampUs Microseconds since UNIX epoch (0 = current time).
    /// @param[in] staleSec Duration in seconds until event is marked stale (default 30.0).
    /// @return CotEvent representing the tactical POI.
    [[nodiscard]] static CotEvent toCot(
        const PoiPack& poi,
        const std::string& parentUid = "",
        std::uint64_t timestampUs = 0U,
        double staleSec = 30.0);

    /// @brief Converts all Points of Interest in an RVT Local Set into CoT events.
    /// @param[in] rvt RVT Local Set containing POIs.
    /// @param[in] parentUid Optional parent platform UID for linking.
    /// @param[in] staleSec Duration in seconds until event is marked stale (default 30.0).
    /// @return Vector of translated CoT events.
    [[nodiscard]] static std::vector<CotEvent> toCot(
        const RvtLocalSet& rvt,
        const std::string& parentUid = "",
        double staleSec = 30.0);

    /// @brief Formats microsecond UNIX epoch timestamp as an ISO-8601 UTC string.
    /// @details Format: YYYY-MM-DDTHH:MM:SS.sssZ.
    /// @param[in] timestampUs Microseconds since midnight Jan 1, 1970.
    /// @return ISO-8601 formatted date-time string.
    [[nodiscard]] static std::string formatIso8601(std::uint64_t timestampUs) noexcept;

    /// @brief Converts MISB CE90 horizontal error (2.146 sigma) to 1-sigma standard error.
    /// @param[in] ce90M Circular Error 90% in meters.
    /// @return 1-sigma horizontal error in meters.
    [[nodiscard]] static double ce90ToSigma1(double ce90M) noexcept;

    /// @brief Converts MISB LE90 vertical error (1.645 sigma) to 1-sigma standard error.
    /// @param[in] le90M Linear Error 90% in meters.
    /// @return 1-sigma vertical error in meters.
    [[nodiscard]] static double le90ToSigma1(double le90M) noexcept;

    /// @brief Maps an RVT target type to a standard CoT type prefix.
    /// @param[in] type Target type enumeration.
    /// @return CoT entity type string (e.g. "a-f-G", "a-h-G").
    [[nodiscard]] static std::string targetTypeToCot(RvtTargetType type) noexcept;
};

} // namespace Klv
