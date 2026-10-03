#pragma once

/// @file SightlineKlv.h
/// @brief Sightline SLA KLV Metadata Module (STANAG 4609 / MISB ST 0601 / ST 0102 / ST 0903 / CoT).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-KLV-Metadata.pdf

#include "../SightlineTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @enum StaticMetadataType
/// @brief Static element identifier types for Message ID 0x14 (SetMetadataStaticValues).
enum class StaticMetadataType : std::uint8_t {
    MissionId = 0U,                ///< Tag 3: Mission Identifier (254 chars max)
    PlatformDesignation = 1U,      ///< Tag 10: Platform Designation (254 chars max)
    ImageSourceSensor = 2U,        ///< Tag 11: Image Source Sensor (254 chars max)
    ImageCoordinateSystem = 3U,    ///< Tag 12: Image Coordinate System
    SecurityClassification = 4U,   ///< Tag 48/1: Security Classification (1 byte)
    ClassifyingCountryMethod = 5U, ///< Tag 48/2: Classifying country coding method (1 byte)
    ClassifyingCountry = 6U,       ///< Tag 48/3: Classifying country
    SciShiInfo = 7U,               ///< Tag 48/4: SCI / SHI information
    Caveats = 8U,                  ///< Tag 48/5: Security caveats
    ReleasingInstructions = 9U,    ///< Tag 48/6: Security releasing instructions
    ObjectCountryMethod = 10U,     ///< Tag 48/2: Object country coding method (1 byte)
    ObjectCountryCode = 11U,       ///< Tag 48/13: Object country code
    MotionImageryCoreId = 12U,     ///< Tag 94: Motion Imagery Core Identifier (MISB ST-1204)
    PlatformTailNumber = 13U,      ///< Tag 4: Platform Tail Number (254 chars max)
    TargetErrorCe90 = 14U,         ///< Tag 45: Target Error Estimate CE90 (uint16)
    TargetErrorLe90 = 15U,         ///< Tag 46: Target Error Estimate LE90 (uint16)
    GenericFlagData = 16U,         ///< Tag 47: Generic Flag Data 01 (uint8)
    PlatformCallSign = 17U,        ///< Tag 59: Platform Call Sign (254 chars max)
    SensorFieldOfView = 18U,       ///< Tag 16: Sensor Field of View (horizontal & vertical)
    SensorAltitudeHae = 19U,       ///< Tag 75: Sensor true altitude HAE
    FrameCenterElevationHae = 20U, ///< Tag 25/78: Frame center elevation HAE
    TargetInfo = 21U               ///< Auto-calculate slant range and target width
};

/// @enum TagSource
/// @brief Valid source selector bitmask/IDs for Message ID 0x98 (TagSourceSelector).
enum class TagSource : std::uint16_t {
    SlaProtocol = 0x0001U,        ///< SVP (SetMetadataValues 0x13, 0x14, 0x15, TagData 0x96)
    Nmea = 0x0002U,               ///< External NMEA strings (GGA, RMC)
    VideoTrackInternal = 0x0004U, ///< VT internally generated (e.g. OLS terrain calculations)
    KlvBlob = 0x0008U,            ///< Externally generated KLV parsed for keys (passthrough)
    Vbi = 0x0010U,                ///< Vertical Blanking Interval
    Test = 0x0020U,               ///< Unit tests / 3rd party apps
    Other = 0x0040U               ///< Reserved
};

/// @enum VmtiChipFormat
/// @brief Image format of VMTI target chips for Message ID 0xAD.
enum class VmtiChipFormat : std::uint8_t {
    Jpeg = 0U,    ///< MISB standard JPEG
    Png = 1U,     ///< MISB standard PNG
    Png16 = 2U,   ///< 16-bit PNG (vendor option)
    SlRaw16 = 3U, ///< 16-bit Grayscale RAW
    SlRaw8 = 4U   ///< 8-bit Grayscale RAW
};

/// @enum VmtiChipSizeType
/// @brief Sizing algorithm for VMTI target chips (Message ID 0xAD).
enum class VmtiChipSizeType : std::uint8_t {
    FixedSize = 0U, ///< Fixed square centered on VMTI object
    Scaled = 1U,    ///< Target scaled to size hint
    Auto = 2U       ///< Auto: target bounding box size used
};

