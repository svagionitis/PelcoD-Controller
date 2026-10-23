#pragma once

/// @file PtzXmlBuilder.h
/// @brief Serializer for ONVIF PTZ Service (tptz) SOAP XML responses.

#include <string>
#include <vector>

#include "OnvifServerTypes.h"
#include "OnvifTypes.h"

namespace Onvif {

/// @class PtzXmlBuilder
/// @brief Formatter for ONVIF PTZ (tptz) XML response payloads.
class PtzXmlBuilder {
public:
    PtzXmlBuilder() = default;
    ~PtzXmlBuilder() = default;

    /// @brief Serializes PTZNode XML.
    [[nodiscard]] static std::string buildPtzNodeXml();

    /// @brief Serializes a PTZConfiguration into XML.
    [[nodiscard]] static std::string buildPtzConfigXml(
        const PTZConfiguration& cfg, const std::string& prefix, const std::string& elem);

    /// @brief Serializes GetConfigurationOptionsResponse XML.
    [[nodiscard]] static std::string buildPtzOptionsXml();

    /// @brief Serializes a PresetTour into XML.
    [[nodiscard]] static std::string buildPresetTourXml(const PresetTour& tour, const std::string& prefix);

    /// @brief Serializes PTZStatus XML.
    [[nodiscard]] static std::string buildPtzStatusXml(const PtzStatus& status);
};

} // namespace Onvif
