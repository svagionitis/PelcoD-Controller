/// @file Live555LiveSubsession.cpp
/// @brief Implementation of Live555 OnDemandServerMediaSubsession subclasses for live streaming.

#if defined(PELCOD_HAS_LIVE555)

#include "Live555LiveSubsession.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <Base64.hh>
#include <GroupsockHelper.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>
#include <H264or5VideoStreamFramer.hh>
#include <SimpleRTPSink.hh>

#include "Live555FrameQueue.h"
#include "Live555H264LiveSource.h"
#include "Live555Mp2tLiveSource.h"

namespace Video {

// ============================================================================
// Live555LiveSubsession (Base Class)
// ============================================================================

Live555LiveSubsession* Live555LiveSubsession::createNew(UsageEnvironment& env, Live555FrameQueue& queue,
    RtspStreamType streamType, bool syncToKeyframe, unsigned estBitrateKbps)
{
    if (streamType == RtspStreamType::H264ElementaryStream) {
        const unsigned bitrate { (estBitrateKbps > 0U) ? estBitrateKbps : 2000U };
        return Live555H264LiveSubsession::createNew(env, queue, syncToKeyframe, bitrate);
    }

    const unsigned bitrate { (estBitrateKbps > 0U) ? estBitrateKbps : 5000U };
    return Live555Mp2tLiveSubsession::createNew(env, queue, syncToKeyframe, bitrate);
}

Live555LiveSubsession::Live555LiveSubsession(UsageEnvironment& env, Live555FrameQueue& queue, RtspStreamType streamType,
    bool syncToKeyframe, unsigned estBitrateKbps)
    : OnDemandServerMediaSubsession(env, False /* reuseFirstSource = False */)
    , m_queue { queue }
    , m_streamType { streamType }
    , m_syncToKeyframe { syncToKeyframe }
    , m_estimatedBitrateKbps { estBitrateKbps }
{
}

RtspStreamType Live555LiveSubsession::streamType() const noexcept
{
    return m_streamType;
}

unsigned Live555LiveSubsession::estimatedBitrateKbps() const noexcept
{
    return m_estimatedBitrateKbps;
}

void Live555LiveSubsession::setEstimatedBitrate(unsigned estBitrateKbps) noexcept
{
    m_estimatedBitrateKbps = estBitrateKbps;
}

Live555FrameQueue& Live555LiveSubsession::frameQueue() noexcept
{
    return m_queue;
}

char const* Live555LiveSubsession::sdpLines(int addressFamily)
{
    return OnDemandServerMediaSubsession::sdpLines(addressFamily);
}

// ============================================================================
// Live555H264LiveSubsession
// ============================================================================

Live555H264LiveSubsession* Live555H264LiveSubsession::createNew(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps)
{
    return new Live555H264LiveSubsession(env, queue, syncToKeyframe, estBitrateKbps);
}

Live555H264LiveSubsession::Live555H264LiveSubsession(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps)
    : Live555LiveSubsession(env, queue, RtspStreamType::H264ElementaryStream, syncToKeyframe, estBitrateKbps)
{
}

Live555H264LiveSubsession::~Live555H264LiveSubsession()
{
    delete[] m_auxSDPLine;
    m_auxSDPLine = nullptr;
}

void Live555H264LiveSubsession::setSpsPps(
    const std::uint8_t* sps, std::size_t spsSize, const std::uint8_t* pps, std::size_t ppsSize)
{
    if ((sps != nullptr) && (spsSize > 0U)) {
        std::size_t spsOffset { 0U };
        if ((spsSize >= 4U) && (sps[0] == 0x00U) && (sps[1] == 0x00U) && (sps[2] == 0x00U) && (sps[3] == 0x01U)) {
            spsOffset = 4U;
        } else if ((spsSize >= 3U) && (sps[0] == 0x00U) && (sps[1] == 0x00U) && (sps[2] == 0x01U)) {
            spsOffset = 3U;
        }
        m_sps.assign(sps + spsOffset, sps + spsSize);
    } else {
        m_sps.clear();
    }

    if ((pps != nullptr) && (ppsSize > 0U)) {
        std::size_t ppsOffset { 0U };
        if ((ppsSize >= 4U) && (pps[0] == 0x00U) && (pps[1] == 0x00U) && (pps[2] == 0x00U) && (pps[3] == 0x01U)) {
            ppsOffset = 4U;
        } else if ((ppsSize >= 3U) && (pps[0] == 0x00U) && (pps[1] == 0x00U) && (pps[2] == 0x01U)) {
            ppsOffset = 3U;
        }
        m_pps.assign(pps + ppsOffset, pps + ppsSize);
    } else {
        m_pps.clear();
    }

    delete[] m_auxSDPLine;
    m_auxSDPLine = nullptr;

    delete[] fSDPLines;
    fSDPLines = nullptr;
}

void Live555H264LiveSubsession::setSpsPps(const std::vector<std::uint8_t>& sps, const std::vector<std::uint8_t>& pps)
{
    setSpsPps(sps.data(), sps.size(), pps.data(), pps.size());
}

bool Live555H264LiveSubsession::hasSpsPps() const noexcept
{
    return (!m_sps.empty()) && (!m_pps.empty());
}

FramedSource* Live555H264LiveSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate)
{
    estBitrate = m_estimatedBitrateKbps;

    auto* liveSource = Live555H264LiveSource::createNew(envir(), m_queue, m_syncToKeyframe);
    if (liveSource == nullptr) {
        return nullptr;
    }

    return H264VideoStreamDiscreteFramer::createNew(envir(), liveSource);
}