/// @struct MsgSetMetadataValues
/// @brief Dynamic platform and target telemetry for KLV insertion (Message ID 0x13).
/// @details Byte-accurate layout conforming to official Sightline SLASetMetadataValues_t (44-byte payload).
struct MsgSetMetadataValues {
    std::uint16_t validDataMask { 0x0FFFU }; ///< Valid data bitmask (bits 0..11)
    std::uint64_t utcTime { 0ULL };          ///< UTC time in microseconds since epoch (bit 0)
    std::uint16_t heading { 0U };            ///< Platform heading [0, 360) deg -> [0, 65535] (bit 1)
    std::int16_t pitch { 0 };                ///< Platform pitch [-20, 20] deg -> [-32767, 32767] (bit 2)
    std::int16_t roll { 0 };                 ///< Platform roll [-50, 50] deg -> [-32767, 32767] (bit 3)
    std::int32_t lat { 0 };                  ///< Sensor latitude [-90, 90] deg -> [-2^31, 2^31-1] (bit 4)
    std::int32_t lon { 0 };                  ///< Sensor longitude [-180, 180] deg -> [-2^31, 2^31-1] (bit 5)
    std::uint16_t alt { 0U };                ///< Sensor altitude [-900, 19000] m -> [0, 65535] (bit 6)
    std::uint16_t hfov { 0U };               ///< Horizontal FOV [0, 180] deg -> [0, 65535] (bit 7)
    std::uint16_t vfov { 0U };               ///< Vertical FOV [0, 180] deg -> [0, 65535] (bit 8)
    std::uint32_t az { 0U };                 ///< Relative azimuth [0, 360) deg -> [0, 2^32-1] (bit 9)
    std::int32_t el { 0 };                   ///< Relative elevation [-180, 180] deg -> [-2^31, 2^31-1] (bit 10)
    std::uint32_t sensorRoll { 0U };         ///< Relative sensor roll [0, 360) deg -> [0, 2^32-1] (bit 11)
    std::uint16_t displayId { 0x0002U };     ///< Network Display ID (0x0002 = Net0, 0x0080 = Net1)

    // Conversions between physical engineering units and MISB fixed-point
    [[nodiscard]] static std::int32_t degToLat(double deg) noexcept
    {
        return static_cast<std::int32_t>((deg / 90.0) * 2147483647.0);
    }
    [[nodiscard]] static double latToDeg(std::int32_t val) noexcept
    {
        return (static_cast<double>(val) * 90.0) / 2147483647.0;
    }
    [[nodiscard]] static std::int32_t degToLon(double deg) noexcept
    {
        return static_cast<std::int32_t>((deg / 180.0) * 2147483647.0);
    }
    [[nodiscard]] static double lonToDeg(std::int32_t val) noexcept
    {
        return (static_cast<double>(val) * 180.0) / 2147483647.0;
    }
    [[nodiscard]] static std::uint16_t degToHeading(double deg) noexcept
    {
        return static_cast<std::uint16_t>((deg / 360.0) * 65535.0);
    }
    [[nodiscard]] static double headingToDeg(std::uint16_t val) noexcept
    {
        return (static_cast<double>(val) * 360.0) / 65535.0;
    }
    [[nodiscard]] static std::int16_t degToPitch(double deg) noexcept
    {
        return static_cast<std::int16_t>((deg / 20.0) * 32767.0);
    }
    [[nodiscard]] static double pitchToDeg(std::int16_t val) noexcept
    {
        return (static_cast<double>(val) * 20.0) / 32767.0;
    }
    [[nodiscard]] static std::int16_t degToRoll(double deg) noexcept
    {
        return static_cast<std::int16_t>((deg / 50.0) * 32767.0);
    }
    [[nodiscard]] static double rollToDeg(std::int16_t val) noexcept
    {
        return (static_cast<double>(val) * 50.0) / 32767.0;
    }
    [[nodiscard]] static std::uint16_t altToMisb(double meters) noexcept
    {
        const double clamped = std::clamp(meters, -900.0, 19000.0);
        return static_cast<std::uint16_t>(((clamped + 900.0) / 19900.0) * 65535.0);
    }
    [[nodiscard]] static double misbToAlt(std::uint16_t val) noexcept
    {
        return ((static_cast<double>(val) * 19900.0) / 65535.0) - 900.0;
    }
    [[nodiscard]] static std::uint16_t degToFov(double deg) noexcept
    {
        return static_cast<std::uint16_t>((deg / 180.0) * 65535.0);
    }
    [[nodiscard]] static double fovToDeg(std::uint16_t val) noexcept
    {
        return (static_cast<double>(val) * 180.0) / 65535.0;
    }
    [[nodiscard]] static std::uint32_t degToAzimuth(double deg) noexcept
    {
        return static_cast<std::uint32_t>((deg / 360.0) * 4294967295.0);
    }
    [[nodiscard]] static double azimuthToDeg(std::uint32_t val) noexcept
    {
        return (static_cast<double>(val) * 360.0) / 4294967295.0;
    }
    [[nodiscard]] static std::int32_t degToElevation(double deg) noexcept
    {
        return static_cast<std::int32_t>((deg / 180.0) * 2147483647.0);
    }
    [[nodiscard]] static double elevationToDeg(std::int32_t val) noexcept
    {
        return (static_cast<double>(val) * 180.0) / 2147483647.0;
    }
};

