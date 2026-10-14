/// @file PelcoDTypes.cpp
/// @brief Implementation of Pelco-D protocol type conversions and string parsers.

#include "PelcoDTypes.h"

#include <string_view>

namespace PelcoD {

QueryKind parseQueryKind(std::string_view tag) noexcept
{
    if (tag.empty()) {
        return QueryKind::None;
    }

    constexpr std::string_view kProbePrefix { "RttProbe:" };
    if (tag.rfind(kProbePrefix, 0) == 0) {
        tag.remove_prefix(kProbePrefix.size());
    }

    if (tag == "QueryPan") {
        return QueryKind::Pan;
    }
    if (tag == "QueryTilt") {
        return QueryKind::Tilt;
    }
    if (tag == "QueryZoom") {
        return QueryKind::Zoom;
    }
    if (tag == "QueryMagnification") {
        return QueryKind::Magnification;
    }
    if (tag == "QueryDeviceType") {
        return QueryKind::DeviceType;
    }
    if (tag == "QueryDiagnostics") {
        return QueryKind::Diagnostics;
    }
    if (tag == "QuerySoftwareVersion") {
        return QueryKind::SoftwareVersion;
    }
    if (tag == "QueryBuildNumber") {
        return QueryKind::BuildNumber;
    }
    if (tag == "QueryTime") {
        return QueryKind::Time;
    }
    if (tag == "QueryAzimuthZero") {
        return QueryKind::AzimuthZero;
    }
    if (tag == "QueryZoomLimit") {
        return QueryKind::ZoomLimit;
    }
    if (tag == "QueryEverestAlarms") {
        return QueryKind::EverestAlarms;
    }
    if (tag == "QueryLimit") {
        return QueryKind::Limit;
    }
    if (tag == "QueryDefinedPresets") {
        return QueryKind::DefinedPresets;
    }
    if (tag == "QueryDefinedPatterns") {
        return QueryKind::DefinedPatterns;
    }
    if (tag == "QueryEverest") {
        return QueryKind::EverestGeneric;
    }
    if (tag == "QueryGeneral") {
        return QueryKind::General;
    }
    if (tag == "QuerySerialNumber") {
        return QueryKind::SerialNumber;
    }
    if (tag == "GenericQuery") {
        return QueryKind::AnyStandard;
    }

    return QueryKind::Custom;
}

} // namespace PelcoD
