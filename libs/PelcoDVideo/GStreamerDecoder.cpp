#include "GStreamerDecoder.h"

#if defined(PELCOD_HAS_GSTREAMER)

#include <algorithm>
#include <chrono>
#include <cstring>
#include <glog/logging.h>
#include <gst/video/video.h>
#include <mutex>
#include <thread>

namespace PelcoD::Video {

static std::once_flag s_gstInitOnce;

void GStreamerDecoder::initGStreamer()
{
    std::call_once(s_gstInitOnce, []() {
        gst_init(nullptr, nullptr);
    });
}

GStreamerDecoder::GStreamerDecoder()
{
    initGStreamer();
}

GStreamerDecoder::~GStreamerDecoder()
{
    close();
}

void GStreamerDecoder::close()
{
    m_isInitialized = false;
    if (m_pipeline) {
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_pipeline.reset();
    }
    m_sink = nullptr;

    m_width = 0;
    m_height = 0;
    m_timestamp = 0.0;
    m_frameRate = 0.0;
    m_duration = 0.0;
    m_rgbBuffer.clear();
    m_reachedEof = false;
    m_reconnectAttempts = 0;
}

std::string GStreamerDecoder::getBusErrorMessage()
{
    if (!m_pipeline) {
        return {};
    }
    GstBus* bus = gst_element_get_bus(m_pipeline.get());
    if (!bus) {
        return {};
    }
    std::string errMsg;
    GstMessage* msg = gst_bus_pop_filtered(bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));
    if (msg != nullptr) {
        GError* err = nullptr;
        gchar* debugInfo = nullptr;
        gst_message_parse_error(msg, &err, &debugInfo);
        if (err != nullptr) {
            errMsg = err->message;
            g_error_free(err);
        }
        if (debugInfo != nullptr) {
            g_free(debugInfo);
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    return errMsg;
}

bool GStreamerDecoder::initialize(std::string_view source,
                                  PixelFormat format,
                                  int threadCount,
                                  DeviceType device)
{
    close();
    const auto start = std::chrono::steady_clock::now();

    m_outputFormat = format;
    m_filePath = std::string(source);
    m_threadCount = threadCount;
    m_deviceType = device;

    // Determine source configuration
    const SourceType srcType = detectSourceType(m_filePath);
    std::string uri;
    if (srcType != SourceType::Device) {
        uri = m_filePath;
        if (uri.find("://") == std::string::npos) {
            gchar* fileUri = gst_filename_to_uri(uri.c_str(), nullptr);
            if (fileUri != nullptr) {
                uri = fileUri;
                g_free(fileUri);
            }
        }
    }

    // Create playbin pipeline
    GstElement* playbinRaw = gst_element_factory_make("playbin", "video-playbin");
    if (!playbinRaw) {
        LOG(ERROR) << "GStreamerDecoder: Failed to create playbin element";
        return false;
    }
    m_pipeline.reset(playbinRaw);

    // Create custom video sink bin: videoconvert -> appsink
    GstElement* sinkBin = gst_bin_new("sink-bin");
    GstElement* conv = gst_element_factory_make("videoconvert", "sink-conv");
    GstElement* appsink = gst_element_factory_make("appsink", "sink-appsink");

    if (!sinkBin || !conv || !appsink) {
        LOG(ERROR) << "GStreamerDecoder: Failed to create video sink elements";
        close();
        return false;
    }

    m_sink = appsink;

    // Configure appsink caps (RGB / BGR)
    const char* formatStr = (m_outputFormat == PixelFormat::RGB24) ? "RGB" : "BGR";
    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, formatStr,
                                        nullptr);
    gst_app_sink_set_caps(GST_APP_SINK(m_sink), caps);
    gst_caps_unref(caps);

    gst_app_sink_set_drop(GST_APP_SINK(m_sink), TRUE);
    gst_app_sink_set_max_buffers(GST_APP_SINK(m_sink), 2);
    gst_base_sink_set_sync(GST_BASE_SINK(m_sink), FALSE);

    gst_bin_add_many(GST_BIN(sinkBin), conv, appsink, nullptr);
    if (!gst_element_link(conv, appsink)) {
        LOG(ERROR) << "GStreamerDecoder: Failed to link videoconvert to appsink";
        close();
        return false;
    }

    // Add ghost pad to sinkBin
    GstPad* pad = gst_element_get_static_pad(conv, "sink");
    GstPad* ghostPad = gst_ghost_pad_new("sink", pad);
    gst_pad_set_active(ghostPad, TRUE);
    gst_element_add_pad(sinkBin, ghostPad);
    gst_object_unref(pad);

    if (srcType == SourceType::Device) {
#ifdef _WIN32
        GstElement* devSrc = gst_element_factory_make("autovideosrc", "cam-src");
#else
        GstElement* devSrc = gst_element_factory_make("v4l2src", "cam-src");
        if (devSrc && m_filePath.rfind("/dev/video", 0) == 0) {
            g_object_set(G_OBJECT(devSrc), "device", m_filePath.c_str(), nullptr);
        }
#endif
        if (devSrc != nullptr) {
            g_object_set(G_OBJECT(m_pipeline.get()), "video-source", devSrc, nullptr);
        }
        g_object_set(G_OBJECT(m_pipeline.get()), "video-sink", sinkBin, nullptr);
    } else {
        g_object_set(G_OBJECT(m_pipeline.get()),
                     "uri", uri.c_str(),
                     "video-sink", sinkBin,
                     nullptr);
    }

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG(ERROR) << "GStreamerDecoder: Failed to set pipeline to PLAYING: " << getBusErrorMessage();
        close();
        return false;
    }

