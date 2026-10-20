#pragma once

/// @file RtspStreamFeeder.h
/// @brief Paced media ingestion worker feeding video sources into Live555RtspServer.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Video {
class Live555RtspServer;
} // namespace Video

namespace RtspServerApp {

/// @enum FeederSourceMode
/// @brief Selected input media type for the stream feeder.
enum class FeederSourceMode : std::uint8_t {
    SyntheticPattern = 0U, ///< Built-in synthetic test pattern (colorbars)
    MpegTsFile = 1U, ///< Pre-multiplexed MPEG-2 TS file (STANAG 4609 .ts)
    H264File = 2U ///< Annex-B H.264 elementary stream file (.h264)
};

/// @struct FeederConfig
/// @brief Configuration settings controlling media ingestion rate and looping.
struct FeederConfig {
    FeederSourceMode mode { FeederSourceMode::SyntheticPattern };
    std::string filePath {}; ///< Local media file path (for TS / H.264 modes)
    bool loopPlayback { true }; ///< Automatically rewind on EOF
    std::uint32_t targetFps { 30U }; ///< Nominal frame pacing rate
    std::uint32_t tsChunkSize { 1316U }; ///< 7 * 188 bytes standard TS UDP payload size
};

/// @struct FeederMetrics
/// @brief Real-time transmission metrics snapshot from the feeder worker.
struct FeederMetrics {
    std::uint64_t bytesStreamed { 0U };
    std::uint64_t framesStreamed { 0U };
    double currentFps { 0.0 };
};

/// @class RtspStreamFeeder
/// @brief Dedicated background worker thread that reads media files or generates patterns,
///        pacing frames into Live555RtspServer.
class RtspStreamFeeder final {
public:
    /// @brief Constructs stream feeder instance.
    RtspStreamFeeder();

    /// @brief Destructor. Automatically stops worker thread if running.
    ~RtspStreamFeeder();

    RtspStreamFeeder(const RtspStreamFeeder&) = delete;
    RtspStreamFeeder& operator=(const RtspStreamFeeder&) = delete;
    RtspStreamFeeder(RtspStreamFeeder&&) = delete;
    RtspStreamFeeder& operator=(RtspStreamFeeder&&) = delete;

    /// @brief Starts background feeding worker thread.
    /// @param[in] config Media feeder options and source selection.
    /// @param[in] server Non-owning pointer to destination Live555RtspServer.
    /// @return True if worker successfully started.
    [[nodiscard]] bool start(const FeederConfig& config, Video::Live555RtspServer* server);

    /// @brief Stops worker thread and waits for termination.
    void stop();

    /// @brief Checks if feeding thread is actively streaming.
    /// @return True if streaming active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Queries current transmission metrics snapshot.
    /// @return Feeder metrics struct.
    [[nodiscard]] FeederMetrics metrics() const noexcept;

    /// @brief Returns active feeder configuration.
    /// @return Reference to current config.
    [[nodiscard]] const FeederConfig& config() const noexcept;

private:
    void workerLoop();
    void runSyntheticMode();
    void runMpegTsMode();
    void runH264Mode();

    FeederConfig m_config {};
    Video::Live555RtspServer* m_server { nullptr };
    std::atomic<bool> m_running { false };
    std::thread m_thread {};

    std::atomic<std::uint64_t> m_bytesStreamed { 0U };
    std::atomic<std::uint64_t> m_framesStreamed { 0U };
    std::atomic<double> m_currentFps { 0.0 };
};

} // namespace RtspServerApp
