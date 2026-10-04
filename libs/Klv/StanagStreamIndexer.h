#pragma once

/// @file StanagStreamIndexer.h
/// @brief Fast single-pass MPEG-TS indexer generating TelemetryTimeIndex tables.

#include "TelemetryTimeIndex.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace Klv {

/// @class StanagStreamIndexer
/// @brief Extracts PTS, file offsets, keyframe flags, and KLV metadata into TelemetryTimeIndex.
class StanagStreamIndexer {
public:
    /// @brief Progress callback reporting percent completion [0, 100].
    using ProgressCallback = std::function<void(int percent)>;

    /// @brief Default constructor.
    StanagStreamIndexer();

    /// @brief Destructor.
    ~StanagStreamIndexer() = default;

    /// @brief Indexes an entire local MPEG-TS file.
    /// @param[in] filePath Path to the .ts or .mpg file.
    /// @param[in] progress Optional progress callback.
    /// @return True if indexing succeeded.
    [[nodiscard]] bool indexFile(std::string_view filePath,
                                 ProgressCallback progress = nullptr);

    /// @brief Ingests arbitrary streaming TS chunks for progressive indexing.
    /// @param[in] data Pointer to raw TS data.
    /// @param[in] size Number of bytes.
    /// @param[in] baseOffset File offset of this chunk.
    /// @return Number of TS packets indexed.
    [[nodiscard]] std::size_t indexChunk(const std::uint8_t* data,
                                         std::size_t size,
                                         std::uint64_t baseOffset);

    /// @brief Serializes generated index to a binary `.sidx` file for instant loading.
    /// @param[in] indexPath Output index file path.
    /// @return True on success.
    [[nodiscard]] bool saveIndex(std::string_view indexPath) const;

    /// @brief Loads a pre-computed binary `.sidx` file from disk.
    /// @param[in] indexPath Path to index file.
    /// @return True on success.
    [[nodiscard]] bool loadIndex(std::string_view indexPath);

    /// @brief Retrieves the populated TelemetryTimeIndex.
    [[nodiscard]] const TelemetryTimeIndex& timeIndex() const noexcept;

    /// @brief Returns mutable reference to TelemetryTimeIndex.
    [[nodiscard]] TelemetryTimeIndex& timeIndex() noexcept;

    /// @brief Returns discovered or configured Video PID.
    [[nodiscard]] std::optional<std::uint16_t> videoPid() const noexcept;

    /// @brief Returns discovered or configured Metadata PID.
    [[nodiscard]] std::optional<std::uint16_t> metadataPid() const noexcept;

    /// @brief Sets Video PID explicitly.
    /// @param[in] pid Elementary Stream PID.
    void setVideoPid(std::uint16_t pid) noexcept;

    /// @brief Sets Metadata PID explicitly.
    /// @param[in] pid Elementary Stream PID.
    void setMetadataPid(std::uint16_t pid) noexcept;

    /// @brief Clears indexer state and resets time index.
    void reset() noexcept;

private:
    TelemetryTimeIndex m_timeIndex {};
    std::optional<std::uint16_t> m_videoPid {};
    std::optional<std::uint16_t> m_metadataPid {};
    std::optional<std::uint16_t> m_pmtPid {};

    std::uint32_t m_videoFrameCount { 0U };
    std::uint32_t m_klvPacketCount { 0U };

    std::uint64_t m_lastVideoPts { 0U };
    std::uint64_t m_lastKlvPts { 0U };

    void parsePat(const std::uint8_t* payload, std::size_t size);
    void parsePmt(const std::uint8_t* payload, std::size_t size);
    void processVideoPes(const std::uint8_t* pesData, std::size_t size, std::uint64_t fileOffset);
    void processKlvPes(const std::uint8_t* pesData, std::size_t size, std::uint64_t fileOffset);
    [[nodiscard]] static std::optional<std::uint64_t> peekUtcTimestamp(const std::uint8_t* data,
                                                                      std::size_t size) noexcept;
};

} // namespace Klv
