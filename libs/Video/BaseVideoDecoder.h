#pragma once

/// @file BaseVideoDecoder.h
/// @brief Abstract intermediate base class providing shared state and default implementations
///        for all concrete video decoder backends (FFmpeg, GStreamer, Mock).

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Video {

/// @class BaseVideoDecoder
/// @brief Abstract intermediate base that implements the seven non-backend-specific
///        IVideoDecoder methods and owns all shared member state.
/// @details Concrete decoders (FFmpegDecoder, GStreamerDecoder, MockVideoDecoder) inherit
///          from this class instead of IVideoDecoder directly. They are still required to
///          implement initialize(), decodeNextFrame(), seek(), and close().
///          During initialize() they must call initTripleBufferSlots() if triple buffering
///          is enabled. During decodeNextFrame() they must call dispatchFrameProcessors()
///          and publishToTripleBuffer() after pixel conversion.
/// @note All public methods of this class are thread-safe with respect to the frame
///       processor list. The frame buffer fields are not independently guarded; callers
///       are expected to synchronise access at the application level.
class BaseVideoDecoder : public IVideoDecoder {
public:
    /// @brief Virtual destructor ensuring correct polymorphic cleanup.
    ~BaseVideoDecoder() override = default;

    // -----------------------------------------------------------------------
    // Concrete implementations — NOT overridden by concrete decoders
    // -----------------------------------------------------------------------

    /// @brief Assembles a FrameInfo snapshot from the current shared frame state.
    /// @return FrameInfo containing buffer pointer, dimensions, timestamp, and format.
    [[nodiscard]] FrameInfo getRawFrameData() const override;

    /// @brief Assembles a VideoMetadata snapshot from the current shared stream state.
    /// @return VideoMetadata with codec name, frame rate, duration, and device type.
    [[nodiscard]] VideoMetadata getVideoMetadata() const override;

    /// @brief Assembles a DecoderPerformanceStats snapshot from the current timing state.
    /// @return DecoderPerformanceStats with init time, frame count, and rolling average.
    [[nodiscard]] DecoderPerformanceStats getPerformanceStats() const override;

    /// @brief Enables or disables lock-free atomic triple buffering.
    /// @details When enabling on an already-initialized decoder, the three buffer slots
    ///          are immediately pre-allocated to the current frame dimensions.
    /// @param[in] enable True to enable triple buffering.
    void enableTripleBuffering(bool enable) override;

    /// @brief Checks whether atomic triple buffering is currently active.
    /// @return True if triple buffering is enabled.
    [[nodiscard]] bool isTripleBufferingEnabled() const override;

    /// @brief Registers a post-processing frame processor.
    /// @param[in] processor Shared pointer to a frame processor implementation.
    /// @note Thread-safe. No-op if processor is nullptr.
    void addFrameProcessor(std::shared_ptr<IFrameProcessor> processor) override;

    /// @brief Removes all registered frame processors.
    /// @note Thread-safe.
    void clearFrameProcessors() override;

protected:
    // -----------------------------------------------------------------------
    // Helpers for concrete decoders to call during their operation
    // -----------------------------------------------------------------------

    /// @brief Pre-allocates the three triple-buffer slots to the given frame dimensions.
    /// @details Should be called by concrete decoders at the end of a successful
    ///          initialize() when triple buffering is active.
    /// @param[in] w Frame width in pixels.
    /// @param[in] h Frame height in pixels.
    /// @param[in] fmt Pixel format of the frame buffer.
    void initTripleBufferSlots(int w, int h, PixelFormat fmt);

    /// @brief Dispatches the decoded frame buffer through all registered processors.
    /// @details Takes a snapshot of the processor list under the mutex before iterating,
    ///          so processors added concurrently are not invoked for this frame.
    /// @param[in,out] data Pointer to the raw packed frame buffer.
    /// @param[in] w Frame width in pixels.
    /// @param[in] h Frame height in pixels.
    /// @param[in] fmt Pixel format of the frame buffer.
    void dispatchFrameProcessors(std::uint8_t* data, int w, int h, PixelFormat fmt);

    /// @brief Copies the current frame into the write slot of the triple buffer and publishes it.
    /// @details No-op if triple buffering is disabled. Resizes the slot buffer if the frame
    ///          dimensions have changed since the last publish.
    /// @param[in] src Pointer to the source pixel data.
    /// @param[in] w Frame width in pixels.
    /// @param[in] h Frame height in pixels.
    /// @param[in] bytes Number of bytes to copy (w * h * 3).
    /// @param[in] ts Presentation timestamp in seconds.
    /// @param[in] decodeMs Frame decode duration in milliseconds.
    /// @param[in] fmt Pixel format of the frame buffer.
    void publishToTripleBuffer(
        const std::uint8_t* src, int w, int h, std::size_t bytes, double ts, double decodeMs, PixelFormat fmt);

    /// @brief Re-initializes the decoder by caching parameters, closing, and re-opening.
    /// @details Shared implementation for FFmpegDecoder and GStreamerDecoder live-stream
    ///          reconnection. Relies on the virtual initialize() being overridden.
    /// @return True if re-initialization succeeded.
    bool reconnect();

    // -----------------------------------------------------------------------
    // Shared state — written by concrete decoders, read by base implementations
    // -----------------------------------------------------------------------

    /// @brief Canonical output pixel buffer (the decoded, colour-converted frame).
    std::vector<std::uint8_t> m_frameBuffer;

    int m_width { 0 }; ///< Current frame width in pixels.
    int m_height { 0 }; ///< Current frame height in pixels.
    double m_timestamp { 0.0 }; ///< Presentation timestamp of the last decoded frame (seconds).
    double m_frameRate { 0.0 }; ///< Nominal stream frame rate (fps).
    double m_duration { 0.0 }; ///< Total stream duration in seconds (0.0 for live).
    std::string m_codecName; ///< Codec identifier (e.g. "h264", "GStreamer", "RAW_MOCK").
    PixelFormat m_outputFormat { PixelFormat::RGB24 }; ///< Output pixel format.

    /// @brief The hardware device type actually used after initialization.
    /// @details For FFmpegDecoder this may differ from the requested device when hardware
    ///          acceleration fallback to CPU occurs. For other backends, set to the
    ///          requested device type.
    DeviceType m_reportedDeviceType { DeviceType::CPU };

    double m_initTimeMs { 0.0 }; ///< Stream open latency in milliseconds.
    double m_lastDecodeTimeMs { 0.0 }; ///< Last frame decode duration in milliseconds.
    double m_totalDecodeTimeMs { 0.0 }; ///< Cumulative decode time for averaging.
    std::uint64_t m_decodedFramesCount { 0U }; ///< Total successfully decoded frames.

    // Reconnect helpers (used by FFmpeg & GStreamer)
    std::string m_filePath; ///< Cached source URI / path for reconnection.
    int m_reconnectAttempts { 0 }; ///< Number of consecutive reconnect attempts.
    int m_threadCount { 0 }; ///< Cached thread count for reconnection.
    DeviceType m_deviceType { DeviceType::CPU }; ///< Requested hardware device type.

    bool m_isInitialized { false }; ///< True if the decoder is open and ready.

    bool m_tripleBufferingEnabled { false }; ///< Whether triple buffering is active.
    mutable AtomicTripleBuffer<FrameBufferSlot> m_tripleBuffer; ///< Lock-free triple buffer.

private:
    mutable std::mutex m_processorMutex;
    std::vector<std::shared_ptr<IFrameProcessor>> m_processors;
};

} // namespace Video
