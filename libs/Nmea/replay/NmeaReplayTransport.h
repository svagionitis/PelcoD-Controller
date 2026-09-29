#pragma once

/// @file NmeaReplayTransport.h
/// @brief Deterministic, drift-compensated virtual maritime log replay transport.

#include "ReplayTypes.h"
#include "Transport/BaseTransport.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Nmea {

/// @class NmeaReplayTransport
/// @brief High-precision maritime replay transport feeding recorded voyage logs into NmeaDevice.
/// @details Implements Transport::BaseTransport to act as a drop-in virtual transport with speed scaling,
///          drift compensation, seeking, single-stepping, and automatic looping.
class NmeaReplayTransport : public Transport::BaseTransport {
public:
    using ProgressCallback = std::function<void(const ReplayProgress&)>;

    NmeaReplayTransport();
    ~NmeaReplayTransport() override;

    // Non-copyable, non-movable
    NmeaReplayTransport(const NmeaReplayTransport&) = delete;
    NmeaReplayTransport& operator=(const NmeaReplayTransport&) = delete;
    NmeaReplayTransport(NmeaReplayTransport&&) = delete;
    NmeaReplayTransport& operator=(NmeaReplayTransport&&) = delete;

    /// @brief Loads an NMEA log from disk.
    /// @param[in] filePath Path to .nmea / .log text file.
    /// @param[in] defaultInterval Fallback interval when timestamps are absent.
    /// @return True if file was successfully read and at least one record was indexed.
    bool loadFile(
        const std::string& filePath, std::chrono::milliseconds defaultInterval = std::chrono::milliseconds(100));

    /// @brief Loads an NMEA log from an in-memory string.
    /// @param[in] logContent Raw log text containing one or more newline-terminated sentences.
    /// @param[in] defaultInterval Fallback interval when timestamps are absent.
    /// @return True if at least one record was indexed.
    bool loadFromMemory(
        std::string_view logContent, std::chrono::milliseconds defaultInterval = std::chrono::milliseconds(100));

    /// @brief Clears all indexed replay records and resets playback position.
    void clear();

    // Transport::ITransport overrides
    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    bool sendData(const std::vector<std::uint8_t>& data) override;

    // Playback state controls
    void play();
    void pause();
    void stop();
    void stepForward();

    // Seeking & Navigation
    bool seek(std::chrono::microseconds targetOffset);
    bool seekRatio(double ratio01);
    bool seekLine(std::size_t lineIndex);

    // Configuration
    void setSpeedMultiplier(double multiplier) noexcept;
    [[nodiscard]] double speedMultiplier() const noexcept;

    void setLoop(bool loop) noexcept;
    [[nodiscard]] bool isLooping() const noexcept;

    /// @brief Configures sentence identifier whitelist (e.g. {"GGA", "RMC", "VDM"}). Empty disables filter.
    void setSentenceFilter(std::vector<std::string> allowedSentenceIds);
    void clearSentenceFilter();

    // Telemetry & Status
    [[nodiscard]] ReplayState state() const noexcept;
    [[nodiscard]] ReplayProgress progress() const;
    [[nodiscard]] std::size_t totalLines() const noexcept;
    [[nodiscard]] std::chrono::microseconds totalDuration() const noexcept;
    [[nodiscard]] std::size_t currentLine() const noexcept;

    // Callbacks
    std::size_t addProgressCallback(ProgressCallback cb);
    void removeProgressCallback(std::size_t id);

private:
    void playbackWorker();
    void parseLines(std::string_view content, std::chrono::microseconds defaultInterval);
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> extractTimestamp(
        std::string_view line, TimestampSource& outSource) const;
    [[nodiscard]] bool isSentenceAllowed(std::string_view sentence) const;
    void emitProgressLocked();

    std::vector<ReplayRecord> m_records {};
    std::vector<std::string> m_filterIds {};

    mutable std::mutex m_replayMutex {};
    std::condition_variable m_cv {};

    std::atomic<bool> m_open { false };
    std::atomic<ReplayState> m_state { ReplayState::Stopped };
    std::atomic<double> m_speedMultiplier { 1.0 };
    std::atomic<bool> m_loop { false };

    std::size_t m_currentIndex { 0U };
    std::chrono::microseconds m_currentOffset { 0 };

    std::chrono::steady_clock::time_point m_playbackAnchorReal {};
    std::chrono::microseconds m_playbackAnchorOffset { 0 };

    mutable std::mutex m_progressMutex {};
    std::size_t m_nextCallbackId { 1U };
    std::vector<std::pair<std::size_t, ProgressCallback>> m_progressCallbacks {};
};

} // namespace Nmea
