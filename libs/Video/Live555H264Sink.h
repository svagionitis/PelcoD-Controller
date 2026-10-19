#pragma once

/// @file Live555H264Sink.h
/// @brief Live555 MediaSink subclass receiving H.264 NALUs and packaging Annex-B streams.

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <liveMedia.hh>

namespace Video {

/// @class Live555H264Sink
/// @brief MediaSink that depacketizes H.264 RTP stream chunks into Annex-B NAL units.
class Live555H264Sink : public MediaSink {
public:
    /// @brief Callback invoked when a complete framed Annex-B NAL unit is ready.
    using NaluCallback = std::function<void(const std::uint8_t* data, std::size_t size, double ptsSeconds)>;

    /// @brief Factory method creating a new instance of Live555H264Sink.
    /// @param[in] env Live555 usage environment.
    /// @param[in] callback Functional callback invoked per completed NAL unit.
    /// @param[in] bufferSize Internal receive buffer size in bytes (defaults to 100 KB).
    /// @return Pointer to newly allocated Live555H264Sink.
    [[nodiscard]] static Live555H264Sink* createNew(
        UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize = 102400U);

    /// @brief Continues playing/receiving frames from the upstream source.
    /// @return True if receiving started successfully.
    Boolean continuePlaying() override;

private:
    /// @brief Constructor initializing internal buffers and callback.
    Live555H264Sink(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize);

    /// @brief Destructor.
    ~Live555H264Sink() override = default;

    /// @brief Static callback triggered by Live555 upon frame arrival.
    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
        struct timeval presentationTime, unsigned durationInMicroseconds);

    /// @brief Member function handling arrival of a frame chunk.
    void handleFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime);

    NaluCallback m_callback;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
