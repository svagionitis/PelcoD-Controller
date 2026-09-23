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

namespace Video {

static int ffmpegInterruptCallback(void* opaque)
{
    if (opaque == nullptr) {
        return 0;
    }
    auto* ctx = static_cast<FFmpegInterruptContext*>(opaque);
    if (ctx->interrupted.load(std::memory_order_relaxed)) {
        return 1;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->lastActivity).count();
    if (elapsed > ctx->timeoutMs) {
        LOG(WARNING) << "FFmpegDecoder: Network I/O timed out after " << elapsed
                     << "ms of inactivity. Aborting operation.";
        return 1;
    }
    return 0;
}

FFmpegDecoder::FFmpegDecoder()
{
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
}

void FFmpegDecoder::close()
{
    m_interruptCtx.interrupted.store(true, std::memory_order_relaxed);
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
    m_frameBuffer.clear();
    m_reachedEof = false;
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

    const SourceType srcType = detectSourceType(m_filePath);
    m_isLiveStream = (srcType == SourceType::Rtsp || srcType == SourceType::Device);

    m_interruptCtx.interrupted.store(false, std::memory_order_relaxed);
    m_interruptCtx.lastActivity = std::chrono::steady_clock::now();
    m_interruptCtx.timeoutMs = (srcType == SourceType::Rtsp) ? 8000 : 10000;

    AVFormatContext* formatCtxRaw = avformat_alloc_context();
    if (formatCtxRaw != nullptr) {
        formatCtxRaw->interrupt_callback.callback = ffmpegInterruptCallback;
        formatCtxRaw->interrupt_callback.opaque = &m_interruptCtx;
    }

    AVDictionary* options = nullptr;
    const AVInputFormat* iformat = nullptr;
    std::string openPath = m_filePath;

    if (srcType == SourceType::Rtsp) {
        // RTSP network socket timeout in microseconds (5 seconds)
        av_dict_set(&options, "stimeout", "5000000", 0);
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

    m_interruptCtx.lastActivity = std::chrono::steady_clock::now();
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
    m_reportedDeviceType = m_actualDeviceType;

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
    m_frameBuffer.resize(rgbSize);

    if (m_tripleBufferingEnabled) {
        initTripleBufferSlots(m_width, m_height, m_outputFormat);
    }

    m_isInitialized = true;
    m_interruptCtx.timeoutMs = 5000;
    const auto end = std::chrono::steady_clock::now();
    m_initTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return true;
}

bool FFmpegDecoder::decodeNextFrame()
{
    if (!m_isInitialized) {
        const bool isLive = (m_duration <= 0.0 || m_isLiveStream || detectSourceType(m_filePath) == SourceType::Rtsp);
        if (m_autoReconnect && isLive && !m_filePath.empty()) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastReconnectAttempt).count();
            if (elapsedMs >= m_reconnectIntervalMs) {
                LOG(INFO) << "FFmpegDecoder: Attempting background live stream reconnection ("
                          << m_reconnectAttempts + 1 << ")...";
                ++m_reconnectAttempts;
                if (reconnect()) {
                    LOG(INFO) << "FFmpegDecoder: Stream successfully reconnected! Resuming playback.";
                    m_reconnectAttempts = 0;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    const auto decodeStart = std::chrono::steady_clock::now();

    while (true) {
        int ret = avcodec_receive_frame(m_codecCtx.get(), m_rawFrame.get());
        if (ret >= 0) {
            m_interruptCtx.lastActivity = std::chrono::steady_clock::now();

            // Frame received — handle resolution change
            if (m_rawFrame->width != m_width || m_rawFrame->height != m_height) {
                m_width = m_rawFrame->width;
                m_height = m_rawFrame->height;
                m_swsCtx.reset();
                m_frameBuffer.resize(static_cast<std::size_t>(m_width * m_height * 3));
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

            std::uint8_t* dstData[4] = { m_frameBuffer.data(), nullptr, nullptr, nullptr };
            int dstLinesize[4] = { m_width * 3, 0, 0, 0 };

            sws_scale(
                m_swsCtx.get(), m_rawFrame->data, m_rawFrame->linesize, 0, m_rawFrame->height, dstData, dstLinesize);

            // Apply registered frame processors in-place
            dispatchFrameProcessors(m_frameBuffer.data(), m_width, m_height, m_outputFormat);

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

            const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
            publishToTripleBuffer(
                m_frameBuffer.data(), m_width, m_height, frameBytes, m_timestamp, m_lastDecodeTimeMs, m_outputFormat);

            av_frame_unref(m_rawFrame.get());
            return true;
        }

        if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            m_interruptCtx.lastActivity = std::chrono::steady_clock::now();
            int readRet = av_read_frame(m_formatCtx.get(), m_packet.get());
            if (readRet >= 0) {
                m_interruptCtx.lastActivity = std::chrono::steady_clock::now();
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
                const bool isLive
                    = (m_duration <= 0.0 || m_isLiveStream || detectSourceType(m_filePath) == SourceType::Rtsp);
                if (isLive && m_autoReconnect) {
                    LOG(WARNING) << "FFmpegDecoder: Unexpected EOF on live stream. Triggering reconnection...";
                    m_isInitialized = false;
                    if (reconnect()) {
                        m_reconnectAttempts = 0;
                        continue;
                    }
                    return false;
                }
                m_reachedEof = true;
                avcodec_send_packet(m_codecCtx.get(), nullptr);
            } else {
                // Read failed on live stream -> auto-reconnect
                const bool isLive
                    = (m_duration <= 0.0 || m_isLiveStream || detectSourceType(m_filePath) == SourceType::Rtsp);
                if (isLive && m_autoReconnect) {
                    LOG(WARNING) << "FFmpegDecoder: Live stream packet error (" << readRet << "). Reconnecting...";
                    ++m_reconnectAttempts;
                    m_isInitialized = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    if (reconnect()) {
                        m_reconnectAttempts = 0;
                        continue;
                    }
                    return false;
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

} // namespace Video

#endif // PELCOD_HAS_FFMPEG
