#pragma once

/// @file OnvifTestClient.h
/// @brief Test-only HTTP client that injects WS-Security UsernameToken headers into SOAP requests.
/// @details Mirrors the subset of the httplib::Client API used by the ONVIF test-suite
///          (Post, Get, set_connection_timeout, set_read_timeout) so that legacy tests migrate
///          mechanically. When credentials are supplied, every POSTed SOAP envelope receives a
///          freshly generated `<wsse:Security>` header (new nonce + timestamp per request).

#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifSecurity.h>

#include <openssl/evp.h>
#include <pugixml.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace OnvifTest {

/// @brief Default administrator credentials used by the ONVIF integration tests.
/// @return Credentials matching OnvifServerConfig::defaultUsers' administrator entry.
[[nodiscard]] inline Onvif::SecurityCredentials defaultAdmin()
{
    Onvif::SecurityCredentials creds {};
    creds.username = "admin";
    creds.password = "admin";
    return creds;
}

/// @brief Builds credentials from a username/password pair.
/// @param[in] username Login name.
/// @param[in] password Plaintext password.
/// @return Populated SecurityCredentials.
[[nodiscard]] inline Onvif::SecurityCredentials makeCreds(std::string username, std::string password)
{
    Onvif::SecurityCredentials creds {};
    creds.username = std::move(username);
    creds.password = std::move(password);
    return creds;
}

/// @brief Inserts a WS-Security header into a SOAP envelope.
/// @details Locates the Envelope root by local name, reuses an existing Header or creates one as
///          the first child (using the Envelope's namespace prefix) and appends the security block.
/// @param[in] envelope Serialized SOAP envelope.
/// @param[in] securityXml Serialized `<wsse:Security>` element.
/// @return Envelope with the header injected, or the original envelope if it cannot be parsed.
[[nodiscard]] inline std::string injectSecurity(const std::string& envelope, const std::string& securityXml)
{
    if (securityXml.empty()) {
        return envelope;
    }
    pugi::xml_document doc {};
    if (!doc.load_string(envelope.c_str())) {
        return envelope;
    }
    pugi::xml_node root = doc.document_element();
    const std::string rootName = root.name();
    const std::size_t colon = rootName.find(':');
    const std::string prefix = (colon == std::string::npos) ? std::string {} : rootName.substr(0, colon + 1U);

    pugi::xml_node header = root.select_node("*[local-name()='Header']").node();
    if (!header) {
        header = root.prepend_child((prefix + "Header").c_str());
    }

    pugi::xml_document secDoc {};
    if (!secDoc.load_string(securityXml.c_str())) {
        return envelope;
    }
    header.append_copy(secDoc.document_element());

    std::ostringstream out {};
    doc.save(out, "", pugi::format_raw);
    return out.str();
}

/// @brief Lower-case hex SHA-256 of a string (test-side, independent of production code).
/// @param[in] input Bytes to hash.
/// @return Hex digest, or an empty string on OpenSSL failure.
[[nodiscard]] inline std::string sha256Hex(const std::string& input)
{
    struct CtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const noexcept
        {
            EVP_MD_CTX_free(ctx);
        }
    };
    const std::unique_ptr<EVP_MD_CTX, CtxDeleter> ctx { EVP_MD_CTX_new() };
    std::array<unsigned char, EVP_MAX_MD_SIZE> out {};
    unsigned int outLen { 0U };
    if ((ctx == nullptr) || (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
        || (EVP_DigestUpdate(ctx.get(), input.data(), input.size()) != 1)
        || (EVP_DigestFinal_ex(ctx.get(), out.data(), &outLen) != 1)) {
        return {};
    }
    constexpr const char* kHex { "0123456789abcdef" };
    std::string hex {};
    for (std::size_t i { 0U }; i < static_cast<std::size_t>(outLen); ++i) {
        const unsigned int b { static_cast<unsigned int>(out.at(i)) };
        hex.push_back(kHex[b >> 4U]);
        hex.push_back(kHex[b & 0x0FU]);
    }
    return hex;
}

/// @brief Extracts a (quoted or token) parameter from a Digest challenge.
/// @param[in] header WWW-Authenticate value.
/// @param[in] name Parameter name.
/// @return Unquoted value, or an empty string if absent.
[[nodiscard]] inline std::string challengeParam(const std::string& header, const std::string& name)
{
    const std::string key { name + "=" };
    std::size_t pos { header.find(key) };
    while ((pos != std::string::npos) && (pos > 0U) && (header.at(pos - 1U) != ' ') && (header.at(pos - 1U) != ',')) {
        pos = header.find(key, pos + 1U);
    }
    if (pos == std::string::npos) {
        return {};
    }
    pos += key.size();
    if ((pos < header.size()) && (header.at(pos) == '"')) {
        const std::size_t end { header.find('"', pos + 1U) };
        return header.substr(pos + 1U, end - pos - 1U);
    }
    const std::size_t end { header.find(',', pos) };
    return header.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
}

