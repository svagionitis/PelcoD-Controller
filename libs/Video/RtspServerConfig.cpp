#include "RtspServerConfig.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <sstream>
#include <string>

namespace Video {

namespace {

    [[nodiscard]] bool isValidStreamPath(const std::string& path) noexcept
    {
        if (path.empty()) {
            return false;
        }

        const std::string invalidChars { " \t\r\n?#&" };
        return path.find_first_of(invalidChars) == std::string::npos;
    }

    [[nodiscard]] bool isValidMulticastAddress(const std::string& ip) noexcept
    {
        std::istringstream iss(ip);
        std::string token {};
        std::size_t octetCount { 0U };
        std::uint32_t firstOctet { 0U };

        while (std::getline(iss, token, '.')) {
            if (token.empty() || (token.size() > 3U)) {
                return false;
            }

            for (const char c : token) {
                if ((c < '0') || (c > '9')) {
                    return false;
                }
            }

            const auto val { static_cast<std::uint32_t>(std::stoul(token)) };
            if (val > 255U) {
                return false;
            }

            if (octetCount == 0U) {
                firstOctet = val;
            }

            ++octetCount;
        }

        // Must have exactly 4 octets and first octet in range [224, 239]
        return (octetCount == 4U) && (firstOctet >= 224U) && (firstOctet <= 239U);
    }

} // namespace

bool RtspServerConfig::validate() const noexcept
{
    if (!isValidStreamPath(streamPath)) {
        return false;
    }

    if ((maxClients < 1U) || (maxClients > 256U)) {
        return false;
    }

    if ((queueCapacity < 2U) || (queueCapacity > 1000U)) {
        return false;
    }

    if (username.has_value() != password.has_value()) {
        return false;
    }

    if (username.has_value() && (username->empty() || realm.empty())) {
        return false;
    }

    if (enableMulticast) {
        if (multicastPort == 0U) {
            return false;
        }
        if (multicastTtl == 0U) {
            return false;
        }
        if (!isValidMulticastAddress(multicastAddress)) {
            return false;
        }
    }

    return true;
}

} // namespace Video
