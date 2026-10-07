/// @file NotificationDispatcher.cpp
/// @brief Implementation of bounded push notification delivery worker.

#include "NotificationDispatcher.h"
#include "HttplibInclude.h"

namespace Onvif {

NotificationDispatcher::NotificationDispatcher(NotificationConfig config)
    : m_config { std::move(config) }
{
}

NotificationDispatcher::~NotificationDispatcher()
{
    stop();
}

void NotificationDispatcher::start()
{
    if (m_running.exchange(true)) {
        return;
    }
    m_worker = std::thread([this]() { workerLoop(); });
}

void NotificationDispatcher::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

bool NotificationDispatcher::enqueue(std::string url, std::string payload)
{
    if (!m_running) {
        return false;
    }
    {
        std::scoped_lock lock(m_mutex);
        if (m_queue.size() >= m_config.maxDispatchQueueSize) {
            // Drop oldest pending item to preserve bounded memory under saturation
            m_queue.pop_front();
        }
        m_queue.push_back({ std::move(url), std::move(payload) });
    }
    m_cv.notify_one();
    return true;
}

std::size_t NotificationDispatcher::getPendingCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_queue.size();
}

bool NotificationDispatcher::isRunning() const noexcept
{
    return m_running.load();
}

void NotificationDispatcher::workerLoop()
{
    while (m_running) {
        PushDeliveryTask task {};
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return !m_queue.empty() || !m_running; });
            if (!m_running && m_queue.empty()) {
                break;
            }
            if (m_queue.empty()) {
                continue;
            }
            task = std::move(m_queue.front());
            m_queue.pop_front();
        }

        try {
            const std::string& url { task.consumerUrl };
            const std::string prefixHttp { "http://" };
            const std::string prefixHttps { "https://" };
            std::string noPrefix {};
            if (url.rfind(prefixHttp, 0U) == 0U) {
                noPrefix = url.substr(prefixHttp.length());
            } else if (url.rfind(prefixHttps, 0U) == 0U) {
                noPrefix = url.substr(prefixHttps.length());
            } else {
                continue;
            }

            const auto slashPos { noPrefix.find('/') };
            const std::string hostPort { (slashPos != std::string::npos) ? noPrefix.substr(0U, slashPos) : noPrefix };
            const std::string path { (slashPos != std::string::npos) ? noPrefix.substr(slashPos) : "/" };

            httplib::Client cli(hostPort);
            const auto connSec { std::chrono::duration_cast<std::chrono::seconds>(m_config.connectTimeoutMs).count() };
            const auto readSec { std::chrono::duration_cast<std::chrono::seconds>(m_config.readTimeoutMs).count() };
            cli.set_connection_timeout(connSec > 0 ? connSec : 1, 0);
            cli.set_read_timeout(readSec > 0 ? readSec : 2, 0);

            const std::string& body { task.payload.empty() ? std::string { "<NotificationMessage/>" } : task.payload };
            cli.Post(path.c_str(), body, "application/soap+xml; charset=utf-8");
        } catch (const std::exception&) {
            // Swallow connection / socket failure on consumer delivery to prevent crashes
        } catch (...) {
            // Swallow unexpected exceptions
        }
    }
}

} // namespace Onvif
