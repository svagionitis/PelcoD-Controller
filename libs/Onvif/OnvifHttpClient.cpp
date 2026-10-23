#include "OnvifHttpClient.h"

#include <curl/curl.h>
#include <openssl/evp.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "OnvifCrypto.h"
#include "OnvifSecurity.h"

namespace Onvif {

namespace {

    std::size_t writeCallback(void* contents, std::size_t size, std::size_t nmemb, void* userp)
    {
        const std::size_t totalSize = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    std::size_t headerCallback(char* buffer, std::size_t size, std::size_t nitems, void* userdata)
    {
        const std::size_t total = size * nitems;
        auto* str = static_cast<std::string*>(userdata);
        if (str && buffer) {
            str->append(buffer, total);
        }
        return total;
    }

    struct CurlSlistDeleter {
        void operator()(curl_slist* list) const
        {
            if (list) {
                curl_slist_free_all(list);
            }
        }
    };

    struct CurlEasyDeleter {
        void operator()(CURL* curl) const
        {
            if (curl) {
                curl_easy_cleanup(curl);
            }
        }
    };

    std::string parseAuthParam(std::string_view header, std::string_view key)
    {
        const auto pos = header.find(key);
        if (pos == std::string_view::npos) {
            return { };
        }
        auto start = pos + key.size();
        while (start < header.size() && (header[start] == ' ' || header[start] == '=')) {
            ++start;
        }
        if (start >= header.size()) {
            return { };
        }
        const bool quoted = (header[start] == '"');
        if (quoted) {
            ++start;
        }
        auto end = start;
        while (end < header.size()
            && (quoted ? header[end] != '"'
                       : (header[end] != ',' && header[end] != ' ' && header[end] != '\r' && header[end] != '\n'))) {
            ++end;
        }
        return std::string(header.substr(start, end - start));
    }

} // namespace

OnvifHttpClient::OnvifHttpClient() = default;

OnvifHttpClient::~OnvifHttpClient() = default;

OnvifHttpClient::OnvifHttpClient(OnvifHttpClient&&) noexcept = default;

OnvifHttpClient& OnvifHttpClient::operator=(OnvifHttpClient&&) noexcept = default;

void OnvifHttpClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

std::chrono::milliseconds OnvifHttpClient::timeout() const
{
    return m_timeout;
}

void OnvifHttpClient::setTlsConfig(TlsConfig config)
{
    m_tlsConfig = std::move(config);
}

const TlsConfig& OnvifHttpClient::tlsConfig() const noexcept
{
    return m_tlsConfig;
}

void OnvifHttpClient::setHttpCredentials(std::string username, std::string password)
{
    m_httpUser = std::move(username);
    m_httpPass = std::move(password);
}

const std::string& OnvifHttpClient::httpUsername() const noexcept
{
    return m_httpUser;
}

HttpResponse OnvifHttpClient::executeRequest(void* curlHandle, const std::string& url, bool withHttpAuth)
{
    HttpResponse response { };
    auto* curl = static_cast<CURL*>(curlHandle);
    if (!curl) {
        response.errorMessage = "Failed to initialize curl handle";
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    const long timeoutMs = static_cast<long>(m_timeout.count());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeoutMs > 3000 ? 3000L : timeoutMs);

    // Apply TLS verification mode (Finding H5 / SEI CERT POS44-C)
    switch (m_tlsConfig.mode) {
    case TlsVerificationMode::Strict:
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        break;
    case TlsVerificationMode::VerifyPeerOnly:
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        break;
    case TlsVerificationMode::PinnedCertificate:
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        if (!m_tlsConfig.pinnedPublicKey.empty()) {
            curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, m_tlsConfig.pinnedPublicKey.c_str());
        }
        break;
    case TlsVerificationMode::InsecureAllowSelfSigned:
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        break;
    }

    if (!m_tlsConfig.caBundlePath.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, m_tlsConfig.caBundlePath.c_str());
    }

    if (withHttpAuth) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST | CURLAUTH_BASIC);
        std::string userPwd { };
        if (!m_httpUser.empty()) {
            userPwd = m_httpUser + ":" + m_httpPass;
            curl_easy_setopt(curl, CURLOPT_USERPWD, userPwd.c_str());
        }
    }

    // Bypass proxies for local/loopback camera addresses and test fixtures
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "127.0.0.1,localhost");

    std::array<char, CURL_ERROR_SIZE> errorBuffer { };
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer.data());

    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.errorMessage = errorBuffer[0] != '\0' ? errorBuffer.data() : curl_easy_strerror(res);
        return response;
    }

    long httpCode { 0 };
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = httpCode;

    return response;
}

