/// @file Live555RtspServer.cpp
/// @brief Implementation of Live555RtspServer embedded streaming server orchestrator.

#include "Live555RtspServer.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <RTSPServer.hh>
#include <ServerMediaSession.hh>
#include <UsageEnvironment.hh>
#include <glog/logging.h>

#include "Live555FrameQueue.h"
#include "Live555FrameTypes.h"
#include "Live555LiveSubsession.h"
#include "RtspServerConfig.h"

namespace Video {

namespace {

    /// @class CustomRtspServer
    /// @brief Internal RTSP server subclass exposing the actual bound port and client count.
    class CustomRtspServer : public RTSPServer {
    public:
        [[nodiscard]] static CustomRtspServer* createNew(UsageEnvironment& env, Port ourPort,
            UserAuthenticationDatabase* authDatabase = nullptr, unsigned reclamationSeconds = 65U)
        {
            int ourSocketIPv4 { setUpOurSocket(env, ourPort, AF_INET) };
            int ourSocketIPv6 { setUpOurSocket(env, ourPort, AF_INET6) };
            if ((ourSocketIPv4 < 0) && (ourSocketIPv6 < 0)) {
                return nullptr;
            }
            return new CustomRtspServer(env, ourSocketIPv4, ourSocketIPv6, ourPort, authDatabase, reclamationSeconds);
        }

        [[nodiscard]] std::uint16_t portNum() const noexcept
        {
            return ntohs(fServerPort.num());
        }

        [[nodiscard]] std::size_t clientCount() const noexcept
        {
            return static_cast<std::size_t>(numClientSessions());
        }

    protected:
        using RTSPServer::RTSPServer;
    };

    /// @brief Determines if an H.264 NAL unit represents a keyframe (SPS, PPS, or IDR).
    [[nodiscard]] bool isH264KeyframeNalu(const std::uint8_t* naluData, std::size_t naluSize) noexcept
    {
        if ((naluData == nullptr) || (naluSize == 0U)) {
            return false;
        }

        std::size_t offset { 0U };
        if ((naluSize >= 4U) && (naluData[0] == 0x00U) && (naluData[1] == 0x00U) && (naluData[2] == 0x00U)
            && (naluData[3] == 0x01U)) {
            offset = 4U;
        } else if ((naluSize >= 3U) && (naluData[0] == 0x00U) && (naluData[1] == 0x00U) && (naluData[2] == 0x01U)) {
            offset = 3U;
        }

        if (offset >= naluSize) {
            return false;
        }

        const std::uint8_t nalUnitType { static_cast<std::uint8_t>(naluData[offset] & 0x1FU) };
        // NAL unit types: 5 = IDR, 7 = SPS, 8 = PPS
        return (nalUnitType == 5U) || (nalUnitType == 7U) || (nalUnitType == 8U);
    }

    /// @brief Checks if an MPEG-2 TS chunk contains keyframe or sync packet indicators.
    [[nodiscard]] bool isMp2tKeyframeChunk(const std::uint8_t* tsData, std::size_t tsSize) noexcept
    {
        if ((tsData == nullptr) || (tsSize < 188U)) {
            return false;
        }

        for (std::size_t i { 0U }; i + 188U <= tsSize; i += 188U) {
            const auto* pkt { tsData + i };
            if (pkt[0] != 0x47U) {
                continue;
            }

            // PID extraction (13 bits)
            const std::uint16_t pid { static_cast<std::uint16_t>(((pkt[1] & 0x1FU) << 8U) | pkt[2]) };
            if (pid == 0U) {
                // PAT (Program Association Table)
                return true;
            }

            // Check adaptation field for random access indicator (RAI)
            const std::uint8_t adaptCtrl { static_cast<std::uint8_t>((pkt[3] >> 4U) & 0x03U) };
            if ((adaptCtrl == 0x02U) || (adaptCtrl == 0x03U)) {
                const std::uint8_t adaptLen { pkt[4] };
                if (adaptLen > 0U) {
                    const std::uint8_t flags { pkt[5] };
                    if ((flags & 0x40U) != 0U) {
                        // Random access indicator set
                        return true;
                    }
                }
            }
        }

        return false;
    }

} // namespace

// ============================================================================
// Live555RtspServer::Impl
// ============================================================================

class Live555RtspServer::Impl final {
public:
    explicit Impl(RtspServerConfig config)
        : m_config { std::move(config) }
        , m_queue { m_config.queueCapacity }
    {
    }

