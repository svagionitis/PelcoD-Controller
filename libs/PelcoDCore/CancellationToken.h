#pragma once

/// @file CancellationToken.h
/// @brief Cooperative cancellation tokens and sources for interrupting long sleeps and tasks.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace PelcoD {

class CancellationTokenSource;

/// @class CancellationToken
/// @brief Lightweight, copyable token queried by asynchronous or long-running workers.
class CancellationToken final {
public:
    using Callback = std::function<void()>;

    CancellationToken() = default;
    ~CancellationToken() = default;

    CancellationToken(const CancellationToken&) = default;
    CancellationToken& operator=(const CancellationToken&) = default;
    CancellationToken(CancellationToken&&) noexcept = default;
    CancellationToken& operator=(CancellationToken&&) noexcept = default;

    /// @brief Checks whether cancellation has been requested.
    /// @return True if cancelled; false otherwise.
    [[nodiscard]] bool isCancellationRequested() const noexcept;

    /// @brief Registers a callback invoked upon cancellation.
    /// @param[in] callback Action to execute when cancelled.
    void registerCallback(Callback callback) const;

    /// @brief Cooperatively sleeps for a duration or until cancelled.
    /// @param[in] duration Maximum duration to sleep.
    /// @return True if sleep completed full duration; false if interrupted by cancellation.
    [[nodiscard]] bool sleepFor(std::chrono::milliseconds duration) const;

private:
    friend class CancellationTokenSource;

    struct State {
        std::atomic<bool> cancelled { false };
        std::mutex mutex {};
        std::condition_variable cv {};
        std::vector<Callback> callbacks {};
    };

    explicit CancellationToken(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> m_state {};
};

/// @class CancellationTokenSource
/// @brief Controls the lifecycle and cancellation state for CancellationToken instances.
class CancellationTokenSource final {
public:
    CancellationTokenSource();
    ~CancellationTokenSource() = default;

    CancellationTokenSource(const CancellationTokenSource&) = delete;
    CancellationTokenSource& operator=(const CancellationTokenSource&) = delete;
    CancellationTokenSource(CancellationTokenSource&&) noexcept = default;
    CancellationTokenSource& operator=(CancellationTokenSource&&) noexcept = default;

    /// @brief Retrieves the associated cancellation token.
    /// @return Associated token instance.
    [[nodiscard]] CancellationToken getToken() const noexcept;

    /// @brief Triggers cooperative cancellation across all linked tokens.
    void cancel();

    /// @brief Checks whether cancellation has been signaled.
    /// @return True if cancelled.
    [[nodiscard]] bool isCancellationRequested() const noexcept;

private:
    std::shared_ptr<CancellationToken::State> m_state {};
};

} // namespace PelcoD