    // Wait for first frame or ready state
    ret = gst_element_get_state(m_pipeline.get(), nullptr, nullptr, 3 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG(ERROR) << "GStreamerDecoder: State change timeout: " << getBusErrorMessage();
        close();
        return false;
    }

    // Query stream duration
    gint64 durationNs = 0;
    if (gst_element_query_duration(m_pipeline.get(), GST_FORMAT_TIME, &durationNs) && durationNs > 0) {
        m_duration = static_cast<double>(durationNs) / GST_SECOND;
    } else {
        m_duration = 0.0; // Live stream
    }

    m_isInitialized = true;
    const auto end = std::chrono::steady_clock::now();
    m_initTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return true;
}

bool GStreamerDecoder::reconnect()
{
    const std::string cachedPath = m_filePath;
    const PixelFormat cachedFormat = m_outputFormat;
    const int cachedThreads = m_threadCount;
    const DeviceType cachedDevice = m_deviceType;

    close();
    return initialize(cachedPath, cachedFormat, cachedThreads, cachedDevice);
}

bool GStreamerDecoder::decodeNextFrame()
{
    if (!m_isInitialized || !m_sink) {
        return false;
    }

    const auto start = std::chrono::steady_clock::now();

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(m_sink));
    if (!sample) {
        if (gst_app_sink_is_eos(GST_APP_SINK(m_sink))) {
            m_reachedEof = true;
            return false;
        }

        // Potential live stream drop -> auto-reconnect
        if (m_duration <= 0.0 && m_reconnectAttempts < 3) {
            LOG(WARNING) << "GStreamerDecoder: Stream sample timeout. Reconnecting (" << m_reconnectAttempts + 1 << "/3)...";
            ++m_reconnectAttempts;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (reconnect()) {
                m_reconnectAttempts = 0;
                return decodeNextFrame();
            }
        }
        return false;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    if (caps) {
        GstVideoInfo vinfo;
        if (gst_video_info_from_caps(&vinfo, caps)) {
            m_width = vinfo.width;
            m_height = vinfo.height;
            if (vinfo.fps_d > 0 && vinfo.fps_n > 0) {
                m_frameRate = static_cast<double>(vinfo.fps_n) / static_cast<double>(vinfo.fps_d);
            }
        }
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        GstMapInfoWrapper map(buffer, GST_MAP_READ);
        if (map.isMapped()) {
            const std::size_t expectedSize = static_cast<std::size_t>(m_width * m_height * 3);
            if (m_rgbBuffer.size() != expectedSize) {
                m_rgbBuffer.resize(expectedSize);
            }
            const std::size_t copyBytes = std::min(map.size(), expectedSize);
            std::memcpy(m_rgbBuffer.data(), map.data(), copyBytes);

            if (GST_BUFFER_PTS_IS_VALID(buffer)) {
                m_timestamp = static_cast<double>(buffer->pts) / GST_SECOND;
            } else {
                m_timestamp = static_cast<double>(m_decodedFramesCount) / ((m_frameRate > 0.0) ? m_frameRate : 30.0);
            }

            ++m_decodedFramesCount;

            const auto end = std::chrono::steady_clock::now();
            m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            m_totalDecodeTimeMs += m_lastDecodeTimeMs;

            if (m_tripleBufferingEnabled) {
                auto& slot = m_tripleBuffer.getWriteBuffer();
                if (slot.buffer.size() != expectedSize) {
                    slot.buffer.resize(expectedSize);
                }
                std::memcpy(slot.buffer.data(), m_rgbBuffer.data(), expectedSize);
                slot.width = m_width;
                slot.height = m_height;
                slot.size = expectedSize;
                slot.timestamp = m_timestamp;
                slot.decodeTimeMs = m_lastDecodeTimeMs;
                slot.format = m_outputFormat;
                m_tripleBuffer.publishWriteBuffer();
            }

            gst_sample_unref(sample);
            return true;
        }
    }

    gst_sample_unref(sample);
    return false;
}

FrameInfo GStreamerDecoder::getRawFrameData() const
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

VideoMetadata GStreamerDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = m_duration;
    meta.codecName = m_codecName;
    meta.format = m_outputFormat;
    meta.deviceType = m_deviceType;
    return meta;
}

DecoderPerformanceStats GStreamerDecoder::getPerformanceStats() const
{
    DecoderPerformanceStats stats;
    stats.initializationTimeMs = m_initTimeMs;
    stats.totalDecodedFrames = m_decodedFramesCount;
    stats.averageDecodeTimeMs = (m_decodedFramesCount > 0U) ? (m_totalDecodeTimeMs / static_cast<double>(m_decodedFramesCount)) : 0.0;
    return stats;
}

bool GStreamerDecoder::seek(double timeInSeconds)
{
    if (!m_isInitialized || !m_pipeline || m_duration <= 0.0) {
        return false;
    }

    const gint64 targetNs = static_cast<gint64>(timeInSeconds * GST_SECOND);
    return (gst_element_seek_simple(m_pipeline.get(),
                                   GST_FORMAT_TIME,
                                   static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                   targetNs) != 0);
}

void GStreamerDecoder::enableTripleBuffering(bool enable)
{
    m_tripleBufferingEnabled = enable;
    if (enable && m_isInitialized && m_width > 0 && m_height > 0) {
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

bool GStreamerDecoder::isTripleBufferingEnabled() const
{
    return m_tripleBufferingEnabled;
}

} // namespace PelcoD::Video

#endif // PELCOD_HAS_GSTREAMER
