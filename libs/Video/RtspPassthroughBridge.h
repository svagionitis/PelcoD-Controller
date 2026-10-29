#pragma once

/// @file RtspPassthroughBridge.h
/// @brief Direct Ingest RTSP/RTP bridge demuxing H.264/H.265 Access Units with zero transcoding.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <string_view>

#include "Klv/VideoTypes.h"
#include "RtspPassthroughTypes.h"

namespace Video {

/// @class RtspPassthroughBridge
/// @brief Thread-safe high-performance RTSP passthrough ingest client.
class RtspPassthroughBridge {
public:
    /// @brief Callback invoked when a complete video Access Unit is assembled.
    using AccessUnitCallback = std::function<void(const Klv::VideoAccessUnit& au)>;

    /// @brief Callback invoked when raw Annex B byte stream chunks are ready.
    using RawChunkCallback = std::function<void(const std::uint8_t* data, std::size_t size, std::uint64_t ptsUs)>;

    /// @brief Callback invoked on connection state transition.
    using StateCallback = std::function<void(RtspBridgeState state, std::string_view message)>;

    /// @brief Constructs bridge with designated configuration.
    /// @param[in] config Bridge configuration parameters.
    explicit RtspPassthroughBridge(const RtspPassthroughConfig& config);

    /// @brief Destructor. Ensures worker thread is stopped and socket closed.
    ~RtspPassthroughBridge();

    RtspPassthroughBridge(const RtspPassthroughBridge&) = delete;
    RtspPassthroughBridge& operator=(const RtspPassthroughBridge&) = delete;
    RtspPassthroughBridge(RtspPassthroughBridge&&) = delete;
    RtspPassthroughBridge& operator=(RtspPassthroughBridge&&) = delete;

    /// @brief Registers callback for assembled VideoAccessUnit instances.
    /// @param[in] callback Access unit observer.
    void setUnitCallback(AccessUnitCallback callback);

    /// @brief Registers callback for raw Annex B chunks (e.g. for pushVideoChunk).
    /// @param[in] callback Raw chunk observer.
    void setChunkCallback(RawChunkCallback callback);

    /// @brief Registers state transition observer.
    /// @param[in] callback State change observer.
    void setStateCallback(StateCallback callback);

    /// @brief Starts background network ingestion thread and connects to camera.
    /// @return True if worker thread started successfully.
    [[nodiscard]] bool start();

    /// @brief Stops network ingestion and sends RTSP TEARDOWN.
    void stop();

    /// @brief Queries whether bridge is actively running.
    /// @return True if background worker thread is active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Retrieves current runtime diagnostics.
    /// @return Snapshot of current operational statistics.
    [[nodiscard]] RtspPassthroughStats stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Video
