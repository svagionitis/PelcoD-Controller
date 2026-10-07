#pragma once

/// @file SoapFault.h
/// @brief SOAP 1.2 fault bodies used by the ONVIF server.

#include <string>
#include <string_view>

namespace Onvif {

/// @class SoapFault
/// @brief Builds SOAP 1.2 `SOAP-ENV:Fault` elements for embedding in a response Body.
/// @details The returned fragments use the `SOAP-ENV` and `ter` prefixes, which are declared on
///          the envelope produced by the server's response wrapper.
class SoapFault {
public:
    /// @brief Fault returned when a request is unauthenticated or not authorized.
    /// @details Code `SOAP-ENV:Sender`, Subcode `ter:NotAuthorized` (ONVIF Core §5.11.2.4).
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string notAuthorized();

    /// @brief Fault returned when a ConsumerReference URL is invalid or blocked by SSRF policy.
    /// @details Code `SOAP-ENV:Sender`, Subcode `ter:InvalidArgVal`, secondary `wsnt:InvalidConsumerReferenceFault`.
    /// @param[in] reason Human-readable reason text.
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string invalidConsumerRef(std::string_view reason);

    /// @brief Fault returned when subscription quota is exceeded or creation fails.
    /// @details Code `SOAP-ENV:Sender`, Subcode `wsnt:SubscribeCreationFailedFault`.
    /// @param[in] reason Human-readable reason text.
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string subscribeCreationFailed(std::string_view reason);

    /// @brief Fault returned when an operation references an unknown or expired subscription.
    /// @details Code `SOAP-ENV:Sender`, Subcode `wsrf-rw:ResourceUnknownFault`.
    /// @param[in] reason Human-readable reason text.
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string resourceUnknown(std::string_view reason);

    /// @brief Fault returned when operational actions are requested on an unprovisioned device.
    /// @details Code `SOAP-ENV:Sender`, Subcode `ter:OperationProhibited`, secondary `ter:DeviceUnprovisioned`.
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string deviceUnprovisioned();

    /// @brief Fault returned when a supplied password fails password policy validation.
    /// @details Code `SOAP-ENV:Sender`, Subcode `ter:InvalidArgVal`, secondary `ter:PasswordTooWeak`.
    /// @param[in] reason Detailed policy failure explanation.
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string passwordTooWeak(std::string_view reason);

    /// @brief Fault returned when an action is not supported by the service.
    /// @details Code `SOAP-ENV:Receiver`, Subcode `ter:ActionNotSupported` (ONVIF Core §5.11.2).
    /// @param[in] reason Human-readable reason text (defaults to "Action Not Supported").
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string actionNotSupported(std::string_view reason = "Action Not Supported");

    /// @brief Generic receiver fault.
    /// @details Formats a SOAP 1.2 Receiver fault with primary and optional secondary subcodes.
    /// @param[in] subcode Qualified ONVIF subcode (e.g. "ter:ActionNotSupported").
    /// @param[in] secondSubcode Optional nested subcode; empty to omit.
    /// @param[in] reason Human-readable reason text (must not contain markup).
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string receiver(
        std::string_view subcode, std::string_view secondSubcode, std::string_view reason);

    /// @brief Generic sender fault.
    /// @param[in] subcode Qualified ONVIF subcode (e.g. "ter:InvalidArgVal").
    /// @param[in] secondSubcode Optional nested subcode (e.g. "ter:UsernameClash"); empty to omit.
    /// @param[in] reason Human-readable reason text (must not contain markup).
    /// @return Serialized Fault element.
    [[nodiscard]] static std::string sender(
        std::string_view subcode, std::string_view secondSubcode, std::string_view reason);
};

} // namespace Onvif
