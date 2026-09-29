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

std::size_t NmeaDevice::addEmergencyBeaconCallback(EmergencyBeaconCallback cb)
{
    return addCallbackInternal(m_emergencyCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeEmergencyBeaconCallback(std::size_t id)
{
    removeCallbackInternal(m_emergencyCallbacks, m_callbackMutex, id);
}

std::vector<AisEmergencyAlert> NmeaDevice::activeEmergencyBeacons() const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    std::vector<AisEmergencyAlert> beacons;
    beacons.reserve(m_emergencyBeacons.size());
    for (const auto& [mmsi, alert] : m_emergencyBeacons) {
        beacons.push_back(alert);
    }
    return beacons;
}

std::optional<AisEmergencyAlert> NmeaDevice::emergencyBeacon(std::uint32_t mmsi) const
{
    std::lock_guard<std::mutex> lock(m_targetMutex);
    const auto it = m_emergencyBeacons.find(mmsi);
    if (it != m_emergencyBeacons.end()) {
        return it->second;
    }
    return std::nullopt;
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

std::size_t NmeaDevice::addRmbCallback(RmbCallback cb)
{
    return addCallbackInternal(m_rmbCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeRmbCallback(std::size_t id)
{
    removeCallbackInternal(m_rmbCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addRteCallback(RteCallback cb)
{
    return addCallbackInternal(m_rteCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeRteCallback(std::size_t id)
{
    removeCallbackInternal(m_rteCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addWplCallback(WplCallback cb)
{
    return addCallbackInternal(m_wplCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeWplCallback(std::size_t id)
{
    removeCallbackInternal(m_wplCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addMtwCallback(MtwCallback cb)
{
    return addCallbackInternal(m_mtwCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeMtwCallback(std::size_t id)
{
    removeCallbackInternal(m_mtwCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addMmbCallback(MmbCallback cb)
{
    return addCallbackInternal(m_mmbCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeMmbCallback(std::size_t id)
{
    removeCallbackInternal(m_mmbCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addMdaCallback(MdaCallback cb)
{
    return addCallbackInternal(m_mdaCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeMdaCallback(std::size_t id)
{
    removeCallbackInternal(m_mdaCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addEnvironmentCallback(EnvironmentCallback cb)
{
    return addCallbackInternal(m_envCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeEnvironmentCallback(std::size_t id)
{
    removeCallbackInternal(m_envCallbacks, m_callbackMutex, id);
}

std::size_t NmeaDevice::addThermalAdviceCallback(ThermalAdviceCallback cb)
{
    return addCallbackInternal(m_thermalAdviceCallbacks, m_callbackMutex, std::move(cb));
}

void NmeaDevice::removeThermalAdviceCallback(std::size_t id)
{
    removeCallbackInternal(m_thermalAdviceCallbacks, m_callbackMutex, id);
}

std::shared_ptr<NmeaRouteManager> NmeaDevice::routeManager() const noexcept
{
    return m_routeManager;
}

std::shared_ptr<ThermalTuningAdvisor> NmeaDevice::thermalAdvisor() const noexcept
{
    return m_thermalAdvisor;
}

std::shared_ptr<Arbiter::NmeaSensorArbiter> NmeaDevice::arbiter() const noexcept
{
    return m_arbiter;
}

NmeaEnvironmentSnapshot NmeaDevice::environmentSnapshot() const
{
    return m_thermalAdvisor ? m_thermalAdvisor->snapshot() : NmeaEnvironmentSnapshot {};
}

ThermalTuningAdvice NmeaDevice::thermalAdvice() const
{
    return m_thermalAdvisor ? m_thermalAdvisor->advice() : ThermalTuningAdvice {};
}

std::optional<RmbData> NmeaDevice::lastRmb() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastRmb;
}

std::optional<MtwData> NmeaDevice::lastMtw() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastMtw;
}

std::optional<MmbData> NmeaDevice::lastMmb() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastMmb;
}

std::optional<MdaData> NmeaDevice::lastMda() const
{
    std::lock_guard<std::mutex> lock(m_maritimeMutex);
    return m_lastMda;
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
            if (m_arbiter) {
                m_arbiter->updateGps(Arbiter::GpsSourceId::Primary, gga);
            }
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
            if (m_arbiter) {
                m_arbiter->updateGps(Arbiter::GpsSourceId::Primary, rmc);
            }
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
            if (m_arbiter) {
                m_arbiter->updateHeading(Arbiter::HeadingSourceId::Primary, hdt.headingDegrees);
            }
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
            if (m_arbiter) {
                m_arbiter->updateHeading(Arbiter::HeadingSourceId::Primary, ths.headingDegrees);
            }
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
            if (m_arbiter && m_navSnapshot.hasAttitude) {
                m_arbiter->updateAttitude(
                    Arbiter::HeadingSourceId::Primary, m_navSnapshot.pitchDegrees, m_navSnapshot.rollDegrees);
            }
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
            std::optional<AisEmergencyAlert> alertOpt;
            if (aisTarget.isEmergencyBeacon || aisTarget.messageType == AisMessageType::SafetyBroadcast14) {
                alertOpt = aisTarget.toEmergencyAlert();
            }

            {
                std::lock_guard<std::mutex> lock(m_targetMutex);
                m_aisTargets[aisTarget.mmsi] = { aisTarget, now };
                if (alertOpt) {
                    m_emergencyBeacons[alertOpt->mmsi] = *alertOpt;
                }
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, AisCallback>>> aisCbs;
            std::shared_ptr<const std::vector<std::pair<std::size_t, EmergencyBeaconCallback>>> emergencyCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                aisCbs = m_aisCallbacks.entries;
                if (alertOpt) {
                    emergencyCbs = m_emergencyCallbacks.entries;
                }
            }
            for (const auto& item : *aisCbs) {
                if (item.second) {
                    item.second(aisTarget);
                }
            }
            if (alertOpt && emergencyCbs) {
                for (const auto& item : *emergencyCbs) {
                    if (item.second) {
                        item.second(*alertOpt);
                    }
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
    case NmeaSentenceId::RMB: {
        RmbData rmb {};
        if (NmeaSentenceParser::parseRmb(sentence, rmb, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastRmb = rmb;
            }
            if (m_routeManager) {
                m_routeManager->ingestRmb(rmb);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, RmbCallback>>> rmbCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                rmbCbs = m_rmbCallbacks.entries;
            }
            for (const auto& item : *rmbCbs) {
                if (item.second) {
                    item.second(rmb);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::RTE: {
        RteData rte {};
        if (NmeaSentenceParser::parseRte(sentence, rte, true)) {
            if (m_routeManager) {
                m_routeManager->ingestRte(rte);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, RteCallback>>> rteCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                rteCbs = m_rteCallbacks.entries;
            }
            for (const auto& item : *rteCbs) {
                if (item.second) {
                    item.second(rte);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::WPL: {
        WplData wpl {};
        if (NmeaSentenceParser::parseWpl(sentence, wpl, true)) {
            if (m_routeManager) {
                m_routeManager->ingestWpl(wpl);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, WplCallback>>> wplCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                wplCbs = m_wplCallbacks.entries;
            }
            for (const auto& item : *wplCbs) {
                if (item.second) {
                    item.second(wpl);
                }
            }
        }
        break;
    }
    case NmeaSentenceId::MTW: {
        MtwData mtw {};
        if (NmeaSentenceParser::parseMtw(sentence, mtw, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastMtw = mtw;
            }
            if (m_thermalAdvisor) {
                m_thermalAdvisor->ingestMtw(mtw);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, MtwCallback>>> mtwCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                mtwCbs = m_mtwCallbacks.entries;
            }
            for (const auto& item : *mtwCbs) {
                if (item.second) {
                    item.second(mtw);
                }
            }
            notifyEnvironmentAndThermal();
        }
        break;
    }
    case NmeaSentenceId::MMB: {
        MmbData mmb {};
        if (NmeaSentenceParser::parseMmb(sentence, mmb, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastMmb = mmb;
            }
            if (m_thermalAdvisor) {
                m_thermalAdvisor->ingestMmb(mmb);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, MmbCallback>>> mmbCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                mmbCbs = m_mmbCallbacks.entries;
            }
            for (const auto& item : *mmbCbs) {
                if (item.second) {
                    item.second(mmb);
                }
            }
            notifyEnvironmentAndThermal();
        }
        break;
    }
    case NmeaSentenceId::MDA: {
        MdaData mda {};
        if (NmeaSentenceParser::parseMda(sentence, mda, true)) {
            {
                std::lock_guard<std::mutex> lock(m_maritimeMutex);
                m_lastMda = mda;
            }
            if (m_thermalAdvisor) {
                m_thermalAdvisor->ingestMda(mda);
            }
            std::shared_ptr<const std::vector<std::pair<std::size_t, MdaCallback>>> mdaCbs;
            {
                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                mdaCbs = m_mdaCallbacks.entries;
            }
            for (const auto& item : *mdaCbs) {
                if (item.second) {
                    item.second(mda);
                }
            }
            notifyEnvironmentAndThermal();
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

void NmeaDevice::notifyEnvironmentAndThermal()
{
    if (!m_thermalAdvisor) {
        return;
    }
    const auto env = m_thermalAdvisor->snapshot();
    const auto adv = m_thermalAdvisor->advice();

    std::shared_ptr<const std::vector<std::pair<std::size_t, EnvironmentCallback>>> envCbs;
    std::shared_ptr<const std::vector<std::pair<std::size_t, ThermalAdviceCallback>>> advCbs;
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        envCbs = m_envCallbacks.entries;
        advCbs = m_thermalAdviceCallbacks.entries;
    }
    for (const auto& item : *envCbs) {
        if (item.second) {
            item.second(env);
        }
    }
    for (const auto& item : *advCbs) {
        if (item.second) {
            item.second(adv);
        }
    }
}

} // namespace Nmea
