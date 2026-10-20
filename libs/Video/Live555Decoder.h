#pragma once

/// @file Live555Decoder.h
/// @brief Ultra-low latency RTSP video decoder backend using Live555 and Cisco OpenH264.

#include "BaseVideoDecoder.h"
#include "DecoderTypes.h"

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#if defined(PELCOD_HAS_LIVE555)
class TaskScheduler;
class UsageEnvironment;
class ISVCDecoder;
#endif

namespace Video {

#if defined(PELCOD_HAS_LIVE555)
class Live555RTSPClient;
#endif

/// @struct NaluPacket
/// @brief Internal container holding a single Annex-B NAL unit and its timestamp.
struct NaluPacket {
    std::vector<std::uint8_t> data; ///< Annex-B NAL unit byte payload
    double timestamp { 0.0 }; ///< Presentation timestamp (PTS) in seconds
};

/// @class Live555Decoder
/// @brief High-performance RTSP video decoder utilizing Live555 for streaming and OpenH264 for decoding.
class Live555Decoder : public BaseVideoDecoder {
public:
    /// @brief Default constructor.
    Live555Decoder();

    /// @brief Destructor releasing network threads and codec resources.
    ~Live555Decoder() override;

    /// @brief Initializes Live555 RTSP session and OpenH264 decoder.
    /// @param[in] source RTSP stream URI.
    /// @param[in] format Output pixel format (defaults to RGB24).
    /// @param[in] threadCount Worker thread count.
    /// @param[in] device Hardware acceleration type (CPU only for OpenH264).
    /// @return True if connection established and decoder ready.
    bool initialize(std::string_view source, PixelFormat format = PixelFormat::RGB24, int threadCount = 0,
        DeviceType device = DeviceType::CPU) override;

    /// @brief Decodes the next available H.264 frame from the input queue.
    /// @return True if a complete video frame was decoded.
    bool decodeNextFrame() override;

    /// @brief Seek operation (unsupported for live RTSP streams).
    /// @param[in] timeInSeconds Target timestamp in seconds.
    /// @return False as live streams do not support seeking.
    bool seek(double timeInSeconds) override;

    /// @brief Closes the RTSP stream and tears down network/decoder workers.
    void close() override;

    /// @brief Asynchronously interrupts active network selects and blocking queues.
    void interrupt() noexcept override;

    /// @brief Enqueues an Annex-B formatted NAL unit into the internal decoding queue.
    /// @param[in] data Pointer to the Annex-B byte buffer.
    /// @param[in] size Buffer size in bytes.
    /// @param[in] pts Presentation timestamp in seconds.
    void queueNalu(const std::uint8_t* data, std::size_t size, double pts = 0.0);

private:
    void startNetworkLoop();
    void stopNetworkLoop();
    void onNaluReceived(const std::uint8_t* data, std::size_t size, double pts);

#if defined(PELCOD_HAS_LIVE555)
    bool handleDecodedFrame(unsigned char* const* planes, int width, int height, int yStride, int uvStride, double pts,
        std::chrono::steady_clock::time_point start);
#endif

    std::atomic<bool> m_running { false };
    std::atomic<bool> m_connected { false };
    std::atomic<bool> m_interrupted { false };

    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<NaluPacket> m_naluQueue;

    std::thread m_networkThread;

#if defined(PELCOD_HAS_LIVE555)
    char volatile m_watchVariable { 0 };
    TaskScheduler* m_scheduler { nullptr };
    UsageEnvironment* m_env { nullptr };
    Live555RTSPClient* m_rtspClient { nullptr };
    ISVCDecoder* m_decoder { nullptr };
#endif
};

} // namespace Video
