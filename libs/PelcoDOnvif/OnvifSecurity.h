#pragma once

/// @file OnvifSecurity.h
/// @brief WS-Security UsernameToken authentication generation for ONVIF SOAP requests.

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @struct SecurityCredentials
/// @brief Authentication credentials for camera access.
struct SecurityCredentials {
    std::string username {}; ///< Authentication username
    std::string password {}; ///< Authentication plaintext password
    std::chrono::seconds clockOffset { 0 }; ///< Camera time offset from local clock
};

/// @struct UsernameTokenData
/// @brief Intermediate tokens used in WS-Security UsernameToken generation.
struct UsernameTokenData {
    std::string username {}; ///< Username
    std::string passwordDigest {}; ///< Base64 encoded SHA-1 password digest
    std::string nonceBase64 {}; ///< Base64 encoded random nonce (16 bytes)
    std::string createdUtc {}; ///< ISO-8601 UTC timestamp string
};

/// @class OnvifSecurity
/// @brief Utility class generating WS-Security authentication headers.
/// @details Implements WS-Security UsernameToken Profile 1.0 using SHA-1 digest:
/// PasswordDigest = Base64(SHA-1(RawNonce + CreatedUtc + Password))
class OnvifSecurity {
public:
    /// @brief Generates random 16-byte nonce.
    /// @return 16 raw random bytes.
    [[nodiscard]] static std::vector<uint8_t> generateNonce();

    /// @brief Formats current UTC timestamp in ISO-8601 format (YYYY-MM-DDTHH:MM:SSZ).
    /// @param[in] offset Optional clock offset to synchronize with camera clock.
    /// @return ISO-8601 formatted UTC timestamp string.
    [[nodiscard]] static std::string generateIsoTimestamp(std::chrono::seconds offset = std::chrono::seconds(0));

    /// @brief Computes base64-encoded SHA-1 digest from raw nonce, timestamp, and password.
    /// @param[in] rawNonce Raw nonce bytes.
    /// @param[in] createdUtc ISO-8601 timestamp string.
    /// @param[in] password Plaintext password.
    /// @return Base64-encoded SHA-1 digest.
    [[nodiscard]] static std::string computePasswordDigest(
        const std::vector<uint8_t>& rawNonce, const std::string& createdUtc, const std::string& password);

    /// @brief Encodes raw byte buffer into Base64 string.
    /// @param[in] data Raw input bytes.
    /// @return Base64 encoded string.
    [[nodiscard]] static std::string base64Encode(const std::vector<uint8_t>& data);

    /// @brief Encodes string view into Base64 string.
    /// @param[in] text Plain text input.
    /// @return Base64 encoded string.
    [[nodiscard]] static std::string base64Encode(const std::string& text);

    /// @brief Builds complete UsernameToken parameters for SOAP envelope.
    /// @param[in] credentials User credentials and clock offset.
    /// @return Generated UsernameToken data.
    [[nodiscard]] static UsernameTokenData createTokenData(const SecurityCredentials& credentials);

    /// @brief Builds the XML `<wsse:Security>` header string for SOAP request envelopes.
    /// @param[in] credentials User credentials and clock offset.
    /// @return XML formatted security header element string. Empty if username is empty.
    [[nodiscard]] static std::string buildSoapSecurityHeader(const SecurityCredentials& credentials);
};

} // namespace PelcoD::Onvif
