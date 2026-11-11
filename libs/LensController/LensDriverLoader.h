#pragma once

/// @file LensDriverLoader.h
/// @brief Dynamic shared library loader and multi-channel port collision guard for lens drivers.

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

/// @class LensDriverLoader
/// @brief Thread-safe manager for dynamic lens driver shared libraries with port collision guard.
/// @details Implements Sightline EAN-Lens-DLL-Development compatibility, resolving
/// `create_lens_object` and `create_object` factory symbols from external .dll/.so libraries.
class LensDriverLoader final {
public:
    static constexpr std::size_t kMaxChannels = 4U;
    static constexpr std::uint8_t kInvalidPort = 0xFFU;

    LensDriverLoader() noexcept;
    ~LensDriverLoader() noexcept;

    LensDriverLoader(const LensDriverLoader&) = delete;
    LensDriverLoader& operator=(const LensDriverLoader&) = delete;
    LensDriverLoader(LensDriverLoader&&) noexcept;
    LensDriverLoader& operator=(LensDriverLoader&&) noexcept;

    /// @brief Loads an external lens driver plugin and initializes it on the target port.
    /// @param cam Channel index [0..3].
    /// @param libraryPath Path or filename of the shared library (.dll / .so).
    /// @param port UART port index (e.g. 1 for COM1 or ttyS1).
    /// @param baud Baud rate in bps (e.g. 9600, 38400, 115200).
    /// @param outDriver Output smart pointer receiving the instantiated driver.
    /// @return Status code indicating success or specific failure reason.
    [[nodiscard]] LensLoadResult loadDriver(CameraIndex cam, std::string_view libraryPath, std::uint8_t port,
        std::uint32_t baud, std::unique_ptr<ILensDriver>& outDriver) noexcept;

    /// @brief Unloads driver for channel, closing library handle and releasing port.
    /// @param cam Channel index [0..3].
    void unloadDriver(CameraIndex cam) noexcept;

    /// @brief Checks whether a specific COM/UART port is currently allocated to any channel.
    /// @param port Target UART index.
    /// @return True if port is actively held by a channel.
    [[nodiscard]] bool isPortBound(std::uint8_t port) const noexcept;

    /// @brief Returns the port assigned to a given camera channel, or kInvalidPort if unassigned.
    /// @param cam Channel index [0..3].
    /// @return 8-bit port identifier or 0xFF.
    [[nodiscard]] std::uint8_t boundPort(CameraIndex cam) const noexcept;

    /// @brief Releases all allocated libraries and resets port reservations.
    void reset() noexcept;

private:
    struct ChannelEntry {
        void* moduleHandle { nullptr };
        std::uint8_t port { kInvalidPort };
        std::string loadedPath { };
    };

    [[nodiscard]] bool isPortBoundLocked(std::uint8_t port, CameraIndex exemptCam) const noexcept;
    [[nodiscard]] void* openLibrary(std::string_view path) noexcept;
    void closeLibrary(void* handle) noexcept;
    [[nodiscard]] void* resolveSymbol(void* handle, const char* symbol) noexcept;

    mutable std::mutex m_mutex { };
    std::array<ChannelEntry, kMaxChannels> m_channels { };
};

} // namespace LensControl
