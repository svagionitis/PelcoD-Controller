#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Nmea::Lwe {

/// @brief Standard IEC 61162-450 Transmission Groups (Channels).
/// @details Mapped to IPv4 multicast addresses in the 239.192.0.0/24 subnet.
enum class TransmissionGroup : std::uint8_t {
    Misc = 1U, ///< 239.192.0.1:60001 (Miscellaneous / Alarms)
    Tgtd = 2U, ///< 239.192.0.2:60002 (Target Data: ARPA TTM, RSD, AIS VDM/VDO)
    Satd = 3U, ///< 239.192.0.3:60003 (Satellite Nav: GGA, RMC, GLL, GNS)
    Navd = 4U, ///< 239.192.0.4:60004 (Navigation Data: HDT, HDG, ROT, MWV)
    Vdrd = 5U, ///< 239.192.0.5:60005 (Voyage Data Recorder)
    Rcom = 6U, ///< 239.192.0.6:60006 (Radio Communication)
    Time = 7U, ///< 239.192.0.7:60007 (Time Synchronization)
    Prop = 8U, ///< 239.192.0.8:60008 (Propulsion & Steering)
    Usrd1 = 9U, ///< 239.192.0.9:60009 (User Defined 1)
    Usrd2 = 10U,
    Usrd3 = 11U,
    Usrd4 = 12U,
    Usrd5 = 13U,
    Usrd6 = 14U,
    Usrd7 = 15U,
    Usrd8 = 16U ///< 239.192.0.16:60016 (User Defined 8)
};

/// @brief Maximum size of an unfragmented IEC 61162-450 UDP datagram (MTU 1500 - 20 IP - 8 UDP).
inline constexpr std::size_t kLweMaxDatagramSize = 1472U;

/// @brief Base port number for IEC 61162-450 transmission groups (60000 + TG).
inline constexpr std::uint16_t kLweBasePort = 60000U;

/// @brief Returns the standard multicast IPv4 address for an IEC 61162-450 transmission group.
/// @param[in] tg Transmission group.
/// @return Multicast IPv4 address string (e.g., "239.192.0.2").
[[nodiscard]] inline std::string getTransmissionGroupIp(TransmissionGroup tg)
{
    const auto idx = static_cast<unsigned int>(tg);
    return "239.192.0." + std::to_string(idx);
}

/// @brief Returns the standard UDP port for an IEC 61162-450 transmission group.
/// @param[in] tg Transmission group.
/// @return UDP port (e.g., 60002 for TGTD).
[[nodiscard]] inline std::uint16_t getTransmissionGroupPort(TransmissionGroup tg) noexcept
{
    return static_cast<std::uint16_t>(kLweBasePort + static_cast<std::uint16_t>(tg));
}

/// @brief Returns a short human-readable name for an IEC 61162-450 transmission group.
/// @param[in] tg Transmission group.
/// @return Name string view (e.g. "TGTD").
[[nodiscard]] inline std::string_view getTransmissionGroupName(TransmissionGroup tg) noexcept
{
    switch (tg) {
    case TransmissionGroup::Misc:
        return "MISC";
    case TransmissionGroup::Tgtd:
        return "TGTD";
    case TransmissionGroup::Satd:
        return "SATD";
    case TransmissionGroup::Navd:
        return "NAVD";
    case TransmissionGroup::Vdrd:
        return "VDRD";
    case TransmissionGroup::Rcom:
        return "RCOM";
    case TransmissionGroup::Time:
        return "TIME";
    case TransmissionGroup::Prop:
        return "PROP";
    case TransmissionGroup::Usrd1:
        return "USRD1";
    case TransmissionGroup::Usrd2:
        return "USRD2";
    case TransmissionGroup::Usrd3:
        return "USRD3";
    case TransmissionGroup::Usrd4:
        return "USRD4";
    case TransmissionGroup::Usrd5:
        return "USRD5";
    case TransmissionGroup::Usrd6:
        return "USRD6";
    case TransmissionGroup::Usrd7:
        return "USRD7";
    case TransmissionGroup::Usrd8:
        return "USRD8";
    default:
        return "UNKNOWN";
    }
}

} // namespace Nmea::Lwe
