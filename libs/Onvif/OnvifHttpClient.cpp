#include "OnvifHttpClient.h"

#include <curl/curl.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace Onvif {

namespace {

    std::size_t writeCallback(void* contents, std::size_t size, std::size_t nmemb, void* userp)
    {
        const std::size_t totalSize = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), totalSize);
        return totalSize;
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

HttpResponse OnvifHttpClient::executeRequest(void* curlHandle, const std::string& url)
{
    HttpResponse response {};
    auto* curl = static_cast<CURL*>(curlHandle);
    if (!curl) {
        response.errorMessage = "Failed to initialize curl handle";
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

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

    // Configure HTTP Digest / Basic authentication (Finding H6 / C1)
    std::string userPwd {};
    if (!m_httpUser.empty()) {
        userPwd = m_httpUser + ":" + m_httpPass;
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST | CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERPWD, userPwd.c_str());
    }

    // Bypass proxies for local/loopback camera addresses and test fixtures
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "127.0.0.1,localhost");

    std::array<char, CURL_ERROR_SIZE> errorBuffer {};
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
        return { 0, {}, "Failed to initialize curl handle" };
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
    std::unique_ptr<CURL, CurlEasyDeleter> curl(curl_easy_init());
    if (!curl) {
        return { 0, {}, "Failed to initialize curl handle" };
    }

    curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

    return executeRequest(curl.get(), url);
}

} // namespace Onvif
