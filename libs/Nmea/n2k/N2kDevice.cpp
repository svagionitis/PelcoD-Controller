#include "N2kDevice.h"

#include <cstring>

namespace Nmea::N2k {

namespace {
    inline std::uint32_t readU32LE(const std::uint8_t* p) noexcept
    {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
            | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }
} // namespace

N2kDevice::N2kDevice(std::uint64_t name, std::uint8_t preferredAddress)
    : m_addressClaimer(name, preferredAddress)
{
    m_posCallbacks = std::make_shared<std::vector<std::pair<std::size_t, PositionCallback>>>();
    m_cogSogCallbacks = std::make_shared<std::vector<std::pair<std::size_t, CogSogCallback>>>();
    m_headingCallbacks = std::make_shared<std::vector<std::pair<std::size_t, HeadingCallback>>>();
    m_attitudeCallbacks = std::make_shared<std::vector<std::pair<std::size_t, AttitudeCallback>>>();
    m_aisACallbacks = std::make_shared<std::vector<std::pair<std::size_t, AisClassACallback>>>();
    m_aisBCallbacks = std::make_shared<std::vector<std::pair<std::size_t, AisClassBCallback>>>();
    m_windCallbacks = std::make_shared<std::vector<std::pair<std::size_t, WindCallback>>>();
    m_rudderCallbacks = std::make_shared<std::vector<std::pair<std::size_t, RudderCallback>>>();
    m_magVarCallbacks = std::make_shared<std::vector<std::pair<std::size_t, MagVariationCallback>>>();
    m_systemTimeCallbacks = std::make_shared<std::vector<std::pair<std::size_t, SystemTimeCallback>>>();
    m_heartbeatCallbacks = std::make_shared<std::vector<std::pair<std::size_t, HeartbeatCallback>>>();
    m_pgnCallbacks = std::make_shared<std::vector<PgnSubscription>>();
}

void N2kDevice::onCanFrame(const CanFrame& frame)
{
    m_addressClaimer.processCanFrame(frame);
    auto maybeMsg = m_assembler.processCanFrame(frame);
    if (maybeMsg.has_value()) {
        onN2kMessage(*maybeMsg);
    }
}

void N2kDevice::onCanFrames(const CanFrame* frames, std::size_t count)
{
    if (frames == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        onCanFrame(frames[i]);
    }
}

void N2kDevice::onRawSocketCanData(const std::uint8_t* bytes, std::size_t len)
{
    if (bytes == nullptr || len < 16U) {
        return;
    }

    constexpr std::size_t kSocketCanFrameSize = 16U;
    const std::size_t numFrames = len / kSocketCanFrameSize;

    for (std::size_t i = 0; i < numFrames; ++i) {
        const std::uint8_t* framePtr = bytes + (i * kSocketCanFrameSize);
        CanFrame cf {};
        cf.id = readU32LE(framePtr) & 0x1FFFFFFFU; // Mask 29-bit CAN ID
        cf.dlc = std::min<std::uint8_t>(framePtr[4], 8U);
        std::memcpy(cf.data.data(), framePtr + 8, 8U);
        onCanFrame(cf);
    }
}

void N2kDevice::onN2kMessage(const N2kMessage& msg)
{
    // 1. Snapshot raw PGN callbacks for lock-free notification
    CallbackList<RawPgnCallback> rawSubscribers;
    std::shared_ptr<const std::vector<PgnSubscription>> pgnSubs;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        pgnSubs = m_pgnCallbacks;
    }

    for (const auto& sub : *pgnSubs) {
        if (sub.pgn == 0U || sub.pgn == msg.header.pgn) {
            sub.cb(msg);
        }
    }

    // 2. Decode standard PGNs
    const auto pgn = static_cast<Pgn>(msg.header.pgn);
    const auto now = std::chrono::steady_clock::now();