/// @struct MsgMetadataStaticValues
/// @brief Static mission identifiers and classification markings (Message ID 0x14).
/// @details Byte-accurate layout conforming to official Sightline SLAMetadataStaticValues_t.
struct MsgMetadataStaticValues {
    StaticMetadataType type { StaticMetadataType::MissionId }; ///< Static element identifier
    std::vector<std::uint8_t> value {};                        ///< Binary or string payload (len <= 254)
    std::uint16_t displayId { 0x0002U };                       ///< Network Display ID

    [[nodiscard]] std::string asString() const
    {
        std::string s {};
        s.reserve(value.size());
        for (const auto b : value) {
            s.push_back(static_cast<char>(b));
        }
        return s;
    }
    void setString(const std::string& str)
    {
        value.clear();
        value.reserve(str.size());
        for (const auto c : str) {
            value.push_back(static_cast<std::uint8_t>(c));
        }
    }
};

/// @struct MsgSetMetadataFrameValues
/// @brief Sets new KLV metadata frame data values and OLS mode (Message ID 0x15).
/// @details Byte-accurate layout conforming to official Sightline SLASetMetadataFrameValues_t (49 bytes).
struct MsgSetMetadataFrameValues {
    std::uint16_t validDataMask { 0x001FU };    ///< Valid data bitmask (bits 0..4: center lat/lon/el, width, slant range)
    std::int32_t frameCenterLat { 0 };          ///< Frame center latitude (MISB format)
    std::int32_t frameCenterLon { 0 };          ///< Frame center longitude (MISB format)
    std::uint16_t frameCenterEl { 0U };         ///< Frame center elevation MSL (MISB format)
    std::uint16_t frameWidth { 0U };            ///< Target width (meters)
    std::uint32_t slantRange { 0U };            ///< Slant range (meters)
    std::uint8_t userSuppliedFlags { 0U };       ///< Bit 0: Center Lat/Lon; Bit 1-2: Target mode; Bit 3: Corners; Bit 4: Center Elev; Bit 5: OLS!
    std::int32_t targetLat { 0 };               ///< Target latitude / row
    std::int32_t targetLon { 0 };               ///< Target longitude / column
    std::uint16_t targetEl { 0U };              ///< Target elevation
    std::uint8_t targetTrackGateHeight { 0U };  ///< Track gate height (pixels)
    std::uint8_t targetTrackGateWidth { 0U };   ///< Track gate width (pixels)
    std::int16_t offsetCornerLat1 { 0 };        ///< Offset corner 1 latitude
    std::int16_t offsetCornerLon1 { 0 };        ///< Offset corner 1 longitude
    std::int16_t offsetCornerLat2 { 0 };        ///< Offset corner 2 latitude
    std::int16_t offsetCornerLon2 { 0 };        ///< Offset corner 2 longitude
    std::int16_t offsetCornerLat3 { 0 };        ///< Offset corner 3 latitude
    std::int16_t offsetCornerLon3 { 0 };        ///< Offset corner 3 longitude
    std::int16_t offsetCornerLat4 { 0 };        ///< Offset corner 4 latitude
    std::int16_t offsetCornerLon4 { 0 };        ///< Offset corner 4 longitude
    std::uint16_t displayId { 0x0002U };        ///< Network Display ID
};

