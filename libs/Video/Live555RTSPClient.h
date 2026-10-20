#pragma once

/// @file Live555RTSPClient.h
/// @brief Asynchronous Live555 RTSP client subclass managing session negotiation and RTP reception.

#if defined(PELCOD_HAS_LIVE555)

#include "DecoderTypes.h"

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <liveMedia.hh>

namespace Video {

/// @class Live555RTSPClient
/// @brief Subclass of Live555 RTSPClient handling DESCRIBE, SETUP, PLAY, and out-of-band SPS/PPS extraction.
class Live555RTSPClient : public RTSPClient {
public:
    /// @brief Callback invoked when a complete framed Annex-B NAL unit is ready.
    using NaluCallback = std::function<void(const std::uint8_t* data, std::size_t size, double ptsSeconds)>;

    /// @brief Callback invoked on connection state transition or unrecoverable error.
    using StatusCallback = std::function<void(bool connected, std::string_view errorMsg)>;

    /// @brief Factory creating a new Live555RTSPClient instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] url Target RTSP stream URL.
    /// @param[in] transport Configured transport protocol (TCP or UDP).
    /// @param[in] username RTSP authentication username.
    /// @param[in] password RTSP authentication password.
    /// @param[in] naluCb Functional callback invoked per completed NAL unit.
    /// @param[in] statusCb Functional callback invoked on connection status changes.
    /// @return Pointer to newly allocated Live555RTSPClient.
    [[nodiscard]] static Live555RTSPClient* createNew(UsageEnvironment& env, std::string_view url,
        RtspTransportMode transport, std::string_view username, std::string_view password, NaluCallback naluCb,
        StatusCallback statusCb);

    /// @brief Begins the asynchronous RTSP handshake by issuing DESCRIBE.
    void startSession();

    /// @brief Shuts down active media sessions, sends TEARDOWN, and releases sinks.
    void shutdown();

    /// @brief Retrieves the active MediaSession.
    /// @return Pointer to MediaSession or nullptr.
    [[nodiscard]] MediaSession* session() const noexcept;

protected:
    /// @brief Protected destructor to enforce Medium::close() object lifetime.
    ~Live555RTSPClient() override;

private:
    Live555RTSPClient(UsageEnvironment& env, char const* url, RtspTransportMode transport, std::string_view username,
        std::string_view password, NaluCallback naluCb, StatusCallback statusCb);

    static void continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString);
    static void continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString);
    static void continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString);
    static void subsessionAfterPlaying(void* clientData);
    static void subsessionByeHandler(void* clientData, char const* reason);

    void handleDescribe(int resultCode, char* resultString);
    void handleSetup(int resultCode, char* resultString);
    void handlePlay(int resultCode, char* resultString);
    void setupNextSubsession();
    void extractSpropParams(MediaSubsession* subsession);

    RtspTransportMode m_transport { RtspTransportMode::Auto };
    std::string m_username;
    std::string m_password;
    std::unique_ptr<Authenticator> m_authenticator;

    NaluCallback m_naluCallback;
    StatusCallback m_statusCallback;

    MediaSession* m_session { nullptr };
    std::unique_ptr<MediaSubsessionIterator> m_subsessionIter;
    MediaSubsession* m_subsession { nullptr };
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
