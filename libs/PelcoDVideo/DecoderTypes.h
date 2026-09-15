#pragma once

/// @file DecoderTypes.h
/// @brief Core data structures and enumerations for the PelcoD video decoding engine.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD::Video {

/// @enum BackendType
/// @brief Identifies the multimedia decoding backend engine.
enum class BackendType {
    FFmpeg,    ///< Native FFmpeg demuxer and codec pipeline (avformat/avcodec/swscale)
    GStreamer, ///< GStreamer pipeline using playbin / appsink
    Mock       ///< Synthetic test pattern and frame generator for testing/offline use
};

/// @enum PixelFormat
/// @brief Pixel color layout of the decoded frame buffer.
enum class PixelFormat {
    RGB24, ///< 24-bit packed RGB (3 bytes per pixel: Red, Green, Blue)
    BGR24  ///< 24-bit packed BGR (3 bytes per pixel: Blue, Green, Red)
};

/// @enum DeviceType
/// @brief Hardware device acceleration type for video decoding.
enum class DeviceType {
    CPU,    ///< Software CPU decoding
    D3D11VA,///< Direct3D11 Video Acceleration (Windows)
    CUDA,   ///< NVIDIA NVDEC / CUDA acceleration
    VAAPI   ///< VA-API acceleration (Linux)
};

/// @struct FrameInfo
/// @brief Snapshot of raw frame buffer data and presentation metadata.
struct FrameInfo {
    const std::uint8_t* data { nullptr }; ///< Pointer to raw packed RGB24/BGR24 buffer
    int width { 0 };                     ///< Frame width in pixels
    int height { 0 };                    ///< Frame height in pixels
    std::size_t size { 0U };             ///< Frame buffer size in bytes (width * height * 3)
    double timestamp { 0.0 };            ///< Presentation timestamp (PTS) in seconds
    double decodeTimeMs { 0.0 };         ///< Active frame decode duration in milliseconds
    PixelFormat format { PixelFormat::RGB24 }; ///< Pixel format of this frame
};

/// @struct FrameBufferSlot
/// @brief Pre-allocated memory slot used in lock-free triple buffer exchanges.
struct FrameBufferSlot {
    std::vector<std::uint8_t> buffer;    ///< Pre-allocated pixel buffer storage
    int width { 0 };                     ///< Frame width in pixels
    int height { 0 };                    ///< Frame height in pixels
    std::size_t size { 0U };             ///< Frame active size in bytes
    double timestamp { 0.0 };            ///< Presentation timestamp (PTS) in seconds
    double decodeTimeMs { 0.0 };         ///< Active decode duration in milliseconds
    PixelFormat format { PixelFormat::RGB24 }; ///< Pixel format of this slot
};

/// @struct VideoMetadata
/// @brief Static properties and stream metrics of a loaded video source.
struct VideoMetadata {
    int width { 0 };                     ///< Stream width in pixels
    int height { 0 };                    ///< Stream height in pixels
    double frameRate { 0.0 };            ///< Nominal framerate in frames per second (FPS)
    double duration { 0.0 };             ///< Total duration in seconds (0.0 for live streams)
    std::string codecName;               ///< Codec name (e.g. "h264", "hevc", "mjpeg")
    PixelFormat format { PixelFormat::RGB24 }; ///< Output pixel format
    DeviceType deviceType { DeviceType::CPU };  ///< Hardware acceleration device in use
};

/// @struct DecoderPerformanceStats
/// @brief Performance statistics and latency measurements of the active decoder.
struct DecoderPerformanceStats {
    double initializationTimeMs { 0.0 }; ///< Initial stream connection latency in milliseconds
    double averageDecodeTimeMs { 0.0 };  ///< Rolling average frame decode time in milliseconds
    std::uint64_t totalDecodedFrames { 0U }; ///< Cumulative count of successfully decoded frames
};

/// @enum StreamState
/// @brief Connection and playback lifecycle states of a video stream worker.
enum class StreamState {
    Disconnected, ///< Decoder is closed and idle
    Connecting,   ///< Establishing network socket / RTSP handshake
    Streaming,    ///< Actively decoding and displaying video frames
    Paused,       ///< Stream paused / suspended
    Reconnecting, ///< Socket drop or packet timeout; attempting auto-recovery
    Error         ///< Fatal stream error occurred
};

/// @enum SourceType
/// @brief Categorizes the origin and protocol of a video media source.
enum class SourceType {
    Rtsp,        ///< RTSP, RTMP, HTTP, UDP, or TCP network live feed
    File,        ///< Local multimedia container file (.mp4, .mkv, .avi, .mov, etc.)
    Device,      ///< Hardware capture device (DirectShow webcam, HDMI card, V4L2 device)
    MockPattern  ///< In-process synthetic test pattern generator
};

/// @struct VideoDeviceInfo
/// @brief Descriptor for an enumerated hardware video capture device.
struct VideoDeviceInfo {
    std::string name;        ///< User-friendly device name (e.g. "Integrated Camera")
    std::string path;        ///< Connection path/identifier (e.g. "video=Integrated Camera" or "/dev/video0")
    std::string description; ///< Bus, driver, or hardware description
};

/// @brief Detects the categorical source type from a URI, file path, or device identifier.
/// @param[in] source Source URI, local filesystem path, or hardware device identifier.
/// @return Deduced SourceType.
[[nodiscard]] inline SourceType detectSourceType(std::string_view source) noexcept
{
    if (source.rfind("mock://", 0) == 0) {
        return SourceType::MockPattern;
    }
    if (source.rfind("rtsp://", 0) == 0 || source.rfind("rtmp://", 0) == 0 ||
        source.rfind("http://", 0) == 0 || source.rfind("https://", 0) == 0 ||
        source.rfind("udp://", 0) == 0 || source.rfind("tcp://", 0) == 0) {
        return SourceType::Rtsp;
    }
    if (source.rfind("video=", 0) == 0 || source.rfind("video:", 0) == 0 ||
        source.rfind("device:", 0) == 0 || source.rfind("device://", 0) == 0 ||
        source.rfind("dshow:", 0) == 0 || source.rfind("/dev/video", 0) == 0) {
        return SourceType::Device;
    }
    return SourceType::File;
}

} // namespace PelcoD::Video
