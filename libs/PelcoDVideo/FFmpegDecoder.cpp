#include "FFmpegDecoder.h"

#if defined(PELCOD_HAS_FFMPEG)

#include <chrono>
#include <cstring>
#include <glog/logging.h>
#include <thread>

// Check if libavformat version is older than 59.4.100
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(59, 4, 100)
#define AV_FIND_BEST_STREAM(ctx, type, wanted, related, codec, flags)                                                  \
    av_find_best_stream(ctx, type, wanted, related, const_cast<AVCodec**>(codec), flags)
#else
#define AV_FIND_BEST_STREAM(ctx, type, wanted, related, codec, flags)                                                  \
    av_find_best_stream(ctx, type, wanted, related, const_cast<const AVCodec**>(codec), flags)
#endif

namespace PelcoD::Video {

FFmpegDecoder::FFmpegDecoder()
{
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
}

void FFmpegDecoder::close()
{
    m_isInitialized = false;
    m_swsCtx.reset();
    m_rawFrame.reset();
    m_packet.reset();
    m_codecCtx.reset();
    m_formatCtx.reset();

    if (m_hwDeviceCtx != nullptr) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }

    m_videoStreamIndex = -1;
    m_width = 0;
    m_height = 0;
    m_timestamp = 0.0;
    m_frameRate = 0.0;
    m_duration = 0.0;
    m_codecName.clear();
    m_rgbBuffer.clear();
    m_reachedEof = false;
    m_reconnectAttempts = 0;
}

