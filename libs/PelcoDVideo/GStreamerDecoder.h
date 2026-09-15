#pragma once

/// @file GStreamerDecoder.h
/// @brief Native GStreamer video decoding backend using playbin and appsink.

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"

#include <memory>
#include <string>
#include <vector>

#if defined(PELCOD_HAS_GSTREAMER)
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

namespace PelcoD::Video {

/// @struct GstElementDeleter
struct GstElementDeleter {
    void operator()(GstElement* ptr) const
    {
        if (ptr != nullptr) {
            gst_element_set_state(ptr, GST_STATE_NULL);
            gst_object_unref(ptr);
        }
    }
};
using GstElementPtr = std::unique_ptr<GstElement, GstElementDeleter>;

/// @struct GstSampleDeleter
struct GstSampleDeleter {
    void operator()(GstSample* ptr) const
    {
        if (ptr != nullptr) {
            gst_sample_unref(ptr);
        }
    }
};
using GstSamplePtr = std::unique_ptr<GstSample, GstSampleDeleter>;

/// @struct GstCapsDeleter
struct GstCapsDeleter {
    void operator()(GstCaps* ptr) const
    {
        if (ptr != nullptr) {
            gst_caps_unref(ptr);
        }
    }
};
using GstCapsPtr = std::unique_ptr<GstCaps, GstCapsDeleter>;

/// @class GstMapInfoWrapper
/// @brief RAII wrapper for mapping and unmapping GstBuffer data.
class GstMapInfoWrapper {
public:
    GstMapInfoWrapper(GstBuffer* buffer, GstMapFlags flags)
        : m_buffer(buffer)
    {
        if (m_buffer != nullptr) {
            m_mapped = (gst_buffer_map(m_buffer, &m_info, flags) != 0);
        }
    }

    ~GstMapInfoWrapper()
    {
        if (m_mapped && m_buffer != nullptr) {
            gst_buffer_unmap(m_buffer, &m_info);
        }
    }

    GstMapInfoWrapper(const GstMapInfoWrapper&) = delete;
    GstMapInfoWrapper& operator=(const GstMapInfoWrapper&) = delete;

    [[nodiscard]] bool isMapped() const noexcept { return m_mapped; }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return m_info.data; }
    [[nodiscard]] std::size_t size() const noexcept { return m_info.size; }

private:
    GstBuffer* m_buffer { nullptr };
    GstMapInfo m_info {};
    bool m_mapped { false };
};

/// @class GStreamerDecoder
/// @brief Concrete implementation of IVideoDecoder using GStreamer APIs.
class GStreamerDecoder : public IVideoDecoder {
public:
    GStreamerDecoder();
    ~GStreamerDecoder() override;

    GStreamerDecoder(const GStreamerDecoder&) = delete;
    GStreamerDecoder& operator=(const GStreamerDecoder&) = delete;

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
    static void initGStreamer();
    std::string getBusErrorMessage();
    bool reconnect();

    GstElementPtr m_pipeline;
    GstElement* m_sink { nullptr };

    std::vector<std::uint8_t> m_rgbBuffer;
    bool m_tripleBufferingEnabled { false };
    mutable AtomicTripleBuffer<FrameBufferSlot> m_tripleBuffer;

    int m_width { 0 };
    int m_height { 0 };
    double m_timestamp { 0.0 };
    double m_frameRate { 0.0 };
    double m_duration { 0.0 };
    std::string m_codecName { "GStreamer" };
    PixelFormat m_outputFormat { PixelFormat::RGB24 };

    double m_initTimeMs { 0.0 };
    double m_lastDecodeTimeMs { 0.0 };
    double m_totalDecodeTimeMs { 0.0 };
    std::uint64_t m_decodedFramesCount { 0U };

    std::string m_filePath;
    int m_reconnectAttempts { 0 };
    int m_threadCount { 0 };
    DeviceType m_deviceType { DeviceType::CPU };

    bool m_isInitialized { false };
    bool m_reachedEof { false };
};

} // namespace PelcoD::Video

#endif // PELCOD_HAS_GSTREAMER
