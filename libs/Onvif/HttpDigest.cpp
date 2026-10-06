/// @file HttpDigest.cpp
/// @brief Implementation of server-side RFC 7616 HTTP Digest authentication.

#include "HttpDigest.h"

#include "OnvifCrypto.h"
#include "OnvifSecurity.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <utility>

namespace Onvif {

namespace {

    constexpr std::size_t kKeyBytes { 32U };
    constexpr std::size_t kStampBytes { 8U };
    constexpr std::size_t kMacBytes { 32U };
    constexpr std::size_t kMaxHeader { 4096U };
    constexpr std::size_t kMaxParams { 16U };
    constexpr std::size_t kMaxCnonce { 128U };
    constexpr std::int64_t kFutureTolerance { 5 };
    constexpr const char* kDummySecret { "\x01-onvif-timing-equaliser-\x01" };

    using ParamMap = std::map<std::string, std::string>;

    [[nodiscard]] char toLowerAscii(char ch) noexcept
    {
        return ((ch >= 'A') && (ch <= 'Z')) ? static_cast<char>((ch - 'A') + 'a') : ch;
    }

    [[nodiscard]] std::string lower(std::string_view text)
    {
        std::string out {};
        out.reserve(text.size());
        for (const char ch : text) {
            out.push_back(toLowerAscii(ch));
        }
        return out;
    }

    [[nodiscard]] bool isSpace(char ch) noexcept
    {
        return (ch == ' ') || (ch == '\t');
    }

