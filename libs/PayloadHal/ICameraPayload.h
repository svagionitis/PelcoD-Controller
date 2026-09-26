#pragma once

/// @file ICameraPayload.h
/// @brief Polymorphic interface for optical daylight and thermal camera payloads.

#include "IDevice.h"
#include "PayloadTypes.h"
#include "VideoStreamTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @class ICameraPayload
/// @brief Unified optical abstraction governing continuous/direct zoom, autofocus,
///        electronic/optical image stabilization, defogging, and thermal palette control.
class ICameraPayload : public virtual IDevice {
public:
    ~ICameraPayload() override = default;

    /// @brief Queries the primary spectral band of the sensor.
    [[nodiscard]] virtual CameraSpectrum spectrum() const noexcept = 0;

    // --- Optical Zoom Controls ---

    /// @brief Drives optical zoom to a normalized position between 0.0 and 1.0.
    /// @param[in] zoom01 Normalized zoom coordinate (0.0 = Full Wide, 1.0 = Max Optical Tele).
    /// @return True if direct zoom command was accepted.
    virtual bool setZoomNormalized(double zoom01) = 0;

    /// @brief Commands continuous optical zooming at designated velocity.
    /// @param[in] velocity Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Wide, +1.0 = Max Tele).
    /// @return True if command was dispatched.
    virtual bool zoomContinuous(float velocity) = 0;

    /// @brief Halts active continuous optical zoom motion immediately.
    /// @return True if zoom motor was stopped.
    virtual bool zoomStop() = 0;

    // --- Focus Controls ---

    /// @brief Toggles continuous automatic focus algorithm.
    /// @param[in] enable True for auto-focus, false for manual focus hold.
    /// @return True if mode was updated.
    virtual bool setFocusAuto(bool enable) = 0;

    /// @brief Drives lens focus group to a normalized position.
    /// @param[in] focus01 Normalized focus position (0.0 = Near Limit, 1.0 = Infinity).
    /// @return True if direct focus command was accepted.
    virtual bool setFocusNormalized(double focus01) = 0;

    /// @brief Commands continuous motorized optical focus motion at designated velocity.
    /// @details Slews focus group towards Near limit (negative velocity) or Far limit (positive velocity).
    /// @param[in] velocity Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Near, +1.0 = Max Far, 0.0 = Stop).
    /// @return True if focus movement command was accepted.
    virtual bool focusContinuous(float velocity) = 0;

    /// @brief Halts active continuous optical focus motion immediately.
    /// @return True if focus motor was stopped.
    virtual bool focusStop() = 0;

    /// @brief Triggers a one-push automatic focus convergence cycle.
    /// @return True if one-push focus cycle was initiated.
    virtual bool triggerOnePushFocus() = 0;

    // --- Iris & Exposure Controls ---

    /// @brief Toggles continuous automatic iris / aperture adjustment algorithm.
    /// @param[in] enable True for auto-iris, false for manual iris hold.
    /// @return True if iris mode was updated.
    virtual bool setIrisAuto(bool enable) = 0;

    /// @brief Drives lens iris aperture to a normalized position.
    /// @param[in] iris01 Normalized iris position (0.0 = Closed, 1.0 = Max Open).
    /// @return True if direct iris command was accepted.
    virtual bool setIrisNormalized(double iris01) = 0;

    /// @brief Commands continuous manual iris aperture adjustment (e.g. from physical joystick).
    /// @param[in] velocity Normalized velocity [-1.0 .. +1.0] (-1.0 = Close, +1.0 = Open, 0.0 = Stop).
    /// @return True if command was dispatched.
    virtual bool irisContinuous(float velocity) = 0;

    /// @brief Halts active motorized iris motion immediately.
    /// @return True if iris motor was stopped.
    virtual bool irisStop() = 0;

    // --- Sensor Processing & Enhancement ---

