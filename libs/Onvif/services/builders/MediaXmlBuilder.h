#pragma once

/// @file MediaXmlBuilder.h
/// @brief Serializer for ONVIF Media Service (trt / tr2) SOAP XML responses.

#include <cstdint>

#include <string>
#include <vector>

#include "OnvifServerTypes.h"
#include "OnvifTypes.h"

namespace Onvif {

/// @class MediaXmlBuilder
/// @brief Formatter for ONVIF Media (trt / tr2) XML response payloads.
class MediaXmlBuilder {
public:
    MediaXmlBuilder() = default;
    ~MediaXmlBuilder() = default;

    /// @brief Serializes a ServerMediaProfile into XML.
    [[nodiscard]] static std::string buildProfileXml(
        const ServerMediaProfile& prof, const std::string& prefix, bool isSingle);

    /// @brief Serializes a VideoSourceConfig into XML.
    [[nodiscard]] static std::string buildVideoSourceXml(
        const VideoSourceConfig& cfg, const std::string& prefix, const std::string& elem);

    /// @brief Serializes a VideoEncoderConfig into XML.
    [[nodiscard]] static std::string buildVideoEncoderXml(
        const VideoEncoderConfig& cfg, const std::string& prefix, const std::string& elem);

    /// @brief Serializes GetStreamUriResponse XML.
    [[nodiscard]] static std::string buildStreamUriXml(const std::string& uri, const std::string& prefix);

    /// @brief Serializes GetSnapshotUriResponse XML.
    [[nodiscard]] static std::string buildSnapshotUriXml(const std::string& uri, const std::string& prefix);

    /// @brief Serializes an OSD configuration into XML.
    [[nodiscard]] static std::string buildOsdXml(const OsdConfig& osd, const std::string& prefix);

    /// @brief Serializes a Privacy Mask configuration into XML.
    [[nodiscard]] static std::string buildMaskXml(const PrivacyMask& mask, const std::string& prefix);

    /// @brief Serializes a MetadataConfiguration into XML.
    [[nodiscard]] static std::string buildMetaConfigXml(const MetadataConfiguration& cfg, const std::string& prefix);
};

} // namespace Onvif
