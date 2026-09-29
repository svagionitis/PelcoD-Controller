#include "BridgeAlertManager.h"
#include "../NmeaSentenceBuilder.h"

#include <chrono>

namespace Nmea::Bam {

BridgeAlertManager::BridgeAlertManager(const BamConfig& config)
    : m_config { config }
    , m_lastHbtTime { std::chrono::steady_clock::now() }
    , m_lastAlcTime { std::chrono::steady_clock::now() }
{
}

AlertRecord BridgeAlertManager::registerAlert(
    std::uint32_t alertId,
    std::uint32_t instance,
    AlertPriority priority,
    AlertCategory category,
    std::string title,
    std::string text)
{
    AlertRecord record {};
    std::string alfSentence {};
    std::string alrSentence {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const AlertKey key { alertId, instance };
        auto it = m_alerts.find(key);
        const auto now = std::chrono::steady_clock::now();

        if (it != m_alerts.end()) {
            record = it->second;
            record.priority = priority;
            record.category = category;
            record.alertTitle = std::move(title);
            record.alertText = std::move(text);

            if (priority == AlertPriority::Caution) {
                record.state = AlertState::ActiveAcknowledged;
            } else if (record.state == AlertState::Normal) {
                record.state = AlertState::ActiveUnacknowledged;
            }
            record.revisionCounter = (record.revisionCounter % 99U) + 1U;
            record.lastStateChangeTime = now;
            it->second = record;
        } else {
            record.alertIdentifier = alertId;
            record.alertInstance = instance;
            record.priority = priority;
            record.category = category;
            record.state = (priority == AlertPriority::Caution) ? AlertState::ActiveAcknowledged
                                                                : AlertState::ActiveUnacknowledged;
            record.revisionCounter = 1U;
            record.escalationCounter = 0U;
            record.alertTitle = std::move(title);
            record.alertText = std::move(text);
            record.lastStateChangeTime = now;
            m_alerts[key] = record;
        }

        alfSentence = generateAlf(record);
        if (m_config.legacyAlrEnabled) {
            alrSentence = generateLegacyAlr(record);
        }
    }

    broadcastSentence(alfSentence);
    if (!alrSentence.empty()) {
        broadcastSentence(alrSentence);
    }
    notifyAlertChange(record);
    return record;
}

bool BridgeAlertManager::rectifyAlert(std::uint32_t alertId, std::uint32_t instance)
{
    AlertRecord record {};
    std::string alfSentence {};
    std::string alrSentence {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const AlertKey key { alertId, instance };
        auto it = m_alerts.find(key);
        if (it == m_alerts.end()) {
            return false;
        }

        record = it->second;
        if (record.state == AlertState::ActiveUnacknowledged) {
            record.state = AlertState::RectifiedUnacknowledged;
        } else {
            record.state = AlertState::Normal;
        }
        record.revisionCounter = (record.revisionCounter % 99U) + 1U;
        record.isSilenced = false;
        record.lastStateChangeTime = std::chrono::steady_clock::now();
        it->second = record;

        alfSentence = generateAlf(record);
        if (m_config.legacyAlrEnabled) {
            alrSentence = generateLegacyAlr(record);
        }
    }

    broadcastSentence(alfSentence);
    if (!alrSentence.empty()) {
        broadcastSentence(alrSentence);
    }
    notifyAlertChange(record);
    return true;
}

bool BridgeAlertManager::acknowledgeAlert(std::uint32_t alertId, std::uint32_t instance)
{
    AlertRecord record {};
    std::string alfSentence {};
    std::string alrSentence {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const AlertKey key { alertId, instance };
        auto it = m_alerts.find(key);
        if (it == m_alerts.end()) {
            return false;
        }

        record = it->second;
        if (record.state == AlertState::RectifiedUnacknowledged) {
            record.state = AlertState::Normal;
        } else if (record.state == AlertState::ActiveUnacknowledged || record.state == AlertState::ActiveSilenced) {
            record.state = AlertState::ActiveAcknowledged;
        }
        record.revisionCounter = (record.revisionCounter % 99U) + 1U;
        record.isSilenced = false;
        record.lastStateChangeTime = std::chrono::steady_clock::now();
        it->second = record;

        alfSentence = generateAlf(record);
        if (m_config.legacyAlrEnabled) {
            alrSentence = generateLegacyAlr(record);
        }
    }

    broadcastSentence(alfSentence);
    if (!alrSentence.empty()) {
        broadcastSentence(alrSentence);
    }
    notifyAlertChange(record);
    return true;
}

bool BridgeAlertManager::silenceAlert(std::uint32_t alertId, std::uint32_t instance)
{
    AlertRecord record {};
    std::string alfSentence {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const AlertKey key { alertId, instance };
        auto it = m_alerts.find(key);
        if (it == m_alerts.end()) {
            return false;
        }

        record = it->second;
        if (record.state == AlertState::ActiveUnacknowledged) {
            record.state = AlertState::ActiveSilenced;
            record.isSilenced = true;
            record.silencedTime = std::chrono::steady_clock::now();
            record.revisionCounter = (record.revisionCounter % 99U) + 1U;
            it->second = record;
            alfSentence = generateAlf(record);
        } else {
            return false;
        }
    }

    broadcastSentence(alfSentence);
    notifyAlertChange(record);
    return true;
}

bool BridgeAlertManager::transferAlert(std::uint32_t alertId, std::uint32_t instance)
{
    AlertRecord record {};
    std::string alfSentence {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const AlertKey key { alertId, instance };
        auto it = m_alerts.find(key);
        if (it == m_alerts.end()) {
            return false;
        }

        record = it->second;
        if (record.state != AlertState::Normal) {
            record.state = AlertState::ActiveResponsibilityTransferred;
            record.revisionCounter = (record.revisionCounter % 99U) + 1U;
            record.lastStateChangeTime = std::chrono::steady_clock::now();
            it->second = record;
            alfSentence = generateAlf(record);
        } else {
            return false;
        }
    }

    broadcastSentence(alfSentence);
    notifyAlertChange(record);
    return true;
}

bool BridgeAlertManager::processArcCommand(const ArcData& arc)
{
    if (arc.command == 'A' || arc.command == static_cast<char>(AlertCommand::Acknowledge)) {
        return acknowledgeAlert(arc.alertIdentifier, arc.alertInstance);
    }
    if (arc.command == 'Q' || arc.command == static_cast<char>(AlertCommand::RequestSilence)) {
        return silenceAlert(arc.alertIdentifier, arc.alertInstance);
    }
    if (arc.command == 'O' || arc.command == static_cast<char>(AlertCommand::RequestResponsibilityTransfer)) {
        return transferAlert(arc.alertIdentifier, arc.alertInstance);
    }
    return false;
}

bool BridgeAlertManager::processAckCommand(const AckData& ack)
{
    return acknowledgeAlert(ack.alertIdentifier, 1U);
}

void BridgeAlertManager::pollEscalations(std::chrono::steady_clock::time_point now)
{
    std::vector<std::string> pendingBroadcasts {};
    std::vector<AlertRecord> updatedAlerts {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& pair : m_alerts) {
            AlertRecord& rec = pair.second;

            // 1. Check silence timeout: revert ActiveSilenced -> ActiveUnacknowledged
            if (rec.state == AlertState::ActiveSilenced && rec.isSilenced) {
                const auto elapsedSilence = std::chrono::duration_cast<std::chrono::seconds>(now - rec.silencedTime);
                if (elapsedSilence >= m_config.silenceTimeoutSec) {
                    rec.state = AlertState::ActiveUnacknowledged;
                    rec.isSilenced = false;
                    rec.revisionCounter = (rec.revisionCounter % 99U) + 1U;
                    rec.lastStateChangeTime = now;
                    pendingBroadcasts.push_back(generateAlf(rec));
                    updatedAlerts.push_back(rec);
                }
            }

            // 2. Check escalation timeout: increment escalationCounter for unack alarms
            if (rec.state == AlertState::ActiveUnacknowledged) {
                const auto elapsedUnack = std::chrono::duration_cast<std::chrono::seconds>(now - rec.lastStateChangeTime);
                if (elapsedUnack >= m_config.escalationTimeoutSec && rec.escalationCounter < 9U) {
                    rec.escalationCounter++;
                    rec.revisionCounter = (rec.revisionCounter % 99U) + 1U;
                    rec.lastStateChangeTime = now;
                    pendingBroadcasts.push_back(generateAlf(rec));
                    updatedAlerts.push_back(rec);
                }
            }
        }

        // 3. Periodic Heartbeat
        if (now - m_lastHbtTime >= m_config.heartbeatIntervalSec) {
            pendingBroadcasts.push_back(generateHeartbeat());
            m_lastHbtTime = now;
        }

        // 4. Periodic ALC Cyclic Alert List
        if (now - m_lastAlcTime >= m_config.cyclicAlertListIntervalSec) {
            pendingBroadcasts.push_back(generateAlertList());
            m_lastAlcTime = now;
        }
    }

