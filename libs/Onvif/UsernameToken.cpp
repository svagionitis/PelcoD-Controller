/// @file UsernameToken.cpp
/// @brief Implementation of server-side WS-Security UsernameToken validation.

#include "UsernameToken.h"

#include "OnvifCrypto.h"
#include "OnvifSecurity.h"

#include <openssl/crypto.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Onvif {

namespace {

    constexpr std::string_view kDigestSuffix { "#PasswordDigest" };
    constexpr std::string_view kTextSuffix { "#PasswordText" };
    constexpr std::size_t kMaxNonceBytes { 128U };
    constexpr const char* kDummySecret { "\x01-onvif-timing-equaliser-\x01" };

    /// @brief Wipes a secret-bearing string when leaving scope.
    class SecretWiper {
    public:
        explicit SecretWiper(std::string& secret) noexcept
            : m_secret { secret }
        {
        }
        ~SecretWiper()
        {
            if (!m_secret.empty()) {
                OPENSSL_cleanse(m_secret.data(), m_secret.size());
            }
        }
        SecretWiper(const SecretWiper&) = delete;
        SecretWiper& operator=(const SecretWiper&) = delete;
        SecretWiper(SecretWiper&&) = delete;
        SecretWiper& operator=(SecretWiper&&) = delete;

    private:
        std::string& m_secret;
    };

    /// @brief Returns the element name without its namespace prefix.
    [[nodiscard]] std::string_view localName(const pugi::xml_node& node) noexcept
    {
        const std::string_view name { node.name() };
        const std::size_t colon { name.find(':') };
        return (colon == std::string_view::npos) ? name : name.substr(colon + 1U);
    }

    /// @brief Finds the first element child with the given local name.
    [[nodiscard]] pugi::xml_node childByLocal(const pugi::xml_node& parent, std::string_view local)
    {
        for (pugi::xml_node child { parent.first_child() }; child; child = child.next_sibling()) {
            if ((child.type() == pugi::node_element) && (localName(child) == local)) {
                return child;
            }
        }
        return pugi::xml_node {};
    }

    /// @brief Returns true if @p text ends with @p suffix.
    [[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix) noexcept
    {
        return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
    }

    /// @brief Strips leading/trailing XML whitespace.
    [[nodiscard]] std::string trim(std::string_view text)
    {
        constexpr std::string_view kWs { " \t\r\n" };
        const std::size_t first { text.find_first_not_of(kWs) };
        if (first == std::string_view::npos) {
            return std::string {};
        }
        const std::size_t last { text.find_last_not_of(kWs) };
        return std::string { text.substr(first, (last - first) + 1U) };
    }

    [[nodiscard]] AuthResult outcomeOnly(AuthOutcome outcome)
    {
        return AuthResult { outcome, Principal {} };
    }

} // namespace

UsernameTokenValidator::UsernameTokenValidator(
    std::shared_ptr<CredentialStore> store, std::shared_ptr<NonceCache> cache, OnvifAuthConfig config)
    : m_store { std::move(store) }
    , m_cache { std::move(cache) }
    , m_config { std::move(config) }
{
}

AuthResult UsernameTokenValidator::validate(
    pugi::xml_node security, std::chrono::system_clock::time_point now) const
{
    if (!security || (m_store == nullptr) || (m_cache == nullptr)) {
        return outcomeOnly(AuthOutcome::NoCredentials);
    }
    const pugi::xml_node token { childByLocal(security, "UsernameToken") };
    if (!token) {
        return outcomeOnly(AuthOutcome::NoCredentials);
    }

    const std::string username { trim(childByLocal(token, "Username").text().as_string()) };
    const pugi::xml_node passwordNode { childByLocal(token, "Password") };
    if (username.empty() || !passwordNode) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }
    const std::string_view type { passwordNode.attribute("Type").as_string() };
    std::string presented { trim(passwordNode.text().as_string()) };
    const SecretWiper presentedWiper { presented };

    const bool isText { type.empty() || endsWith(type, kTextSuffix) };
    const bool isDigest { endsWith(type, kDigestSuffix) };
    if ((isText && !m_config.allowPasswordText) || (!isText && !isDigest)) {
        return outcomeOnly(AuthOutcome::Unsupported);
    }

    const std::optional<std::vector<std::uint8_t>> nonce { OnvifSecurity::base64DecodeStrict(
        trim(childByLocal(token, "Nonce").text().as_string())) };
    const std::string createdText { trim(childByLocal(token, "Created").text().as_string()) };
    const std::optional<std::chrono::system_clock::time_point> created { OnvifSecurity::parseIsoUtc(createdText) };
    if (!nonce.has_value() || nonce->empty() || (nonce->size() > kMaxNonceBytes) || !created.has_value()) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    const auto skew { now - *created };
    if ((skew > m_config.maxClockSkew) || (skew < -m_config.maxClockSkew)) {
        return outcomeOnly(AuthOutcome::Stale);
    }

    std::optional<OnvifUser> user { m_store->find(username) };
    std::string secret { user.has_value() ? user->password : std::string { kDummySecret } };
    const SecretWiper secretWiper { secret };
    if (user.has_value()) {
        OPENSSL_cleanse(user->password.data(), user->password.size());
    }

    const std::string expected { isDigest ? OnvifSecurity::computePasswordDigest(*nonce, createdText, secret)
                                          : secret };
    const bool match { Crypto::constTimeEqual(expected, presented) };
    if (!user.has_value() || !match || expected.empty()) {
        return outcomeOnly(AuthOutcome::InvalidCredentials);
    }

    if (!m_cache->insert("ut:" + OnvifSecurity::base64Encode(*nonce), std::chrono::steady_clock::now())) {
        return outcomeOnly(AuthOutcome::Replay);
    }
    return AuthResult { AuthOutcome::Success, Principal { username, user->level } };
}

pugi::xml_node findSecurityHeader(const pugi::xml_document& doc)
{
    const pugi::xml_node envelope { doc.document_element() };
    if (!envelope || (localName(envelope) != "Envelope")) {
        return pugi::xml_node {};
    }
    const pugi::xml_node header { childByLocal(envelope, "Header") };
    if (!header) {
        return pugi::xml_node {};
    }
    return childByLocal(header, "Security");
}

} // namespace Onvif
