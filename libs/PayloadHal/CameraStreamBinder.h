#pragma once

/// @file CameraStreamBinder.h
/// @brief Synchronizes decoded or ingested video frames with live camera optics and gimbal telemetry.

#include "GeoreferenceUtils.h"
#include "ICameraPayload.h"
#include "IDemProvider.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "Klv/KlvTypes.h"
#include "PayloadTypes.h"
#include "VideoStreamTypes.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @struct RawVideoFrame
/// @brief Ingested video frame container holding raw or decoded pixel buffers and timing metadata.
struct RawVideoFrame {
    std::vector<std::uint8_t> data {};                            ///< Pixel buffer (e.g. BGR24, RGB24, NV12)
    int width { 0 };                                              ///< Image width in pixels
    int height { 0 };                                             ///< Image height in pixels
    std::string pixelFormat { "BGR24" };                          ///< Pixel format identifier ("BGR24", "RGB24", "NV12", "YUV420P")
    std::uint64_t frameNumber { 0 };                              ///< Monotonically increasing frame sequence index
    std::chrono::system_clock::time_point timestamp {             ///< Frame capture / arrival timestamp
        std::chrono::system_clock::now()
    };
};

/// @struct SynchronizedVideoFrame
/// @brief Bound video frame container paired with exact optical FOV, gimbal orientation, and ground geometry.
struct SynchronizedVideoFrame {
    RawVideoFrame frame {};                                       ///< Video pixel data and metadata
    CameraTelemetry cameraTelemetry {};                           ///< Paired optical zoom, magnification, and FOV
    GimbalTelemetry gimbalTelemetry {};                           ///< Paired gimbal pan/tilt orientation and rates
    VideoStreamDescriptor streamDescriptor {};                    ///< Stream profile and transport connection info
    std::optional<Klv::GeoPoint3D> targetGroundIntersection {};  ///< Computed boresight ground intercept, if platform state set
    std::optional<Klv::FrustumCorners> frustumCorners {};         ///< Computed 4-corner ground projection polygon, if platform state set
};

/// @class CameraStreamBinder
/// @brief Ingestion and telemetry-pairing engine binding video frames to live sensor telemetry.
/// @details Maintains a sliding window of high-rate optical and gimbal telemetry to match frames
///          arriving with arbitrary latency, and computes georeferenced line-of-sight ground intersections.
class CameraStreamBinder {
public:
    /// @brief Callback invoked whenever an ingested frame is paired with synchronized telemetry.
    using FrameCallback = std::function<void(const SynchronizedVideoFrame& syncdFrame)>;

    /// @brief Constructs a stream binder attached to an IPayload composite station.
    /// @param[in] payload Shared pointer to composite payload station.
    /// @param[in] profile Stream profile to bind against (defaults to Primary).
    explicit CameraStreamBinder(std::shared_ptr<IPayload> payload,
        VideoStreamProfile profile = VideoStreamProfile::Primary);

    /// @brief Constructs a stream binder directly from camera and pan-tilt units.
    /// @param[in] camera Camera payload subsystem.
    /// @param[in] ptu Pan-Tilt gimbal subsystem (optional).
    /// @param[in] descriptor Video stream descriptor.
    CameraStreamBinder(std::shared_ptr<ICameraPayload> camera,
        std::shared_ptr<IPanTiltUnit> ptu,
        VideoStreamDescriptor descriptor);

    ~CameraStreamBinder();

    // Non-copyable, movable
    CameraStreamBinder(const CameraStreamBinder&) = delete;
    CameraStreamBinder& operator=(const CameraStreamBinder&) = delete;
    CameraStreamBinder(CameraStreamBinder&&) noexcept = default;
    CameraStreamBinder& operator=(CameraStreamBinder&&) noexcept = default;

    /// @brief Configures host platform geodetic coordinates and heading for ground projection.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] headingDeg Platform true compass heading in degrees [0.0, 360.0).
    void setPlatformState(const Klv::GeoPoint3D& platformPos, double headingDeg) noexcept;

    /// @brief Clears active host platform geodetic state.
    void clearPlatformState() noexcept;

    /// @brief Registers an asynchronous consumer callback for synchronized video frames.
    /// @param[in] cb Callback receiving SynchronizedVideoFrame.
    void setFrameCallback(FrameCallback cb);

    /// @brief Ingests an incoming raw video frame into the processing pipeline and fires callback.
    /// @param[in] frame Raw video frame to process.
    void ingestFrame(RawVideoFrame frame);

    /// @brief Synchronously pairs a raw video frame with the most accurate telemetry snapshot.
    /// @param[in] frame Input video frame with timestamp.
    /// @return Complete SynchronizedVideoFrame record.
    [[nodiscard]] SynchronizedVideoFrame bindFrame(RawVideoFrame frame) const;

    /// @brief Accesses the associated video stream descriptor.
    [[nodiscard]] const VideoStreamDescriptor& descriptor() const noexcept;

    /// @brief Accesses the underlying camera payload.
    [[nodiscard]] std::shared_ptr<ICameraPayload> camera() const noexcept;

    /// @brief Accesses the underlying pan-tilt unit.
    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept;

    /// @brief Accesses the composite payload, if constructed from one.
    [[nodiscard]] std::shared_ptr<IPayload> payload() const noexcept;

    /// @brief Explicitly records an optical telemetry sample into the synchronization buffer.
    /// @param[in] telem Optical camera telemetry record.
    void recordCameraTelemetry(const CameraTelemetry& telem);

    /// @brief Explicitly records a gimbal telemetry sample into the synchronization buffer.
    /// @param[in] telem Gimbal orientation and rate record.
    void recordGimbalTelemetry(const GimbalTelemetry& telem);

private:
    struct SharedBuffer;
    std::shared_ptr<SharedBuffer> m_buffer {};

    [[nodiscard]] CameraTelemetry findMatchingCameraTelemetry(
        std::chrono::system_clock::time_point timestamp) const;

    [[nodiscard]] GimbalTelemetry findMatchingGimbalTelemetry(
        std::chrono::system_clock::time_point timestamp) const;

    std::shared_ptr<IPayload> m_payload {};
    std::shared_ptr<ICameraPayload> m_camera {};
    std::shared_ptr<IPanTiltUnit> m_ptu {};
    VideoStreamDescriptor m_descriptor {};

    mutable std::mutex m_mutex;
    FrameCallback m_frameCallback {};

    // Platform state for georeferencing
    std::optional<Klv::GeoPoint3D> m_platformPos {};
    double m_platformHeadingDeg { 0.0 };
};

} // namespace PayloadHal
