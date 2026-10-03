#pragma once

/// @file RecordingValidator.h
/// @brief Pre-flight validation engine for Sightline recording commands and parameters.
/// @details Verifies filename safety, camera indexes, snapshot configurations, and storage targets.

#include "SightlineRecording.h"

#include <cstdint>
#include <string_view>

namespace Sightline {

/// @class RecordingValidator
/// @brief Static validation routines for SLA file recording and snapshot operations.
class RecordingValidator {
public:
    /// @brief Maximum permitted length of a recording base filename in characters.
    static constexpr std::size_t MaxFilenameLength { 64U };

    /// @brief Minimum free space required to accept a recording start command (50 MB).
    static constexpr std::uint64_t MinRequiredFreeBytes { 52428800ULL };

    /// @brief Validates a base filename against syntax, length, and trailing numeral rules.
    /// @details Ensures non-empty, length <= 64, no forbidden path separator/wildcard characters,
    ///          and rejects trailing numerals '0'-'9' unless AllowNumericOverwrite flag is set.
    /// @param[in] filename Candidate base filename string view.
    /// @param[in] flags Bitmask of RecordingFlags.
    /// @return RecordingStatusCode::Success if compliant, or specific error code.
    [[nodiscard]] static RecordingStatusCode checkFilename(
        std::string_view filename,
        std::uint8_t flags) noexcept;

    /// @brief Validates camera sensor index boundary.
    /// @param[in] cameraIndex Zero-based camera index (0..2), or 0xFF if allowAllCameras is true.
    /// @param[in] allowAllCameras Whether 0xFF (Snap All Cams) is a valid index.
    /// @return RecordingStatusCode::Success if valid, ErrChannelUnsupported otherwise.
    [[nodiscard]] static RecordingStatusCode checkCamera(
        std::uint8_t cameraIndex,
        bool allowAllCameras = false) noexcept;

    /// @brief Validates parameters for a snapshot request.
    /// @param[in] msg Snapshot configuration message.
    /// @return RecordingStatusCode::Success if valid, or specific error code.
    [[nodiscard]] static RecordingStatusCode checkSnapshot(
        const MsgDoSnapShotV2& msg) noexcept;

    /// @brief Validates storage target capacity and write status.
    /// @param[in] destination Storage target (MicroSD, UsbDrive, or FtpPush).
    /// @param[in] availableBytes Available free storage bytes on the target.
    /// @param[in] isMounted Whether the target filesystem is mounted and writable.
    /// @param[out] outFreeMB Output parameter populated with available storage in MB.
    /// @return RecordingStatusCode::Success if storage is ready, or specific error code.
    [[nodiscard]] static RecordingStatusCode checkStorage(
        StorageDestination destination,
        std::uint64_t availableBytes,
        bool isMounted,
        std::uint32_t& outFreeMB) noexcept;
};

} // namespace Sightline