RTPSink* Live555H264LiveSubsession::createNewRTPSink(
    Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* /*inputSource*/)
{
    if (hasSpsPps()) {
        return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, m_sps.data(),
            static_cast<unsigned>(m_sps.size()), m_pps.data(), static_cast<unsigned>(m_pps.size()));
    }

    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}

char const* Live555H264LiveSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* /*inputSource*/)
{
    if (m_auxSDPLine != nullptr) {
        return m_auxSDPLine;
    }

    if (rtpSink != nullptr) {
        char const* auxLine { rtpSink->auxSDPLine() };
        if (auxLine != nullptr) {
            delete[] m_auxSDPLine;
            m_auxSDPLine = strDup(auxLine);
            return m_auxSDPLine;
        }
    }

    if (hasSpsPps()) {
        std::vector<std::uint8_t> spsWeb(m_sps.size());
        const unsigned spsWebSize = removeH264or5EmulationBytes(
            spsWeb.data(), static_cast<unsigned>(spsWeb.size()), m_sps.data(), static_cast<unsigned>(m_sps.size()));

        if (spsWebSize >= 4U) {
            const std::uint32_t profileLevelId = (static_cast<std::uint32_t>(spsWeb[1]) << 16U)
                | (static_cast<std::uint32_t>(spsWeb[2]) << 8U) | static_cast<std::uint32_t>(spsWeb[3]);

            char* spsBase64 { base64Encode(
                reinterpret_cast<const char*>(m_sps.data()), static_cast<unsigned>(m_sps.size())) };
            char* ppsBase64 { base64Encode(
                reinterpret_cast<const char*>(m_pps.data()), static_cast<unsigned>(m_pps.size())) };

            if ((spsBase64 != nullptr) && (ppsBase64 != nullptr)) {
                constexpr const char* fmtpFmt { "a=fmtp:96 packetization-mode=1"
                                                ";profile-level-id=%06X"
                                                ";sprop-parameter-sets=%s,%s\r\n" };

                const std::size_t fmtpSize { std::strlen(fmtpFmt) + 16U + std::strlen(spsBase64)
                    + std::strlen(ppsBase64) };

                m_auxSDPLine = new char[fmtpSize];
                std::snprintf(m_auxSDPLine, fmtpSize, fmtpFmt, profileLevelId, spsBase64, ppsBase64);
            }

            delete[] spsBase64;
            delete[] ppsBase64;

            return m_auxSDPLine;
        }
    }

    return nullptr;
}

// ============================================================================
// Live555Mp2tLiveSubsession
// ============================================================================

Live555Mp2tLiveSubsession* Live555Mp2tLiveSubsession::createNew(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps)
{
    return new Live555Mp2tLiveSubsession(env, queue, syncToKeyframe, estBitrateKbps);
}

Live555Mp2tLiveSubsession::Live555Mp2tLiveSubsession(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe, unsigned estBitrateKbps)
    : Live555LiveSubsession(env, queue, RtspStreamType::Mpeg2TransportStream, syncToKeyframe, estBitrateKbps)
{
}

FramedSource* Live555Mp2tLiveSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate)
{
    estBitrate = m_estimatedBitrateKbps;
    return Live555Mp2tLiveSource::createNew(envir(), m_queue, m_syncToKeyframe);
}

RTPSink* Live555Mp2tLiveSubsession::createNewRTPSink(
    Groupsock* rtpGroupsock, unsigned char /*rtpPayloadTypeIfDynamic*/, FramedSource* /*inputSource*/)
{
    return SimpleRTPSink::createNew(envir(), rtpGroupsock, 33U, 90000U, "video", "MP2T", 1U, True, False);
}

char const* Live555Mp2tLiveSubsession::getAuxSDPLine(RTPSink* /*rtpSink*/, FramedSource* /*inputSource*/)
{
    return nullptr;
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
