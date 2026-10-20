#include "Live555Decoder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <glog/logging.h>

#if defined(PELCOD_HAS_LIVE555)
#include <BasicUsageEnvironment.hh>
#include <liveMedia.hh>
#include <wels/codec_api.h>
#include <wels/codec_app_def.h>

#include "ColorConverter.h"
#include "Live555RTSPClient.h"
#endif

namespace Video {

namespace {

#if defined(PELCOD_HAS_LIVE555)
    void openH264TraceCallback(void* /*ctx*/, int level, const char* message)
    {
        if (message == nullptr) {
            return;
        }
        if (level == WELS_LOG_ERROR) {
            LOG(ERROR) << "OpenH264: " << message;
        } else if (level == WELS_LOG_WARNING) {
            LOG(WARNING) << "OpenH264: " << message;
        } else {
            VLOG(1) << "OpenH264: " << message;
        }
    }
#endif

} // namespace

Live555Decoder::Live555Decoder()
{
    m_codecName = "h264";
    m_reportedDeviceType = DeviceType::CPU;
}

Live555Decoder::~Live555Decoder()
{
    close();
}

bool Live555Decoder::initialize(std::string_view source, PixelFormat format, int threadCount, DeviceType /*device*/)
{
    close();

    m_filePath = std::string(source);
    m_outputFormat = format;
    m_threadCount = threadCount;
    m_deviceType = DeviceType::CPU;
    m_reportedDeviceType = DeviceType::CPU;
    m_interrupted.store(false);

#if defined(PELCOD_HAS_LIVE555)
    const auto startTime { std::chrono::steady_clock::now() };

    // Initialize Cisco OpenH264 decoder instance
    if (WelsCreateDecoder(&m_decoder) != 0 || m_decoder == nullptr) {
        LOG(ERROR) << "Live555Decoder: Failed to instantiate OpenH264 decoder.";
        return false;
    }

    WelsTraceCallback traceCb { &openH264TraceCallback };
    m_decoder->SetOption(DECODER_OPTION_TRACE_CALLBACK, &traceCb);
    int traceLevel { WELS_LOG_WARNING };
    m_decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &traceLevel);

    if (m_threadCount > 0) {
        int threads { std::clamp(m_threadCount, 1, 16) };
        m_decoder->SetOption(DECODER_OPTION_NUM_OF_THREADS, &threads);
    }

    SDecodingParam decParam;
    std::memset(&decParam, 0, sizeof(decParam));
    decParam.bParseOnly = false;
    decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    decParam.uiCpuLoad = 0U;
    decParam.uiTargetDqLayer = 0U;

    if (m_decoder->Initialize(&decParam) != 0) {
        LOG(ERROR) << "Live555Decoder: Failed to initialize OpenH264 decoder parameters.";
        WelsDestroyDecoder(m_decoder);
        m_decoder = nullptr;
        return false;
    }

    const bool isMemorySource { (source.rfind("memory://", 0) == 0) || (source.rfind("mock://", 0) == 0)
        || (source.rfind("test://", 0) == 0) };
    if (isMemorySource) {
        m_running.store(true);
        m_connected.store(true);
        m_isInitialized = true;
        const auto endTime { std::chrono::steady_clock::now() };
        m_initTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return true;
    }

    startNetworkLoop();

