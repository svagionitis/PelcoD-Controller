#pragma once

/// @file FrameExtension.h
/// @brief Shared-owned protocol extension hook invoked by PelcoDDevice on the RX thread.

#include "DeviceStatus.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD {

/// @brief Result of asking an extension whether a frame answers a pending query.
enum class ExtMatch : std::uint8_t {
    NotHandled = 0x00U, ///< The query tag does not belong to the extension; use the base matcher.
    Matched = 0x01U, ///< The tag belongs to the extension and the frame answers it.
    Rejected = 0x02U ///< The tag belongs to the extension and the frame does not answer it.
};

/// @class IFrameExtension
/// @brief Vendor protocol extension that PelcoDDevice consults for every received frame.
/// @details Replaces the former protected virtual hooks (`dispatchFrame`, `isResponseMatchingQuery`)
///          that the RX thread used to call on the device object itself. Because the device holds the
///          extension through a `std::shared_ptr`, the extension stays alive until the device's
///          `stop()` has drained every in-flight RX callback, even while a derived device's own
///          members are being destroyed (review finding C3a, CERT OOP50-CPP).
/// @note All methods are called on transport RX threads, never concurrently for the same frame, but
///       possibly concurrently with user-thread calls into the extension. Implementations must be
///       internally synchronised and must not call back into the device's lifecycle (start/stop).
class IFrameExtension {
public:
    virtual ~IFrameExtension() = default;

    /// @brief Classifies @p frame against the pending query identified by @p queryTag.
    /// @param[in] queryTag Tag of the query the device is waiting for.
    /// @param[in] frame Complete received frame.
    /// @return ExtMatch::NotHandled to defer to the base matcher, otherwise the extension's verdict.
    [[nodiscard]] virtual ExtMatch matchQuery(
        const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept = 0;

    /// @brief Applies @p frame to the extension's own state model.
    /// @param[in] frame Complete received frame (any address; the extension filters as needed).
    /// @return True if the extension state changed; the device then resolves any pending query wait
    ///         and calls publish().
    [[nodiscard]] virtual bool onFrame(const std::vector<std::uint8_t>& frame) = 0;

    /// @brief Notifies the extension's subscribers after onFrame() returned true.
    /// @param[in] base Snapshot of the device's base status at publication time.
    virtual void publish(const DeviceStatus& base) = 0;

protected:
    IFrameExtension() = default;
    IFrameExtension(const IFrameExtension&) = default;
    IFrameExtension& operator=(const IFrameExtension&) = default;
    IFrameExtension(IFrameExtension&&) = default;
    IFrameExtension& operator=(IFrameExtension&&) = default;
};

} // namespace PelcoD
