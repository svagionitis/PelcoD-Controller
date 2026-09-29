#include "NmeaDevice.h"

#include <algorithm>

namespace Nmea {

NmeaDevice::NmeaDevice(std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer)
    : m_transport(std::move(transport))
    , m_accumulator(maxAccumulatorBuffer)
    , m_aisDecoder()
{
}

NmeaDevice::~NmeaDevice()
{
    stop();
}

bool NmeaDevice::start()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (m_running.load()) {
        return true;
    }

    if (m_transport) {
        m_transport->setDataCallback([this](const std::vector<std::uint8_t>& data) { handleIncomingBytes(data); });
        m_transport->setStateCallback(
            [this](Transport::TransportState state, const std::string& err) { handleTransportState(state, err); });

        if (!m_transport->isOpen()) {
            (void)m_transport->open();
        }
    }

    m_running.store(true);
    return isConnected();
}

void NmeaDevice::stop()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    if (m_transport) {
        m_transport->setDataCallback(nullptr);
        m_transport->setStateCallback(nullptr);
        m_transport->close();
    }

    m_accumulator.clear();
    m_aisDecoder.reset();
}

bool NmeaDevice::isConnected() const noexcept
{
    return m_transport && m_transport->isOpen();
}

std::shared_ptr<Transport::ITransport> NmeaDevice::transport() const noexcept
{
    return m_transport;
}

bool NmeaDevice::sendSentence(std::string_view sentence, bool appendChecksum)
{
    if (sentence.empty() || !m_transport || !m_transport->isOpen()) {
        return false;
    }

    std::string toSend {};
    if (appendChecksum) {
        toSend = NmeaChecksum::frameSentence(sentence);
    } else {
        toSend = std::string(sentence);
        if (toSend.empty() || (toSend.back() != '\n')) {
            toSend += "\r\n";
        }
    }

    const std::vector<std::uint8_t> bytes(toSend.begin(), toSend.end());
    const bool success = m_transport->sendData(bytes);

    if (success) {
        std::shared_ptr<const std::vector<std::pair<std::size_t, RawSentenceCallback>>> rawCbs;
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            rawCbs = m_rawCallbacks.entries;
        }
        for (const auto& item : *rawCbs) {
            if (item.second) {
                item.second(toSend, true);
            }
        }
    }

    return success;
}

NmeaNavSnapshot NmeaDevice::navSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_navMutex);
    return m_navSnapshot;
}

std::vector<TtmData> NmeaDevice::activeRadarTargets() const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    std::vector<TtmData> targets {};
    targets.reserve(m_radarTargets.size());
    for (const auto& kv : m_radarTargets) {
        targets.push_back(kv.second.data);
    }
    return targets;
}

std::optional<TtmData> NmeaDevice::radarTarget(std::uint32_t targetNumber) const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    const auto it = m_radarTargets.find(targetNumber);
    if (it != m_radarTargets.end()) {
        return it->second.data;
    }
    return std::nullopt;
}

std::vector<AisVesselTarget> NmeaDevice::activeAisTargets() const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    std::vector<AisVesselTarget> targets {};
    targets.reserve(m_aisTargets.size());
    for (const auto& kv : m_aisTargets) {
        targets.push_back(kv.second.data);
    }
    return targets;
}

std::optional<AisVesselTarget> NmeaDevice::aisTarget(std::uint32_t mmsi) const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    const auto it = m_aisTargets.find(mmsi);
    if (it != m_aisTargets.end()) {
        return it->second.data;
    }
    return std::nullopt;
}

