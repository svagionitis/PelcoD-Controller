#pragma once

/// @file OnvifSecurity.h
/// @brief WS-Security UsernameToken authentication generation for ONVIF SOAP requests.

#include "OnvifTypes.h"

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
/// @brief Utility class generating WS-Security authentication headers and X.509 PKI assets.
/// @details Implements WS-Security UsernameToken Profile 1.0 using SHA-1 digest,
/// and OpenSSL cryptographic helpers for X.509 certificates and PKCS#10 CSRs.
class OnvifSecurity {
public:
    /// @brief Generates random 16-byte nonce.
    /// @return 16 raw random bytes.
    [[nodiscard]] static std::vector<std::uint8_t> generateNonce();

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
        const std::vector<std::uint8_t>& rawNonce, const std::string& createdUtc, const std::string& password);

    /// @brief Encodes raw byte buffer into Base64 string.
    /// @param[in] data Raw input bytes.
    /// @return Base64 encoded string.
    [[nodiscard]] static std::string base64Encode(const std::vector<std::uint8_t>& data);

    /// @brief Encodes string view into Base64 string.
    /// @param[in] text Plain text input.
    /// @return Base64 encoded string.
    [[nodiscard]] static std::string base64Encode(const std::string& text);

    /// @brief Decodes Base64 encoded string into raw bytes.
    /// @param[in] base64Text Base64 input string.
    /// @return Decoded byte vector.
    [[nodiscard]] static std::vector<std::uint8_t> base64Decode(const std::string& base64Text);

    /// @brief Builds complete UsernameToken parameters for SOAP envelope.
    /// @param[in] credentials User credentials and clock offset.
    /// @return Generated UsernameToken data.
    [[nodiscard]] static UsernameTokenData createTokenData(const SecurityCredentials& credentials);

    /// @brief Builds the XML `<wsse:Security>` header string for SOAP request envelopes.
    /// @param[in] credentials User credentials and clock offset.
    /// @return XML formatted security header element string. Empty if username is empty.
    [[nodiscard]] static std::string buildSoapSecurityHeader(const SecurityCredentials& credentials);

    /// @brief Generates a self-signed X.509 certificate and returns an OnvifCertificate record.
    /// @param[in] certificateId Certificate token identifier.
    /// @param[in] subject Distinguished name (e.g. "CN=PelcoD-Camera, O=Security").
    /// @param[in] daysValid Certificate validity duration in days.
    /// @return Complete OnvifCertificate with base64 DER data and decoded info.
    [[nodiscard]] static OnvifCertificate generateSelfSignedCertificate(
        const std::string& certificateId, const std::string& subject = "", int daysValid = 365);

    /// @brief Generates a PKCS#10 Certificate Signing Request (CSR) in base64/PEM format.
    /// @param[in] certificateId Associated certificate identifier.
    /// @param[in] subject Distinguished name.
    /// @return Pkcs10Request struct with base64 CSR payload.
    [[nodiscard]] static Pkcs10Request generatePkcs10Csr(
        const std::string& certificateId, const std::string& subject = "");

    /// @brief Parses an X.509 DER base64 encoded certificate into CertificateInformation.
    /// @param[in] certificateId Certificate ID to assign.
    /// @param[in] x509DerBase64 Base64 encoded DER data.
    /// @return Decoded CertificateInformation metadata.
    [[nodiscard]] static CertificateInformation parseCertificateInfo(
        const std::string& certificateId, const std::string& x509DerBase64);
};

} // namespace PelcoD::Onvif