bool FFmpegDecoder::initialize(std::string_view source, PixelFormat format, int threadCount, DeviceType device)
{
    close();
    const auto start = std::chrono::steady_clock::now();

    m_outputFormat = format;
    m_filePath = std::string(source);
    m_threadCount = threadCount;
    m_deviceType = device;
    m_actualDeviceType = DeviceType::CPU;

    AVFormatContext* formatCtxRaw = nullptr;
    AVDictionary* options = nullptr;

    const SourceType srcType = detectSourceType(m_filePath);
    const AVInputFormat* iformat = nullptr;
    std::string openPath = m_filePath;

    if (srcType == SourceType::Rtsp) {
        // Sub-second RTSP/network timeouts
        av_dict_set(&options, "stimeout", "1000000", 0);
        av_dict_set(&options, "rw_timeout", "1000000", 0);
        if (m_filePath.rfind("rtsp://", 0) == 0) {
            av_dict_set(&options, "rtsp_transport", "tcp", 0);
        }
    } else if (srcType == SourceType::Device) {
#ifdef _WIN32
        iformat = av_find_input_format("dshow");
        if (openPath.rfind("device:", 0) == 0 || openPath.rfind("device://", 0) == 0
            || openPath.rfind("dshow:", 0) == 0) {
            const auto pos = openPath.find_first_of(":/");
            const std::string rem = openPath.substr(openPath.find_first_not_of(":/", pos));
            if (rem.rfind("video=", 0) != 0) {
                openPath = "video=" + rem;
            } else {
                openPath = rem;
            }
        } else if (openPath.rfind("video:", 0) == 0) {
            openPath = "video=" + openPath.substr(6);
        }
#else
        iformat = av_find_input_format("v4l2");
#endif
    }

    int ret = avformat_open_input(&formatCtxRaw, openPath.c_str(), const_cast<AVInputFormat*>(iformat), &options);
    if (options != nullptr) {
        av_dict_free(&options);
    }
    if (ret < 0) {
        LOG(ERROR) << "FFmpegDecoder: Failed to open source: " << m_filePath << " (resolved: " << openPath
                   << ", error: " << ret << ")";
        return false;
    }
    m_formatCtx.reset(formatCtxRaw);

    ret = avformat_find_stream_info(m_formatCtx.get(), nullptr);
    if (ret < 0) {
        LOG(ERROR) << "FFmpegDecoder: Failed to find stream info for: " << m_filePath;
        close();
        return false;
    }

    const AVCodec* codec = nullptr;
    ret = AV_FIND_BEST_STREAM(m_formatCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (ret < 0 || codec == nullptr) {
        LOG(ERROR) << "FFmpegDecoder: Failed to find video stream in: " << m_filePath;
        close();
        return false;
    }
    m_videoStreamIndex = ret;

    AVCodecContext* codecCtxRaw = avcodec_alloc_context3(codec);
    if (codecCtxRaw == nullptr) {
        LOG(ERROR) << "FFmpegDecoder: Failed to allocate codec context";
        close();
        return false;
    }
    m_codecCtx.reset(codecCtxRaw);

    ret = avcodec_parameters_to_context(m_codecCtx.get(), m_formatCtx->streams[m_videoStreamIndex]->codecpar);
    if (ret < 0) {
        LOG(ERROR) << "FFmpegDecoder: Failed to copy parameters to codec context";
        close();
        return false;
    }

    m_codecCtx->thread_count = m_threadCount;
    m_codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    // Configure hardware acceleration
    if (m_deviceType != DeviceType::CPU) {
        AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
        if (m_deviceType == DeviceType::D3D11VA) {
            hwType = AV_HWDEVICE_TYPE_D3D11VA;
        } else if (m_deviceType == DeviceType::CUDA) {
            hwType = AV_HWDEVICE_TYPE_CUDA;
        } else if (m_deviceType == DeviceType::VAAPI) {
            hwType = AV_HWDEVICE_TYPE_VAAPI;
        }

        if (hwType != AV_HWDEVICE_TYPE_NONE) {
            int hwErr = av_hwdevice_ctx_create(&m_hwDeviceCtx, hwType, nullptr, nullptr, 0);
            if (hwErr >= 0) {
                m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
                m_actualDeviceType = m_deviceType;
            } else {
                m_hwDeviceCtx = nullptr;
                m_actualDeviceType = DeviceType::CPU;
            }
        }
    }

    ret = avcodec_open2(m_codecCtx.get(), codec, nullptr);
    if (ret < 0) {
        LOG(ERROR) << "FFmpegDecoder: Failed to open codec (error: " << ret << ")";
        close();
        return false;
    }

    m_width = m_codecCtx->width;
    m_height = m_codecCtx->height;
    m_codecName = codec->name ? codec->name : "unknown";

    const AVRational rFrameRate
        = av_guess_frame_rate(m_formatCtx.get(), m_formatCtx->streams[m_videoStreamIndex], nullptr);
    if (rFrameRate.den > 0 && rFrameRate.num > 0) {
        m_frameRate = av_q2d(rFrameRate);
    } else {
        m_frameRate = 30.0;
    }

    if (m_formatCtx->duration > 0) {
        m_duration = static_cast<double>(m_formatCtx->duration) / AV_TIME_BASE;
    } else {
        m_duration = 0.0; // Live stream
    }

    m_rawFrame.reset(av_frame_alloc());
    m_packet.reset(av_packet_alloc());
    if (!m_rawFrame || !m_packet) {
        LOG(ERROR) << "FFmpegDecoder: Failed to allocate AVFrame or AVPacket";
        close();
        return false;
    }

    const std::size_t rgbSize = static_cast<std::size_t>(m_width * m_height * 3);
    m_rgbBuffer.resize(rgbSize);

    if (m_tripleBufferingEnabled) {
        for (auto& slot : m_tripleBuffer.getSlots()) {
            slot.buffer.resize(rgbSize);
            slot.width = m_width;
            slot.height = m_height;
            slot.size = rgbSize;
            slot.format = m_outputFormat;
        }
    }

    m_isInitialized = true;
    const auto end = std::chrono::steady_clock::now();
    m_initTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return true;
}

bool FFmpegDecoder::reconnect()
{
    const std::string cachedPath = m_filePath;
    const PixelFormat cachedFormat = m_outputFormat;
    const int cachedThreads = m_threadCount;
    const DeviceType cachedDevice = m_deviceType;

    close();
    return initialize(cachedPath, cachedFormat, cachedThreads, cachedDevice);
}

bool FFmpegDecoder::decodeNextFrame()
{
    if (!m_isInitialized) {
        return false;
    }

    const auto decodeStart = std::chrono::steady_clock::now();

    while (true) {
        int ret = avcodec_receive_frame(m_codecCtx.get(), m_rawFrame.get());
        if (ret >= 0) {
            // Frame received!
            if (m_rawFrame->width != m_width || m_rawFrame->height != m_height) {
                m_width = m_rawFrame->width;
                m_height = m_rawFrame->height;
                m_swsCtx.reset();
                m_rgbBuffer.resize(static_cast<std::size_t>(m_width * m_height * 3));
            }

            const AVPixelFormat dstPixFmt
                = (m_outputFormat == PixelFormat::RGB24) ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_BGR24;

            if (!m_swsCtx) {
                m_swsCtx.reset(sws_getContext(m_rawFrame->width, m_rawFrame->height,
                    static_cast<AVPixelFormat>(m_rawFrame->format), m_width, m_height, dstPixFmt, SWS_BILINEAR, nullptr,
                    nullptr, nullptr));
                if (!m_swsCtx) {
                    LOG(ERROR) << "FFmpegDecoder: Failed to allocate SwsContext";
                    return false;
                }
            }

            std::uint8_t* dstData[4] = { m_rgbBuffer.data(), nullptr, nullptr, nullptr };
            int dstLinesize[4] = { m_width * 3, 0, 0, 0 };

            sws_scale(
                m_swsCtx.get(), m_rawFrame->data, m_rawFrame->linesize, 0, m_rawFrame->height, dstData, dstLinesize);

            // Apply registered frame processors in-place
            for (auto& processor : m_processors) {
                if (processor) {
                    processor->process(m_rgbBuffer.data(), m_width, m_height, m_outputFormat);
                }
            }

            // Compute presentation timestamp
            if (m_rawFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
                const AVRational tb = m_formatCtx->streams[m_videoStreamIndex]->time_base;
                m_timestamp = static_cast<double>(m_rawFrame->best_effort_timestamp) * av_q2d(tb);
            } else {
                m_timestamp = static_cast<double>(m_decodedFramesCount) / ((m_frameRate > 0.0) ? m_frameRate : 30.0);
            }

            ++m_decodedFramesCount;

            const auto decodeEnd = std::chrono::steady_clock::now();
            m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();
            m_totalDecodeTimeMs += m_lastDecodeTimeMs;

            if (m_tripleBufferingEnabled) {
                auto& slot = m_tripleBuffer.getWriteBuffer();
                const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
                if (slot.buffer.size() != frameBytes) {
                    slot.buffer.resize(frameBytes);
                }
                std::memcpy(slot.buffer.data(), m_rgbBuffer.data(), frameBytes);
                slot.width = m_width;
                slot.height = m_height;
                slot.size = frameBytes;
                slot.timestamp = m_timestamp;
                slot.decodeTimeMs = m_lastDecodeTimeMs;
                slot.format = m_outputFormat;
                m_tripleBuffer.publishWriteBuffer();
            }

            av_frame_unref(m_rawFrame.get());
            return true;
        }

        if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            int readRet = av_read_frame(m_formatCtx.get(), m_packet.get());
            if (readRet >= 0) {
                if (m_packet->stream_index == m_videoStreamIndex) {
                    int sendRet = avcodec_send_packet(m_codecCtx.get(), m_packet.get());
                    av_packet_unref(m_packet.get());
                    if (sendRet < 0) {
                        LOG(ERROR) << "FFmpegDecoder: Error sending packet to decoder";
                        return false;
                    }
                } else {
                    av_packet_unref(m_packet.get());
                }
            } else if (readRet == AVERROR_EOF) {
                m_reachedEof = true;
                avcodec_send_packet(m_codecCtx.get(), nullptr);
            } else {
                // Read failed on live stream -> auto-reconnect
                const bool isLive = (m_duration <= 0.0);
                if (isLive && m_reconnectAttempts < 3) {
                    LOG(WARNING) << "FFmpegDecoder: Live stream packet error. Reconnecting (" << m_reconnectAttempts + 1
                                 << "/3)...";
                    ++m_reconnectAttempts;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (reconnect()) {
                        m_reconnectAttempts = 0;
                        continue;
                    }
                }
                LOG(ERROR) << "FFmpegDecoder: Error reading packet (" << readRet << ")";
                return false;
            }
        } else if (ret == AVERROR_EOF) {
            return false;
        } else {
            LOG(ERROR) << "FFmpegDecoder: Error during decoding: " << ret;
            return false;
        }
    }
}

