#pragma once

#include "ViscaFrame.h"
#include <Transport/ITransport.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Visca {

/// @enum ScanError
/// @brief Error and diagnostic status classifications during VISCA bus enumeration.
enum class ScanError {
    None,                     ///< No error occurred.
    TransportNotOpen,         ///< Transport interface is null or not open.
    AddressSetSendFailed,     ///< Failed to transmit AddressSet broadcast frame.
    AddressSetTimeout,        ///< Timeout waiting for AddressSet response from cameras.
    VersionInquirySendFailed, ///< Failed to transmit CAM_VersionInq frame for a camera address.
    VersionInquiryTimeout,    ///< Timeout waiting for CAM_VersionInq response from camera.
    Cancelled                 ///< Scan was aborted by cancellation predicate.
};

/// @brief Converts a @ref ScanError enum value to a human-readable diagnostic string.
/// @param[in] err Scan error enum value.
/// @return Null-terminated string describing the error.
[[nodiscard]] const char* scanErrorToString(ScanError err) noexcept;

/// @struct DiscoveredCamera
/// @brief Raw camera identification retrieved during VISCA bus enumeration.
struct DiscoveredCamera {
    uint8_t address { 1 };   ///< Bus address assigned to camera (1..7)
    uint16_t vendorId { 0 }; ///< Vendor ID (e.g. 0x0020 for Sony)
    uint16_t modelId { 0 };  ///< Model ID
    uint16_t romVersion { 0 }; ///< Firmware ROM version
    uint8_t maxSockets { 2 }; ///< Maximum concurrent sockets supported
};

/// @class ViscaBusScanner
/// @brief Daisy-chain bus scanner for VISCA multi-camera configurations.
/// @details Automatically configures camera addresses using AddressSet (88 30 01 FF),
/// and sweeps detected addresses with CAM_VersionInq to retrieve hardware IDs.
class ViscaBusScanner {
public:
    /// @brief Progress callback reporting the current step and total steps.
    using ProgressCallback = std::function<void(size_t currentStep, size_t totalSteps)>;

    /// @brief Callback invoked whenever a camera is successfully identified on the bus.
    using CameraDiscoveredCallback = std::function<void(const DiscoveredCamera& camera)>;

    /// @brief Callback reporting non-fatal or fatal scan errors and diagnostic warnings.
    using ErrorCallback = std::function<void(ScanError error, const std::string& message)>;

    /// @brief Predicate queried to determine if the scan should be aborted early.
    using CancellationPredicate = std::function<bool()>;

    /// @brief Constructs a ViscaBusScanner with the specified transport.
    /// @param[in] transport Shared pointer to @ref Transport::ITransport.
    explicit ViscaBusScanner(std::shared_ptr<::Transport::ITransport> transport);

    /// @brief Scans the bus synchronously.
    /// @param[in] onProgress Optional progress reporting callback.
    /// @param[in] onFound Optional per-camera discovery callback.
    /// @param[in] onError Optional error and diagnostic reporting callback.
    /// @param[in] isCancelled Optional cancellation check callback returning true to abort.
    /// @return Vector of discovered cameras on the bus.
    [[nodiscard]] std::vector<DiscoveredCamera> scanBus(
        ProgressCallback onProgress = nullptr,
        CameraDiscoveredCallback onFound = nullptr,
        ErrorCallback onError = nullptr,
        CancellationPredicate isCancelled = nullptr);

private:
    std::shared_ptr<::Transport::ITransport> m_transport;
};

} // namespace Visca
