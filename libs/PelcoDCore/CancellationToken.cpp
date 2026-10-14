#include "CancellationToken.h"

#include <utility>

namespace PelcoD {

CancellationToken::CancellationToken(std::shared_ptr<State> state) noexcept
    : m_state { std::move(state) }
{
}

bool CancellationToken::isCancellationRequested() const noexcept
{
    return m_state && m_state->cancelled.load(std::memory_order_relaxed);
}

void CancellationToken::registerCallback(Callback callback) const
{
    if (!m_state || !callback) {
        return;
    }

    bool shouldFireImmediately { false };
    {
        std::scoped_lock lock(m_state->mutex);
        if (m_state->cancelled.load(std::memory_order_relaxed)) {
            shouldFireImmediately = true;
        } else {
            m_state->callbacks.push_back(std::move(callback));
        }
    }

    if (shouldFireImmediately) {
        callback();
    }
}

bool CancellationToken::sleepFor(std::chrono::milliseconds duration) const
{
    if (!m_state) {
        return true;
    }

    if (m_state->cancelled.load(std::memory_order_relaxed)) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_state->mutex);
    return !m_state->cv.wait_for(lock, duration, [this] { return m_state->cancelled.load(std::memory_order_relaxed); });
}

CancellationTokenSource::CancellationTokenSource()
    : m_state { std::make_shared<CancellationToken::State>() }
{
}

CancellationToken CancellationTokenSource::getToken() const noexcept
{
    return CancellationToken(m_state);
}

void CancellationTokenSource::cancel()
{
    if (!m_state) {
        return;
    }

    std::vector<CancellationToken::Callback> toExecute {};
    {
        std::scoped_lock lock(m_state->mutex);
        if (m_state->cancelled.exchange(true, std::memory_order_relaxed)) {
            return;
        }
        toExecute = std::move(m_state->callbacks);
    }

    m_state->cv.notify_all();

    for (const auto& cb : toExecute) {
        if (cb) {
            cb();
        }
    }
}

bool CancellationTokenSource::isCancellationRequested() const noexcept
{
    return m_state && m_state->cancelled.load(std::memory_order_relaxed);
}

} // namespace PelcoD