    for (const auto& sentence : pendingBroadcasts) {
        broadcastSentence(sentence);
    }
    for (const auto& rec : updatedAlerts) {
        notifyAlertChange(rec);
    }
}

std::string BridgeAlertManager::generateHeartbeat()
{
    HbtData hbt {};
    hbt.configuredIntervalSec = static_cast<double>(m_config.heartbeatIntervalSec.count());
    hbt.equipmentStatus = 'A';
    hbt.sequentialSentenceId = m_hbtSeqId;
    m_hbtSeqId = static_cast<std::uint8_t>((m_hbtSeqId + 1U) % 10U);
    hbt.valid = true;
    return NmeaSentenceBuilder::buildHbt(hbt, m_config.talkerId);
}

std::string BridgeAlertManager::generateAlertList()
{
    AlcData alc {};
    alc.totalSentences = 1U;
    alc.sentenceNumber = 1U;
    alc.sequentialMessageId = m_alcSeqId;
    m_alcSeqId = static_cast<std::uint8_t>((m_alcSeqId + 1U) % 10U);

    for (const auto& pair : m_alerts) {
        if (pair.second.state != AlertState::Normal) {
            AlcEntry entry {};
            entry.alertIdentifier = pair.second.alertIdentifier;
            entry.alertInstance = pair.second.alertInstance;
            entry.revisionCounter = pair.second.revisionCounter;
            alc.alertEntries.push_back(entry);
        }
    }
    alc.alertCount = static_cast<std::uint8_t>(alc.alertEntries.size());
    alc.valid = true;
    return NmeaSentenceBuilder::buildAlc(alc, m_config.talkerId);
}

