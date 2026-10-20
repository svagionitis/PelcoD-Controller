#pragma once

/// @file Live555LiveSubsession.h
/// @brief Live555 OnDemandServerMediaSubsession subclasses for live H.264 and MPEG-2 TS streaming.

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>

#include <memory>
#include <vector>

#include <OnDemandServerMediaSubsession.hh>

#include "Live555FrameTypes.h"
#include "RtspServerConfig.h"

namespace Video {

class Live555FrameQueue;
class Live555H264LiveSource;
class Live555Mp2tLiveSource;

/// @class Live555LiveSubsession
/// @brief Abstract base subsession managing shared frame queue and client session lifecycle.
/// @details Extends Live555 OnDemandServerMediaSubsession, binding video and metadata frame queues
///          to unicast RTP/RTCP transport pipelines with per-client reader isolation.
class Live555LiveSubsession : public OnDemandServerMediaSubsession {
public:
    /// @brief Factory creating the appropriate subsession based on RtspStreamType.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live packets.
    /// @param[in] streamType Target stream format (H264ElementaryStream or Mpeg2TransportStream).
    /// @param[in] syncToKeyframe True if client readers synchronize to newest keyframe on connect.
    /// @param[in] estBitrateKbps Estimated stream bitrate in kbps (0 for default).
    /// @return Pointer to newly allocated Live555LiveSubsession.
    [[nodiscard]] static Live555LiveSubsession* createNew(UsageEnvironment& env, Live555FrameQueue& queue,
        RtspStreamType streamType, bool syncToKeyframe = true, unsigned estBitrateKbps = 0U);

    /// @brief Retrieves the stream type handled by this subsession.
    /// @return Handled stream type enum.
    [[nodiscard]] RtspStreamType streamType() const noexcept;

    /// @brief Retrieves current estimated bitrate in kbps.
    /// @return Estimated bitrate.
    [[nodiscard]] unsigned estimatedBitrateKbps() const noexcept;

    /// @brief Updates estimated bitrate in kbps.
    /// @param[in] estBitrateKbps New bitrate estimate.
    void setEstimatedBitrate(unsigned estBitrateKbps) noexcept;

    /// @brief Retrieves reference to parent frame queue.
    /// @return Reference to frame queue.
    [[nodiscard]] Live555FrameQueue& frameQueue() noexcept;

    /// @brief Wakes all active client stream sources awaiting incoming data.
    virtual void wakeAllSources();

    /// @brief Generates SDP description lines for this subsession.
    /// @param[in] addressFamily Socket address family (e.g. AF_INET).
    /// @return Pointer to formatted SDP string.
    char const* sdpLines(int addressFamily = 2 /* AF_INET */) override;

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue.
    /// @param[in] streamType Handled stream format.
    /// @param[in] syncToKeyframe True if client readers sync to keyframe.
    /// @param[in] estBitrateKbps Initial estimated bitrate.
    Live555LiveSubsession(UsageEnvironment& env, Live555FrameQueue& queue, RtspStreamType streamType,
        bool syncToKeyframe, unsigned estBitrateKbps);

    /// @brief Destructor.
    ~Live555LiveSubsession() override = default;

    Live555FrameQueue& m_queue;
    RtspStreamType m_streamType { RtspStreamType::H264ElementaryStream };
    bool m_syncToKeyframe { true };
    unsigned m_estimatedBitrateKbps { 2000U };
};

/// @class Live555H264LiveSubsession
/// @brief OnDemandServerMediaSubsession for RFC 6184 H.264 live elementary streams.
/// @details Creates H264VideoStreamDiscreteFramer and H264VideoRTPSink, managing
///          SPS/PPS parameter caching and SDP fmtp generation.
class Live555H264LiveSubsession : public Live555LiveSubsession {
public:
    /// @brief Factory creating a new Live555H264LiveSubsession instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing H.264 NALUs.
    /// @param[in] syncToKeyframe True if client readers sync to newest IDR.
    /// @param[in] estBitrateKbps Estimated bitrate in kbps (default 2000 kbps).
    /// @return Pointer to newly allocated Live555H264LiveSubsession.
    [[nodiscard]] static Live555H264LiveSubsession* createNew(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe = true, unsigned estBitrateKbps = 2000U);

    /// @brief Explicitly configures SPS and PPS parameter sets for SDP generation.
    /// @param[in] sps Raw SPS NAL unit buffer (excluding start code).
    /// @param[in] spsSize Size of SPS buffer in bytes.
    /// @param[in] pps Raw PPS NAL unit buffer (excluding start code).
    /// @param[in] ppsSize Size of PPS buffer in bytes.
    void setSpsPps(const std::uint8_t* sps, std::size_t spsSize, const std::uint8_t* pps, std::size_t ppsSize);