    switch (pgn) {
    case Pgn::PositionRapidUpdate: {
        PositionRapid pos {};
        if (N2kDecoder::parsePgn129025(msg.payload.data(), msg.payload.size(), pos)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestPosition = pos;
            }
            CallbackList<PositionCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_posCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(pos);
            }
        }
        break;
    }
    case Pgn::CogSogRapidUpdate: {
        CogSogRapid cogSog {};
        if (N2kDecoder::parsePgn129026(msg.payload.data(), msg.payload.size(), cogSog)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestCogSog = cogSog;
            }
            CallbackList<CogSogCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_cogSogCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(cogSog);
            }
        }
        break;
    }
    case Pgn::VesselHeading: {
        VesselHeading hdg {};
        if (N2kDecoder::parsePgn127250(msg.payload.data(), msg.payload.size(), hdg)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestHeading = hdg;
            }
            CallbackList<HeadingCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_headingCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(hdg);
            }
        }
        break;
    }
    case Pgn::Attitude: {
        Attitude att {};
        if (N2kDecoder::parsePgn127257(msg.payload.data(), msg.payload.size(), att)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestAttitude = att;
            }
            CallbackList<AttitudeCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_attitudeCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(att);
            }
        }
        break;
    }
    case Pgn::AisClassAPositionReport: {
        AisClassAPosition ais {};
        if (N2kDecoder::parsePgn129038(msg.payload.data(), msg.payload.size(), ais)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_aisClassATargets[ais.mmsi] = TrackedAisA { ais, now };
            }
            CallbackList<AisClassACallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_aisACallbacks;
            }
            for (const auto& item : *subs) {
                item.second(ais);
            }
        }
        break;
    }
    case Pgn::AisClassBPositionReport: {
        AisClassBPosition ais {};
        if (N2kDecoder::parsePgn129039(msg.payload.data(), msg.payload.size(), ais)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_aisClassBTargets[ais.mmsi] = TrackedAisB { ais, now };
            }
            CallbackList<AisClassBCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_aisBCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(ais);
            }
        }
        break;
    }
    case Pgn::WindData: {
        WindData wind {};
        if (N2kDecoder::parsePgn130306(msg.payload.data(), msg.payload.size(), wind)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestWind = wind;
            }
            CallbackList<WindCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_windCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(wind);
            }
        }
        break;
    }
    case Pgn::Rudder: {
        RudderData rudder {};
        if (N2kDecoder::parsePgn127245(msg.payload.data(), msg.payload.size(), rudder)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestRudder = rudder;
            }
            CallbackList<RudderCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_rudderCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(rudder);
            }
        }
        break;
    }
    case Pgn::MagneticVariation: {
        MagneticVariation magVar {};
        if (N2kDecoder::parsePgn127258(msg.payload.data(), msg.payload.size(), magVar)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestMagVariation = magVar;
            }
            CallbackList<MagVariationCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_magVarCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(magVar);
            }
        }
        break;
    }
    case Pgn::SystemTime: {
        SystemTimeData sysTime {};
        if (N2kDecoder::parsePgn126992(msg.payload.data(), msg.payload.size(), sysTime)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestSystemTime = sysTime;
            }
            CallbackList<SystemTimeCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_systemTimeCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(sysTime);
            }
        }
        break;
    }
    case Pgn::Heartbeat: {
        HeartbeatData hb {};
        if (N2kDecoder::parsePgn126993(msg.payload.data(), msg.payload.size(), hb)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_latestHeartbeat = hb;
            }
            CallbackList<HeartbeatCallback> subs;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                subs = m_heartbeatCallbacks;
            }
            for (const auto& item : *subs) {
                item.second(hb);
            }
        }
        break;
    }
    default:
        break;
    }
}

// --- Subscription Helpers ---

template <typename T> std::size_t N2kDevice::registerCallback(CallbackList<T>& list, T cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    const std::size_t id = m_nextSubscriptionId++;
    auto updated = std::make_shared<std::vector<std::pair<std::size_t, T>>>(*list);
    updated->emplace_back(id, std::move(cb));
    list = updated;
    return id;
}

template <typename T> void N2kDevice::unregisterCallback(CallbackList<T>& list, std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto updated = std::make_shared<std::vector<std::pair<std::size_t, T>>>();
    updated->reserve(list->size());
    for (const auto& item : *list) {
        if (item.first != id) {
            updated->push_back(item);
        }
    }
    list = updated;
}

std::size_t N2kDevice::addPositionCallback(PositionCallback cb)
{
    return registerCallback(m_posCallbacks, std::move(cb));
}

void N2kDevice::removePositionCallback(std::size_t id)
{
    unregisterCallback(m_posCallbacks, id);
}

std::size_t N2kDevice::addCogSogCallback(CogSogCallback cb)
{
    return registerCallback(m_cogSogCallbacks, std::move(cb));
}

void N2kDevice::removeCogSogCallback(std::size_t id)
{
    unregisterCallback(m_cogSogCallbacks, id);
}

std::size_t N2kDevice::addHeadingCallback(HeadingCallback cb)
{
    return registerCallback(m_headingCallbacks, std::move(cb));
}

void N2kDevice::removeHeadingCallback(std::size_t id)
{
    unregisterCallback(m_headingCallbacks, id);
}

std::size_t N2kDevice::addAttitudeCallback(AttitudeCallback cb)
{
    return registerCallback(m_attitudeCallbacks, std::move(cb));
}

void N2kDevice::removeAttitudeCallback(std::size_t id)
{
    unregisterCallback(m_attitudeCallbacks, id);
}

