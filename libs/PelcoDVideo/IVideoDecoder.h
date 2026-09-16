#pragma once

/// @file IVideoDecoder.h
/// @brief Abstract interface for video decoding backends (FFmpeg, GStreamer, Mock).

#include "DecoderTypes.h"

#include <memory>
#include <string_view>
#include <vector>

namespace PelcoD::Video {

/// @class IVideoDecoder
/// @brief Abstract interface defining the lifecycle and operations of a video decoder.
class IVideoDecoder {
public:
    /// @brief Virtual destructor ensuring clean polymorphic cleanup.
    virtual ~IVideoDecoder() = default;

    /// @brief Initializes the video decoder for a given URI or device path.
    /// @param[in] source Stream URI (e.g. "rtsp://...", file path, or "mock://test").
    /// @param[in] format Requested output pixel layout (defaults to RGB24).
    /// @param[in] threadCount Number of decoding worker threads (0 for auto).
    /// @param[in] device Hardware acceleration device type (defaults to CPU).
    /// @return True if initialized successfully and ready to decode.
    virtual bool initialize(std::string_view source, PixelFormat format = PixelFormat::RGB24, int threadCount = 0,
        DeviceType device = DeviceType::CPU)
        = 0;

    /// @brief Decodes the next available frame in the video stream.
    /// @return True if a new frame was successfully decoded and cached.
    virtual bool decodeNextFrame() = 0;

    /// @brief Retrieves the raw frame buffer pointer and metadata of the current decoded frame.
    /// @return FrameInfo containing buffer address, width, height, and timestamp.
    [[nodiscard]] virtual FrameInfo getRawFrameData() const = 0;

    /// @brief Retrieves static video stream metadata.
    /// @return VideoMetadata structure.
    [[nodiscard]] virtual VideoMetadata getVideoMetadata() const = 0;

    /// @brief Retrieves runtime performance and latency metrics.
    /// @return DecoderPerformanceStats structure.
    [[nodiscard]] virtual DecoderPerformanceStats getPerformanceStats() const = 0;

    /// @brief Seeks to a specific timestamp in the stream (file streams only).
    /// @param[in] timeInSeconds Target presentation timestamp.
    /// @return True if seek succeeded.
    virtual bool seek(double timeInSeconds) = 0;

    /// @brief Enables or disables lock-free atomic triple buffering.
    /// @param[in] enable True to enable triple buffering.
    virtual void enableTripleBuffering(bool enable) = 0;

    /// @brief Checks whether atomic triple buffering is active.
    /// @return True if active.
    [[nodiscard]] virtual bool isTripleBufferingEnabled() const = 0;

    /// @brief Adds a post-processing frame processor to the decoder.
    /// @param[in] processor Shared pointer to frame processor implementation.
    virtual void addFrameProcessor(std::shared_ptr<IFrameProcessor> processor) = 0;

    /// @brief Clears all registered post-processing frame processors.
    virtual void clearFrameProcessors() = 0;

    /// @brief Closes the video stream and releases decoder resources.
    virtual void close() = 0;
};

} // namespace PelcoD::Video