    /// @brief Explicitly configures SPS and PPS parameter sets from byte vectors.
    /// @param[in] sps Vector containing SPS NAL unit.
    /// @param[in] pps Vector containing PPS NAL unit.
    void setSpsPps(const std::vector<std::uint8_t>& sps, const std::vector<std::uint8_t>& pps);

    /// @brief Checks if valid SPS and PPS parameter sets are currently cached.
    /// @return True if both SPS and PPS are present.
    [[nodiscard]] bool hasSpsPps() const noexcept;

    /// @brief Creates discrete framer wrapping Live555H264LiveSource.
    /// @param[in] clientSessionId Client session ID assigned by server.
    /// @param[out] estBitrate Populated with estimated bitrate in kbps.
    /// @return Pointer to newly created H264VideoStreamDiscreteFramer.
    FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) override;

    /// @brief Creates RFC 6184 H264VideoRTPSink.
    /// @param[in] rtpGroupsock Groupsock socket pair for RTP transport.
    /// @param[in] rtpPayloadTypeIfDynamic RTP payload format number (typically 96).
    /// @param[in] inputSource Pointer to upstream FramedSource.
    /// @return Pointer to newly created H264VideoRTPSink.
    RTPSink* createNewRTPSink(
        Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) override;

    /// @brief Generates or returns cached fmtp SDP parameter line.
    /// @param[in] rtpSink Active or dummy RTPSink.
    /// @param[in] inputSource Active or dummy FramedSource.
    /// @return Formatted SDP string or nullptr if parameters unavailable.
    char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) override;

    /// @brief Wakes all active H.264 client stream sources awaiting incoming data.
    void wakeAllSources() override;

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue.
    /// @param[in] syncToKeyframe True if reader synchronizes to keyframe.
    /// @param[in] estBitrateKbps Estimated bitrate in kbps.
    Live555H264LiveSubsession(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps);

    /// @brief Destructor releasing cached SDP lines.
    ~Live555H264LiveSubsession() override;

private:
    std::vector<std::uint8_t> m_sps {};
    std::vector<std::uint8_t> m_pps {};
    char* m_auxSDPLine { nullptr };
    std::vector<Live555H264LiveSource*> m_activeSources {};
};

/// @class Live555Mp2tLiveSubsession
/// @brief OnDemandServerMediaSubsession for RFC 2250 MPEG-2 TS live transport streams.
/// @details Directly feeds Live555Mp2tLiveSource to SimpleRTPSink on payload type 33.
class Live555Mp2tLiveSubsession : public Live555LiveSubsession {
public:
    /// @brief Factory creating a new Live555Mp2tLiveSubsession instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing MPEG-2 TS packets.
    /// @param[in] syncToKeyframe True if client readers sync to newest PAT/PMT/IDR.
    /// @param[in] estBitrateKbps Estimated bitrate in kbps (default 5000 kbps).
    /// @return Pointer to newly allocated Live555Mp2tLiveSubsession.
    [[nodiscard]] static Live555Mp2tLiveSubsession* createNew(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe = true, unsigned estBitrateKbps = 5000U);

    /// @brief Creates Live555Mp2tLiveSource instance reading from queue.
    /// @param[in] clientSessionId Client session ID assigned by server.
    /// @param[out] estBitrate Populated with estimated bitrate in kbps.
    /// @return Pointer to newly created Live555Mp2tLiveSource.
    FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) override;

    /// @brief Creates RFC 2250 SimpleRTPSink on payload format 33.
    /// @param[in] rtpGroupsock Groupsock socket pair for RTP transport.
    /// @param[in] rtpPayloadTypeIfDynamic Unused for static payload format 33.
    /// @param[in] inputSource Pointer to upstream FramedSource.
    /// @return Pointer to newly created SimpleRTPSink.
    RTPSink* createNewRTPSink(
        Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) override;

    /// @brief Returns nullptr (RFC 2250 MP2T does not require aux SDP lines).
    /// @return nullptr.
    char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) override;

    /// @brief Wakes all active MP2T client stream sources awaiting incoming data.
    void wakeAllSources() override;

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue.
    /// @param[in] syncToKeyframe True if reader synchronizes to keyframe.
    /// @param[in] estBitrateKbps Estimated bitrate in kbps.
    Live555Mp2tLiveSubsession(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps);

    /// @brief Destructor.
    ~Live555Mp2tLiveSubsession() override = default;

private:
    std::vector<Live555Mp2tLiveSource*> m_activeSources {};
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