    ~Impl()
    {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    [[nodiscard]] bool start()
    {
        if (m_running.load()) {
            return true;
        }

        m_watchVariable = 0;
        std::promise<bool> startPromise;
        auto startFuture = startPromise.get_future();

        m_workerThread = std::thread([this, p = std::move(startPromise)]() mutable { workerThreadFunc(std::move(p)); });

        const bool started { startFuture.get() };
        if (!started) {
            if (m_workerThread.joinable()) {
                m_workerThread.join();
            }
        }

        return started;
    }

    void stop()
    {
        if (!m_running.load() && !m_workerThread.joinable()) {
            return;
        }

        m_watchVariable = 1;

        if ((m_scheduler != nullptr) && (m_stopTriggerId != 0U)) {
            m_scheduler->triggerEvent(m_stopTriggerId, this);
        }

        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        m_boundPort.store(0U);
        m_running.store(false);
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_running.load();
    }

    [[nodiscard]] std::uint16_t listeningPort() const noexcept
    {
        return m_boundPort.load();
    }

    [[nodiscard]] std::string getUrl(const std::string& host) const
    {
        std::ostringstream oss;
        oss << "rtsp://" << host << ":" << m_boundPort.load() << "/" << m_config.streamPath;
        return oss.str();
    }

    [[nodiscard]] bool pushH264Nalu(const std::uint8_t* naluData, std::size_t naluSize, double ptsSeconds)
    {
        if ((naluData == nullptr) || (naluSize == 0U)) {
            return false;
        }

        const bool isKey { isH264KeyframeNalu(naluData, naluSize) };
        return m_queue.push(naluData, naluSize, ptsSeconds, isKey);
    }

    [[nodiscard]] bool pushTsPackets(const std::uint8_t* tsData, std::size_t tsSize)
    {
        if ((tsData == nullptr) || (tsSize == 0U) || ((tsSize % 188U) != 0U)) {
            return false;
        }

        const bool isKey { isMp2tKeyframeChunk(tsData, tsSize) };
        return m_queue.push(tsData, tsSize, 0.0, isKey);
    }

    [[nodiscard]] std::size_t activeClientCount() const noexcept
    {
        if (m_rtspServer != nullptr) {
            return m_rtspServer->clientCount();
        }
        return 0U;
    }

    void setEstimatedBitrate(unsigned estBitrateKbps)
    {
        if (m_subsession != nullptr) {
            m_subsession->setEstimatedBitrate(estBitrateKbps);
        }
    }

    void setH264SpsPps(const std::vector<std::uint8_t>& sps, const std::vector<std::uint8_t>& pps)
    {
        if (m_h264Subsession != nullptr) {
            m_h264Subsession->setSpsPps(sps, pps);
        }
    }

    [[nodiscard]] FrameQueueStats queueStats() const
    {
        return m_queue.getStats();
    }

    [[nodiscard]] const RtspServerConfig& config() const noexcept
    {
        return m_config;
    }

private:
    void scheduleHeartbeat()
    {
        if (m_env != nullptr) {
            m_heartbeatToken = m_env->taskScheduler().scheduleDelayedTask(10000, onHeartbeat0, this);
        }
    }

    static void onHeartbeat0(void* clientData)
    {
        auto* impl { static_cast<Impl*>(clientData) };
        if (impl != nullptr) {
            impl->scheduleHeartbeat();
        }
    }

    static void onWakeTrigger0(void* clientData)
    {
        auto* impl { static_cast<Impl*>(clientData) };
        if ((impl != nullptr) && (impl->m_subsession != nullptr)) {
            impl->m_subsession->wakeAllSources();
        }
    }

    static void onStopTrigger0(void* clientData)
    {
        auto* impl { static_cast<Impl*>(clientData) };
        if (impl != nullptr) {
            impl->m_watchVariable = 1;
        }
    }

    void workerThreadFunc(std::promise<bool> startPromise)
    {
        m_scheduler = BasicTaskScheduler::createNew();
        if (m_scheduler == nullptr) {
            LOG(ERROR) << "Live555RtspServer: Failed to allocate BasicTaskScheduler.";
            startPromise.set_value(false);
            return;
        }

        m_env = BasicUsageEnvironment::createNew(*m_scheduler);
        if (m_env == nullptr) {
            LOG(ERROR) << "Live555RtspServer: Failed to allocate BasicUsageEnvironment.";
            delete m_scheduler;
            m_scheduler = nullptr;
            startPromise.set_value(false);
            return;
        }

        // Set up authentication database if credentials supplied
        if (m_config.username.has_value() && m_config.password.has_value()) {
            auto* rawAuth { new UserAuthenticationDatabase(m_config.realm.c_str(), False) };
            rawAuth->addUserRecord(m_config.username->c_str(), m_config.password->c_str());
            m_authDb.reset(rawAuth);
        }

        // Create custom RTSP server bound to configured or dynamic port
        Port port(m_config.port);
        m_rtspServer = CustomRtspServer::createNew(*m_env, port, m_authDb.get(), 65U);
        if (m_rtspServer == nullptr) {
            LOG(ERROR) << "Live555RtspServer: Failed to bind RTSP server to port " << m_config.port;
            m_authDb.reset();
            m_env->reclaim();
            m_env = nullptr;
            delete m_scheduler;
            m_scheduler = nullptr;
            startPromise.set_value(false);
            return;
        }

        m_boundPort.store(m_rtspServer->portNum());

        // Allocate TaskScheduler triggers
        m_wakeTriggerId = m_env->taskScheduler().createEventTrigger(onWakeTrigger0);
        m_stopTriggerId = m_env->taskScheduler().createEventTrigger(onStopTrigger0);

        // Create ServerMediaSession
        m_mediaSession = ServerMediaSession::createNew(*m_env, m_config.streamPath.c_str(), m_config.streamPath.c_str(),
            "PelcoD Live RTSP Egress", m_config.enableMulticast);
        if (m_mediaSession == nullptr) {
            LOG(ERROR) << "Live555RtspServer: Failed to create ServerMediaSession.";
            cleanupServerResources();
            startPromise.set_value(false);
            return;
        }

        // Create and register the appropriate subsession
        if (m_config.streamType == RtspStreamType::H264ElementaryStream) {
            m_h264Subsession = Live555H264LiveSubsession::createNew(*m_env, m_queue, true, 2000U);
            m_subsession = m_h264Subsession;
        } else {
            m_subsession = Live555Mp2tLiveSubsession::createNew(*m_env, m_queue, true, 5000U);
            m_h264Subsession = nullptr;
        }

        if (m_subsession == nullptr) {
            LOG(ERROR) << "Live555RtspServer: Failed to allocate Live555LiveSubsession.";
            cleanupServerResources();
            startPromise.set_value(false);
            return;
        }

        m_mediaSession->addSubsession(m_subsession);
        m_rtspServer->addServerMediaSession(m_mediaSession);

        // Connect queue event trigger to scheduler wake trigger
        m_queue.setEventTrigger([this]() {
            if ((m_scheduler != nullptr) && (m_wakeTriggerId != 0U)) {
                m_scheduler->triggerEvent(m_wakeTriggerId, this);
            }
        });

        // Schedule periodic 10ms tick to ensure select() never stalls
        scheduleHeartbeat();

        m_running.store(true);
        startPromise.set_value(true);

        LOG(INFO) << "Live555RtspServer: Running at " << getUrl("127.0.0.1");

        // Execute Live555 cooperative event loop
        m_scheduler->doEventLoop(&m_watchVariable);

        LOG(INFO) << "Live555RtspServer: Event loop terminated. Cleaning up.";

        cleanupServerResources();
    }

    void cleanupServerResources()
    {
        m_running.store(false);
        m_queue.setEventTrigger(nullptr);

        if (m_env != nullptr) {
            if (m_heartbeatToken != nullptr) {
                m_env->taskScheduler().unscheduleDelayedTask(m_heartbeatToken);
                m_heartbeatToken = nullptr;
            }
            if (m_wakeTriggerId != 0U) {
                m_env->taskScheduler().deleteEventTrigger(m_wakeTriggerId);
                m_wakeTriggerId = 0U;
            }
            if (m_stopTriggerId != 0U) {
                m_env->taskScheduler().deleteEventTrigger(m_stopTriggerId);
                m_stopTriggerId = 0U;
            }
            if (m_rtspServer != nullptr) {
                Medium::close(m_rtspServer);
                m_rtspServer = nullptr;
            }
            m_mediaSession = nullptr;
            m_subsession = nullptr;
            m_h264Subsession = nullptr;

            m_authDb.reset();

            m_env->reclaim();
            m_env = nullptr;
        }

        if (m_scheduler != nullptr) {
            delete m_scheduler;
            m_scheduler = nullptr;
        }
    }

    RtspServerConfig m_config;
    Live555FrameQueue m_queue;

    std::atomic<bool> m_running { false };
    std::atomic<std::uint16_t> m_boundPort { 0U };
    std::thread m_workerThread {};

    TaskScheduler* m_scheduler { nullptr };
    UsageEnvironment* m_env { nullptr };
    CustomRtspServer* m_rtspServer { nullptr };
    ServerMediaSession* m_mediaSession { nullptr };
    Live555LiveSubsession* m_subsession { nullptr };
    Live555H264LiveSubsession* m_h264Subsession { nullptr };

    std::unique_ptr<UserAuthenticationDatabase, void (*)(UserAuthenticationDatabase*)> m_authDb { nullptr,
        [](UserAuthenticationDatabase* db) { delete db; } };

    char m_watchVariable { 0 };
    EventTriggerId m_wakeTriggerId { 0U };
    EventTriggerId m_stopTriggerId { 0U };
    TaskToken m_heartbeatToken { nullptr };
};

// ============================================================================
// Live555RtspServer (Public Wrapper)
// ============================================================================

Live555RtspServer::Live555RtspServer(RtspServerConfig config)
    : m_impl { std::make_unique<Impl>(std::move(config)) }
{
}

Live555RtspServer::~Live555RtspServer() = default;

bool Live555RtspServer::start()
{
    return m_impl->start();
}

void Live555RtspServer::stop()
{
    m_impl->stop();
}

bool Live555RtspServer::isRunning() const noexcept
{
    return m_impl->isRunning();
}

std::uint16_t Live555RtspServer::listeningPort() const noexcept
{
    return m_impl->listeningPort();
}

std::string Live555RtspServer::getUrl(const std::string& host) const
{
    return m_impl->getUrl(host);
}

bool Live555RtspServer::pushH264Nalu(const std::uint8_t* naluData, std::size_t naluSize, double ptsSeconds)
{
    return m_impl->pushH264Nalu(naluData, naluSize, ptsSeconds);
}

bool Live555RtspServer::pushTsPackets(const std::uint8_t* tsData, std::size_t tsSize)
{
    return m_impl->pushTsPackets(tsData, tsSize);
}

std::size_t Live555RtspServer::activeClientCount() const noexcept
{
    return m_impl->activeClientCount();
}

void Live555RtspServer::setEstimatedBitrate(unsigned estBitrateKbps)
{
    m_impl->setEstimatedBitrate(estBitrateKbps);
}

void Live555RtspServer::setH264SpsPps(const std::vector<std::uint8_t>& sps, const std::vector<std::uint8_t>& pps)
{
    m_impl->setH264SpsPps(sps, pps);
}

FrameQueueStats Live555RtspServer::queueStats() const
{
    return m_impl->queueStats();
}

const RtspServerConfig& Live555RtspServer::config() const noexcept
{
    return m_impl->config();
}

} // namespace Video

#else // !PELCOD_HAS_LIVE555

namespace Video {

class Live555RtspServer::Impl final {
public:
    explicit Impl(RtspServerConfig config)
        : m_config { std::move(config) }
    {
    }

