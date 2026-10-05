#pragma once

/// @file SightlineDigestAuth.h
/// @brief RFC 2069 Digest Authentication and .htpasswd processing for Sightline RTSP.
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-RTSP.pdf (Section 6)
/// @see https://datatracker.ietf.org/doc/html/rfc2069

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Sightline {

/// @struct HtpasswdEntry
/// @brief Parsed entry from Sightline .htpasswd authentication file.
/// @details Format: <username>:<realm>:<MD5(username:realm:password)>
struct HtpasswdEntry {
    std::string username {}; ///< User identifier
    std::string realm {}; ///< Protection domain realm (e.g. sla_rtspserver)
    std::string ha1 {}; ///< Precomputed MD5 hash in lowercase hex
};

/// @struct DigestChallenge
/// @brief Parsed fields from RTSP 401 WWW-Authenticate: Digest challenge header.
struct DigestChallenge {
    std::string realm {}; ///< Required protection realm
    std::string nonce {}; ///< Required server-generated unique nonce
    std::string opaque {}; ///< Optional opaque server data string
    std::string algorithm { "MD5" }; ///< Hash algorithm (default MD5)
};

/// @struct DigestResponseParams
/// @brief Parameters required to generate an RFC 2069 Authorization: Digest header.
struct DigestResponseParams {
    std::string username {}; ///< Client username
    std::string realm {}; ///< Realm from server challenge
    std::string nonce {}; ///< Nonce from server challenge
    std::string uri {}; ///< Target RTSP URI (e.g. rtsp://192.168.1.15:554/net0)
    std::string response {}; ///< Calculated digest response hash
    std::string opaque {}; ///< Opaque token if provided in challenge
};

/// @class SightlineDigestAuth
/// @brief Cryptographic and protocol utilities for RFC 2069 Digest Authentication.
class SightlineDigestAuth {
public:
    /// @brief Computes raw MD5 hash in lowercase hexadecimal.
    /// @param[in] data Input byte sequence or text.
    /// @return 32-character lowercase hex string.
    [[nodiscard]] static std::string computeMd5Hex(std::string_view data);

    /// @brief Computes HA1 user digest hash: MD5(username:realm:password).
    /// @param[in] user Username identifier.
    /// @param[in] realm Authentication realm (e.g. "sla_rtspserver").
    /// @param[in] pass Plaintext user password.
    /// @return 32-character lowercase hex digest.
    [[nodiscard]] static std::string computeHa1(std::string_view user, std::string_view realm, std::string_view pass);

    /// @brief Computes HA2 request digest hash: MD5(method:digestURI).
    /// @param[in] method RTSP method string (e.g. "DESCRIBE", "SETUP", "PLAY").
    /// @param[in] uri Target RTSP request URI.
    /// @return 32-character lowercase hex digest.
    [[nodiscard]] static std::string computeHa2(std::string_view method, std::string_view uri);

    /// @brief Computes RFC 2069 final response hash: MD5(HA1:nonce:HA2).
    /// @param[in] ha1 Precomputed HA1 hex digest.
    /// @param[in] nonce Server challenge nonce string.
    /// @param[in] ha2 Precomputed HA2 hex digest.
    /// @return 32-character lowercase hex response digest.
    [[nodiscard]] static std::string computeResponse(
        std::string_view ha1, std::string_view nonce, std::string_view ha2);

    /// @brief Formats a single line for a Sightline .htpasswd file.
    /// @param[in] user Username.
    /// @param[in] realm Realm string (default "sla_rtspserver").
    /// @param[in] pass Plaintext password.
    /// @return Formatted line "<username>:<realm>:<HA1>".
    [[nodiscard]] static std::string formatHtpasswdLine(
        std::string_view user, std::string_view realm, std::string_view pass);

    /// @brief Parses an entire .htpasswd file into a list of user records.
    /// @param[in] content Content of the .htpasswd file.
    /// @return Vector of valid parsed entries.
    [[nodiscard]] static std::vector<HtpasswdEntry> parseHtpasswd(std::string_view content);

    /// @brief Finds a user entry by username within parsed .htpasswd entries.
    /// @param[in] entries List of parsed htpasswd entries.
    /// @param[in] user Target username to find.
    /// @return Optional matching HtpasswdEntry.
    [[nodiscard]] static std::optional<HtpasswdEntry> findHtpasswdUser(
        const std::vector<HtpasswdEntry>& entries, std::string_view user);

    /// @brief Parses a WWW-Authenticate header string into structured challenge parameters.
    /// @param[in] header Header value string (e.g. 'Digest realm="sla", nonce="..."').
    /// @return Parsed DigestChallenge structure.
    [[nodiscard]] static DigestChallenge parseDigestChallenge(std::string_view header);

    /// @brief Formats an RFC 2069 Authorization header value string.
    /// @param[in] params Digest response parameters.
    /// @return Formatted header string: 'Digest username="...", realm="...", ...'.
    [[nodiscard]] static std::string formatDigestAuthHeader(const DigestResponseParams& params);

    /// @brief Redacts plaintext passwords from RTSP URIs for logging and telemetry safety.
    /// @details Turns "rtsp://user:pass@host:port/path" into "rtsp://user:***@host:port/path".
    /// @param[in] uri Raw input URI string.
    /// @return Sanitized URI string with masked password.
    [[nodiscard]] static std::string sanitizeRtspUri(std::string_view uri);
};

} // namespace Sightline
