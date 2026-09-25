#pragma once

#include "SonyCameraModel.h"
#include "SonyViscaBuilder.h"
#include "SonyViscaParser.h"
#include "SonyViscaTypes.h"
#include <ViscaDevice.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace Visca::Sony {

/// @class SonyFCBDevice
/// @brief High-level device controller for Sony FCB-EV9520L and FCB-EW9500H cameras.
/// @details Automatically probes camera model upon initialization, enforces hardware capability
/// gating (e.g. preventing distortion compensation on EW9500H or 4K on EV9520L),
/// and coordinates Block Inquiry polling for complete status telemetry.
class SonyFCBDevice {
public:
    /// @brief Constructs a SonyFCBDevice wrapping a VISCA device.
    /// @param[in] transport Underlying transport instance.
    /// @param[in] cameraAddress Camera address on bus (1..7).
    explicit SonyFCBDevice(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress = 1);

    /// @brief Virtual destructor.
    virtual ~SonyFCBDevice() = default;

    /// @brief Initializes device communication, clears interface, and identifies model capabilities.
    /// @return True if camera responded to version inquiry and was successfully identified.
    bool initialize();

    /// @brief Retrieves the identified camera model type.
    [[nodiscard]] SonyCameraModelType modelType() const noexcept;

    /// @brief Retrieves the camera capabilities descriptor.
    [[nodiscard]] const CameraCapabilities& capabilities() const noexcept;

    /// @brief Retrieves current cached camera status telemetry.
    [[nodiscard]] SonyFCBStatus status() const noexcept;

    /// @brief Accesses the underlying VISCA device state machine.
    [[nodiscard]] ViscaDevice& device() noexcept
    {
        return m_device;
    }

    /// @brief Polls all Block Inquiries (00 to 04) to update status telemetry.
    /// @return True if all block inquiries were answered and parsed.
    bool pollStatus();

    // --- Lens & Optics Controls ---
    bool setZoomDirect(uint16_t position);
    bool zoomTele(uint8_t speed = 4);
    bool zoomWide(uint8_t speed = 4);
    bool zoomStop();

    bool setFocusAuto(bool autoMode);
    bool setFocusDirect(uint16_t position);
    bool focusFar(uint8_t speed = 4);
    bool focusNear(uint8_t speed = 4);
    bool focusStop();
    bool focusOnePush();
    bool setFocusNearLimit(uint16_t limit);

    // --- Exposure Controls ---
    bool setExposureMode(SonyExposureMode mode);
    bool setShutter(uint8_t position);
    bool setIris(uint8_t position);
    bool setGain(uint8_t position);
    bool setExposureCompensation(bool on, uint8_t position = 0);

    // --- White Balance ---
    bool setWhiteBalance(SonyWhiteBalanceMode mode);
    bool setRgaiDirect(uint8_t position);
    bool setBgainDirect(uint8_t position);
    bool triggerOnePushWb();

    // --- Image Processing & Enhancements ---
    bool setStabilizer(SonyStabilizerMode mode);
    bool setDefog(SonyDefogMode mode);
    bool setIcr(bool on);
    bool setAutoIcr(bool on);

    // --- Hardware Gated Controls ---

    /// @brief Enables or disables lens distortion compensation (Register 0x57).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] on True to enable, false to disable.
    /// @return True if command executed, false if unsupported by hardware.
    bool setDistortionCompensation(bool on);

    /// @brief Enables or disables optical axis gap compensation (Register 0x47).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] on True to enable, false to disable.
    /// @return True if command executed, false if unsupported by hardware.
    bool setOpticalAxisGapCompensation(bool on);

    /// @brief Configures video operating mode / resolution (Register 0x72).
    /// @note Gated: Modes >= 0x25 are 4K UHD and only supported on FCB-EW9500H.
    /// @param[in] mode Register 0x72 operating mode code.
    /// @return True if command executed, false if unsupported mode requested.
    bool setOperatingMode(uint8_t mode);

    /// @brief Sets LVDS serial video output mode (Register 0x74).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] mode Mode value.
    /// @return True if command executed, false if unsupported.
    bool setLvdsMode(uint8_t mode);

    /// @brief Sets TMDS / HDMI digital output mode (Register 0x60).
    /// @note Gated: Supported on FCB-EW9500H, NOT supported on FCB-EV9520L.
    /// @param[in] mode Mode value.
    /// @return True if command executed, false if unsupported.
    bool setDigitalOutputMode(uint8_t mode);

private:
    ViscaDevice m_device;
    SonyCameraModelType m_modelType { SonyCameraModelType::Unknown };
    CameraCapabilities m_capabilities {};

    mutable std::mutex m_statusMutex {};
    SonyFCBStatus m_status {};
};

} // namespace Visca::Sony