/// @struct MsgSetKlvData
/// @brief User constructed KLV blob injection (Message ID 0x61).
/// @details Conforming to official Sightline SLASetKlvData_t.
struct MsgSetKlvData {
    std::uint16_t displayId { 0x0002U };
    std::vector<std::uint8_t> klvData {};
};

/// @struct MsgSetMetadataRate
/// @brief Telemetry output rate and MISB packet frequency (Message ID 0x62).
/// @details Conforming to official Sightline SLASetMetadataRate_t (11 bytes).
struct MsgSetMetadataRate {
    std::uint64_t enables { 0ULL };
    std::uint8_t frameStep { 1U };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgCurrentMetadataValues
/// @brief Current platform and sensor telemetry reply (Message ID 0x8B).
/// @details Conforming to official Sightline SLACurrentMetadataValues_t (42 bytes).
struct MsgCurrentMetadataValues {
    std::uint64_t utcTime { 0ULL };
    std::uint16_t heading { 0U };
    std::int16_t pitch { 0 };
    std::int16_t roll { 0 };
    std::int32_t lat { 0 };
    std::int32_t lon { 0 };
    std::uint16_t alt { 0U };
    std::uint16_t hfov { 0U };
    std::uint16_t vfov { 0U };
    std::uint32_t az { 0U };
    std::int32_t el { 0 };
    std::uint32_t sensorRoll { 0U };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgCurrentMetadataFrameValues
/// @brief Current frame metadata values reply (Message ID 0x8C).
/// @details Conforming to official Sightline SLACurrentMetadataFrameValues_t (47 bytes).
struct MsgCurrentMetadataFrameValues {
    std::int32_t frameCenterLat { 0 };
    std::int32_t frameCenterLon { 0 };
    std::uint16_t frameCenterEl { 0U };
    std::uint16_t frameWidth { 0U };
    std::uint32_t slantRange { 0U };
    std::uint8_t userSuppliedFlags { 0U };
    std::int32_t targetLat { 0 };
    std::int32_t targetLon { 0 };
    std::uint16_t targetEl { 0U };
    std::uint8_t targetTrackGateHeight { 0U };
    std::uint8_t targetTrackGateWidth { 0U };
    std::int16_t offsetCornerLat1 { 0 };
    std::int16_t offsetCornerLon1 { 0 };
    std::int16_t offsetCornerLat2 { 0 };
    std::int16_t offsetCornerLon2 { 0 };
    std::int16_t offsetCornerLat3 { 0 };
    std::int16_t offsetCornerLon3 { 0 };
    std::int16_t offsetCornerLat4 { 0 };
    std::int16_t offsetCornerLon4 { 0 };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgCurrentMetadataRate
/// @brief Current metadata rate query reply (Message ID 0x8D).
/// @details Conforming to official Sightline SLACurrentMetadataRate_t (4 bytes).
struct MsgCurrentMetadataRate {
    std::uint8_t index { 0U };
    std::uint8_t frameStep { 0U };
    std::uint16_t displayId { 0x0002U };
};

/// @struct VmtiTargetPack
/// @brief Single external target detection entry for Message ID 0x84 (SLASetVMTI_t).
struct VmtiTargetPack {
    std::uint8_t targetId { 1U };                ///< Transmitted as 128..255 (MISB 74.101.OEB)
    std::uint8_t confidence { 100U };            ///< 0-100 (MISB 74.101.5)
    std::uint16_t col { 0U };                    ///< Centroid column (pixels)
    std::uint16_t row { 0U };                    ///< Centroid row (pixels)
    std::uint16_t width { 0U };                  ///< Target width (pixels)
    std::uint16_t height { 0U };                 ///< Target height (pixels)
    std::uint16_t newTargetDetectionFlag { 1U }; ///< Detection counter (MISB 74.101.6)
};

/// @struct MsgSetVmti
/// @brief Injects external target detections into KLV Tag 74 (Message ID 0x84).
/// @details Conforming to official Sightline SLASetVMTI_t.
struct MsgSetVmti {
    std::vector<VmtiTargetPack> targets {};
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgAppendedMetadata
/// @brief Appends user-specified binary metadata (Tag 100) (Message ID 0x89).
/// @details Conforming to official Sightline SLAAppendedMetadata_t.
struct MsgAppendedMetadata {
    std::vector<std::uint8_t> data {};
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgTagData
/// @brief Custom or extended MISB Tag and SubTag value (Message ID 0x96).
/// @details Conforming to official Sightline SLATagData_t.
struct MsgTagData {
    std::uint8_t reserved1 { 0U };
    std::uint8_t reserved2 { 0U };
    std::uint8_t tagId { 0U };
    std::uint8_t tagSubId { 0U };
    std::uint16_t reservedInternal { 0U };
    std::uint16_t displayId { 0x0002U };
    std::vector<std::uint8_t> data {};
};

/// @struct MsgTagDataRate
/// @brief Frequency / frame step control for KLV tags (Message ID 0x97).
/// @details Conforming to official Sightline SLATagDataRate_t.
struct MsgTagDataRate {
    std::uint8_t reserved1 { 0U };
    std::uint8_t reserved2 { 0U };
    std::uint8_t mode { 0U }; ///< 0: Single tag, 1: Range of tags
    std::uint8_t tagId1 { 0U };
    std::uint8_t tagId2 { 0U };
    std::uint16_t frameStep { 1U };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgTagSourceSelector
/// @brief Selects source for a KLV tag or range of tags (Message ID 0x98).
/// @details Conforming to official Sightline SLATagSourceSelector_t.
struct MsgTagSourceSelector {
    std::uint8_t reserved1 { 0U };
    std::uint8_t reserved2 { 0U };
    std::uint8_t mode { 0U }; ///< 0: Single tag, 1: Range of tags
    std::uint8_t tagId1 { 0U };
    std::uint8_t tagId2 { 0U };
    std::uint16_t selector { static_cast<std::uint16_t>(TagSource::SlaProtocol) };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgAncillaryTextMetadata
/// @brief Injects free text into KLV elementary stream (MISB ST 0808, Message ID 0xAC).
/// @details Conforming to official Sightline SLAAncillaryTextMetadata_t.
struct MsgAncillaryTextMetadata {
    std::uint64_t creationTime { 0ULL };
    std::string source { "operator" };
    std::string originator {};
    std::string messageBody {};
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgVmtiChips
/// @brief Configures KLV VMTI target image chips (Message ID 0xAD).
/// @details Conforming to official Sightline SLAVMTIChips_t.
struct MsgVmtiChips {
    std::uint8_t mode { 1U }; ///< bit 0: enable chips
    VmtiChipFormat format { VmtiChipFormat::Jpeg };
    VmtiChipSizeType sizeType { VmtiChipSizeType::FixedSize };
    std::uint16_t sizeHint { 32U };
    std::uint8_t maxPerFrame { 1U };
    std::uint8_t minFramesBetween { 1U };
    std::uint8_t reserved0 { 0U };
    std::uint16_t displayId { 0x0002U };
};

/// @struct MsgCursorOnTarget
/// @brief Configures Cursor-on-Target (CoT) XML broadcast parameters (Message ID 0xB0).
/// @details Conforming to official Sightline SLACursorOnTarget_t.
struct MsgCursorOnTarget {
    std::uint16_t mode { 0U };
    std::uint32_t ipAddress { 0xEFE00A0AU }; ///< Destination IPv4 (e.g. 239.224.10.10)
    std::uint16_t port { 1870U };            ///< Destination port
    std::uint16_t rate { 1U };               ///< Frame step rate (0 = disabled)
    std::uint16_t displayId { 0x0002U };     ///< Network Display ID
};

/// @struct MsgVmtiFields
/// @brief Configures active VMTI fields and Ontology Series update rate (Message ID 0xBF).
/// @details Conforming to official Sightline SLAVMTIFields_t.
struct MsgVmtiFields {
    std::uint16_t displayId { 0x0002U };
    std::uint16_t fields { 0x0007U };         ///< Bit 0: Offset Lat/Lon, Bit 1: Full Lat/Lon, Bit 2: Classification
    std::uint16_t ontologySeriesRate { 10U }; ///< Seconds
};

} // namespace Sightline
