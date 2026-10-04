#pragma once

/// @file SightlineKlvRadiometryBridge.h
/// @brief Bridge between Sightline radiometric telemetry, tracking states, and STANAG 4609 / MISB KLV.
/// @details Translates calibrated temperature metrics (SightlineRadiometry) and target coordinates into:
///          1. MISB ST 0903 Video Moving Target Indicator (VMTI) Target Packs (Tag 9 Target Intensity)
///          2. MISB ST 0601 UAS Datalink Local Set (Tag 95 Wavelength Bands and Tag 74 nested VMTI)
///          3. Sightline Hardware KLV injection messages (0x84 MsgSetVmti, 0x96 MsgTagData, 0x15 MsgSetMetadataFrameValues)
///
/// ASCII Architecture Flow:
/// +------------------------------------+     +----------------------------------+
/// | RadiometricSpotStats (Radiometry)  |     |   TrackCoordinate (Tracking)     |
/// | (Kelvin, Celsius, Min/Max/Mean)    |     |   (Bounding Box, Centroid, Conf) |
/// +-----------------+------------------+     +-----------------+----------------+
///                   |                                          |
///                   +--------------------+---------------------+
///                                        |
///                                        v
///                    +---------------------------------------+
///                    |     SightlineKlvRadiometryBridge      |
///                    +-------------------+-------------------+
///                                        |
///         +------------------------------+-----------------------------+
///         |                                                            |
///         v                                                            v
/// +----------------------------------+              +------------------------------------+
/// | STANAG 4609 / MISB Local Sets    |              | Sightline Hardware Injection       |
/// | - MISB ST 0903 VTargetPack Tag 9 |              | - MsgSetVmti (0x84 / SLASetVMTI_t) |
/// | - MISB ST 0601 Tag 95 Band Mask  |              | - MsgTagData (0x96 / SLATagData_t) |
/// | - MISB ST 0601 Tag 74 Nested VMTI|              | - MsgSetMetadataFrameValues (0x15) |
/// +----------------------------------+              +------------------------------------+
///
/// Mermaid Architecture Flow:
/// @code{.mermaid}
/// graph TD
///     A["RadiometricSpotStats<br/>(min/max/mean temp)"] --> C["SightlineKlvRadiometryBridge"]
///     B["TrackCoordinate<br/>(pixel bbox & centroid)"] --> C
///     C --> D["MISB ST 0903 VTargetPack<br/>(Tag 9: TargetIntensity)"]
///     C --> E["MISB ST 0601 Local Set<br/>(Tag 95: WavelengthBands)"]
///     C --> F["Sightline MsgSetVmti<br/>(0x84 SLASetVMTI_t)"]
///     C --> G["Sightline MsgTagData<br/>(0x96 SLATagData_t)"]
///     C --> H["Sightline FrameValues<br/>(0x15 Frame Gate Width/Height)"]
/// @endcode

#include "SightlineKlv.h"
#include "SightlineRadiometry.h"
#include "../SightlineTypes.h"
#include "Klv/KlvTypes.h"
#include "Klv/VmtiTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @enum WavelengthBand
/// @brief Standard spectral wavelength band bitmask flags conforming to MISB ST 0601 Tag 95.
enum class WavelengthBand : std::uint8_t {
    VisibleEo = 0x01U, ///< 0.40 - 0.75 um (Electro-Optical)
    NearIr    = 0x02U, ///< 0.75 - 1.1 um (Near Infrared)
    ShortWave = 0x04U, ///< 1.1 - 3.0 um (SWIR)
    MidWave   = 0x08U, ///< 3.0 - 5.0 um (MWIR)
    LongWave  = 0x10U, ///< 8.0 - 14.0 um (LWIR / Thermal)
    VeryLong  = 0x20U, ///< 14.0 - 30.0 um (VLIR)
    FarIr     = 0x40U  ///< 30.0 - 1000.0 um (FIR)
};

/// @class SightlineKlvRadiometryBridge
/// @brief Interoperability bridge connecting Sightline radiometry telemetry with STANAG 4609 / MISB KLV.
class SightlineKlvRadiometryBridge final {
public:
    SightlineKlvRadiometryBridge() = delete;

    /// @brief Maps sensor type to standard MISB ST 0601 Tag 95 wavelength band bitmask.
    /// @param[in] sensor Radiometric infrared sensor type.
    /// @return 8-bit bitmask (e.g. 0x10 for LWIR sensors like Tau 2, Boson, Tamarisk).
    [[nodiscard]] static constexpr std::uint8_t toWavelengthMask(RadiometricSensor sensor) noexcept;

    /// @brief Returns descriptive sensor payload name string conforming to MISB ST 0601 Tag 11.
    /// @param[in] sensor Radiometric infrared sensor type.
    /// @return Sensor name string.
    [[nodiscard]] static std::string sensorModelName(RadiometricSensor sensor);

    /// @brief Builds a MISB ST 0903 VTargetPack from radiometric spot statistics and tracking state.
    /// @param[in] stats Calibrated radiometric spot temperature statistics.
    /// @param[in] track Visual tracking coordinates and bounding box.
    /// @param[in] scale Temperature scale for Tag 9 Target Intensity (default: Kelvin).
    /// @return Populated VTargetPack.
    [[nodiscard]] static Klv::VTargetPack buildTargetPack(
        const RadiometricSpotStats& stats,
        const TrackCoordinate& track,
        TemperatureScale scale = TemperatureScale::Kelvin) noexcept;

