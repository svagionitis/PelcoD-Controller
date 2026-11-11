#pragma once

/// @file ViscaLensDriver.h
/// @brief Zero-heap adapter implementing ILensDriver for Sony VISCA optical camera blocks.

#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

/// @class ViscaLensDriver
/// @brief Zero-heap adapter implementing ILensDriver for Sony VISCA optical camera blocks.
/// @details Generates compliant VISCA command frames for zoom, focus, one-push AF,
/// and zoom-focus tracking modes, parsing incoming inquiry responses without heap allocation.
class ViscaLensDriver final : public ILensDriver {
public:
    using TransmitCallback = void (*)(const std::uint8_t* data, std::size_t len, void* userCtx);

    /// @brief Constructs a ViscaLensDriver.
    /// @param cameraAddress Destination camera address on VISCA bus [1..7].
    /// @param ranges Physical optical travel boundaries.
    explicit ViscaLensDriver(std::uint8_t cameraAddress = 1U, LensRanges ranges = { }) noexcept;
    ~ViscaLensDriver() override = default;

    ViscaLensDriver(const ViscaLensDriver&) = delete;
    ViscaLensDriver& operator=(const ViscaLensDriver&) = delete;
    ViscaLensDriver(ViscaLensDriver&&) = delete;
    ViscaLensDriver& operator=(ViscaLensDriver&&) = delete;

    // --- ILensDriver Interface Methods ---

    /// @copydoc ILensDriver::initialize
    [[nodiscard]] bool initialize(std::uint8_t port, std::uint32_t baud) noexcept override;

    /// @copydoc ILensDriver::moveFocus
    void moveFocus(std::int16_t steps, MotorSpeed speed) noexcept override;

    /// @copydoc ILensDriver::moveZoom
    void moveZoom(std::int16_t steps, MotorSpeed speed) noexcept override;

    /// @copydoc ILensDriver::setFocusPosition
    void setFocusPosition(MotorPosition pos, MotorSpeed speed) noexcept override;

    /// @copydoc ILensDriver::setZoomPosition
    void setZoomPosition(MotorPosition pos, MotorSpeed speed) noexcept override;

    /// @copydoc ILensDriver::stopMotion
    void stopMotion() noexcept override;

    /// @copydoc ILensDriver::currentFocus
    [[nodiscard]] MotorPosition currentFocus() const noexcept override;

    /// @copydoc ILensDriver::currentZoom
    [[nodiscard]] MotorPosition currentZoom() const noexcept override;

    /// @copydoc ILensDriver::limitFlags
    [[nodiscard]] std::uint8_t limitFlags() const noexcept override;

    /// @copydoc ILensDriver::sendRawCommand
    void sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept override;

    /// @copydoc ILensDriver::hasBuiltinAutofocus
    [[nodiscard]] bool hasBuiltinAutofocus() const noexcept override;

    /// @copydoc ILensDriver::triggerBuiltinAutofocus
    bool triggerBuiltinAutofocus() noexcept override;

    /// @copydoc ILensDriver::hasBuiltinZoomTracking
    [[nodiscard]] bool hasBuiltinZoomTracking() const noexcept override;

    /// @copydoc ILensDriver::setBuiltinZoomTracking
    bool setBuiltinZoomTracking(bool enable) noexcept override;

    /// @copydoc ILensDriver::lensRanges
    [[nodiscard]] LensRanges lensRanges() const noexcept override;

    // --- Driver Specific Control & Telemetry ---

    /// @brief Sets callback for transmitting generated VISCA frames to serial port or transport.
    /// @param cb Function pointer to transmitter.
    /// @param userCtx User context pointer passed to callback.
    void setTransmitCallback(TransmitCallback cb, void* userCtx) noexcept;

    /// @brief Ingests raw serial bytes received from camera block.
    /// @param data Pointer to incoming buffer.
    /// @param len Number of bytes.
    void processIncomingBytes(const std::uint8_t* data, std::size_t len) noexcept;

    /// @brief Returns the active VISCA camera bus address [1..7].
    [[nodiscard]] std::uint8_t cameraAddress() const noexcept;

    /// @brief Sets the VISCA camera bus address [1..7].
    void setCameraAddress(std::uint8_t address) noexcept;

    /// @brief Emits a Zoom Position Inquiry command (8x 09 04 47 FF).
    void queryZoomPosition() noexcept;

    /// @brief Emits a Focus Position Inquiry command (8x 09 04 48 FF).
    void queryFocusPosition() noexcept;

    /// @brief Returns last transmitted frame for testing assertions.
    [[nodiscard]] std::vector<std::uint8_t> lastCommand() const;

private:
    [[nodiscard]] static constexpr std::uint8_t toViscaSpeed(MotorSpeed speed) noexcept
    {
        return static_cast<std::uint8_t>(
            std::clamp((static_cast<std::uint32_t>(speed.value()) * 7U + 127U) / 255U, 0U, 7U));
    }

    [[nodiscard]] std::uint8_t makeHeader() const noexcept
    {
        return static_cast<std::uint8_t>(0x80U | (m_cameraAddress & 0x0FU));
    }

    void transmit(const std::uint8_t* data, std::size_t len) noexcept;
    void updateLimitFlagsLocked() noexcept;

    enum class LastInquiry : std::uint8_t { None = 0U, Zoom = 1U, Focus = 2U };

    mutable std::mutex m_mutex { };
    std::uint8_t m_cameraAddress { 1U };
    LensRanges m_ranges { };
    MotorPosition m_currentFocus { 0U };
    MotorPosition m_currentZoom { 0U };
    std::uint8_t m_limitFlags { 0U };
    std::uint8_t m_port { 0xFFU };
    std::uint32_t m_baud { 9600U };
    bool m_trackingEnabled { false };
    LastInquiry m_lastInquiry { LastInquiry::None };

    TransmitCallback m_txCallback { nullptr };
    void* m_userCtx { nullptr };

    std::vector<std::uint8_t> m_lastCmd { };
};

} // namespace LensControl
