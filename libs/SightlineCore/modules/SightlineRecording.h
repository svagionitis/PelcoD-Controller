#pragma once

/// @file SightlineRecording.h
/// @brief Sightline SLA Recording Module (SD card video recording and snapshots).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__record.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>

namespace Sightline {

/// @enum RecordingStatusCode
/// @brief Status codes returned in CommandAck (Message ID 0xC3).
enum class RecordingStatusCode : std::uint8_t {
    Success = 0x00U,               ///< Command accepted and committed
    ErrMalformedPayload = 0x01U,   ///< Packet length or layout invalid
    ErrMediaUnavailable = 0x02U,   ///< Storage medium unmounted / absent
    ErrMediaReadOnly = 0x03U,      ///< Filesystem mounted read-only
    ErrInsufficientStorage = 0x04U,///< Available space below minimum threshold
    ErrNumericFilename = 0x05U,    ///< Trailing numeric suffix without overwrite flag
    ErrInvalidCharacters = 0x06U,  ///< Path contains illegal characters or too long
    ErrBusyFlushing = 0x07U,       ///< System actively flushing prior recording
    ErrChannelUnsupported = 0x08U  ///< Specified cameraIndex invalid or offline
};

/// @enum RecordingAction
/// @brief Action commands for SetFileRecordingParamsV2 (Message ID 0xC4).
enum class RecordingAction : std::uint8_t {
    Stop = 0x00U,                  ///< Stop active video recording
    Start = 0x01U,                 ///< Start new video recording stream
    Split = 0x02U                  ///< Force an immediate file rollover / split
};

/// @enum StorageDestination
/// @brief Persistent storage target for recording and snapshots.
enum class StorageDestination : std::uint8_t {
    MicroSD = 0x00U,               ///< Onboard MicroSD card slot
    UsbDrive = 0x01U,              ///< External USB 3.0 SSD/HDD mass storage
    FtpPush = 0x02U                ///< Remote FTP destination
};

/// @enum RecordingFlags
/// @brief Bitwise policy flags for SetFileRecordingParamsV2.
enum class RecordingFlags : std::uint8_t {
    None = 0x00U,
    AllowNumericOverwrite = 0x01U,  ///< Allow base filenames ending in 0-9
    ExcludeOverlays = 0x02U,        ///< Record clean source without OSD graphics
    TimestampSuffix = 0x04U         ///< Append UTC timestamp instead of sequence
};

/// @enum SnapshotFormat
/// @brief Encoding format for snapshot captures.
enum class SnapshotFormat : std::uint8_t {
    Jpeg = 0x00U,                  ///< 8-bit lossy baseline JPEG with EXIF
    Png = 0x01U,                   ///< 8-bit or 16-bit lossless PNG
    Slraw = 0x02U,                 ///< Sightline raw uncompressed binary (0x51acd00d)
    Tiff16 = 0x03U                 ///< 16-bit grayscale uncompressed TIFF
};

/// @enum SnapshotDomain
/// @brief Pipeline tap point for snapshot capture.
enum class SnapshotDomain : std::uint8_t {
    Capture = 0x00U,               ///< Pre-processing raw sensor buffer
    Display = 0x01U,               ///< Post-processing display buffer with overlays
    Both = 0x02U                   ///< Capture and save both pre and post buffers
};

/// @struct MsgCommandAck
/// @brief Synchronous command acknowledgment and transaction status (Message ID 0xC3).
struct MsgCommandAck {
    std::uint16_t sequenceId { 0U };        ///< Correlated client transaction token
    std::uint8_t originalMsgId { 0U };      ///< Acknowledged command Message ID
    RecordingStatusCode statusCode { RecordingStatusCode::Success }; ///< Execution status
    std::uint32_t freeStorageMB { 0U };     ///< Available storage on target medium (MB)
    std::uint8_t subsystemState { 0U };     ///< Bit 0: Mounted, Bit 1: Active, Bit 2: FIFO Full
};

/// @struct MsgSetFileRecordingParamsV2
/// @brief Hardened video recording parameters with sequence tracking (Message ID 0xC4).
struct MsgSetFileRecordingParamsV2 {
    std::uint16_t sequenceId { 0U };        ///< Non-zero transaction ID (0=legacy fire-and-forget)
    std::uint8_t cameraIndex { 0U };        ///< Explicit camera index (0, 1, 2)
    RecordingAction action { RecordingAction::Stop }; ///< Start, Stop, or Split
    StorageDestination destination { StorageDestination::MicroSD }; ///< Storage target
    std::uint8_t flags { 0U };              ///< Bitmask of RecordingFlags
    std::uint32_t maxSplitSizeBytes { 1073741824U }; ///< Split boundary (default: 1 GB)
    std::uint16_t maxSplitFrames { 0U };    ///< Split boundary in frames (0=disabled)
    std::string baseFilename {};            ///< Base filename (max 64 chars)
};

/// @struct MsgDoSnapShotV2
/// @brief Hardened snapshot capture with explicit sensor routing (Message ID 0xC5).
struct MsgDoSnapShotV2 {
    std::uint16_t sequenceId { 0U };        ///< Correlated transaction ID
    std::uint8_t cameraIndex { 0U };        ///< Explicit camera index (0..2, 0xFF=All)
    SnapshotFormat format { SnapshotFormat::Jpeg }; ///< Encoding format
    SnapshotDomain domain { SnapshotDomain::Capture }; ///< Tap domain
    std::uint8_t qualityLevel { 80U };      ///< Quality compression level (1..100)
    std::uint8_t burstCount { 1U };         ///< Number of burst frames (1..120)
    std::string customFilename {};          ///< Optional custom filename override
};

/// @enum RecordingEventType
/// @brief Event types emitted in FileRecordingEvent (Message ID 0xC6).
enum class RecordingEventType : std::uint8_t {
    Started = 0x01U,         ///< Recording started and first file opened
    FileSplit = 0x02U,       ///< Boundary reached; rollover to new container
    Stopped = 0x03U,         ///< Stop flush completed; file closed and synced
    LowWatermark = 0x04U,    ///< Storage free space below low threshold (< 10%)
    CriticalStorage = 0x05U, ///< Storage free space critical (< 2%)
    BufferOverrun = 0x06U,   ///< Memory ring buffer full; frames throttled
    SnapshotSaved = 0x07U,   ///< Still frame capture committed to disk
    FifoPruned = 0x08U       ///< Oldest file unlinked to reclaim storage
};

/// @struct MsgFileRecordingEvent
/// @brief Asynchronous push-based recording notification (Message ID 0xC6).
struct MsgFileRecordingEvent {
    std::uint64_t timestampUs { 0ULL };     ///< Microsecond precision UTC timestamp
    std::uint8_t cameraIndex { 0U };        ///< Originating sensor channel (0..2)
    RecordingEventType eventType { RecordingEventType::Started }; ///< Event category
    std::uint32_t statusCode { 0U };        ///< Status or error code (0=Success)
    std::uint32_t freeStorageMB { 0U };     ///< Remaining storage capacity in MB
    std::uint8_t queueFullPercent { 0U };   ///< Memory ring buffer fullness (0..100%)
    std::string eventPayload {};            ///< Contextual string (e.g. filename)
};

/// @struct MsgCurrentRecordingStatusV2
/// @brief Comprehensive periodic recording health telemetry (Message ID 0xC7).
struct MsgCurrentRecordingStatusV2 {
    std::uint16_t sequenceId { 0U };        ///< Telemetry sequence counter
    std::uint8_t cameraIndex { 0U };        ///< Active sensor channel (0..2)
    std::uint8_t recordingState { 0U };     ///< 0: Idle, 1: Active, 2: Flushing, 3: Error
    std::uint32_t currentBitrateKbps { 0U };///< Instantaneous write throughput in kbps
    std::uint64_t totalBytesWritten { 0ULL };///< Cumulative session bytes written
    std::uint32_t freeStorageMB { 0U };     ///< Unallocated storage capacity in MB
    std::uint16_t estRemainingSecs { 0U };  ///< Estimated duration until media full
    std::uint8_t ringBufferPercent { 0U };  ///< Ring buffer memory utilization (0..100%)
    std::uint16_t droppedFrames { 0U };     ///< Cumulative dropped frame count
    std::uint16_t activeFileFrameCount { 0U }; ///< Frames committed to active file
    std::string activeFilename {};          ///< Relative path of active file
};

/// @struct MsgSetSDRecordingParameters
/// @brief Onboard SD card video recording control (Message ID 0x1E).
struct MsgSetSDRecordingParameters {
    std::uint8_t recordingState { 0U }; // 0: Stop, 1: Start, 2: Snapshot
    std::uint8_t cameraIndex { 0U };
    std::string filenamePrefix {};
};

/// @struct MsgCurrentSnapShot
/// @brief Snapshot status and image path reply (Message ID 0x5D / 0x5F).
struct MsgCurrentSnapShot {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t status { 0U };
    std::string fileName {};
};

} // namespace Sightline
