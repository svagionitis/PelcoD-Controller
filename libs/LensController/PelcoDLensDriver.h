#pragma once

/// @file PelcoDLensDriver.h
/// @brief Zero-heap adapter implementing ILensDriver for Pelco-D motorized lens mechanisms.

#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

/// @class PelcoDLensDriver
/// @brief Zero-heap adapter implementing ILensDriver for Pelco-D motorized lens mechanisms.
/// @details Formats standard 7-byte Pelco-D protocol frames with modulo-256 checksums,
/// supports relative jog (Tele/Wide, Near/Far), extended absolute positioning opcodes
/// (0x4F, 0x4B), and telemetry inquiries (0x5D, 0x57).
class PelcoDLensDriver final : public ILensDriver {
public:
    using TransmitCallback = void (*)(const std::uint8_t* data, std::size_t len, void* userCtx);

    /// @brief Constructs a PelcoDLensDriver.
    /// @param cameraAddress Pelco-D RS-485 device address [1..255].
    /// @param ranges Physical optical travel boundaries.
    explicit PelcoDLensDriver(std::uint8_t cameraAddress = 1U, LensRanges ranges = { }) noexcept;
    ~PelcoDLensDriver() override = default;

    PelcoDLensDriver(const PelcoDLensDriver&) = delete;
    PelcoDLensDriver& operator=(const PelcoDLensDriver&) = delete;
    PelcoDLensDriver(PelcoDLensDriver&&) = delete;
    PelcoDLensDriver& operator=(PelcoDLensDriver&&) = delete;

    // --- ILensDriver Interface Methods ---

    [[nodiscard]] bool initialize(std::uint8_t port, std::uint32_t baud) noexcept override;
    void moveFocus(std::int16_t steps, MotorSpeed speed) noexcept override;
    void moveZoom(std::int16_t steps, MotorSpeed speed) noexcept override;
    void setFocusPosition(MotorPosition pos, MotorSpeed speed) noexcept override;
    void setZoomPosition(MotorPosition pos, MotorSpeed speed) noexcept override;
    void stopMotion() noexcept override;
    [[nodiscard]] MotorPosition currentFocus() const noexcept override;
    [[nodiscard]] MotorPosition currentZoom() const noexcept override;
    [[nodiscard]] std::uint8_t limitFlags() const noexcept override;
    void sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept override;
    [[nodiscard]] bool hasBuiltinAutofocus() const noexcept override;
    bool triggerBuiltinAutofocus() noexcept override;
    [[nodiscard]] bool hasBuiltinZoomTracking() const noexcept override;
    bool setBuiltinZoomTracking(bool enable) noexcept override;
    [[nodiscard]] LensRanges lensRanges() const noexcept override;

    // --- Driver Specific Control & Telemetry ---

    /// @brief Sets callback for transmitting generated Pelco-D frames to serial port or transport.
    /// @param cb Function pointer to transmitter.
    /// @param userCtx User context pointer passed to callback.
    void setTransmitCallback(TransmitCallback cb, void* userCtx) noexcept;

    /// @brief Ingests raw serial bytes received from Pelco-D device.
    /// @param data Pointer to incoming buffer.
    /// @param len Number of bytes.
    void processIncomingBytes(const std::uint8_t* data, std::size_t len) noexcept;

    /// @brief Returns the active Pelco-D bus address [1..255].
    [[nodiscard]] std::uint8_t cameraAddress() const noexcept;

    /// @brief Sets the Pelco-D bus address [1..255].
    void setCameraAddress(std::uint8_t address) noexcept;

    /// @brief Emits a Query Zoom command (FF Addr 00 5D 00 00 Checksum).
    void queryZoomPosition() noexcept;

    /// @brief Emits a Query Focus command (FF Addr 00 57 00 00 Checksum).
    void queryFocusPosition() noexcept;

    /// @brief Returns last transmitted frame for testing assertions.
    [[nodiscard]] std::vector<std::uint8_t> lastCommand() const;

private:
    void sendFrame(std::uint8_t cmd1, std::uint8_t cmd2, std::uint8_t data1, std::uint8_t data2) noexcept;
    void transmit(const std::uint8_t* data, std::size_t len) noexcept;
    void updateLimitFlagsLocked() noexcept;

    mutable std::mutex m_mutex { };
    std::uint8_t m_cameraAddress { 1U };
    LensRanges m_ranges { };
    MotorPosition m_currentFocus { 0U };
    MotorPosition m_currentZoom { 0U };
    std::uint8_t m_limitFlags { 0U };
    std::uint8_t m_port { 0xFFU };
    std::uint32_t m_baud { 9600U };

    TransmitCallback m_txCallback { nullptr };
    void* m_userCtx { nullptr };

    std::vector<std::uint8_t> m_lastCmd { };
};

} // namespace LensControl
