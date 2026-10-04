#include "TelemetryTimeIndex.h"

#include <algorithm>
#include <cmath>

namespace Klv {

void TelemetryTimeIndex::addVideoEntry(const VideoIndexEntry& entry)
{
    m_videoEntries.push_back(entry);
    m_finalized = false;
}

void TelemetryTimeIndex::addKlvEntry(const KlvIndexEntry& entry)
{
    m_klvEntries.push_back(entry);
    m_finalized = false;
}

void TelemetryTimeIndex::finalize()
{
    if (m_finalized) {
        return;
    }

    // If KLV packets have uninitialized or constant PES PTS but valid Tag 2 UTC timestamps,
    // synthesize PTS ticks aligned to base video PTS using UTC delta
    if (!m_klvEntries.empty()) {
        bool allPtsSame = true;
        const std::uint64_t firstKlvPts = m_klvEntries.front().ptsTicks;
        for (const auto& k : m_klvEntries) {
            if (k.ptsTicks != firstKlvPts) {
                allPtsSame = false;
                break;
            }
        }

        std::uint64_t firstUtc = 0U;
        for (const auto& k : m_klvEntries) {
            if (k.utcTimestampUs > 0U) {
                firstUtc = k.utcTimestampUs;
                break;
            }
        }

        if (allPtsSame && firstUtc > 0U) {
            const std::uint64_t baseVidPts = !m_videoEntries.empty() ? m_videoEntries.front().ptsTicks : firstKlvPts;
            for (auto& k : m_klvEntries) {
                if (k.utcTimestampUs >= firstUtc) {
                    const std::uint64_t deltaUs = k.utcTimestampUs - firstUtc;
                    k.ptsTicks = baseVidPts + ((deltaUs * 90000U) / 1000000U);
                } else {
                    k.ptsTicks = baseVidPts;
                }
            }
        }
    }

    std::stable_sort(m_videoEntries.begin(), m_videoEntries.end(),
        [](const VideoIndexEntry& a, const VideoIndexEntry& b) noexcept {
            return a.ptsTicks < b.ptsTicks;
        });

    std::stable_sort(m_klvEntries.begin(), m_klvEntries.end(),
        [](const KlvIndexEntry& a, const KlvIndexEntry& b) noexcept {
            return a.ptsTicks < b.ptsTicks;
        });

    m_keyframeIndices.clear();
    for (std::size_t i = 0U; i < m_videoEntries.size(); ++i) {
        m_videoEntries[i].frameIndex = static_cast<std::uint32_t>(i);
        if (m_videoEntries[i].isKeyframe) {
            m_keyframeIndices.push_back(i);
        }
    }

    for (std::size_t i = 0U; i < m_klvEntries.size(); ++i) {
        m_klvEntries[i].messageIndex = static_cast<std::uint32_t>(i);
    }

    m_basePts = 0U;
    if (!m_videoEntries.empty() && !m_klvEntries.empty()) {
        m_basePts = std::min(m_videoEntries.front().ptsTicks, m_klvEntries.front().ptsTicks);
    } else if (!m_videoEntries.empty()) {
        m_basePts = m_videoEntries.front().ptsTicks;
    } else if (!m_klvEntries.empty()) {
        m_basePts = m_klvEntries.front().ptsTicks;
    }

    m_baseUtcUs = 0U;
    if (!m_klvEntries.empty() && m_klvEntries.front().utcTimestampUs > 0U) {
        m_baseUtcUs = m_klvEntries.front().utcTimestampUs;
    }

    std::uint64_t maxPts { m_basePts };
    if (!m_videoEntries.empty()) {
        maxPts = std::max(maxPts, m_videoEntries.back().ptsTicks);
    }
    if (!m_klvEntries.empty()) {
        maxPts = std::max(maxPts, m_klvEntries.back().ptsTicks);
    }

    if (maxPts >= m_basePts) {
        m_durationSeconds = static_cast<double>(maxPts - m_basePts) / 90000.0;
    } else {
        m_durationSeconds = 0.0;
    }

    m_finalized = true;
}

void TelemetryTimeIndex::clear() noexcept
{
    m_videoEntries.clear();
    m_klvEntries.clear();
    m_keyframeIndices.clear();
    m_basePts = 0U;
    m_baseUtcUs = 0U;
    m_durationSeconds = 0.0;
    m_finalized = false;
}

std::optional<VideoIndexEntry> TelemetryTimeIndex::findPrecedingKeyframe(std::uint64_t pts) const noexcept
{
    if (m_keyframeIndices.empty()) {
        return std::nullopt;
    }

    auto it = std::upper_bound(m_keyframeIndices.begin(), m_keyframeIndices.end(), pts,
        [this](std::uint64_t target, std::size_t kfIdx) noexcept {
            return target < m_videoEntries[kfIdx].ptsTicks;
        });

    if (it == m_keyframeIndices.begin()) {
        return m_videoEntries[m_keyframeIndices.front()];
    }

    --it;
    return m_videoEntries[*it];
}

std::optional<VideoIndexEntry> TelemetryTimeIndex::findVideoFrame(std::uint64_t pts) const noexcept
{
    if (m_videoEntries.empty()) {
        return std::nullopt;
    }

    auto it = std::lower_bound(m_videoEntries.begin(), m_videoEntries.end(), pts,
        [](const VideoIndexEntry& entry, std::uint64_t target) noexcept {
            return entry.ptsTicks < target;
        });

    if (it == m_videoEntries.end()) {
        return m_videoEntries.back();
    }
    if (it == m_videoEntries.begin()) {
        return m_videoEntries.front();
    }

    const auto prev = it - 1;
    const std::uint64_t delta1 = pts - prev->ptsTicks;
    const std::uint64_t delta2 = it->ptsTicks - pts;
    return (delta1 <= delta2) ? *prev : *it;
}

std::optional<KlvIndexEntry> TelemetryTimeIndex::findTelemetry(std::uint64_t pts,
                                                              std::uint64_t maxDeltaUs) const noexcept
{
    if (m_klvEntries.empty()) {
        return std::nullopt;
    }

    auto it = std::lower_bound(m_klvEntries.begin(), m_klvEntries.end(), pts,
        [](const KlvIndexEntry& entry, std::uint64_t target) noexcept {
            return entry.ptsTicks < target;
        });

    KlvIndexEntry candidate = m_klvEntries.back();
    if (it == m_klvEntries.begin()) {
        candidate = m_klvEntries.front();
    } else if (it != m_klvEntries.end()) {
        const auto prev = it - 1;
        const std::uint64_t deltaPrev = pts - prev->ptsTicks;
        const std::uint64_t deltaCurr = it->ptsTicks - pts;
        candidate = (deltaPrev <= deltaCurr) ? *prev : *it;
    }

    const std::uint64_t ptsDiff = (pts >= candidate.ptsTicks)
        ? (pts - candidate.ptsTicks)
        : (candidate.ptsTicks - pts);
    const std::uint64_t deltaUs = (ptsDiff * 1000000U) / 90000U;

    if (deltaUs <= maxDeltaUs) {
        return candidate;
    }

    return std::nullopt;
}

bool TelemetryTimeIndex::findTelemetryBounds(std::uint64_t pts,
                                             KlvIndexEntry& before,
                                             KlvIndexEntry& after) const noexcept
{
    if (m_klvEntries.empty()) {
        return false;
    }

    if (m_klvEntries.size() == 1U) {
        before = m_klvEntries.front();
        after = m_klvEntries.front();
        return true;
    }

    if (pts <= m_klvEntries.front().ptsTicks) {
        before = m_klvEntries.front();
        after = m_klvEntries[1];
        return true;
    }

    if (pts >= m_klvEntries.back().ptsTicks) {
        before = m_klvEntries[m_klvEntries.size() - 2U];
        after = m_klvEntries.back();
        return true;
    }

    auto it = std::upper_bound(m_klvEntries.begin(), m_klvEntries.end(), pts,
        [](std::uint64_t target, const KlvIndexEntry& entry) noexcept {
            return target < entry.ptsTicks;
        });

    after = *it;
    before = *(it - 1);
    return true;
}

std::vector<KlvIndexEntry> TelemetryTimeIndex::findWindow(std::uint64_t startPts,
                                                         std::uint64_t endPts) const
{
    std::vector<KlvIndexEntry> result {};
    if (m_klvEntries.empty() || startPts > endPts) {
        return result;
    }

    auto startIt = std::lower_bound(m_klvEntries.begin(), m_klvEntries.end(), startPts,
        [](const KlvIndexEntry& entry, std::uint64_t target) noexcept {
            return entry.ptsTicks < target;
        });

    auto endIt = std::upper_bound(m_klvEntries.begin(), m_klvEntries.end(), endPts,
        [](std::uint64_t target, const KlvIndexEntry& entry) noexcept {
            return target < entry.ptsTicks;
        });

    if (startIt < endIt) {
        result.assign(startIt, endIt);
    }

    return result;
}

double TelemetryTimeIndex::ptsToSeconds(std::uint64_t pts) const noexcept
{
    if (pts < m_basePts) {
        return 0.0;
    }
    return static_cast<double>(pts - m_basePts) / 90000.0;
}

std::uint64_t TelemetryTimeIndex::secondsToPts(double seconds) const noexcept
{
    if (seconds <= 0.0) {
        return m_basePts;
    }
    return m_basePts + static_cast<std::uint64_t>(std::llround(seconds * 90000.0));
}

std::uint64_t TelemetryTimeIndex::ptsToUtc(std::uint64_t pts) const noexcept
{
    if (m_baseUtcUs == 0U) {
        return 0U;
    }
    if (pts < m_basePts) {
        return m_baseUtcUs;
    }
    const std::uint64_t deltaTicks = pts - m_basePts;
    return m_baseUtcUs + ((deltaTicks * 1000000U) / 90000U);
}

std::uint64_t TelemetryTimeIndex::utcToPts(std::uint64_t utcUs) const noexcept
{
    if (utcUs < m_baseUtcUs || m_baseUtcUs == 0U) {
        return m_basePts;
    }
    const std::uint64_t deltaUs = utcUs - m_baseUtcUs;
    return m_basePts + ((deltaUs * 90000U) / 1000000U);
}

std::size_t TelemetryTimeIndex::videoFrameCount() const noexcept
{
    return m_videoEntries.size();
}

std::size_t TelemetryTimeIndex::keyframeCount() const noexcept
{
    return m_keyframeIndices.size();
}

std::size_t TelemetryTimeIndex::klvPacketCount() const noexcept
{
    return m_klvEntries.size();
}

double TelemetryTimeIndex::durationSeconds() const noexcept
{
    return m_durationSeconds;
}

bool TelemetryTimeIndex::empty() const noexcept
{
    return m_videoEntries.empty() && m_klvEntries.empty();
}

bool TelemetryTimeIndex::isFinalized() const noexcept
{
    return m_finalized;
}

std::uint64_t TelemetryTimeIndex::basePts() const noexcept
{
    return m_basePts;
}

std::uint64_t TelemetryTimeIndex::baseUtcUs() const noexcept
{
    return m_baseUtcUs;
}

} // namespace Klv
