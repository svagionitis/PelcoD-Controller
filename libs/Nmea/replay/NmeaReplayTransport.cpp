#include "NmeaReplayTransport.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Nmea {

namespace {

    [[nodiscard]] std::string_view trimWhitespace(std::string_view sv) noexcept
    {
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
            sv.remove_prefix(1U);
        }
        while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
            sv.remove_suffix(1U);
        }
        return sv;
    }

    [[nodiscard]] bool parseTwoDigits(const char* ptr, int& val) noexcept
    {
        if (std::isdigit(static_cast<unsigned char>(ptr[0])) == 0
            || std::isdigit(static_cast<unsigned char>(ptr[1])) == 0) {
            return false;
        }
        val = (ptr[0] - '0') * 10 + (ptr[1] - '0');
        return true;
    }

} // namespace

NmeaReplayTransport::NmeaReplayTransport()
{
    m_running.store(true);
    m_readThread = std::thread(&NmeaReplayTransport::playbackWorker, this);
}

NmeaReplayTransport::~NmeaReplayTransport()
{
    close();
    m_running.store(false);
    {
        std::lock_guard<std::mutex> lock(m_replayMutex);
        m_cv.notify_all();
    }
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
}

bool NmeaReplayTransport::loadFile(const std::string& filePath, std::chrono::milliseconds defaultInterval)
{
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return loadFromMemory(ss.str(), defaultInterval);
}

bool NmeaReplayTransport::loadFromMemory(std::string_view logContent, std::chrono::milliseconds defaultInterval)
{
    stop();

    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_records.clear();
    m_currentIndex = 0U;
    m_currentOffset = std::chrono::microseconds(0);

    const auto interval = (defaultInterval.count() > 0)
        ? std::chrono::duration_cast<std::chrono::microseconds>(defaultInterval)
        : std::chrono::microseconds(100000);

    parseLines(logContent, interval);
    emitProgressLocked();
    return !m_records.empty();
}

void NmeaReplayTransport::clear()
{
    stop();
    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_records.clear();
    m_currentIndex = 0U;
    m_currentOffset = std::chrono::microseconds(0);
    emitProgressLocked();
}

bool NmeaReplayTransport::open()
{
    m_open.store(true);
    notifyState(Transport::TransportState::Connected, "Maritime Replay Transport Ready");
    return true;
}

void NmeaReplayTransport::close()
{
    stop();
    if (m_open.exchange(false)) {
        notifyState(Transport::TransportState::Disconnected, "Maritime Replay Transport Closed");
    }
}

bool NmeaReplayTransport::isOpen() const noexcept
{
    return m_open.load();
}

bool NmeaReplayTransport::sendData(const std::vector<std::uint8_t>& /*data*/)
{
    // Replay transport is an input source; transmitted bytes are accepted/discarded
    return true;
}

void NmeaReplayTransport::play()
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    if (m_records.empty()) {
        return;
    }

    if (m_state.load() == ReplayState::Finished) {
        m_currentIndex = 0U;
        m_currentOffset = std::chrono::microseconds(0);
    }

    m_playbackAnchorReal = std::chrono::steady_clock::now();
    m_playbackAnchorOffset = m_currentOffset;
    m_state.store(ReplayState::Playing);
    m_cv.notify_all();
    emitProgressLocked();
}

void NmeaReplayTransport::pause()
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    if (m_state.load() == ReplayState::Playing) {
        m_state.store(ReplayState::Paused);
        m_cv.notify_all();
        emitProgressLocked();
    }
}

void NmeaReplayTransport::stop()
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_state.store(ReplayState::Stopped);
    m_currentIndex = 0U;
    m_currentOffset = std::chrono::microseconds(0);
    m_playbackAnchorOffset = std::chrono::microseconds(0);
    m_cv.notify_all();
    emitProgressLocked();
}

