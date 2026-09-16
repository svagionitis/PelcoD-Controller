#include "OnvifHttpClient.h"

#include <curl/curl.h>

#include <memory>

namespace PelcoD::Onvif {

namespace {

    size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp)
    {
        const size_t totalSize = size * nmemb;
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

HttpResponse OnvifHttpClient::sendPost(
    const std::string& url, const std::string& soapXml, const std::string& soapAction)
{
    HttpResponse response {};

    std::unique_ptr<CURL, CurlEasyDeleter> curl(curl_easy_init());
    if (!curl) {
        response.errorMessage = "Failed to initialize curl handle";
        return response;
    }

    curl_slist* rawHeaders { nullptr };
    rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/soap+xml; charset=utf-8");
    rawHeaders = curl_slist_append(rawHeaders, "Accept: application/soap+xml, text/xml");

    if (!soapAction.empty()) {
        const std::string actionHeader = "SOAPAction: \"" + soapAction + "\"";
        rawHeaders = curl_slist_append(rawHeaders, actionHeader.c_str());
    }

    std::unique_ptr<curl_slist, CurlSlistDeleter> headers(rawHeaders);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, soapXml.data());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(soapXml.size()));

    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body);

    const long timeoutMs = static_cast<long>(m_timeout.count());
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, timeoutMs > 3000 ? 3000L : timeoutMs);

    // Camera LAN devices frequently use self-signed certificates for HTTPS
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 0L);

    char errorBuffer[CURL_ERROR_SIZE] { 0 };
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errorBuffer);

    const CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        response.errorMessage = errorBuffer[0] != '\0' ? errorBuffer : curl_easy_strerror(res);
        return response;
    }

    long httpCode { 0 };
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = httpCode;

    return response;
}

} // namespace PelcoD::Onvif