std::string BridgeAlertManager::generateAlf(const AlertRecord& record)
{
    AlfData alf {};
    alf.totalSentences = 1U;
    alf.sentenceNumber = 1U;
    alf.sequentialMessageId = 0U;
    alf.alertPriority = static_cast<char>(record.priority);
    alf.alertCategory = static_cast<char>(record.category);
    alf.alertState = static_cast<char>(record.state);
    alf.alertIdentifier = record.alertIdentifier;
    alf.alertInstance = record.alertInstance;
    alf.revisionCounter = record.revisionCounter;
    alf.escalationCounter = record.escalationCounter;
    alf.alertText = record.alertText.empty() ? record.alertTitle : record.alertText;
    alf.valid = true;
    return NmeaSentenceBuilder::buildAlf(alf, m_config.talkerId);
}

std::string BridgeAlertManager::generateLegacyAlr(const AlertRecord& record)
{
    AlrData alr {};
    alr.alertIdentifier = record.alertIdentifier;
    alr.condition = (record.state == AlertState::Normal) ? 'V' : 'A';
    alr.acknowledgeState = (record.state == AlertState::ActiveAcknowledged || record.state == AlertState::Normal) ? 'A' : 'V';
    alr.alertText = record.alertTitle.empty() ? record.alertText : record.alertTitle;
    alr.valid = true;
    return NmeaSentenceBuilder::buildAlr(alr, m_config.talkerId);
}

std::vector<AlertRecord> BridgeAlertManager::activeAlerts() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AlertRecord> res {};
    res.reserve(m_alerts.size());
    for (const auto& pair : m_alerts) {
        if (pair.second.state != AlertState::Normal) {
            res.push_back(pair.second);
        }
    }
    return res;
}

std::optional<AlertRecord> BridgeAlertManager::alertById(std::uint32_t alertId, std::uint32_t instance) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const AlertKey key { alertId, instance };
    const auto it = m_alerts.find(key);
    if (it != m_alerts.end()) {
        return it->second;
    }
    return std::nullopt;
}

void BridgeAlertManager::setConfig(const BamConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

BamConfig BridgeAlertManager::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void BridgeAlertManager::setBroadcastCallback(BroadcastCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_broadcastCb = std::move(cb);
}

void BridgeAlertManager::setStateCallback(AlertCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alertCb = std::move(cb);
}

void BridgeAlertManager::broadcastSentence(const std::string& sentence)
{
    BroadcastCallback cb {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_broadcastCb;
    }
    if (cb && !sentence.empty()) {
        cb(sentence);
    }
}

void BridgeAlertManager::notifyAlertChange(const AlertRecord& record)
{
    AlertCallback cb {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_alertCb;
    }
    if (cb) {
        cb(record);
    }
}

} // namespace Nmea::Bam