void NmeaReplayTransport::stepForward()
{
    std::string lineToEmit {};
    {
        std::lock_guard<std::mutex> lock(m_replayMutex);
        if (m_records.empty() || m_currentIndex >= m_records.size()) {
            return;
        }

        m_state.store(ReplayState::Paused);

        const auto& record = m_records[m_currentIndex];
        m_currentOffset = record.offsetFromStart;
        if (isSentenceAllowed(record.sentence)) {
            lineToEmit = record.sentence;
        }
        ++m_currentIndex;

        if (m_currentIndex >= m_records.size() && !m_loop.load()) {
            m_state.store(ReplayState::Finished);
        }

        m_playbackAnchorReal = std::chrono::steady_clock::now();
        m_playbackAnchorOffset = m_currentOffset;
        emitProgressLocked();
    }

    if (!lineToEmit.empty()) {
        const std::vector<std::uint8_t> data(lineToEmit.begin(), lineToEmit.end());
        invokeDataCallback(data);
    }
}

bool NmeaReplayTransport::seek(std::chrono::microseconds targetOffset)
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    if (m_records.empty()) {
        return false;
    }

    const auto total = m_records.back().offsetFromStart;
    if (targetOffset < std::chrono::microseconds(0)) {
        targetOffset = std::chrono::microseconds(0);
    } else if (targetOffset > total) {
        targetOffset = total;
    }

    // Binary search for closest record matching targetOffset
    auto it = std::lower_bound(m_records.begin(), m_records.end(), targetOffset,
        [](const ReplayRecord& rec, std::chrono::microseconds offset) { return rec.offsetFromStart < offset; });

    if (it == m_records.end()) {
        m_currentIndex = m_records.size() - 1U;
    } else {
        m_currentIndex = static_cast<std::size_t>(std::distance(m_records.begin(), it));
    }

    m_currentOffset = m_records[m_currentIndex].offsetFromStart;
    m_playbackAnchorReal = std::chrono::steady_clock::now();
    m_playbackAnchorOffset = m_currentOffset;

    if (m_state.load() == ReplayState::Finished && m_currentIndex < m_records.size() - 1U) {
        m_state.store(ReplayState::Paused);
    }

    m_cv.notify_all();
    emitProgressLocked();
    return true;
}

bool NmeaReplayTransport::seekRatio(double ratio01)
{
    if (ratio01 < 0.0) {
        ratio01 = 0.0;
    } else if (ratio01 > 1.0) {
        ratio01 = 1.0;
    }
    const auto total = totalDuration();
    const auto targetOffset = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double, std::micro>(static_cast<double>(total.count()) * ratio01));
    return seek(targetOffset);
}

bool NmeaReplayTransport::seekLine(std::size_t lineIndex)
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    if (m_records.empty()) {
        return false;
    }

    if (lineIndex >= m_records.size()) {
        lineIndex = m_records.size() - 1U;
    }

    m_currentIndex = lineIndex;
    m_currentOffset = m_records[m_currentIndex].offsetFromStart;
    m_playbackAnchorReal = std::chrono::steady_clock::now();
    m_playbackAnchorOffset = m_currentOffset;

    if (m_state.load() == ReplayState::Finished && m_currentIndex < m_records.size() - 1U) {
        m_state.store(ReplayState::Paused);
    }

    m_cv.notify_all();
    emitProgressLocked();
    return true;
}

void NmeaReplayTransport::setSpeedMultiplier(double multiplier) noexcept
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_speedMultiplier.store((multiplier >= 0.0) ? multiplier : 1.0);
    m_playbackAnchorReal = std::chrono::steady_clock::now();
    m_playbackAnchorOffset = m_currentOffset;
    m_cv.notify_all();
}

double NmeaReplayTransport::speedMultiplier() const noexcept
{
    return m_speedMultiplier.load();
}

void NmeaReplayTransport::setLoop(bool loop) noexcept
{
    m_loop.store(loop);
}

bool NmeaReplayTransport::isLooping() const noexcept
{
    return m_loop.load();
}

void NmeaReplayTransport::setSentenceFilter(std::vector<std::string> allowedSentenceIds)
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_filterIds = std::move(allowedSentenceIds);
}

void NmeaReplayTransport::clearSentenceFilter()
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    m_filterIds.clear();
}