    /// @brief Builds a complete MISB ST 0903 VMTI Local Set from radiometric spot statistics and tracks.
    /// @param[in] statsList List of calibrated radiometric spot temperature statistics.
    /// @param[in] trackList List of visual tracking coordinates.
    /// @param[in] frameWidth Video frame width in pixels (e.g. 1920).
    /// @param[in] frameHeight Video frame height in pixels (e.g. 1080).
    /// @param[in] timestampUs Microseconds since UNIX epoch (0 = use system clock).
    /// @param[in] scale Temperature scale for Tag 9 Target Intensity.
    /// @return Populated VmtiLocalSet.
    [[nodiscard]] static Klv::VmtiLocalSet buildVmtiLocalSet(
        const std::vector<RadiometricSpotStats>& statsList,
        const std::vector<TrackCoordinate>& trackList,
        std::uint32_t frameWidth = 1920U,
        std::uint32_t frameHeight = 1080U,
        std::uint64_t timestampUs = 0ULL,
        TemperatureScale scale = TemperatureScale::Kelvin) noexcept;

    /// @brief Builds a MISB ST 0601 UAS Datalink Message embedding Tag 95 and Tag 74 VMTI Local Set.
    /// @param[in] statsList List of radiometric spot statistics.
    /// @param[in] trackList List of tracking coordinates.
    /// @param[in] sensor Radiometric sensor model for Tag 95 and Tag 11.
    /// @param[in] frameWidth Video frame width in pixels.
    /// @param[in] frameHeight Video frame height in pixels.
    /// @param[in] timestampUs Microseconds since UNIX epoch.
    /// @param[in] scale Temperature scale for VMTI target intensity.
    /// @return Populated UasDatalinkMessage.
    [[nodiscard]] static Klv::UasDatalinkMessage buildUasMessage(
        const std::vector<RadiometricSpotStats>& statsList,
        const std::vector<TrackCoordinate>& trackList,
        RadiometricSensor sensor,
        std::uint32_t frameWidth = 1920U,
        std::uint32_t frameHeight = 1080U,
        std::uint64_t timestampUs = 0ULL,
        TemperatureScale scale = TemperatureScale::Kelvin) noexcept;

    /// @brief Builds Sightline hardware KLV injection Message 0x84 (MsgSetVmti) from visual tracks.
    /// @param[in] tracks List of active visual tracks.
    /// @param[in] displayId Network Display ID (default: 0x0002).
    /// @return Populated MsgSetVmti.
    [[nodiscard]] static MsgSetVmti buildMsgSetVmti(
        const std::vector<TrackCoordinate>& tracks,
        std::uint16_t displayId = 0x0002U) noexcept;

    /// @brief Builds Sightline hardware KLV injection Message 0x84 (MsgSetVmti) for a single track.
    /// @param[in] track Visual track coordinate.
    /// @param[in] displayId Network Display ID (default: 0x0002).
    /// @return Populated MsgSetVmti.
    [[nodiscard]] static MsgSetVmti buildMsgSetVmti(
        const TrackCoordinate& track,
        std::uint16_t displayId = 0x0002U) noexcept;

    /// @brief Builds Sightline Message 0x96 (MsgTagData) for MISB ST 0601 Tag 95 Wavelength Bands.
    /// @param[in] sensor Radiometric sensor model.
    /// @param[in] displayId Network Display ID (default: 0x0002).
    /// @return Populated MsgTagData.
    [[nodiscard]] static MsgTagData buildWavelengthTag(
        RadiometricSensor sensor,
        std::uint16_t displayId = 0x0002U) noexcept;

    /// @brief Builds Sightline Message 0x15 (MsgSetMetadataFrameValues) track gate dimensions.
    /// @param[in] track Active visual track.
    /// @param[in] slantRangeM Slant range in meters.
    /// @param[in] displayId Network Display ID.
    /// @return Populated MsgSetMetadataFrameValues.
    [[nodiscard]] static MsgSetMetadataFrameValues buildFrameValues(
        const TrackCoordinate& track,
        double slantRangeM = 0.0,
        std::uint16_t displayId = 0x0002U) noexcept;

    /// @brief Encodes MsgSetVmti into a framed Sightline SLA binary command packet.
    /// @param[in] msg MsgSetVmti struct.
    /// @return Framed byte vector ready for transmission.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVmtiPacket(
        const MsgSetVmti& msg);

    /// @brief Encodes MsgTagData into a framed Sightline SLA binary command packet.
    /// @param[in] msg MsgTagData struct.
    /// @return Framed byte vector ready for transmission.
    [[nodiscard]] static std::vector<std::uint8_t> buildTagDataPacket(
        const MsgTagData& msg);

    /// @brief Encodes MsgSetMetadataFrameValues into a framed Sightline SLA binary command packet.
    /// @param[in] msg MsgSetMetadataFrameValues struct.
    /// @return Framed byte vector ready for transmission.
    [[nodiscard]] static std::vector<std::uint8_t> buildFrameValuesPacket(
        const MsgSetMetadataFrameValues& msg);

    /// @brief Encodes a MISB ST 0601 UAS Datalink Message into a complete binary KLV packet with Universal Label.
    /// @param[in] msg UasDatalinkMessage struct.
    /// @return Framed KLV packet with checksum.
    [[nodiscard]] static std::vector<std::uint8_t> encodeUasPacket(
        const Klv::UasDatalinkMessage& msg);
};

constexpr std::uint8_t SightlineKlvRadiometryBridge::toWavelengthMask(RadiometricSensor sensor) noexcept {
    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
    case RadiometricSensor::FlirTau2HighRes:
    case RadiometricSensor::FlirBosonHighGain:
    case RadiometricSensor::FlirBosonLowGain:
    case RadiometricSensor::DrsTamarisk:
        return static_cast<std::uint8_t>(WavelengthBand::LongWave); // 0x10U (LWIR: 8-14 um)
    case RadiometricSensor::CustomLinear:
    default:
        return static_cast<std::uint8_t>(WavelengthBand::LongWave);
    }
}

} // namespace Sightline