HttpResponse OnvifHttpClient::sendPost(
    const std::string& url, const std::string& soapXml, const std::string& soapAction)
{
    std::unique_ptr<CURL, CurlEasyDeleter> curl(curl_easy_init());
    if (!curl) {
        return { 0, { }, "Failed to initialize curl handle", { } };
    }

    curl_slist* rawHeaders { nullptr };
    rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/soap+xml; charset=utf-8");
    rawHeaders = curl_slist_append(rawHeaders, "Accept: application/soap+xml, text/xml");

    if (!soapAction.empty()) {
        const std::string actionHeader = "SOAPAction: \"" + soapAction + "\"";
        rawHeaders = curl_slist_append(rawHeaders, actionHeader.c_str());
    }

    std::unique_ptr<curl_slist, CurlSlistDeleter> headers(rawHeaders);

    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, soapXml.data());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(soapXml.size()));

    return executeRequest(curl.get(), url);
}

HttpResponse OnvifHttpClient::sendGet(const std::string& url)
{
    std::string user = m_httpUser;
    std::string pass = m_httpPass;
    std::string cleanUrl = url;
    const auto atPos = cleanUrl.find('@');
    const auto schemePos = cleanUrl.find("://");
    if (atPos != std::string::npos && schemePos != std::string::npos && atPos > schemePos + 3) {
        const std::string creds = cleanUrl.substr(schemePos + 3, atPos - (schemePos + 3));
        const auto colonPos = creds.find(':');
        if (colonPos != std::string::npos) {
            if (user.empty()) {
                user = creds.substr(0, colonPos);
                pass = creds.substr(colonPos + 1);
            }
        }
        cleanUrl.erase(schemePos + 3, atPos - (schemePos + 3) + 1);
    }

    std::unique_ptr<CURL, CurlEasyDeleter> curl(curl_easy_init());
    if (!curl) {
        return { 0, { }, "Failed to initialize curl handle", { } };
    }
    curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);
    HttpResponse resp = executeRequest(curl.get(), cleanUrl, true);

    if ((resp.statusCode == 401 || !resp.errorMessage.empty()) && !user.empty()) {
        const std::string& hdrs = resp.headers;
        const auto authPos = hdrs.find("WWW-Authenticate:");
        if (authPos != std::string::npos) {
            const std::string_view authView { hdrs.c_str() + authPos, hdrs.size() - authPos };
            const std::string realm = parseAuthParam(authView, "realm");
            const std::string nonce = parseAuthParam(authView, "nonce");
            const std::string qop = parseAuthParam(authView, "qop");
            std::string alg = parseAuthParam(authView, "algorithm");
            if (alg.empty()) {
                alg = "MD5";
            }

            std::string path = "/";
            const auto sPos = cleanUrl.find("://");
            if (sPos != std::string::npos) {
                const auto slashPos = cleanUrl.find('/', sPos + 3);
                if (slashPos != std::string::npos) {
                    path = cleanUrl.substr(slashPos);
                }
            }

            std::string cnonce = "0123456789abcdef";
            const auto nonceBytes = OnvifSecurity::generateNonce();
            if (!nonceBytes.empty()) {
                const auto hexOpt = Crypto::hexDigest(EVP_sha256(),
                    std::string_view(reinterpret_cast<const char*>(nonceBytes.data()), nonceBytes.size()));
                if (hexOpt && hexOpt->size() >= 16) {
                    cnonce = hexOpt->substr(0, 16);
                }
            }

            const EVP_MD* md = (alg == "SHA-256") ? EVP_sha256() : EVP_md5();
            const auto ha1 = Crypto::hexDigest(md, user + ":" + realm + ":" + pass);
            const auto ha2 = Crypto::hexDigest(md, "GET:" + path);
            if (ha1 && ha2) {
                const std::string nc = "00000001";
                std::string respHash;
                if (!qop.empty()) {
                    respHash = Crypto::hexDigest(md, *ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":auth:" + *ha2)
                                   .value_or("");
                } else {
                    respHash = Crypto::hexDigest(md, *ha1 + ":" + nonce + ":" + *ha2).value_or("");
                }

                if (!respHash.empty()) {
                    std::ostringstream authHdr;
                    authHdr << "Authorization: Digest username=\"" << user << "\", realm=\"" << realm << "\", nonce=\""
                            << nonce << "\", uri=\"" << path << "\", algorithm=" << alg;
                    if (!qop.empty()) {
                        authHdr << ", qop=auth, nc=" << nc << ", cnonce=\"" << cnonce << "\"";
                    }
                    authHdr << ", response=\"" << respHash << "\"";

                    std::unique_ptr<CURL, CurlEasyDeleter> retryCurl(curl_easy_init());
                    if (retryCurl) {
                        curl_slist* rawList = curl_slist_append(nullptr, authHdr.str().c_str());
                        std::unique_ptr<curl_slist, CurlSlistDeleter> retryHeaders(rawList);
                        curl_easy_setopt(retryCurl.get(), CURLOPT_HTTPHEADER, retryHeaders.get());
                        curl_easy_setopt(retryCurl.get(), CURLOPT_HTTPGET, 1L);
                        return executeRequest(retryCurl.get(), cleanUrl, false);
                    }
                }
            }
        }
    }

    return resp;
}

} // namespace Onvif