ReplayState NmeaReplayTransport::state() const noexcept
{
    return m_state.load();
}

ReplayProgress NmeaReplayTransport::progress() const
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    ReplayProgress p {};
    p.state = m_state.load();
    p.currentOffset = m_currentOffset;
    p.totalDuration = m_records.empty() ? std::chrono::microseconds(0) : m_records.back().offsetFromStart;
    p.currentLine = m_currentIndex;
    p.totalLines = m_records.size();
    p.speedMultiplier = m_speedMultiplier.load();
    p.isLooping = m_loop.load();
    p.progressRatio = (p.totalDuration.count() > 0)
        ? std::clamp(
            static_cast<double>(p.currentOffset.count()) / static_cast<double>(p.totalDuration.count()), 0.0, 1.0)
        : 0.0;
    return p;
}

std::size_t NmeaReplayTransport::totalLines() const noexcept
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    return m_records.size();
}

std::chrono::microseconds NmeaReplayTransport::totalDuration() const noexcept
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    return m_records.empty() ? std::chrono::microseconds(0) : m_records.back().offsetFromStart;
}

std::size_t NmeaReplayTransport::currentLine() const noexcept
{
    std::lock_guard<std::mutex> lock(m_replayMutex);
    return m_currentIndex;
}

std::size_t NmeaReplayTransport::addProgressCallback(ProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_progressMutex);
    const std::size_t id = m_nextCallbackId++;
    m_progressCallbacks.emplace_back(id, std::move(cb));
    return id;
}

void NmeaReplayTransport::removeProgressCallback(std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_progressMutex);
    m_progressCallbacks.erase(std::remove_if(m_progressCallbacks.begin(), m_progressCallbacks.end(),
                                  [id](const auto& pair) { return pair.first == id; }),
        m_progressCallbacks.end());
}

void NmeaReplayTransport::emitProgressLocked()
{
    ReplayProgress p {};
    p.state = m_state.load();
    p.currentOffset = m_currentOffset;
    p.totalDuration = m_records.empty() ? std::chrono::microseconds(0) : m_records.back().offsetFromStart;
    p.currentLine = m_currentIndex;
    p.totalLines = m_records.size();
    p.speedMultiplier = m_speedMultiplier.load();
    p.isLooping = m_loop.load();
    p.progressRatio = (p.totalDuration.count() > 0)
        ? std::clamp(
            static_cast<double>(p.currentOffset.count()) / static_cast<double>(p.totalDuration.count()), 0.0, 1.0)
        : 0.0;

    std::vector<ProgressCallback> callbacks;
    {
        std::lock_guard<std::mutex> pLock(m_progressMutex);
        callbacks.reserve(m_progressCallbacks.size());
        for (const auto& [id, cb] : m_progressCallbacks) {
            if (cb) {
                callbacks.push_back(cb);
            }
        }
    }

    for (const auto& cb : callbacks) {
        cb(p);
    }
}

