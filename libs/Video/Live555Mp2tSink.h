#pragma once

/// @file Live555Mp2tSink.h
/// @brief Live555 MediaSink subclass receiving MPEG-2 TS packets and driving MpegTsH264Demuxer.

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>

#include <functional>
#include <vector>

#include <liveMedia.hh>

#include "MpegTsH264Demuxer.h"

namespace Video {

/// @class Live555Mp2tSink
/// @brief MediaSink for receiving RTP MP2T streams and demuxing H.264 elementary streams.
class Live555Mp2tSink : public MediaSink {
public:
    /// @brief Callback invoked when a complete Annex-B NAL unit is extracted.
    using NaluCallback = MpegTsH264Demuxer::NaluCallback;

    /// @brief Factory creating a new instance of Live555Mp2tSink.
    /// @param[in] env Live555 usage environment.
    /// @param[in] callback Output NALU callback.
    /// @param[in] bufferSize Internal receive buffer size (defaults to 102400 bytes).
    /// @return Pointer to newly allocated Live555Mp2tSink.
    [[nodiscard]] static Live555Mp2tSink* createNew(
        UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize = 102400U);

    /// @brief Continues playing/receiving frames from the upstream source.
    /// @return True if receiving started successfully.
    Boolean continuePlaying() override;

private:
    /// @brief Constructor initializing internal buffers and demuxer.
    Live555Mp2tSink(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize);

    /// @brief Destructor.
    ~Live555Mp2tSink() override = default;

    /// @brief Static callback triggered by Live555 upon frame arrival.
    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
        struct timeval presentationTime, unsigned durationInMicroseconds);

    /// @brief Member function handling arrival of a TS frame chunk.
    void handleFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime);

    MpegTsH264Demuxer m_demuxer;
    std::vector<std::uint8_t> m_buffer;
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