    const RtspServerConfig& config() const noexcept
    {
        return m_config;
    }

private:
    RtspServerConfig m_config;
};

Live555RtspServer::Live555RtspServer(RtspServerConfig config)
    : m_impl { std::make_unique<Impl>(std::move(config)) }
{
}

Live555RtspServer::~Live555RtspServer() = default;

bool Live555RtspServer::start()
{
    return false;
}

void Live555RtspServer::stop()
{
}

bool Live555RtspServer::isRunning() const noexcept
{
    return false;
}

std::uint16_t Live555RtspServer::listeningPort() const noexcept
{
    return 0U;
}

std::string Live555RtspServer::getUrl(const std::string& host) const
{
    return "rtsp://" + host + ":0/disabled";
}

bool Live555RtspServer::pushH264Nalu(const std::uint8_t* /*naluData*/, std::size_t /*naluSize*/, double /*ptsSeconds*/)
{
    return false;
}

bool Live555RtspServer::pushTsPackets(const std::uint8_t* /*tsData*/, std::size_t /*tsSize*/)
{
    return false;
}

std::size_t Live555RtspServer::activeClientCount() const noexcept
{
    return 0U;
}

void Live555RtspServer::setEstimatedBitrate(unsigned /*estBitrateKbps*/)
{
}

void Live555RtspServer::setH264SpsPps(
    const std::vector<std::uint8_t>& /*sps*/, const std::vector<std::uint8_t>& /*pps*/)
{
}

FrameQueueStats Live555RtspServer::queueStats() const
{
    return FrameQueueStats {};
}

const RtspServerConfig& Live555RtspServer::config() const noexcept
{
    return m_impl->config();
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
