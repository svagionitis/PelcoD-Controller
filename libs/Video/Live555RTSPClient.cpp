#include "Live555RTSPClient.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <H264VideoRTPSource.hh>
#include <glog/logging.h>
#include <liveMedia.hh>

#include "Live555H264Sink.h"

namespace Video {

Live555RTSPClient* Live555RTSPClient::createNew(UsageEnvironment& env, std::string_view url,
    RtspTransportMode transport, std::string_view username, std::string_view password, NaluCallback naluCb,
    StatusCallback statusCb)
{
    const std::string urlStr(url);
    return new Live555RTSPClient(
        env, urlStr.c_str(), transport, username, password, std::move(naluCb), std::move(statusCb));
}

Live555RTSPClient::Live555RTSPClient(UsageEnvironment& env, char const* url, RtspTransportMode transport,
    std::string_view username, std::string_view password, NaluCallback naluCb, StatusCallback statusCb)
    : RTSPClient(env, url, 0, "PelcoD-Controller", 0, -1)
    , m_transport(transport)
    , m_username(username)
    , m_password(password)
    , m_naluCallback(std::move(naluCb))
    , m_statusCallback(std::move(statusCb))
{
    if (!m_username.empty()) {
        m_authenticator = std::make_unique<Authenticator>(m_username.c_str(), m_password.c_str());
    }
}

Live555RTSPClient::~Live555RTSPClient()
{
    shutdown();
}

void Live555RTSPClient::startSession()
{
    sendDescribeCommand(continueAfterDESCRIBE, m_authenticator.get());
}

void Live555RTSPClient::shutdown()
{
    if (m_session != nullptr) {
        bool activeSubsessions { false };
        MediaSubsessionIterator iter(*m_session);
        MediaSubsession* sub { nullptr };

        while ((sub = iter.next()) != nullptr) {
            if (sub->sink != nullptr) {
                Medium::close(sub->sink);
                sub->sink = nullptr;

                if (sub->rtcpInstance() != nullptr) {
                    sub->rtcpInstance()->setByeHandler(nullptr, nullptr);
                }
                activeSubsessions = true;
            }
        }

        if (activeSubsessions) {
            sendTeardownCommand(*m_session, nullptr, m_authenticator.get());
        }

        Medium::close(m_session);
        m_session = nullptr;
    }

    m_subsessionIter.reset();
    m_subsession = nullptr;
}

MediaSession* Live555RTSPClient::session() const noexcept
{
    return m_session;
}

void Live555RTSPClient::continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString)
{
    auto* self { static_cast<Live555RTSPClient*>(client) };
    if (self != nullptr) {
        self->handleDescribe(resultCode, resultString);
    }
}

void Live555RTSPClient::handleDescribe(int resultCode, char* resultString)
{
    if (resultCode != 0) {
        const std::string err { (resultString != nullptr) ? resultString : "Unknown DESCRIBE error" };
        LOG(ERROR) << "Live555RTSPClient: DESCRIBE failed: " << err;
        delete[] resultString;
        if (m_statusCallback) {
            m_statusCallback(false, err);
        }
        return;
    }

    char* const sdpDescription { resultString };
    m_session = MediaSession::createNew(envir(), sdpDescription);
    delete[] sdpDescription;

    if (m_session == nullptr) {
        LOG(ERROR) << "Live555RTSPClient: Failed to create MediaSession from SDP: " << envir().getResultMsg();
        if (m_statusCallback) {
            m_statusCallback(false, "Failed to parse SDP description");
        }
        return;
    }

    if (!m_session->hasSubsessions()) {
        LOG(ERROR) << "Live555RTSPClient: SDP description contains no media subsessions.";
        if (m_statusCallback) {
            m_statusCallback(false, "SDP description contains no media subsessions");
        }
        return;
    }

    m_subsessionIter = std::make_unique<MediaSubsessionIterator>(*m_session);
    setupNextSubsession();
}

void Live555RTSPClient::setupNextSubsession()
{
    if (!m_subsessionIter) {
        return;
    }

    m_subsession = m_subsessionIter->next();
    if (m_subsession != nullptr) {
        if (std::strcmp(m_subsession->mediumName(), "video") != 0) {
            setupNextSubsession();
            return;
        }

        if (std::strcmp(m_subsession->codecName(), "H264") != 0) {
            LOG(WARNING) << "Live555RTSPClient: Skipping non-H264 video codec: " << m_subsession->codecName();
            setupNextSubsession();
            return;
        }

        extractSpropParams(m_subsession);

        if (!m_subsession->initiate()) {
            LOG(ERROR) << "Live555RTSPClient: Failed to initiate subsession: " << envir().getResultMsg();
            setupNextSubsession();
        } else {
            const Boolean streamUsingTCP { (m_transport == RtspTransportMode::Tcp) ? True : False };
            sendSetupCommand(*m_subsession, continueAfterSETUP, False, streamUsingTCP, False, m_authenticator.get());
        }
        return;
    }

    LOG(ERROR) << "Live555RTSPClient: No suitable H.264 video subsession found in stream.";
    if (m_statusCallback) {
        m_statusCallback(false, "No suitable H.264 video subsession found in stream");
    }
}

