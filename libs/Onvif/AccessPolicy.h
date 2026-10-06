#pragma once

/// @file AccessPolicy.h
/// @brief ONVIF Core §5.9.4 operation classification and user-level authorization matrix.

#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"

#include <string_view>

namespace Onvif {

/// @class AccessPolicy
/// @brief Stateless mapping from (service, operation) to AccessClass and from user level to
///        permitted access classes.
/// @details
/// @verbatim
///   Class                 Administrator  Operator  User  Anonymous
///   PreAuth                     X           X       X       X
///   ReadSystem                  X           X       X
///   ReadSystemSensitive         X           X
///   ReadSystemSecret            X
///   WriteSystem                 X
///   Unrecoverable               X
///   ReadMedia                   X           X       X
///   Actuate                     X           X
/// @endverbatim
///          Operations absent from the table, unknown services and empty operation names are
///          classified as AccessClass::Unrecoverable (fail closed: administrators only).
///          OnvifUserLevel::Extended is treated as Anonymous.
class AccessPolicy {
public:
    /// @brief Classifies a SOAP operation.
    /// @param[in] service Service name as passed by OnvifServer (e.g. "Device", "PTZ").
    /// @param[in] operation Body element name, optionally namespace-prefixed (e.g. "tptz:Stop").
    /// @return The operation's access class (Unrecoverable if unknown).
    [[nodiscard]] static AccessClass classify(std::string_view service, std::string_view operation) noexcept;

    /// @brief Checks whether a user level may invoke an access class.
    /// @param[in] level Authenticated user's level (Anonymous for unauthenticated callers).
    /// @param[in] cls Required access class.
    /// @return True if permitted.
    [[nodiscard]] static bool permits(OnvifUserLevel level, AccessClass cls) noexcept;
};

} // namespace Onvif
