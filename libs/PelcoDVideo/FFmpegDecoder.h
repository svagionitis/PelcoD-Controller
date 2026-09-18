#pragma once

/// @file FFmpegDecoder.h
/// @brief Native FFmpeg video decoding backend for RTSP and media streams.

#include "BaseVideoDecoder.h"

#include <memory>
#include <string>

#if defined(PELCOD_HAS_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace PelcoD::Video {

/// @struct AVFormatContextDeleter
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ptr) const
    {
        if (ptr != nullptr) {
            avformat_close_input(&ptr);
        }
    }
};
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

/// @struct AVCodecContextDeleter
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ptr) const
    {
        if (ptr != nullptr) {
            avcodec_free_context(&ptr);
        }
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

/// @struct AVFrameDeleter
struct AVFrameDeleter {
    void operator()(AVFrame* ptr) const
    {
        if (ptr != nullptr) {
            av_frame_free(&ptr);
        }
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

/// @struct AVPacketDeleter
struct AVPacketDeleter {
    void operator()(AVPacket* ptr) const
    {
        if (ptr != nullptr) {
            av_packet_free(&ptr);
        }
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/// @struct SwsContextDeleter
struct SwsContextDeleter {
    void operator()(SwsContext* ptr) const
    {
        if (ptr != nullptr) {
            sws_freeContext(ptr);
        }
    }
};
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

/// @class FFmpegDecoder
/// @brief Concrete implementation of IVideoDecoder using FFmpeg APIs.
/// @details Inherits shared state and non-backend-specific method implementations
///          from BaseVideoDecoder. Only initialize(), decodeNextFrame(), seek(), and
///          close() are implemented here.
class FFmpegDecoder : public BaseVideoDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    bool initialize(std::string_view source, PixelFormat format = PixelFormat::RGB24, int threadCount = 0,
        DeviceType device = DeviceType::CPU) override;

    bool decodeNextFrame() override;

    bool seek(double timeInSeconds) override;

    void close() override;

private:
    AVFormatContextPtr m_formatCtx;
    AVCodecContextPtr m_codecCtx;
    AVFramePtr m_rawFrame;
    AVPacketPtr m_packet;
    SwsContextPtr m_swsCtx;

    int m_videoStreamIndex { -1 };
    bool m_reachedEof { false };

    DeviceType m_actualDeviceType { DeviceType::CPU };
    AVBufferRef* m_hwDeviceCtx { nullptr };
};

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FFMPEG
