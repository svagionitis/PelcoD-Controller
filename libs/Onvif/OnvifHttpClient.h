#pragma once

/// @file OnvifHttpClient.h
/// @brief HTTP POST SOAP client wrapper using libcurl.

#include "OnvifAuthTypes.h"

#include <chrono>
#include <string>

namespace Onvif {

/// @struct HttpResponse
/// @brief Encapsulates HTTP response data and status code.
struct HttpResponse {
    long statusCode { 0 }; ///< HTTP response code (e.g. 200, 401, 500)
    std::string body { }; ///< Response payload body
    std::string errorMessage { }; ///< curl error message if request failed
    std::string headers { }; ///< Response headers

    /// @brief Checks if response indicates HTTP success (2xx).
    /// @return True if HTTP status code is 200-299.
    [[nodiscard]] bool isSuccess() const
    {
        return statusCode >= 200 && statusCode < 300;
    }
};

/// @class OnvifHttpClient
/// @brief RAII HTTP/HTTPS client transmitting SOAP requests via libcurl.
class OnvifHttpClient {
public:
    /// @brief Default constructor setting default timeout.
    OnvifHttpClient();

    /// @brief Destructor.
    ~OnvifHttpClient();

    OnvifHttpClient(const OnvifHttpClient&) = delete;
    OnvifHttpClient& operator=(const OnvifHttpClient&) = delete;
    OnvifHttpClient(OnvifHttpClient&&) noexcept;
    OnvifHttpClient& operator=(OnvifHttpClient&&) noexcept;

    /// @brief Sets connection and request timeout.
    /// @param[in] timeout Request timeout duration.
    void setTimeout(std::chrono::milliseconds timeout);

    /// @brief Gets current timeout.
    /// @return Timeout duration.
    [[nodiscard]] std::chrono::milliseconds timeout() const;

    /// @brief Configures TLS transport security options.
    /// @param[in] config TLS verification mode and certificate paths.
    void setTlsConfig(TlsConfig config);

    /// @brief Gets current TLS transport configuration.
    /// @return Current TlsConfig.
    [[nodiscard]] const TlsConfig& tlsConfig() const noexcept;

    /// @brief Configures HTTP transport credentials for Digest/Basic authentication.
    /// @param[in] username HTTP authentication username.
    /// @param[in] password HTTP authentication password.
    void setHttpCredentials(std::string username, std::string password);

    /// @brief Gets current HTTP authentication username.
    /// @return Configured username string.
    [[nodiscard]] const std::string& httpUsername() const noexcept;

    /// @brief Executes a synchronous HTTP POST request with SOAP payload.
    /// @param[in] url Destination endpoint URL (e.g. http://192.168.1.100/onvif/device_service).
    /// @param[in] soapXml XML payload body to transmit.
    /// @param[in] soapAction Optional SOAPAction header string.
    /// @return HttpResponse containing status code and response body.
    [[nodiscard]] HttpResponse sendPost(
        const std::string& url, const std::string& soapXml, const std::string& soapAction = "");

    /// @brief Executes a synchronous HTTP GET request.
    /// @param[in] url Destination URL.
    /// @return HttpResponse containing status code and response body.
    [[nodiscard]] HttpResponse sendGet(const std::string& url);

private:
    [[nodiscard]] HttpResponse executeRequest(void* curlHandle, const std::string& url, bool withHttpAuth = true);

    std::chrono::milliseconds m_timeout { 5000 };
    TlsConfig m_tlsConfig { };
    std::string m_httpUser { };
    std::string m_httpPass { };
};

} // namespace Onvif
