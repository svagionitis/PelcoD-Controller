#pragma once

/// @file FFmpegDecoder.h
/// @brief Native FFmpeg video decoding backend for RTSP and media streams.

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"

#include <memory>
#include <string>
#include <vector>

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
class FFmpegDecoder : public IVideoDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    bool initialize(std::string_view source,
                    PixelFormat format = PixelFormat::RGB24,
                    int threadCount = 0,
                    DeviceType device = DeviceType::CPU) override;

    bool decodeNextFrame() override;

    [[nodiscard]] FrameInfo getRawFrameData() const override;

    [[nodiscard]] VideoMetadata getVideoMetadata() const override;

    [[nodiscard]] DecoderPerformanceStats getPerformanceStats() const override;

    bool seek(double timeInSeconds) override;

    void enableTripleBuffering(bool enable) override;

    [[nodiscard]] bool isTripleBufferingEnabled() const override;

    void close() override;

private:
    bool reconnect();

    AVFormatContextPtr m_formatCtx;
    AVCodecContextPtr m_codecCtx;
    AVFramePtr m_rawFrame;
    AVPacketPtr m_packet;
    SwsContextPtr m_swsCtx;

    int m_videoStreamIndex { -1 };
    std::vector<std::uint8_t> m_rgbBuffer;
    bool m_tripleBufferingEnabled { false };
    mutable AtomicTripleBuffer<FrameBufferSlot> m_tripleBuffer;

    int m_width { 0 };
    int m_height { 0 };
    double m_timestamp { 0.0 };
    double m_frameRate { 0.0 };
    double m_duration { 0.0 };
    std::string m_codecName;
    PixelFormat m_outputFormat { PixelFormat::RGB24 };

    double m_initTimeMs { 0.0 };
    double m_lastDecodeTimeMs { 0.0 };
    double m_totalDecodeTimeMs { 0.0 };
    std::uint64_t m_decodedFramesCount { 0U };

    std::string m_filePath;
    int m_reconnectAttempts { 0 };
    int m_threadCount { 0 };
    DeviceType m_deviceType { DeviceType::CPU };
    DeviceType m_actualDeviceType { DeviceType::CPU };
    AVBufferRef* m_hwDeviceCtx { nullptr };

    bool m_isInitialized { false };
    bool m_reachedEof { false };
};

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FFMPEG