    /// @brief Controls mechanical Infrared Cut (ICR) filter engagement.
    /// @param[in] nightMode True to retract IR filter for night vision / IR illuminator pass.
    /// @return True if filter solenoid was actuated.
    virtual bool setDayNightIcr(bool nightMode) = 0;

    /// @brief Toggles optical or digital atmospheric defogging.
    /// @param[in] enable True to engage defogging processing.
    /// @return True if defog mode was updated.
    virtual bool setDefog(bool enable) = 0;

    /// @brief Toggles optical (OIS) or electronic (EIS) image stabilization.
    /// @param[in] enable True to engage image stabilizer.
    /// @return True if stabilizer was actuated.
    virtual bool setStabilizer(bool enable) = 0;

    // --- Thermal Sensor Enhancements (Optional / Gated) ---

    /// @brief Configures false-color palette / polarity for thermal sensors.
    /// @param[in] polarity Target palette (WhiteHot, BlackHot, FusionColor, Rainbow).
    /// @return True if applied, false if unsupported or daylight sensor.
    virtual bool setThermalPolarity(ThermalPolarity polarity)
    {
        (void)polarity;
        return false;
    }

    /// @brief Actuates mechanical calibration flag for Non-Uniformity Correction (NUC).
    /// @return True if calibration cycle was initiated, false if unsupported.
    virtual bool triggerNucCalibration()
    {
        return false;
    }

    // --- Telemetry Callback ---

    /// @brief Callback signature for periodic optical telemetry and field-of-view updates.
    using TelemetryCallback = std::function<void(const CameraTelemetry& telemetry)>;

    /// @brief Registers an observer callback for live optical telemetry updates.
    /// @param[in] cb Callable receiving CameraTelemetry records.
    virtual void registerTelemetryCallback(TelemetryCallback cb) = 0;

    /// @brief Retrieves the latest cached optical magnification, focus, and FOV telemetry synchronously.
    /// @return Current CameraTelemetry snapshot.
    [[nodiscard]] virtual CameraTelemetry currentTelemetry() const = 0;

    // --- Video Streaming Binding ---

    /// @brief Retrieves the video streaming connection URI for the designated profile.
    /// @param[in] profile Video stream profile (Primary, Secondary, Thermal, Snapshot).
    /// @return Stream URI string, or empty if unconfigured/unsupported.
    [[nodiscard]] virtual std::string videoStreamUri(VideoStreamProfile profile = VideoStreamProfile::Primary) const
    {
        (void)profile;
        return {};
    }

    /// @brief Configures or overrides the video streaming connection URI for the designated profile.
    /// @param[in] uri Connection URI (e.g. "rtsp://...", "sim://...", "v4l2://...").
    /// @param[in] profile Target profile.
    /// @return True if updated, false if unsupported.
    virtual bool setVideoStreamUri(const std::string& uri, VideoStreamProfile profile = VideoStreamProfile::Primary)
    {
        (void)uri;
        (void)profile;
        return false;
    }

    /// @brief Queries all video stream descriptors exposed by this camera payload.
    /// @return Vector of VideoStreamDescriptor records.
    [[nodiscard]] virtual std::vector<VideoStreamDescriptor> availableStreams() const
    {
        std::vector<VideoStreamDescriptor> list;
        const auto primary = videoStreamUri(VideoStreamProfile::Primary);
        if (!primary.empty()) {
            list.push_back(VideoStreamDescriptor {
                primary, VideoStreamProfile::Primary, deduceTransportProtocol(primary),
                1920, 1080, 30.0, "H264", true
            });
        }
        return list;
    }

    /// @brief Finds the stream descriptor for the requested profile.
    /// @param[in] profile Target profile.
    /// @return Descriptor if found, std::nullopt otherwise.
    [[nodiscard]] virtual std::optional<VideoStreamDescriptor> streamDescriptor(
        VideoStreamProfile profile = VideoStreamProfile::Primary) const
    {
        for (const auto& s : availableStreams()) {
            if (s.profile == profile) {
                return s;
            }
        }
        return std::nullopt;
    }
};

} // namespace PayloadHal