void Live555RTSPClient::extractSpropParams(MediaSubsession* subsession)
{
    if ((subsession == nullptr) || (subsession->fmtp_spropparametersets() == nullptr)) {
        return;
    }

    unsigned numSPropRecords { 0U };
    SPropRecord* records { parseSPropParameterSets(subsession->fmtp_spropparametersets(), numSPropRecords) };
    if (records == nullptr) {
        return;
    }

    for (unsigned i { 0U }; i < numSPropRecords; ++i) {
        if ((records[i].sPropLength > 0U) && (records[i].sPropBytes != nullptr) && m_naluCallback) {
            std::vector<std::uint8_t> nalu(static_cast<std::size_t>(records[i].sPropLength) + 4U, 0U);
            nalu[0] = 0x00U;
            nalu[1] = 0x00U;
            nalu[2] = 0x00U;
            nalu[3] = 0x01U;
            std::memcpy(&nalu[4], records[i].sPropBytes, records[i].sPropLength);
            m_naluCallback(nalu.data(), nalu.size(), 0.0);
        }
    }

    delete[] records;
}

void Live555RTSPClient::continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString)
{
    auto* self { static_cast<Live555RTSPClient*>(client) };
    if (self != nullptr) {
        self->handleSetup(resultCode, resultString);
    }
}

void Live555RTSPClient::handleSetup(int resultCode, char* resultString)
{
    if (resultCode != 0) {
        const std::string err { (resultString != nullptr) ? resultString : "Unknown SETUP error" };
        LOG(ERROR) << "Live555RTSPClient: SETUP failed: " << err;
        delete[] resultString;
        if (m_statusCallback) {
            m_statusCallback(false, err);
        }
        return;
    }

    delete[] resultString;

    if (m_subsession != nullptr) {
        m_subsession->sink = Live555H264Sink::createNew(envir(), m_naluCallback);
        if (m_subsession->sink == nullptr) {
            LOG(ERROR) << "Live555RTSPClient: Failed to create Live555H264Sink: " << envir().getResultMsg();
            if (m_statusCallback) {
                m_statusCallback(false, "Failed to create Live555H264Sink");
            }
            return;
        }

        m_subsession->miscPtr = this;
        m_subsession->sink->startPlaying(*(m_subsession->readSource()), subsessionAfterPlaying, m_subsession);

        if (m_subsession->rtcpInstance() != nullptr) {
            m_subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, m_subsession);
        }

        if (m_session != nullptr) {
            sendPlayCommand(*m_session, continueAfterPLAY, 0.0, -1.0, 1.0f, m_authenticator.get());
        }
    }
}

void Live555RTSPClient::continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString)
{
    auto* self { static_cast<Live555RTSPClient*>(client) };
    if (self != nullptr) {
        self->handlePlay(resultCode, resultString);
    }
}

void Live555RTSPClient::handlePlay(int resultCode, char* resultString)
{
    if (resultCode != 0) {
        const std::string err { (resultString != nullptr) ? resultString : "Unknown PLAY error" };
        LOG(ERROR) << "Live555RTSPClient: PLAY failed: " << err;
        delete[] resultString;
        if (m_statusCallback) {
            m_statusCallback(false, err);
        }
        return;
    }

    delete[] resultString;
    LOG(INFO) << "Live555RTSPClient: RTSP session playing successfully.";
    if (m_statusCallback) {
        m_statusCallback(true, "");
    }
}

void Live555RTSPClient::subsessionAfterPlaying(void* clientData)
{
    auto* subsession { static_cast<MediaSubsession*>(clientData) };
    if (subsession == nullptr) {
        return;
    }

    auto* client { static_cast<Live555RTSPClient*>(subsession->miscPtr) };
    if (client != nullptr) {
        LOG(INFO) << "Live555RTSPClient: Subsession stream ended.";
        if (client->m_statusCallback) {
            client->m_statusCallback(false, "Stream ended");
        }
    }
}

void Live555RTSPClient::subsessionByeHandler(void* clientData, char const* reason)
{
    auto* subsession { static_cast<MediaSubsession*>(clientData) };
    if (subsession == nullptr) {
        return;
    }

    auto* client { static_cast<Live555RTSPClient*>(subsession->miscPtr) };
    if (reason != nullptr) {
        LOG(INFO) << "Live555RTSPClient: Received RTCP BYE (reason: " << reason << ")";
        delete[] reason;
    }

    if (client != nullptr) {
        if (client->m_statusCallback) {
            client->m_statusCallback(false, "Server sent RTCP BYE");
        }
    }
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
