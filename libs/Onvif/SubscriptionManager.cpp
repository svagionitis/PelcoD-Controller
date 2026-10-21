/// @file SubscriptionManager.cpp
/// @brief Implementation of SubscriptionManager for ONVIF event subscriptions.

#include "SubscriptionManager.h"

#include <cstddef>
#include <ctime>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Onvif {

namespace {

    /// @brief Formats a system time point to ISO 8601 UTC string.
    /// @param[in] tp System clock time point.
    /// @return ISO 8601 UTC string (e.g. "2026-10-07T12:00:00Z").
    [[nodiscard]] std::string formatIso8601Utc(std::chrono::system_clock::time_point tp)
    {
        const std::time_t t { std::chrono::system_clock::to_time_t(tp) };
        std::tm tmBuf {};
#ifdef _WIN32
        gmtime_s(&tmBuf, &t);
#else
        gmtime_r(&t, &tmBuf);
#endif
        std::ostringstream ss {};
        ss << std::put_time(&tmBuf, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    /// @brief Parses an ISO 8601 UTC dateTime string into a system clock time point.
    /// @param[in] dtStr ISO 8601 UTC string (e.g. "2026-10-21T15:30:00Z").
    /// @return System clock time point on success; nullopt on malformed string.
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> parseIso8601Utc(std::string_view dtStr) noexcept
    {
        // Expect at least "YYYY-MM-DDTHH:MM:SS" (19 chars)
        if (dtStr.size() < 19U) {
            return std::nullopt;
        }
        if (dtStr[4] != '-' || dtStr[7] != '-' || dtStr[10] != 'T' || dtStr[13] != ':' || dtStr[16] != ':') {
            return std::nullopt;
        }
        const auto parseNum = [](const char* p, std::size_t len, int& out) noexcept -> bool {
            const auto res = std::from_chars(p, p + len, out);
            return res.ec == std::errc {};
        };
        int year { 0 };
        int month { 0 };
        int day { 0 };
        int hour { 0 };
        int min { 0 };
        int sec { 0 };
        if (!parseNum(&dtStr[0], 4U, year) || !parseNum(&dtStr[5], 2U, month) || !parseNum(&dtStr[8], 2U, day)
            || !parseNum(&dtStr[11], 2U, hour) || !parseNum(&dtStr[14], 2U, min) || !parseNum(&dtStr[17], 2U, sec)) {
            return std::nullopt;
        }
        std::tm tmBuf {};
        tmBuf.tm_year = year - 1900;
        tmBuf.tm_mon = month - 1;
        tmBuf.tm_mday = day;
        tmBuf.tm_hour = hour;
        tmBuf.tm_min = min;
        tmBuf.tm_sec = sec;
        tmBuf.tm_isdst = 0;
#ifdef _WIN32
        const std::time_t t { _mkgmtime(&tmBuf) };
#else
        const std::time_t t { timegm(&tmBuf) };
#endif
        if (t == static_cast<std::time_t>(-1)) {
            return std::nullopt;
        }
        return std::chrono::system_clock::from_time_t(t);
    }

} // namespace

SubscriptionManager::SubscriptionManager(NotificationConfig config)
    : m_config { std::move(config) }
{
    start();
}

SubscriptionManager::~SubscriptionManager()
{
    stop();
}

void SubscriptionManager::start()
{
    bool expected { false };
    if (m_sweeperRunning.compare_exchange_strong(expected, true)) {
        m_sweepThread = std::thread(&SubscriptionManager::sweeperLoop, this);
    }
}

void SubscriptionManager::stop()
{
    bool expected { true };
    if (m_sweeperRunning.compare_exchange_strong(expected, false)) {
        m_sweepCv.notify_all();
        if (m_sweepThread.joinable()) {
            m_sweepThread.join();
        }
    }
    notifyAll();
}

void SubscriptionManager::sweeperLoop()
{
    std::unique_lock<std::mutex> lock(m_sweepMutex);
    while (m_sweeperRunning.load()) {
        if (m_sweepCv.wait_for(lock, m_config.sweepInterval, [this] { return !m_sweeperRunning.load(); })) {
            break;
        }
        sweepExpired();
    }
}

std::chrono::seconds SubscriptionManager::parseLease(std::string_view termStr) const
{
    if (termStr.empty()) {
        return m_config.defaultLease;
    }

    // Check if it's an ISO 8601 absolute UTC dateTime (e.g. "2026-10-21T15:30:00Z")
    if (termStr.size() >= 19U && termStr.find('-') != std::string_view::npos
        && termStr.find('T') != std::string_view::npos) {
        const auto tp = parseIso8601Utc(termStr);
        if (tp) {
            const auto diff = std::chrono::duration_cast<std::chrono::seconds>(*tp - std::chrono::system_clock::now());
            if (diff.count() <= 0) {
                return m_config.minLease;
            }
            return std::clamp(diff, m_config.minLease, m_config.maxLease);
        }
    }

    // Check if it's an ISO 8601 duration starting with P or PT
    if (termStr.front() == 'P') {
        long totalSeconds { 0 };
        long currentVal { 0 };
        bool inTime { false };

        for (std::size_t i { 1U }; i < termStr.size(); ++i) {
            const char c { termStr[i] };
            if (c == 'T') {
                inTime = true;
            } else if (c >= '0' && c <= '9') {
                currentVal = (currentVal * 10) + (c - '0');
            } else if (c == 'D') {
                totalSeconds += currentVal * 86400;
                currentVal = 0;
            } else if (c == 'H' && inTime) {
                totalSeconds += currentVal * 3600;
                currentVal = 0;
            } else if (c == 'M' && inTime) {
                totalSeconds += currentVal * 60;
                currentVal = 0;
            } else if (c == 'S' && inTime) {
                totalSeconds += currentVal;
                currentVal = 0;
            }
        }
        if (totalSeconds > 0) {
            const auto sec { std::chrono::seconds(totalSeconds) };
            return std::clamp(sec, m_config.minLease, m_config.maxLease);
        }
    }

    // Try parsing as simple integer seconds
    int rawSec { 0 };
    const auto res { std::from_chars(termStr.data(), termStr.data() + termStr.size(), rawSec) };
    if (res.ec == std::errc {} && rawSec > 0) {
        return std::clamp(std::chrono::seconds(rawSec), m_config.minLease, m_config.maxLease);
    }

    return m_config.defaultLease;
}

SubCreateResult SubscriptionManager::createPullSub(std::string_view termTimeStr)
{
    std::scoped_lock lock(m_mutex);
    // Sweep expired before checking quota
    const auto nowSteady { std::chrono::steady_clock::now() };
    for (auto it { m_pullSubs.begin() }; it != m_pullSubs.end();) {
        if (it->second->terminationTime <= nowSteady) {
            it = m_pullSubs.erase(it);
        } else {
            ++it;
        }
    }

    if (m_pullSubs.size() >= m_config.maxSubscriptions) {
        return { false, "", "", "PullPoint subscription quota exceeded" };
    }

    const auto lease { parseLease(termTimeStr) };
    const auto subId { std::to_string(m_nextSubId++) };
    auto sub { std::make_shared<PullPointSub>() };
    sub->id = subId;
    sub->terminationTime = nowSteady + lease;

    m_pullSubs[subId] = sub;

    const auto termSys { std::chrono::system_clock::now() + lease };
    return { true, subId, formatIso8601Utc(termSys), "" };
}

SubCreateResult SubscriptionManager::createPushSub(std::string_view consumerUrl, std::string_view termTimeStr)
{
    std::scoped_lock lock(m_mutex);
    const auto nowSteady { std::chrono::steady_clock::now() };
    for (auto it { m_pushSubs.begin() }; it != m_pushSubs.end();) {
        if (it->second.terminationTime <= nowSteady) {
            it = m_pushSubs.erase(it);
        } else {
            ++it;
        }
    }

    if (m_pushSubs.size() >= m_config.maxPushSubscriptions) {
        return { false, "", "", "Push subscription quota exceeded" };
    }

    const auto lease { parseLease(termTimeStr) };
    const auto subId { std::to_string(m_nextSubId++) };
    PushSub sub {};
    sub.id = subId;
    sub.consumerUrl = std::string { consumerUrl };
    sub.terminationTime = nowSteady + lease;

    m_pushSubs[subId] = std::move(sub);

    const auto termSys { std::chrono::system_clock::now() + lease };
    return { true, subId, formatIso8601Utc(termSys), "" };
}

bool SubscriptionManager::renewSub(std::string_view subId, std::string_view termTimeStr, std::string& outNewTermTime)
{
    std::scoped_lock lock(m_mutex);
    const auto nowSteady { std::chrono::steady_clock::now() };
    const auto lease { parseLease(termTimeStr) };
    const std::string idStr { subId };

    const auto itPull { m_pullSubs.find(idStr) };
    if (itPull != m_pullSubs.end()) {
        if (itPull->second->terminationTime <= nowSteady) {
            m_pullSubs.erase(itPull);
            return false;
        }
        itPull->second->terminationTime = nowSteady + lease;
        outNewTermTime = formatIso8601Utc(std::chrono::system_clock::now() + lease);
        return true;
    }

    const auto itPush { m_pushSubs.find(idStr) };
    if (itPush != m_pushSubs.end()) {
        if (itPush->second.terminationTime <= nowSteady) {
            m_pushSubs.erase(itPush);
            return false;
        }
        itPush->second.terminationTime = nowSteady + lease;
        outNewTermTime = formatIso8601Utc(std::chrono::system_clock::now() + lease);
        return true;
    }

    return false;
}

bool SubscriptionManager::unsubscribe(std::string_view subId)
{
    std::scoped_lock lock(m_mutex);
    const std::string idStr { subId };

    const auto itPull { m_pullSubs.find(idStr) };
    if (itPull != m_pullSubs.end()) {
        {
            std::scoped_lock subLock(itPull->second->mutex);
            itPull->second->cv.notify_all();
        }
        m_pullSubs.erase(itPull);
        return true;
    }

    const auto itPush { m_pushSubs.find(idStr) };
    if (itPush != m_pushSubs.end()) {
        m_pushSubs.erase(itPush);
        return true;
    }

    return false;
}

bool SubscriptionManager::pullMessages(std::string_view subId, int limit, int timeoutSec,
    std::vector<OnvifEvent>& outEvents, std::string& outCurTime, std::string& outTermTime,
    const std::atomic<bool>& serverRunning)
{
    const int clampedLimit { std::clamp(limit, 1, 100) };
    const int clampedTimeoutSec { std::clamp(timeoutSec, 0, 10) };

    std::shared_ptr<PullPointSub> sub {};
    {
        std::scoped_lock lock(m_mutex);
        const auto nowSteady { std::chrono::steady_clock::now() };
        const auto it { m_pullSubs.find(std::string { subId }) };
        if (it == m_pullSubs.end()) {
            return false;
        }
        if (it->second->terminationTime <= nowSteady) {
            {
                std::scoped_lock subLock(it->second->mutex);
                it->second->cv.notify_all();
            }
            m_pullSubs.erase(it);
            return false;
        }
        sub = it->second;
    }

    {
        std::unique_lock<std::mutex> subLock(sub->mutex);
        if (sub->queue.empty() && clampedTimeoutSec > 0) {
            const auto now = std::chrono::steady_clock::now();
            const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(sub->terminationTime - now);
            const auto timeoutMs = std::chrono::milliseconds(clampedTimeoutSec * 1000);
            const auto waitDuration = std::min(timeoutMs, std::max(std::chrono::milliseconds(0), remainingMs));

            if (waitDuration.count() > 0) {
                sub->cv.wait_for(subLock, waitDuration, [&]() {
                    return !sub->queue.empty() || !serverRunning.load()
                        || (std::chrono::steady_clock::now() >= sub->terminationTime);
                });
            }
        }

        while (!sub->queue.empty() && static_cast<int>(outEvents.size()) < clampedLimit) {
            outEvents.push_back(std::move(sub->queue.front()));
            sub->queue.pop_front();
        }

        if (outEvents.empty()) {
            const auto now = std::chrono::steady_clock::now();
            if (now < sub->terminationTime && (sub->terminationTime - now) <= std::chrono::milliseconds(50)) {
                std::this_thread::sleep_until(sub->terminationTime);
            }
            if (std::chrono::steady_clock::now() >= sub->terminationTime) {
                return false;
            }
        }
    }

    const auto nowSys { std::chrono::system_clock::now() };
    outCurTime = formatIso8601Utc(nowSys);
    const auto remaining { std::chrono::duration_cast<std::chrono::seconds>(
        sub->terminationTime - std::chrono::steady_clock::now()) };
    outTermTime = formatIso8601Utc(nowSys + (remaining.count() > 0 ? remaining : std::chrono::seconds(0)));
    return true;
}

void SubscriptionManager::publishEvent(const OnvifEvent& event, std::vector<std::string>& outPushUrls)
{
    std::scoped_lock lock(m_mutex);
    const auto nowSteady { std::chrono::steady_clock::now() };

    for (auto it { m_pullSubs.begin() }; it != m_pullSubs.end();) {
        if (it->second->terminationTime <= nowSteady) {
            {
                std::scoped_lock subLock(it->second->mutex);
                it->second->cv.notify_all();
            }
            it = m_pullSubs.erase(it);
        } else {
            std::scoped_lock subLock(it->second->mutex);
            while (it->second->queue.size() >= m_config.maxQueuePerSubscription) {
                it->second->queue.pop_front();
            }
            it->second->queue.push_back(event);
            it->second->cv.notify_one();
            ++it;
        }
    }

    for (auto it { m_pushSubs.begin() }; it != m_pushSubs.end();) {
        if (it->second.terminationTime <= nowSteady) {
            it = m_pushSubs.erase(it);
        } else {
            if (!it->second.consumerUrl.empty()) {
                outPushUrls.push_back(it->second.consumerUrl);
            }
            ++it;
        }
    }
}

std::size_t SubscriptionManager::sweepExpired()
{
    std::scoped_lock lock(m_mutex);
    const auto nowSteady { std::chrono::steady_clock::now() };
    std::size_t swept { 0U };

    for (auto it { m_pullSubs.begin() }; it != m_pullSubs.end();) {
        if (it->second->terminationTime <= nowSteady) {
            {
                std::scoped_lock subLock(it->second->mutex);
                it->second->cv.notify_all();
            }
            it = m_pullSubs.erase(it);
            ++swept;
        } else {
            ++it;
        }
    }

    for (auto it { m_pushSubs.begin() }; it != m_pushSubs.end();) {
        if (it->second.terminationTime <= nowSteady) {
            it = m_pushSubs.erase(it);
            ++swept;
        } else {
            ++it;
        }
    }

    return swept;
}

void SubscriptionManager::notifyAll()
{
    std::scoped_lock lock(m_mutex);
    for (auto& [id, sub] : m_pullSubs) {
        std::scoped_lock subLock(sub->mutex);
        sub->cv.notify_all();
    }
}

std::size_t SubscriptionManager::getPullSubCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_pullSubs.size();
}

std::size_t SubscriptionManager::getPushSubCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_pushSubs.size();
}

} // namespace Onvif