    [[nodiscard]] bool isHex(char ch) noexcept
    {
        return ((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F'));
    }

    /// @brief Parses the auth-param list of a Digest credential (RFC 7616 §3.4).
    /// @return Lower-cased parameter names mapped to unquoted values, or std::nullopt if malformed.
    [[nodiscard]] std::optional<ParamMap> parseParams(std::string_view s)
    {
        ParamMap params {};
        std::size_t pos { 0U };
        while (pos < s.size()) {
            while ((pos < s.size()) && (isSpace(s[pos]) || (s[pos] == ','))) {
                ++pos;
            }
            if (pos >= s.size()) {
                break;
            }
            const std::size_t nameStart { pos };
            while ((pos < s.size()) && (s[pos] != '=') && (s[pos] != ',') && !isSpace(s[pos])) {
                ++pos;
            }
            const std::string name { lower(s.substr(nameStart, pos - nameStart)) };
            while ((pos < s.size()) && isSpace(s[pos])) {
                ++pos;
            }
            if (name.empty() || (pos >= s.size()) || (s[pos] != '=')) {
                return std::nullopt;
            }
            ++pos;
            while ((pos < s.size()) && isSpace(s[pos])) {
                ++pos;
            }
            std::string value {};
            if ((pos < s.size()) && (s[pos] == '"')) {
                ++pos;
                bool closed { false };
                while (pos < s.size()) {
                    char ch { s[pos] };
                    if (ch == '"') {
                        closed = true;
                        ++pos;
                        break;
                    }
                    if (ch == '\\') {
                        ++pos;
                        if (pos >= s.size()) {
                            return std::nullopt;
                        }
                        ch = s[pos];
                    }
                    value.push_back(ch);
                    ++pos;
                }
                if (!closed) {
                    return std::nullopt;
                }
            } else {
                const std::size_t valueStart { pos };
                while ((pos < s.size()) && (s[pos] != ',') && !isSpace(s[pos])) {
                    ++pos;
                }
                value = std::string { s.substr(valueStart, pos - valueStart) };
            }
            if ((params.count(name) != 0U) || (params.size() >= kMaxParams)) {
                return std::nullopt;
            }
            static_cast<void>(params.emplace(name, std::move(value)));
        }
        return params;
    }

    /// @brief Returns a required, non-empty parameter or std::nullopt.
    [[nodiscard]] std::optional<std::string> required(const ParamMap& params, const char* name)
    {
        const auto it { params.find(name) };
        if ((it == params.end()) || it->second.empty()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] AuthResult outcomeOnly(AuthOutcome outcome)
    {
        return AuthResult { outcome, Principal {} };
    }

    [[nodiscard]] std::int64_t epochSeconds(std::chrono::system_clock::time_point tp)
    {
        return static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(tp - std::chrono::system_clock::from_time_t(0)).count());
    }

} // namespace

DigestValidator::DigestValidator(
    std::shared_ptr<CredentialStore> store, std::shared_ptr<NonceCache> cache, OnvifAuthConfig config)
    : m_store { std::move(store) }
    , m_cache { std::move(cache) }
    , m_config { std::move(config) }
    , m_key { Crypto::randomBytes(kKeyBytes) }
{
}

std::vector<std::uint8_t> DigestValidator::nonceMac(const std::vector<std::uint8_t>& stamp) const
{
    std::vector<std::uint8_t> message { stamp };
    for (const char ch : m_config.realm) {
        message.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(ch)));
    }
    return Crypto::hmacSha256(m_key, message);
}

std::string DigestValidator::makeNonce(std::chrono::system_clock::time_point now) const
{
    const std::int64_t secs { epochSeconds(now) };
    const std::uint64_t ts { (secs > 0) ? static_cast<std::uint64_t>(secs) : 0U };
    std::vector<std::uint8_t> stamp(kStampBytes, std::uint8_t { 0U });
    for (std::size_t i { 0U }; i < kStampBytes; ++i) {
        const std::size_t shift { (kStampBytes - 1U - i) * 8U };
        stamp.at(i) = static_cast<std::uint8_t>((ts >> shift) & 0xFFU);
    }
    std::vector<std::uint8_t> raw { stamp };
    const std::vector<std::uint8_t> mac { nonceMac(stamp) };
    raw.insert(raw.end(), mac.begin(), mac.end());
    return OnvifSecurity::base64Encode(raw);
}

DigestValidator::NonceState DigestValidator::checkNonce(
    std::string_view nonce, std::chrono::system_clock::time_point now) const
{
    if (m_key.size() != kKeyBytes) {
        return NonceState::Forged;
    }
    const std::optional<std::vector<std::uint8_t>> raw { OnvifSecurity::base64DecodeStrict(nonce) };
    if (!raw.has_value() || (raw->size() != (kStampBytes + kMacBytes))) {
        return NonceState::Forged;
    }
    const std::vector<std::uint8_t> stamp(raw->begin(), raw->begin() + static_cast<std::ptrdiff_t>(kStampBytes));
    const std::vector<std::uint8_t> mac(raw->begin() + static_cast<std::ptrdiff_t>(kStampBytes), raw->end());
    if (!Crypto::constTimeEqual(mac, nonceMac(stamp))) {
        return NonceState::Forged;
    }
    std::uint64_t ts { 0U };
    for (const std::uint8_t b : stamp) {
        ts = (ts << 8U) | static_cast<std::uint64_t>(b);
    }
    if (ts > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return NonceState::Forged;
    }
    const std::int64_t age { epochSeconds(now) - static_cast<std::int64_t>(ts) };
    if (age < -kFutureTolerance) {
        return NonceState::Forged;
    }
    if (age > static_cast<std::int64_t>(m_config.digestNonceTtl.count())) {
        return NonceState::Expired;
    }
    return NonceState::Valid;
}

std::vector<std::string> DigestValidator::makeChallenges(bool stale, std::chrono::system_clock::time_point now) const
{
    const std::string nonce { makeNonce(now) };
    const std::string staleSuffix { stale ? ", stale=true" : "" };
    std::vector<std::string> out {};
    for (const char* alg : { "SHA-256", "MD5" }) {
        out.push_back("Digest realm=\"" + m_config.realm + "\", qop=\"auth\", algorithm=" + std::string { alg }
            + ", nonce=\"" + nonce + "\"" + staleSuffix);
    }
    return out;
}

AuthResult DigestValidator::validate(std::string_view header, std::string_view method, std::string_view uri,
    std::chrono::system_clock::time_point now) const
{
    if (header.empty()) {
        return outcomeOnly(AuthOutcome::NoCredentials);
    }
    constexpr std::string_view kScheme { "digest" };
    const bool schemeMatches { (header.size() >= kScheme.size()) && (lower(header.substr(0U, kScheme.size())) == kScheme)
        && ((header.size() == kScheme.size()) || isSpace(header[kScheme.size()])) };
    if (!schemeMatches) {
        return outcomeOnly(AuthOutcome::NoCredentials);
    }
    if ((header.size() > kMaxHeader) || (m_store == nullptr) || (m_cache == nullptr)) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    const std::optional<ParamMap> params { parseParams(header.substr(kScheme.size())) };
    if (!params.has_value()) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }
    const auto username { required(*params, "username") };
    const auto realm { required(*params, "realm") };
    const auto nonce { required(*params, "nonce") };
    const auto dUri { required(*params, "uri") };
    const auto response { required(*params, "response") };
    const auto qop { required(*params, "qop") };
    const auto nc { required(*params, "nc") };
    const auto cnonce { required(*params, "cnonce") };
    if (!username || !realm || !nonce || !dUri || !response || !qop || !nc || !cnonce) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    const auto algIt { params->find("algorithm") };
    const std::string algorithm { (algIt == params->end()) ? std::string { "md5" } : lower(algIt->second) };
    const EVP_MD* md { nullptr };
    if (algorithm == "sha-256") {
        md = EVP_sha256();
    } else if (algorithm == "md5") {
        md = EVP_md5();
    } else {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    const bool ncOk { (nc->size() == 8U) && std::all_of(nc->begin(), nc->end(), [](char ch) { return isHex(ch); }) };
    if ((*realm != m_config.realm) || (lower(*qop) != "auth") || !ncOk || (cnonce->size() > kMaxCnonce)
        || (*dUri != uri)) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    const NonceState state { checkNonce(*nonce, now) };
    if (state == NonceState::Forged) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }
    if (state == NonceState::Expired) {
        return outcomeOnly(AuthOutcome::Stale);
    }

    std::optional<OnvifUser> user { m_store->find(*username) };
    std::string secret { user.has_value() ? user->password : std::string { kDummySecret } };
    if (user.has_value()) {
        OPENSSL_cleanse(user->password.data(), user->password.size());
    }
    std::string a1 { *username + ":" + *realm + ":" + secret };
    const auto ha1 { Crypto::hexDigest(md, a1) };
    OPENSSL_cleanse(a1.data(), a1.size());
    OPENSSL_cleanse(secret.data(), secret.size());
    const auto ha2 { Crypto::hexDigest(md, std::string { method } + ":" + *dUri) };
    if (!ha1 || !ha2) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }
    const auto expected { Crypto::hexDigest(md, *ha1 + ":" + *nonce + ":" + *nc + ":" + *cnonce + ":auth:" + *ha2) };
    const bool match { expected.has_value() && Crypto::constTimeEqual(*expected, lower(*response)) };
    if (!user.has_value() || !match) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    if (!m_cache->insert("dg:" + *nonce + ":" + *nc + ":" + *cnonce, std::chrono::steady_clock::now())) {
        return outcomeOnly(AuthOutcome::Replay);
    }
    return AuthResult { AuthOutcome::Success, Principal { *username, user->level } };
}

} // namespace Onvif