void NmeaDevice::pruneStaleTargets(std::chrono::milliseconds radarTtl, std::chrono::milliseconds aisTtl)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_targetMutex);

    for (auto it = m_radarTargets.begin(); it != m_radarTargets.end();) {
        if (now - it->second.lastUpdated > radarTtl) {
            it = m_radarTargets.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_aisTargets.begin(); it != m_aisTargets.end();) {
        if (now - it->second.lastUpdated > aisTtl) {
            it = m_aisTargets.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t NmeaDevice::addNavCallback(NavCallback cb)
{
    return addCallbackInternal(m_navCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeNavCallback(std::size_t id)
{
    removeCallbackInternal(m_navCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addRadarCallback(RadarCallback cb)
{
    return addCallbackInternal(m_radarCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeRadarCallback(std::size_t id)
{
    removeCallbackInternal(m_radarCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addAisCallback(AisCallback cb)
{
    return addCallbackInternal(m_aisCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeAisCallback(std::size_t id)
{
    removeCallbackInternal(m_aisCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addRawCallback(RawSentenceCallback cb)
{
    return addCallbackInternal(m_rawCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeRawCallback(std::size_t id)
{
    removeCallbackInternal(m_rawCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addRsdCallback(RsdCallback cb)
{
    return addCallbackInternal(m_rsdCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeRsdCallback(std::size_t id)
{
    removeCallbackInternal(m_rsdCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addApbCallback(ApbCallback cb)
{
    return addCallbackInternal(m_apbCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeApbCallback(std::size_t id)
{
    removeCallbackInternal(m_apbCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addMwvCallback(MwvCallback cb)
{
    return addCallbackInternal(m_mwvCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeMwvCallback(std::size_t id)
{
    removeCallbackInternal(m_mwvCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addHdgCallback(HdgCallback cb)
{
    return addCallbackInternal(m_hdgCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeHdgCallback(std::size_t id)
{
    removeCallbackInternal(m_hdgCallbacks, m_callbackMutex, id);
}

std::optional<RsdData> NmeaDevice::lastRsd() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastRsd;
}

std::optional<ApbData> NmeaDevice::lastApb() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastApb;
}

std::optional<MwvData> NmeaDevice::lastMwv() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastMwv;
}

std::optional<HdgData> NmeaDevice::lastHdg() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastHdg;
}

void NmeaDevice::feedRawBytes(const std::vector<std::uint8_t>& rawData)
{
    handleIncomingBytes(rawData);
}

void NmeaDevice::handleIncomingBytes(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) {
        return;
    }

    const auto sentences = m_accumulator.push(data.data(), data.size(), true);
    for (const auto& sentence : sentences) {
        processSentence(sentence);
    }
}

void NmeaDevice::handleTransportState(Transport::TransportState /*state*/, const std::string& /*errorMsg*/)
{
    // Transport state transitions can be tracked or logged if needed
}

void NmeaDevice::processSentence(std::string_view sentence)
{
    // 1. Dispatch raw sentence subscribers
    std::shared_ptr<const std::vector<std::pair<std::size_t, RawSentenceCallback>>> rawCbs;
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        rawCbs = m_rawCallbacks.entries;
    }
    for (const auto& item : *rawCbs) {
        if (item.second) {
            item.second(sentence, false);
        }
    }

    const auto id = NmeaSentenceParser::identifySentence(sentence);
    const auto now = std::chrono::steady_clock::now();

    bool navUpdated { false };
    NmeaNavSnapshot currentNav {};

    switch (id) {
    case NmeaSentenceId::GGA: {
        GgaData gga {};
        if (NmeaSentenceParser::parseGga(sentence, gga, true)) {
            std::lock_guard<std::mutex> lock(m_navMutex);
            m_navSnapshot.position = gga.coordinates;
            m_navSnapshot.altitudeMeters = gga.altitudeMeters;
            m_navSnapshot.fixQuality = gga.fixQuality;
            m_navSnapshot.hasPosition = (gga.fixQuality != NmeaFixQuality::Invalid);
            m_navSnapshot.timestamp = now;
            currentNav = m_navSnapshot;
            navUpdated = true;
        }
        break;
    }
    case NmeaSentenceId::RMC: {
        RmcData rmc {};
        if (NmeaSentenceParser::parseRmc(sentence, rmc, true)) {
            std::lock_guard<std::mutex> lock(m_navMutex);
            m_navSnapshot.position = rmc.coordinates;
            m_navSnapshot.sogKnots = rmc.speedOverGroundKnots;
            m_navSnapshot.cogDegrees = rmc.courseOverGroundDegrees;
            m_navSnapshot.hasPosition = rmc.statusActive;
            m_navSnapshot.timestamp = now;
            currentNav = m_navSnapshot;
            navUpdated = true;
        }
        break;
    }
    case NmeaSentenceId::HDT: {
        HdtData hdt {};
        if (NmeaSentenceParser::parseHdt(sentence, hdt, true)) {
            std::lock_guard<std::mutex> lock(m_navMutex);
            m_navSnapshot.trueHeadingDegrees = hdt.headingDegrees;
            m_navSnapshot.hasHeading = true;
            m_navSnapshot.timestamp = now;
            currentNav = m_navSnapshot;
            navUpdated = true;
        }
        break;
    }
    case NmeaSentenceId::THS: {
        ThsData ths {};
        if (NmeaSentenceParser::parseThs(sentence, ths, true)) {
            std::lock_guard<std::mutex> lock(m_navMutex);
            m_navSnapshot.trueHeadingDegrees = ths.headingDegrees;
            m_navSnapshot.hasHeading = (ths.mode != NmeaFaaMode::NotValid);
            m_navSnapshot.timestamp = now;
            currentNav = m_navSnapshot;
            navUpdated = true;
        }
        break;
    }
    case NmeaSentenceId::XDR: {
        XdrData xdr {};
        if (NmeaSentenceParser::parseXdr(sentence, xdr, true)) {
            std::lock_guard<std::mutex> lock(m_navMutex);
            for (const auto& tr : xdr.transducers) {
                if (tr.id == "PITCH") {
                    m_navSnapshot.pitchDegrees = tr.measurement;
                    m_navSnapshot.hasAttitude = true;
                } else if (tr.id == "ROLL") {
                    m_navSnapshot.rollDegrees = tr.measurement;
                    m_navSnapshot.hasAttitude = true;
                }
            }
            m_navSnapshot.timestamp = now;
            currentNav = m_navSnapshot;
            navUpdated = true;
        }
        break;
    }
    case NmeaSentenceId::TTM: {
        TtmData ttm {};
        if (NmeaSentenceParser::parseTtm(sentence, ttm, true)) {
            {
                std::lock_guard<std::mutex> lock(m_targetMutex);
                m_radarTargets[ttm.targetNumber] = { ttm, now };
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, RadarCallback>>> radarCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                radarCbs = m_radarCallbacks.entries;
            }
            for (const auto& item : *radarCbs) {
                if (item.second) {
                    item.second(ttm);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::TLL: {
        TllData tll {};
        if (NmeaSentenceParser::parseTll(sentence, tll, true)) {
            TtmData ttm {};
            ttm.targetNumber = tll.targetNumber;
            ttm.targetName = tll.targetName;
            ttm.status = tll.status;
            ttm.referenceTarget = tll.referenceTarget;
            ttm.utcTimeTag = tll.utcTimeTag;
            ttm.valid = true;

            {
                std::lock_guard<std::mutex> lock(m_targetMutex);
                m_radarTargets[ttm.targetNumber] = { ttm, now };
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, RadarCallback>>> radarCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                radarCbs = m_radarCallbacks.entries;
            }
            for (const auto& item : *radarCbs) {
                if (item.second) {
                    item.second(ttm);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::VDM:
    case NmeaSentenceId::VDO: {
        AisVesselTarget aisTarget {};
        if (m_aisDecoder.decodeSentence(sentence, aisTarget, true)) {
            {
                std::lock_guard<std::mutex> lock(m_targetMutex);
                m_aisTargets[aisTarget.mmsi] = { aisTarget, now };
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, AisCallback>>> aisCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                aisCbs = m_aisCallbacks.entries;
            }
            for (const auto& item : *aisCbs) {
                if (item.second) {
                    item.second(aisTarget);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::RSD: {
        RsdData rsd {};
        if (NmeaSentenceParser::parseRsd(sentence, rsd, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastRsd = rsd;
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, RsdCallback>>> rsdCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                rsdCbs = m_rsdCallbacks.entries;
            }
            for (const auto& item : *rsdCbs) {
                if (item.second) {
                    item.second(rsd);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::APB: {
        ApbData apb {};
        if (NmeaSentenceParser::parseApb(sentence, apb, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastApb = apb;
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, ApbCallback>>> apbCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                apbCbs = m_apbCallbacks.entries;
            }
            for (const auto& item : *apbCbs) {
                if (item.second) {
                    item.second(apb);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::MWV: {
        MwvData mwv {};
        if (NmeaSentenceParser::parseMwv(sentence, mwv, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastMwv = mwv;
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, MwvCallback>>> mwvCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                mwvCbs = m_mwvCallbacks.entries;
            }
            for (const auto& item : *mwvCbs) {
                if (item.second) {
                    item.second(mwv);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::HDG: {
        HdgData hdg {};
        if (NmeaSentenceParser::parseHdg(sentence, hdg, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastHdg = hdg;
            }
            {
                std::lock_guard<std::mutex> lock(m_navMutex);
                if (!m_navSnapshot.hasHeading) {
                    double trueHdg = hdg.magneticHeadingDeg;
                    if (hdg.hasVariation) {
                        trueHdg += hdg.magneticVariationDeg;
                        if (trueHdg < 0.0) {
                            trueHdg += 360.0;
                        }
                        if (trueHdg >= 360.0) {
                            trueHdg -= 360.0;
                        }
                    }
                    m_navSnapshot.trueHeadingDegrees = trueHdg;
                    m_navSnapshot.hasHeading = true;
                    m_navSnapshot.timestamp = now;
                    currentNav = m_navSnapshot;
                    navUpdated = true;
                }
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, HdgCallback>>> hdgCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                hdgCbs = m_hdgCallbacks.entries;
            }
            for (const auto& item : *hdgCbs) {
                if (item.second) {
                    item.second(hdg);
                }
            }
        }
        break;
    }
    default:
        break;
    }

    if (navUpdated) {
        std::shared_ptr<const std::vector<std::pair<std::size_t, NavCallback>>> navCbs;
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            navCbs = m_navCallbacks.entries;
        }
        for (const auto& item : *navCbs) {
            if (item.second) {
                item.second(currentNav);
            }
        }
    }
}

} // namespace Nmea
