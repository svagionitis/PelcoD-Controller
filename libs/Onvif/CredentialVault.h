#pragma once

/// @file CredentialVault.h
/// @brief Authenticated AES-256-GCM encryption and persistent storage for ONVIF credentials.

#include "OnvifTypes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @class CredentialVault
/// @brief Secure persistent serialization and authenticated encryption for user secrets.
/// @details Uses AES-256-GCM with a random 12-byte IV and 16-byte authentication tag.
///          Keys are derived using PBKDF2-HMAC-SHA256 (100,000 iterations).
class CredentialVault {
public:
    /// @brief Encrypts user records into an authenticated base64 envelope.
    /// @param[in] users Plaintext user records.
    /// @param[in] key 32-byte encryption key.
    /// @return Base64-encoded encrypted envelope, or empty string on error.
    [[nodiscard]] static std::string encrypt(const std::vector<OnvifUser>& users, std::string_view key);

    /// @brief Decrypts user records from an authenticated base64 envelope.
    /// @param[in] envelope Base64-encoded encrypted envelope.
    /// @param[in] key 32-byte encryption key.
    /// @return Deserialized user records, or std::nullopt if decryption or auth tag fails.
    [[nodiscard]] static std::optional<std::vector<OnvifUser>> decrypt(std::string_view envelope, std::string_view key);

    /// @brief Saves encrypted users to a filesystem file.
    /// @param[in] path Target file path.
    /// @param[in] users Plaintext user records to encrypt.
    /// @param[in] key 32-byte encryption key.
    /// @return True on success, false on write or encryption error.
    [[nodiscard]] static bool saveToFile(
        const std::string& path, const std::vector<OnvifUser>& users, std::string_view key);

    /// @brief Loads and decrypts user records from a filesystem file.
    /// @param[in] path Source file path.
    /// @param[in] key 32-byte encryption key.
    /// @return Deserialized user records, or std::nullopt if file missing or decrypt fails.
    [[nodiscard]] static std::optional<std::vector<OnvifUser>> loadFromFile(
        const std::string& path, std::string_view key);

    /// @brief Derives a 32-byte AES key using PBKDF2-HMAC-SHA256.
    /// @param[in] secret Passphrase or master secret.
    /// @param[in] salt Salt buffer.
    /// @return 32-byte derived key, or empty string on error.
    [[nodiscard]] static std::string deriveKey(std::string_view secret, std::string_view salt);
};

} // namespace Onvif
