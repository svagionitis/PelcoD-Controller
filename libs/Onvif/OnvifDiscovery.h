#pragma once

/// @file OnvifDiscovery.h
/// @brief WS-Discovery multicast probe and response parser for ONVIF camera discovery.

#include "OnvifTypes.h"

#include <chrono>
#include <string>
#include <vector>

namespace Onvif {

/// @class OnvifDiscovery
/// @brief Manages WS-Discovery UDP multicast scanning and XML response parsing.
class OnvifDiscovery {
public:
    /// @brief Generates WS-Discovery SOAP Probe XML payload.
    /// @param[in] messageUuid Unique UUID for MessageID header (or empty to auto-generate).
    /// @return XML string formatted as WS-Discovery Probe envelope.
    [[nodiscard]] static std::string createProbePayload(const std::string& messageUuid = "");

    /// @brief Parses a WS-Discovery ProbeMatches SOAP XML response into DiscoveredDevice entries.
    /// @param[in] xmlResponse Raw XML payload received from camera.
    /// @param[in] senderIp Optional sender IPv4 string if known.
    /// @return List of discovered devices extracted from the response.
    [[nodiscard]] static std::vector<DiscoveredDevice> parseProbeMatches(
        const std::string& xmlResponse, const std::string& senderIp = "");

    /// @brief Broadcasts multicast probe on LAN and listens for camera responses.
    /// @param[in] timeout Total time to listen for responses (default: 2000 ms).
    /// @return Vector of unique discovered ONVIF devices.
    [[nodiscard]] static std::vector<DiscoveredDevice> discoverDevices(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));
};

} // namespace Onvif

namespace PelcoD {
namespace Onvif = ::Onvif;
} // namespace PelcoD
