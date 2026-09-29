#pragma once

/// @file BridgeAlertTypes.h
/// @brief Strongly-typed data models, enums, and structures for IEC 62923 BAM and IEC 61162-1 alert sentences.

#include "../NmeaTypes.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Nmea::Bam {

/// @enum AlertPriority
/// @brief Alert priority levels per IEC 62923-1 / IEC 62923-2.
enum class AlertPriority : char {
    Emergency = 'E', ///< Emergency Alarm: immediate danger to human life or ship
    Alarm = 'A',     ///< Alarm: immediate danger requiring immediate crew awareness and action
    Warning = 'W',   ///< Warning: precautionary condition requiring attention
    Caution = 'C'    ///< Caution: lowest priority, awareness of unusual condition without acoustic alarm
};

/// @enum AlertCategory
/// @brief Functional alert category per IEC 62923-1.
enum class AlertCategory : char {
    CategoryA = 'A', ///< Category A: Alert handled exclusively at the source equipment (e.g. ECDIS collision)
    CategoryB = 'B', ///< Category B: General alert that can be acknowledged from the central BAM bridge workstation
    CategoryC = 'C'  ///< Category C: Alert with internal operational responsibility transfer
};

/// @enum AlertState
/// @brief Operational state of an active or rectified alert per IEC 62923-1 state machine.
enum class AlertState : char {
    Normal = 'N',                         ///< Alert condition is inactive / normal
    ActiveUnacknowledged = 'V',           ///< Active condition, unacknowledged (acoustic/visual active)
    ActiveSilenced = 'S',                 ///< Active condition, audible annunciator silenced temporarily
    ActiveAcknowledged = 'A',             ///< Active condition, acknowledged by operator
    ActiveResponsibilityTransferred = 'O',///< Active condition, responsibility transferred to another station
    RectifiedUnacknowledged = 'U'         ///< Fault condition cleared, but requires crew acknowledgment
};

/// @enum AlertCommand
/// @brief Command requested in an ARC (Alert Command Request) sentence.
enum class AlertCommand : char {
    Acknowledge = 'A',                  ///< Operator acknowledges alert
    RequestSilence = 'Q',               ///< Operator requests temporary acoustic silencing
    RequestResponsibilityTransfer = 'O' ///< Operator transfers alert responsibility
};

/// @struct AlfData
/// @brief Telemetry unpacked from $--ALF Alert sentence (IEC 62923-1 / IEC 61162-1).
struct AlfData {
    std::uint8_t totalSentences { 1U };      ///< Total sentences in sequence [1 .. 2]
    std::uint8_t sentenceNumber { 1U };      ///< Sentence number in sequence [1 .. 2]
    std::uint8_t sequentialMessageId { 0U }; ///< Sequential message identifier [0 .. 9]
    NmeaUtcTime timeOfLastChange {};         ///< UTC time of last state transition (hhmmss.ss)
    char alertPriority { 'A' };              ///< 'E'=Emergency, 'A'=Alarm, 'W'=Warning, 'C'=Caution
    char alertCategory { 'B' };              ///< 'A'=Cat A, 'B'=Cat B, 'C'=Cat C
    char alertState { 'V' };                 ///< 'V'=Active Unack, 'S'=Silenced, 'A'=Active Ack, 'O'=Transferred, 'U'=Rectified Unack, 'N'=Normal
    std::uint32_t alertIdentifier { 0U };    ///< Manufacturer or standard Alert ID (e.g. 1001)
    std::uint32_t alertInstance { 1U };      ///< Instance of alert [1 .. 999999]
    std::uint32_t revisionCounter { 1U };    ///< Revision counter incremented on state change [1 .. 99]
    std::uint32_t escalationCounter { 0U };  ///< Escalation count [0 .. 9]
    std::string alertText {};                ///< Alert description and cause text
    bool valid { false };
};

/// @struct AlcEntry
/// @brief Single alert descriptor within an ALC cyclic alert list.
struct AlcEntry {
    std::uint32_t alertIdentifier { 0U };
    std::uint32_t alertInstance { 1U };
    std::uint32_t revisionCounter { 1U };
};

/// @struct AlcData
/// @brief Cyclic active alert list unpacked from $--ALC sentence (IEC 62923-1 / IEC 61162-1).
struct AlcData {
    std::uint8_t totalSentences { 1U };      ///< Total sentences in cyclic sequence [1 .. 8]
    std::uint8_t sentenceNumber { 1U };      ///< Sentence number [1 .. 8]
    std::uint8_t sequentialMessageId { 0U }; ///< Sequential message identifier [0 .. 9]
    std::uint8_t alertCount { 0U };          ///< Number of alert entries in this sentence
    std::vector<AlcEntry> alertEntries {};   ///< Triplet descriptors (AlertID, Instance, Revision)
    bool valid { false };
};

/// @struct ArcData
/// @brief Alert command request unpacked from $--ARC sentence (IEC 62923-1 / IEC 61162-1).
struct ArcData {
    NmeaUtcTime releaseTime {};           ///< UTC time command was released
    std::uint32_t alertIdentifier { 0U }; ///< Target Alert ID
    std::uint32_t alertInstance { 1U };   ///< Target Alert Instance
    char command { 'A' };                 ///< 'A'=Acknowledge, 'Q'=Silence, 'O'=Transfer
    bool valid { false };
};

/// @struct HbtData
/// @brief Heartbeat supervision status unpacked from $--HBT sentence (IEC 61162-1).
struct HbtData {
    double configuredIntervalSec { 60.0 };   ///< Configured heartbeat broadcast interval in seconds
    char equipmentStatus { 'A' };            ///< 'A'=Normal operation, 'V'=Defective / degraded
    std::uint8_t sequentialSentenceId { 0U };///< Sequential sentence identifier [0 .. 9]
    bool valid { false };
};

/// @struct AlrData
/// @brief Legacy alert condition and acknowledge status unpacked from $--ALR sentence.
struct AlrData {
    NmeaUtcTime timeOfLastChange {};      ///< UTC time of condition change
    std::uint32_t alertIdentifier { 0U }; ///< Unique alarm identifier
    char condition { 'A' };               ///< 'A'=Threshold exceeded / in alarm, 'V'=Normal
    char acknowledgeState { 'V' };        ///< 'A'=Acknowledged, 'V'=Unacknowledged
    std::string alertText {};             ///< Descriptive alarm text
    bool valid { false };
};

/// @struct AckData
/// @brief Legacy alarm acknowledge unpacked from $--ACK sentence.
struct AckData {
    std::uint32_t alertIdentifier { 0U }; ///< Unique alarm identifier being acknowledged
    bool valid { false };
};

/// @struct AlertRecord
/// @brief Complete in-memory record tracked by the BridgeAlertManager.
struct AlertRecord {
    std::uint32_t alertIdentifier { 0U };
    std::uint32_t alertInstance { 1U };
    AlertPriority priority { AlertPriority::Alarm };
    AlertCategory category { AlertCategory::CategoryB };
    AlertState state { AlertState::Normal };
    std::uint32_t revisionCounter { 1U };
    std::uint32_t escalationCounter { 0U };
    std::string alertTitle {};
    std::string alertText {};
    std::chrono::steady_clock::time_point lastStateChangeTime {};
    std::chrono::steady_clock::time_point silencedTime {};
    bool isSilenced { false };
};

} // namespace Nmea::Bam