bool NmeaReplayTransport::isSentenceAllowed(std::string_view sentence) const
{
    if (m_filterIds.empty()) {
        return true;
    }

    // Strip leading tag block if present
    if (!sentence.empty() && sentence.front() == '\\') {
        const auto endTag = sentence.find('\\', 1U);
        if (endTag != std::string_view::npos) {
            sentence = sentence.substr(endTag + 1U);
        }
    }
    // Strip leading $ or !
    if (!sentence.empty() && (sentence.front() == '$' || sentence.front() == '!')) {
        sentence.remove_prefix(1U);
    }

    // Isolate talker & sentence ID (first token before comma)
    const auto commaPos = sentence.find(',');
    const auto header = (commaPos != std::string_view::npos) ? sentence.substr(0U, commaPos) : sentence;

    for (const auto& filter : m_filterIds) {
        if (header.find(filter) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

std::optional<std::chrono::system_clock::time_point> NmeaReplayTransport::extractTimestamp(
    std::string_view line, TimestampSource& outSource) const
{
    // 1. Tag Block format: \c:<timestamp>*hh\ (IEC 61162-1)
    if (line.size() > 5U && line.front() == '\\') {
        const auto secondSlash = line.find('\\', 1U);
        if (secondSlash != std::string_view::npos) {
            const auto tagContent = line.substr(1U, secondSlash - 1U);
            const auto cPos = tagContent.find("c:");
            if (cPos != std::string_view::npos) {
                const auto valStart = cPos + 2U;
                auto valEnd = tagContent.find_first_of(",*", valStart);
                if (valEnd == std::string_view::npos) {
                    valEnd = tagContent.size();
                }
                const auto numStr = tagContent.substr(valStart, valEnd - valStart);
                std::uint64_t epochVal { 0U };
                const auto res = std::from_chars(numStr.data(), numStr.data() + numStr.size(), epochVal);
                if (res.ec == std::errc {}) {
                    outSource = TimestampSource::TagBlock;
                    if (epochVal > 1000000000000ULL) { // Milliseconds
                        return std::chrono::system_clock::time_point(std::chrono::milliseconds(epochVal));
                    }
                    return std::chrono::system_clock::time_point(std::chrono::seconds(epochVal));
                }
            }
        }
    }

    // 2. Syslog / ISO Log prefix [YYYY-MM-DD hh:mm:ss.zzz]
    if (line.size() > 21U && line.front() == '[') {
        const auto closeBracket = line.find(']');
        if (closeBracket != std::string_view::npos && closeBracket >= 19U) {
            const auto dateStr = line.substr(1U, closeBracket - 1U);
            // Expected format: YYYY-MM-DD[T ]hh:mm:ss(.zzz)
            if (dateStr.size() >= 19U && dateStr[4] == '-' && dateStr[7] == '-') {
                std::tm tm {};
                int year { 0 };
                int month { 0 };
                int day { 0 };
                int hour { 0 };
                int min { 0 };
                int sec { 0 };
                if (parseTwoDigits(dateStr.data(), year) && parseTwoDigits(dateStr.data() + 2, year)
                    && parseTwoDigits(dateStr.data() + 5, month) && parseTwoDigits(dateStr.data() + 8, day)
                    && parseTwoDigits(dateStr.data() + 11, hour) && parseTwoDigits(dateStr.data() + 14, min)
                    && parseTwoDigits(dateStr.data() + 17, sec)) {
                    // Re-parse year fully
                    year = (dateStr[0] - '0') * 1000 + (dateStr[1] - '0') * 100 + (dateStr[2] - '0') * 10
                        + (dateStr[3] - '0');
                    tm.tm_year = year - 1900;
                    tm.tm_mon = month - 1;
                    tm.tm_mday = day;
                    tm.tm_hour = hour;
                    tm.tm_min = min;
                    tm.tm_sec = sec;
                    tm.tm_isdst = 0;

#if defined(_WIN32)
                    const std::time_t epochSec = _mkgmtime(&tm);
#else
                    const std::time_t epochSec = timegm(&tm);
#endif
                    if (epochSec != static_cast<std::time_t>(-1)) {
                        int millisec { 0 };
                        if (dateStr.size() >= 23U && (dateStr[19] == '.' || dateStr[19] == ',')) {
                            std::from_chars(dateStr.data() + 20, dateStr.data() + dateStr.size(), millisec);
                        }
                        outSource = TimestampSource::LogPrefix;
                        return std::chrono::system_clock::time_point(
                            std::chrono::seconds(epochSec) + std::chrono::milliseconds(millisec));
                    }
                }
            }
        }
    }

    outSource = TimestampSource::Synthesized;
    return std::nullopt;
}

void NmeaReplayTransport::parseLines(std::string_view content, std::chrono::microseconds defaultInterval)
{
    std::optional<std::chrono::system_clock::time_point> firstTime {};
    std::chrono::microseconds accumulatedOffset { 0 };

    std::size_t lineStart { 0U };
    while (lineStart < content.size()) {
        auto lineEnd = content.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = content.size();
        }

        std::string_view rawLine = content.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1U;

        rawLine = trimWhitespace(rawLine);
        if (rawLine.empty()) {
            continue;
        }

        TimestampSource source { TimestampSource::Synthesized };
        const auto absTime = extractTimestamp(rawLine, source);

        // Strip log prefix [TIMESTAMP] if it was parsed as LogPrefix
        if (source == TimestampSource::LogPrefix && rawLine.front() == '[') {
            const auto closeBracket = rawLine.find(']');
            if (closeBracket != std::string_view::npos && closeBracket + 1U < rawLine.size()) {
                rawLine = trimWhitespace(rawLine.substr(closeBracket + 1U));
            }
        }

        if (rawLine.empty()) {
            continue;
        }

        // Calculate relative offset from beginning
        std::chrono::microseconds offset { 0 };
        if (absTime.has_value()) {
            if (!firstTime.has_value()) {
                firstTime = absTime;
                offset = std::chrono::microseconds(0);
            } else {
                const auto diff = std::chrono::duration_cast<std::chrono::microseconds>(*absTime - *firstTime);
                if (diff >= accumulatedOffset) {
                    offset = diff;
                } else {
                    offset = accumulatedOffset; // Prevent time reversals
                }
            }
        } else {
            // Synthesized pacing
            if (!m_records.empty()) {
                offset = accumulatedOffset + defaultInterval;
            } else {
                offset = std::chrono::microseconds(0);
            }
        }

        accumulatedOffset = offset;

        std::string sentence(rawLine);
        if (sentence.back() != '\n') {
            sentence += "\r\n";
        }

        ReplayRecord record {};
        record.offsetFromStart = offset;
        record.absoluteTime = absTime;
        record.timeSource = source;
        record.sentence = std::move(sentence);

        m_records.push_back(std::move(record));
    }
}

void NmeaReplayTransport::playbackWorker()
{
    while (m_running.load()) {
        std::string sentenceToSend {};

        {
            std::unique_lock<std::mutex> lock(m_replayMutex);

            // Wait until Playing or terminating
            m_cv.wait(lock, [this]() { return !m_running.load() || m_state.load() == ReplayState::Playing; });

            if (!m_running.load()) {
                break;
            }

            if (m_state.load() != ReplayState::Playing) {
                continue;
            }

            // Check EOF
            if (m_records.empty() || m_currentIndex >= m_records.size()) {
                if (m_loop.load() && !m_records.empty()) {
                    m_currentIndex = 0U;
                    m_currentOffset = std::chrono::microseconds(0);
                    m_playbackAnchorReal = std::chrono::steady_clock::now();
                    m_playbackAnchorOffset = std::chrono::microseconds(0);
                    emitProgressLocked();
                    continue;
                }
                m_state.store(ReplayState::Finished);
                emitProgressLocked();
                continue;
            }

            const auto& record = m_records[m_currentIndex];
            const double speed = m_speedMultiplier.load();

            if (speed > 0.0) {
                // Calculate target time with drift compensation
                const auto recordOffsetFromAnchor = record.offsetFromStart - m_playbackAnchorOffset;
                const auto realDelayMicro
                    = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double, std::micro>(
                        static_cast<double>(recordOffsetFromAnchor.count()) / speed));
                const auto targetRealTime = m_playbackAnchorReal + realDelayMicro;

                const auto now = std::chrono::steady_clock::now();
                if (targetRealTime > now) {
                    m_cv.wait_until(lock, targetRealTime,
                        [this]() { return !m_running.load() || m_state.load() != ReplayState::Playing; });

                    if (!m_running.load() || m_state.load() != ReplayState::Playing) {
                        continue;
                    }
                }
            }

            // Retrieve sentence
            m_currentOffset = record.offsetFromStart;
            if (isSentenceAllowed(record.sentence)) {
                sentenceToSend = record.sentence;
            }
            ++m_currentIndex;

            if (m_currentIndex >= m_records.size() && !m_loop.load()) {
                m_state.store(ReplayState::Finished);
            }

            emitProgressLocked();
        }

        if (!sentenceToSend.empty()) {
            const std::vector<std::uint8_t> data(sentenceToSend.begin(), sentenceToSend.end());
            invokeDataCallback(data);
        }
    }
}

} // namespace Nmea
