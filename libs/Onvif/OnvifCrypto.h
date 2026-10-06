#pragma once

/// @file OnvifCrypto.h
/// @brief Internal RAII wrappers around OpenSSL primitives used by server-side authentication.
/// @details Not part of the public Onvif API; consumed by UsernameToken, HttpDigest and
///          Authenticator translation units only.

#include <openssl/evp.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif::Crypto {

/// @brief Computes a message digest and returns it as lower-case hexadecimal.
/// @param[in] md Digest algorithm (e.g. EVP_sha256()).
/// @param[in] input Bytes to hash.
/// @return Hex digest, or std::nullopt if OpenSSL reports an error.
[[nodiscard]] std::optional<std::string> hexDigest(const EVP_MD* md, std::string_view input);

/// @brief Computes HMAC-SHA256.
/// @param[in] key Secret key bytes.
/// @param[in] data Message bytes.
/// @return 32-byte MAC, or an empty vector on error.
[[nodiscard]] std::vector<std::uint8_t> hmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data);

/// @brief Compares two byte strings in time independent of their content.
/// @details Lengths are compared first (length is not considered secret); equal-length
///          contents are compared with CRYPTO_memcmp.
/// @param[in] a First operand.
/// @param[in] b Second operand.
/// @return True if equal.
[[nodiscard]] bool constTimeEqual(std::string_view a, std::string_view b) noexcept;

/// @brief Byte-vector overload of constTimeEqual().
/// @param[in] a First operand.
/// @param[in] b Second operand.
/// @return True if equal.
[[nodiscard]] bool constTimeEqual(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) noexcept;

/// @brief Fills a buffer with cryptographically secure random bytes.
/// @param[in] count Number of bytes.
/// @return Random bytes, or an empty vector if the CSPRNG failed.
[[nodiscard]] std::vector<std::uint8_t> randomBytes(std::size_t count);

} // namespace Onvif::Crypto