    // Wait for connection to establish or timeout after 3 seconds
    const auto connDeadline { std::chrono::steady_clock::now() + std::chrono::milliseconds(3000) };
    while (!m_connected.load() && m_running.load() && !m_interrupted.load()) {
        if (std::chrono::steady_clock::now() > connDeadline) {
            LOG(WARNING) << "Live555Decoder: Connection timed out for URI: " << m_filePath;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    const auto endTime { std::chrono::steady_clock::now() };
    m_initTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    m_isInitialized = m_connected.load();
    if (!m_isInitialized) {
        stopNetworkLoop();
        if (m_decoder != nullptr) {
            m_decoder->Uninitialize();
            WelsDestroyDecoder(m_decoder);
            m_decoder = nullptr;
        }
        return false;
    }

    if (isTripleBufferingEnabled() && (m_width > 0) && (m_height > 0)) {
        initTripleBufferSlots(m_width, m_height, m_outputFormat);
    }

    return true;
#else
    LOG(ERROR) << "Live555Decoder: Compiled without PELCOD_HAS_LIVE555 support.";
    return false;
#endif
}

bool Live555Decoder::decodeNextFrame()
{
#if defined(PELCOD_HAS_LIVE555)
    if (!m_isInitialized || (m_decoder == nullptr)) {
        return false;
    }

    const auto decodeStart { std::chrono::steady_clock::now() };
    const auto timeout { std::chrono::milliseconds(100) };

    while (!m_interrupted.load() && m_running.load()) {
        NaluPacket packet;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (!m_queueCv.wait_for(lock, timeout,
                    [this]() { return !m_naluQueue.empty() || m_interrupted.load() || !m_running.load(); })) {
                return false;
            }
            if (m_interrupted.load() || !m_running.load() || m_naluQueue.empty()) {
                return false;
            }
            packet = std::move(m_naluQueue.front());
            m_naluQueue.pop_front();
        }

        if (packet.data.empty()) {
            continue;
        }

        unsigned char* pDstPlanes[3] { nullptr, nullptr, nullptr };
        SBufferInfo dstBufInfo;
        std::memset(&dstBufInfo, 0, sizeof(dstBufInfo));

        const DECODING_STATE rv { m_decoder->DecodeFrameNoDelay(
            packet.data.data(), static_cast<int>(packet.data.size()), pDstPlanes, &dstBufInfo) };

        if ((rv == dsErrorFree) && (dstBufInfo.iBufferStatus == 1)) {
            const int w { dstBufInfo.UsrData.sSystemBuffer.iWidth };
            const int h { dstBufInfo.UsrData.sSystemBuffer.iHeight };
            const int yStride { dstBufInfo.UsrData.sSystemBuffer.iStride[0] };
            const int uvStride { dstBufInfo.UsrData.sSystemBuffer.iStride[1] };

            return handleDecodedFrame(pDstPlanes, w, h, yStride, uvStride, packet.timestamp, decodeStart);
        }
    }

    return false;
#else
    return false;
#endif
}

#if defined(PELCOD_HAS_LIVE555)
bool Live555Decoder::handleDecodedFrame(unsigned char* const* planes, int width, int height, int yStride, int uvStride,
    double pts, std::chrono::steady_clock::time_point start)
{
    if ((width <= 0) || (height <= 0) || (planes == nullptr) || (planes[0] == nullptr) || (planes[1] == nullptr)
        || (planes[2] == nullptr)) {
        return false;
    }

    const std::size_t requiredBytes { static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U };
    if (m_frameBuffer.size() != requiredBytes) {
        m_frameBuffer.resize(requiredBytes);
        if (isTripleBufferingEnabled()) {
            initTripleBufferSlots(width, height, m_outputFormat);
        }
    }

    m_width = width;
    m_height = height;
    m_timestamp = pts;

    ColorConverter::yuv420ToRgb(planes[0], planes[1], planes[2], width, height, yStride, uvStride, uvStride,
        m_frameBuffer.data(), m_outputFormat);

    const auto decodeEnd { std::chrono::steady_clock::now() };
    m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(decodeEnd - start).count();
    m_totalDecodeTimeMs += m_lastDecodeTimeMs;
    ++m_decodedFramesCount;
    m_decodedAtNs = std::chrono::duration_cast<std::chrono::nanoseconds>(decodeEnd.time_since_epoch()).count();

    dispatchFrameProcessors(m_frameBuffer.data(), m_width, m_height, m_outputFormat);
    publishToTripleBuffer(
        m_frameBuffer.data(), m_width, m_height, m_frameBuffer.size(), m_timestamp, m_lastDecodeTimeMs, m_outputFormat);

    return true;
}
#endif

bool Live555Decoder::seek(double /*timeInSeconds*/)
{
    // Live RTSP streams do not support arbitrary seek operations
    return false;
}

void Live555Decoder::close()
{
    interrupt();
    stopNetworkLoop();

#if defined(PELCOD_HAS_LIVE555)
    if (m_decoder != nullptr) {
        m_decoder->Uninitialize();
        WelsDestroyDecoder(m_decoder);
        m_decoder = nullptr;
    }
#endif

    m_isInitialized = false;
    m_connected.store(false);
}

void Live555Decoder::interrupt() noexcept
{
    m_interrupted.store(true);
#if defined(PELCOD_HAS_LIVE555)
    m_watchVariable = 1;
#endif
    m_queueCv.notify_all();
}

void Live555Decoder::queueNalu(const std::uint8_t* data, std::size_t size, double pts)
{
    if ((data == nullptr) || (size == 0U)) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_queueMutex);
    // Bounded queue of 30 NALUs to prevent buffer bloat and preserve ultra-low latency
    if (m_naluQueue.size() >= 30U) {
        m_naluQueue.pop_front();
    }

    NaluPacket packet;
    packet.data.assign(data, data + size);
    packet.timestamp = pts;
    m_naluQueue.push_back(std::move(packet));
    m_queueCv.notify_one();
}

void Live555Decoder::onNaluReceived(const std::uint8_t* data, std::size_t size, double pts)
{
    queueNalu(data, size, pts);
}

void Live555Decoder::startNetworkLoop()
{
    m_running.store(true);
    m_networkThread = std::thread([this]() {
#if defined(PELCOD_HAS_LIVE555)
        m_scheduler = BasicTaskScheduler::createNew();
        if (m_scheduler == nullptr) {
            m_running.store(false);
            return;
        }

        m_env = BasicUsageEnvironment::createNew(*m_scheduler);
        if (m_env == nullptr) {
            delete m_scheduler;
            m_scheduler = nullptr;
            m_running.store(false);
            return;
        }

        m_watchVariable = 0;

        auto naluCb
            = [this](const std::uint8_t* data, std::size_t size, double pts) { onNaluReceived(data, size, pts); };

        auto statusCb = [this](bool connected, std::string_view errorMsg) {
            if (connected) {
                m_connected.store(true);
            } else {
                m_connected.store(false);
                if (!errorMsg.empty()) {
                    LOG(WARNING) << "Live555Decoder: RTSP client error: " << errorMsg;
                }
            }
        };

        m_rtspClient = Live555RTSPClient::createNew(*m_env, m_filePath, m_rtspTransport, m_rtspUsername, m_rtspPassword,
            std::move(naluCb), std::move(statusCb));

        if (m_rtspClient != nullptr) {
            m_rtspClient->startSession();
            m_scheduler->doEventLoop(&m_watchVariable);
        }

        m_connected.store(false);

        if (m_rtspClient != nullptr) {
            Medium::close(m_rtspClient);
            m_rtspClient = nullptr;
        }

        m_env->reclaim();
        m_env = nullptr;
        delete m_scheduler;
        m_scheduler = nullptr;
#endif
    });
}

void Live555Decoder::stopNetworkLoop()
{
    m_running.store(false);
#if defined(PELCOD_HAS_LIVE555)
    m_watchVariable = 1;
#endif
    if (m_networkThread.joinable()) {
        m_networkThread.join();
    }

    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_naluQueue.clear();
}

} // namespace Video