FrameInfo FFmpegDecoder::getRawFrameData() const
{
    FrameInfo info;
    info.data = m_rgbBuffer.data();
    info.width = m_width;
    info.height = m_height;
    info.size = m_rgbBuffer.size();
    info.timestamp = m_timestamp;
    info.decodeTimeMs = m_lastDecodeTimeMs;
    info.format = m_outputFormat;
    return info;
}

VideoMetadata FFmpegDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = m_duration;
    meta.codecName = m_codecName;
    meta.format = m_outputFormat;
    meta.deviceType = m_actualDeviceType;
    return meta;
}

DecoderPerformanceStats FFmpegDecoder::getPerformanceStats() const
{
    DecoderPerformanceStats stats;
    stats.initializationTimeMs = m_initTimeMs;
    stats.totalDecodedFrames = m_decodedFramesCount;
    stats.averageDecodeTimeMs
        = (m_decodedFramesCount > 0U) ? (m_totalDecodeTimeMs / static_cast<double>(m_decodedFramesCount)) : 0.0;
    return stats;
}

bool FFmpegDecoder::seek(double timeInSeconds)
{
    if (!m_isInitialized || m_duration <= 0.0) {
        return false;
    }

    const std::int64_t targetTs = static_cast<std::int64_t>(timeInSeconds * AV_TIME_BASE);
    const int ret = av_seek_frame(m_formatCtx.get(), -1, targetTs, AVSEEK_FLAG_BACKWARD);
    if (ret >= 0) {
        avcodec_flush_buffers(m_codecCtx.get());
        m_reachedEof = false;
        return true;
    }
    return false;
}

void FFmpegDecoder::enableTripleBuffering(bool enable)
{
    m_tripleBufferingEnabled = enable;
    if (enable && m_isInitialized) {
        const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
        for (auto& slot : m_tripleBuffer.getSlots()) {
            slot.buffer.resize(frameBytes);
            slot.width = m_width;
            slot.height = m_height;
            slot.size = frameBytes;
            slot.format = m_outputFormat;
        }
    }
}

bool FFmpegDecoder::isTripleBufferingEnabled() const
{
    return m_tripleBufferingEnabled;
}

void FFmpegDecoder::addFrameProcessor(std::shared_ptr<IFrameProcessor> processor)
{
    if (processor) {
        m_processors.push_back(processor);
    }
}

void FFmpegDecoder::clearFrameProcessors()
{
    m_processors.clear();
}

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FFMPEG
