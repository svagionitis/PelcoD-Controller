#pragma once

/// @file VideoStreamTypes.h
/// @brief Strong types and descriptors for camera payload video stream binding.

#include <cstdint>
#include <string>

namespace PayloadHal {

/// @enum VideoStreamProfile
/// @brief Operational profile role for a video stream.
enum class VideoStreamProfile : std::uint8_t {
    Primary,   ///< Main high-resolution live stream (e.g., 4K / 1080p60 for display & recording)
    Secondary, ///< Low-latency / sub-stream (e.g., 720p / D1 for computer vision / AI auto-tracking)
    Thermal,   ///< Dedicated thermal infrared stream
    Snapshot   ///< Still picture JPEG / PNG snapshot URI
};

/// @enum StreamTransportProtocol
/// @brief Transport protocol mechanism.
enum class StreamTransportProtocol : std::uint8_t {
    Rtsp,       ///< Real-Time Streaming Protocol (rtsp://)
    V4L2,       ///< Video4Linux2 local device (v4l2:///dev/video0)
    DirectShow, ///< Windows DirectShow device (dshow://...)
    UdpMpegTs,  ///< UDP / RTP MPEG-TS unicast/multicast (udp://@host:port)
    WebRtc,     ///< WebRTC WHEP / WHIP endpoint
    Simulated   ///< Synthetic procedural test stream (sim://...)
};

/// @struct VideoStreamDescriptor
/// @brief Technical specifications and connection parameters for a video stream.
struct VideoStreamDescriptor {
    std::string uri {};                                           ///< Full connection URI
    VideoStreamProfile profile { VideoStreamProfile::Primary };    ///< Profile designation
    StreamTransportProtocol transport { StreamTransportProtocol::Rtsp }; ///< Protocol
    int width { 1920 };                                           ///< Nominal video width in pixels
    int height { 1080 };                                          ///< Nominal video height in pixels
    double framerateFps { 30.0 };                                 ///< Nominal framerate in frames/sec
    std::string encoding { "H264" };                              ///< Codec format: H264, H265, MJPEG, RAW
    bool isDefault { true };                                      ///< True if default stream for profile
};

/// @brief Deduces the transport protocol enum from a stream URI prefix.
/// @param[in] uri Connection URI string.
/// @return Inferred StreamTransportProtocol.
inline StreamTransportProtocol deduceTransportProtocol(const std::string& uri) noexcept
{
    if (uri.rfind("rtsp://", 0) == 0 || uri.rfind("rtsps://", 0) == 0) {
        return StreamTransportProtocol::Rtsp;
    }
    if (uri.rfind("sim://", 0) == 0) {
        return StreamTransportProtocol::Simulated;
    }
    if (uri.rfind("v4l2://", 0) == 0 || uri.rfind("/dev/video", 0) == 0) {
        return StreamTransportProtocol::V4L2;
    }
    if (uri.rfind("dshow://", 0) == 0) {
        return StreamTransportProtocol::DirectShow;
    }
    if (uri.rfind("udp://", 0) == 0 || uri.rfind("rtp://", 0) == 0) {
        return StreamTransportProtocol::UdpMpegTs;
    }
    if (uri.rfind("webrtc://", 0) == 0 || uri.rfind("whep://", 0) == 0) {
        return StreamTransportProtocol::WebRtc;
    }
    return StreamTransportProtocol::Rtsp;
}

/// @brief Converts VideoStreamProfile to human-readable string.
/// @param[in] profile Profile enum.
/// @return String representation.
inline const char* videoStreamProfileToString(VideoStreamProfile profile) noexcept
{
    switch (profile) {
    case VideoStreamProfile::Primary:
        return "Primary";
    case VideoStreamProfile::Secondary:
        return "Secondary";
    case VideoStreamProfile::Thermal:
        return "Thermal";
    case VideoStreamProfile::Snapshot:
        return "Snapshot";
    }
    return "Unknown";
}

/// @brief Converts StreamTransportProtocol to human-readable string.
/// @param[in] protocol Protocol enum.
/// @return String representation.
inline const char* streamTransportProtocolToString(StreamTransportProtocol protocol) noexcept
{
    switch (protocol) {
    case StreamTransportProtocol::Rtsp:
        return "RTSP";
    case StreamTransportProtocol::V4L2:
        return "V4L2";
    case StreamTransportProtocol::DirectShow:
        return "DirectShow";
    case StreamTransportProtocol::UdpMpegTs:
        return "UDP/MPEG-TS";
    case StreamTransportProtocol::WebRtc:
        return "WebRTC";
    case StreamTransportProtocol::Simulated:
        return "Simulated";
    }
    return "Unknown";
}

} // namespace PayloadHal