std::size_t N2kDevice::addAisClassACallback(AisClassACallback cb)
{
    return registerCallback(m_aisACallbacks, std::move(cb));
}

void N2kDevice::removeAisClassACallback(std::size_t id)
{
    unregisterCallback(m_aisACallbacks, id);
}

std::size_t N2kDevice::addAisClassBCallback(AisClassBCallback cb)
{
    return registerCallback(m_aisBCallbacks, std::move(cb));
}

void N2kDevice::removeAisClassBCallback(std::size_t id)
{
    unregisterCallback(m_aisBCallbacks, id);
}

std::size_t N2kDevice::addWindCallback(WindCallback cb)
{
    return registerCallback(m_windCallbacks, std::move(cb));
}

void N2kDevice::removeWindCallback(std::size_t id)
{
    unregisterCallback(m_windCallbacks, id);
}

std::size_t N2kDevice::addPgnCallback(std::uint32_t pgn, RawPgnCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    const std::size_t id = m_nextSubscriptionId++;
    auto updated = std::make_shared<std::vector<PgnSubscription>>(*m_pgnCallbacks);
    updated->push_back(PgnSubscription { id, pgn, std::move(cb) });
    m_pgnCallbacks = updated;
    return id;
}

void N2kDevice::removePgnCallback(std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto updated = std::make_shared<std::vector<PgnSubscription>>();
    updated->reserve(m_pgnCallbacks->size());
    for (const auto& item : *m_pgnCallbacks) {
        if (item.id != id) {
            updated->push_back(item);
        }
    }
    m_pgnCallbacks = updated;
}

// --- Telemetry Cache Accessors ---

std::optional<PositionRapid> N2kDevice::position() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestPosition;
}

std::optional<CogSogRapid> N2kDevice::cogSog() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestCogSog;
}

std::optional<VesselHeading> N2kDevice::heading() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestHeading;
}

std::optional<Attitude> N2kDevice::attitude() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestAttitude;
}

std::optional<WindData> N2kDevice::wind() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestWind;
}

std::optional<AisClassAPosition> N2kDevice::aisClassATarget(std::uint32_t mmsi) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    const auto it = m_aisClassATargets.find(mmsi);
    if (it != m_aisClassATargets.end()) {
        return it->second.data;
    }
    return std::nullopt;
}

std::optional<AisClassBPosition> N2kDevice::aisClassBTarget(std::uint32_t mmsi) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    const auto it = m_aisClassBTargets.find(mmsi);
    if (it != m_aisClassBTargets.end()) {
        return it->second.data;
    }
    return std::nullopt;
}

std::size_t N2kDevice::aisTargetCount() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_aisClassATargets.size() + m_aisClassBTargets.size();
}

void N2kDevice::pruneAisTargets(std::chrono::seconds ttl)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_stateMutex);

    for (auto it = m_aisClassATargets.begin(); it != m_aisClassATargets.end();) {
        if ((now - it->second.timestamp) > ttl) {
            it = m_aisClassATargets.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_aisClassBTargets.begin(); it != m_aisClassBTargets.end();) {
        if ((now - it->second.timestamp) > ttl) {
            it = m_aisClassBTargets.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t N2kDevice::addRudderCallback(RudderCallback cb)
{
    return registerCallback(m_rudderCallbacks, std::move(cb));
}

void N2kDevice::removeRudderCallback(std::size_t id)
{
    unregisterCallback(m_rudderCallbacks, id);
}

std::size_t N2kDevice::addMagVariationCallback(MagVariationCallback cb)
{
    return registerCallback(m_magVarCallbacks, std::move(cb));
}

void N2kDevice::removeMagVariationCallback(std::size_t id)
{
    unregisterCallback(m_magVarCallbacks, id);
}

std::size_t N2kDevice::addSystemTimeCallback(SystemTimeCallback cb)
{
    return registerCallback(m_systemTimeCallbacks, std::move(cb));
}

void N2kDevice::removeSystemTimeCallback(std::size_t id)
{
    unregisterCallback(m_systemTimeCallbacks, id);
}

std::size_t N2kDevice::addHeartbeatCallback(HeartbeatCallback cb)
{
    return registerCallback(m_heartbeatCallbacks, std::move(cb));
}

void N2kDevice::removeHeartbeatCallback(std::size_t id)
{
    unregisterCallback(m_heartbeatCallbacks, id);
}

std::optional<RudderData> N2kDevice::rudder() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestRudder;
}

std::optional<MagneticVariation> N2kDevice::magneticVariation() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestMagVariation;
}

std::optional<SystemTimeData> N2kDevice::systemTime() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestSystemTime;
}

std::optional<HeartbeatData> N2kDevice::heartbeat() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_latestHeartbeat;
}

} // namespace Nmea::N2k