/// @class OnvifTestClient
/// @brief httplib::Client facade that authenticates SOAP POSTs with WS-UsernameToken.
/// @details Not thread-safe; create one instance per test thread.
class OnvifTestClient {
public:
    /// @brief Creates a client bound to host:port with optional credentials.
    /// @param[in] host Server host name or IP.
    /// @param[in] port Server TCP port.
    /// @param[in] creds Credentials; an empty username sends unauthenticated requests.
    OnvifTestClient(const std::string& host, int port, Onvif::SecurityCredentials creds = defaultAdmin())
        : m_client(host, port)
        , m_creds(std::move(creds))
    {
    }

    /// @brief Sets the TCP connection timeout.
    /// @param[in] timeout Timeout duration.
    template <class Rep, class Period> void set_connection_timeout(const std::chrono::duration<Rep, Period>& timeout)
    {
        m_client.set_connection_timeout(timeout);
    }

    /// @brief Sets the TCP connection timeout (httplib seconds/microseconds overload).
    /// @param[in] sec Seconds.
    /// @param[in] usec Microseconds.
    void set_connection_timeout(std::time_t sec, std::time_t usec = 0)
    {
        m_client.set_connection_timeout(sec, usec);
    }

    /// @brief Sets the socket read timeout.
    /// @param[in] timeout Timeout duration.
    template <class Rep, class Period> void set_read_timeout(const std::chrono::duration<Rep, Period>& timeout)
    {
        m_client.set_read_timeout(timeout);
    }

    /// @brief Sets the socket read timeout (httplib seconds/microseconds overload).
    /// @param[in] sec Seconds.
    /// @param[in] usec Microseconds.
    void set_read_timeout(std::time_t sec, std::time_t usec = 0)
    {
        m_client.set_read_timeout(sec, usec);
    }

    /// @brief POSTs a SOAP envelope, injecting a fresh WS-Security header when credentials are set.
    /// @param[in] path Request path.
    /// @param[in] body SOAP envelope.
    /// @param[in] contentType Content-Type header value.
    /// @return httplib result.
    httplib::Result Post(const std::string& path, const std::string& body, const std::string& contentType)
    {
        const std::string secured = injectSecurity(body, Onvif::OnvifSecurity::buildSoapSecurityHeader(m_creds));
        return m_client.Post(path.c_str(), secured, contentType.c_str());
    }

    /// @brief Performs an HTTP GET, answering a 401 Digest challenge with SHA-256 credentials.
    /// @param[in] path Request path.
    /// @return httplib result of the final request.
    httplib::Result Get(const std::string& path)
    {
        httplib::Result first = m_client.Get(path.c_str());
        if (!first || (first->status != 401) || m_creds.username.empty()) {
            return first;
        }
        const std::string challenge { first->get_header_value("WWW-Authenticate", 0U) };
        const std::string realm { challengeParam(challenge, "realm") };
        const std::string nonce { challengeParam(challenge, "nonce") };
        const std::string cnonce { Onvif::OnvifSecurity::base64Encode(Onvif::OnvifSecurity::generateNonce()) };
        const std::string nc { "00000001" };
        const std::string ha1 { sha256Hex(m_creds.username + ":" + realm + ":" + m_creds.password) };
        const std::string ha2 { sha256Hex("GET:" + path) };
        const std::string response { sha256Hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2) };
        const std::string auth { "Digest username=\"" + m_creds.username + "\", realm=\"" + realm + "\", nonce=\""
            + nonce + "\", uri=\"" + path + "\", algorithm=SHA-256, qop=auth, nc=" + nc + ", cnonce=\"" + cnonce
            + "\", response=\"" + response + "\"" };
        const httplib::Headers headers { { "Authorization", auth } };
        return m_client.Get(path.c_str(), headers);
    }

    /// @brief Provides access to the underlying httplib client for raw requests.
    /// @return Reference to the wrapped client.
    [[nodiscard]] httplib::Client& raw() noexcept
    {
        return m_client;
    }

private:
    httplib::Client m_client;
    Onvif::SecurityCredentials m_creds {};
};

} // namespace OnvifTest
