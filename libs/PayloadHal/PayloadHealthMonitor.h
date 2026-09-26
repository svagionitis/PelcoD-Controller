#pragma once

/// @file PayloadHealthMonitor.h
/// @brief Built-In-Test (BIT) & Health Monitoring Subsystem for tactical multi-sensor payloads.
///        Orchestrates Power-On BIT (PBIT), Continuous BIT (CBIT), and Initiated BIT (IBIT).

#include "IDevice.h"
#include "PayloadTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {

/// @enum BitTestType
/// @brief Category of Built-In-Test.
enum class BitTestType : std::uint8_t {
    PowerOn,    ///< Automatic cold/warm boot test (PBIT)
    Continuous, ///< Background non-intrusive monitoring (CBIT)
    Initiated   ///< Operator-commanded active diagnostic routine (IBIT)
};

/// @enum BitResultStatus
/// @brief High-level health outcome of a test or subsystem.
enum class BitResultStatus : std::uint8_t {
    NotRun,
    Passed,
    Warning,
    Degraded,
    Failed,
    Aborted,
    Running
};

/// @enum BitSeverity
/// @brief Severity rating of diagnostic fault records.
enum class BitSeverity : std::uint8_t {
    Info,
    Warning,
    Critical,
    Fatal
};

/// @enum SubsystemComponent
/// @brief Specific hardware component or subsystem within the payload station.
enum class SubsystemComponent : std::uint8_t {
    SystemMaster,
    GimbalPtu,
    DaylightCamera,
    ThermalCamera,
    LaserRangeFinder,
    LaserIlluminator,
    PowerSupply,
    TransportLink,
    InertialSensors
};

/// @namespace DiagnosticFaultCode
/// @brief Standardized diagnostic numeric error codes across payload hardware.
namespace DiagnosticFaultCode {
    constexpr std::uint32_t None = 0U;
    constexpr std::uint32_t PbitFailed = 1001U;
    constexpr std::uint32_t CommTimeout = 2001U;
    constexpr std::uint32_t CommPacketDrop = 2002U;
    constexpr std::uint32_t MotorStallPan = 3001U;
    constexpr std::uint32_t MotorStallTilt = 3002U;
    constexpr std::uint32_t MotorOverCurrent = 3003U;
    constexpr std::uint32_t OverTemperatureWarning = 4001U;
    constexpr std::uint32_t OverTemperatureCritical = 4002U;
    constexpr std::uint32_t LaserInterlockFailure = 5001U;
    constexpr std::uint32_t LaserDiodeOverTemp = 5002U;
    constexpr std::uint32_t VoltageOutOfRange = 6001U;
    constexpr std::uint32_t OpticalDriveStall = 7001U;
    constexpr std::uint32_t SensorStreamLost = 7002U;
} // namespace DiagnosticFaultCode

/// @struct DiagnosticFaultRecord
/// @brief Detailed diagnostic entry for a detected fault or anomaly.
struct DiagnosticFaultRecord {
    std::uint32_t code { 0U };                       ///< Standardized numeric fault code
    SubsystemComponent component { SubsystemComponent::SystemMaster };
    BitSeverity severity { BitSeverity::Warning };
    std::string message {};                          ///< Human-readable diagnostic description
    std::chrono::system_clock::time_point timestamp {};
    bool active { true };                            ///< True if fault is currently persisting
};

/// @struct SystemHealthReport
/// @brief Snapshot of instantaneous system-wide health and vital signs.
struct SystemHealthReport {
    DeviceState overallState { DeviceState::Ready };
    BitResultStatus pbitStatus { BitResultStatus::NotRun };
    BitResultStatus cbitStatus { BitResultStatus::Passed };
    BitResultStatus ibitStatus { BitResultStatus::NotRun };

    // Vital signs
    double enclosureTempC { 25.0 };
    double ptuMotorCurrentA { 0.0 };
    double ptuSupplyVoltageV { 28.0 }; // Typical 28V DC mil/aero rail
    bool motorStallDetected { false };
    bool thermalThrottlingActive { false };
    std::uint32_t commPacketDropCount { 0U };

    std::vector<DiagnosticFaultRecord> activeFaults {};
    std::chrono::system_clock::time_point timestamp {};
};

/// @struct IbitProgress
/// @brief Dynamic execution telemetry for an active Initiated Built-In-Test.
struct IbitProgress {
    bool running { false };
    float progress01 { 0.0f };                      ///< Progress completion [0.0, 1.0]
    std::string currentStepName {};                 ///< Descriptive name of active diagnostic stage
    BitResultStatus currentStatus { BitResultStatus::NotRun };
};

/// @class PayloadHealthMonitor
/// @brief Comprehensive diagnostic engine managing PBIT, CBIT, IBIT, and real-time vital signs.
class PayloadHealthMonitor {
public:
    PayloadHealthMonitor();
    virtual ~PayloadHealthMonitor();

    // --- Fault Reporting & State Management ---

    /// @brief Asserts or logs a diagnostic fault record.
    /// @param[in] fault Fault record details.
    void reportFault(const DiagnosticFaultRecord& fault);

    /// @brief Clears an active fault by numeric code.
    /// @param[in] code Diagnostic fault code.
    void clearFault(std::uint32_t code);

    /// @brief Clears all currently active faults.
    void clearAllFaults();

    /// @brief Checks whether a specific fault code is currently active.
    [[nodiscard]] bool hasFault(std::uint32_t code) const;

    /// @brief Retrieves a list of all currently active fault records.
    [[nodiscard]] std::vector<DiagnosticFaultRecord> activeFaults() const;

    // --- Three Pillars of Built-In-Test ---

    /// @brief Executes Power-On Built-In-Test (PBIT).
    /// @return Result status (Passed if all systems nominal, Degraded/Failed if errors detected).
    BitResultStatus runPbit();

    /// @brief Retrieves the status result of the most recent PBIT.
    [[nodiscard]] BitResultStatus pbitStatus() const;

    /// @brief Starts background Continuous Built-In-Test (CBIT) monitoring thread.
    /// @param[in] interval Sampling period between CBIT cycles.
    void startCbit(std::chrono::milliseconds interval = std::chrono::milliseconds(200));

    /// @brief Stops the background CBIT monitoring thread.
    void stopCbit();

    /// @brief Checks whether background CBIT is actively running.
    [[nodiscard]] bool isCbitRunning() const;

    /// @brief Retrieves the status result of the most recent CBIT cycle.
    [[nodiscard]] BitResultStatus cbitStatus() const;

    /// @brief Initiates an operator-commanded Initiated Built-In-Test (IBIT) routine.
    /// @param[in] intrusive If true, actively sweeps mechanical axes and cycles optics.
    /// @return true if IBIT was successfully launched, false if already running or rejected.
    bool startIbit(bool intrusive = true);

    /// @brief Aborts an actively running IBIT routine immediately.
    void abortIbit();

    /// @brief Queries current IBIT execution progress and step metadata.
    [[nodiscard]] IbitProgress ibitProgress() const;

    /// @brief Retrieves the final or current status of IBIT.
    [[nodiscard]] BitResultStatus ibitStatus() const;

    // --- Overall System Health & Telemetry ---

    /// @brief Generates an instantaneous comprehensive system health report.
    [[nodiscard]] SystemHealthReport healthReport() const;

    /// @brief Updates live simulated/hardware vital signs.
    /// @param[in] tempC Enclosure / sensor temperature in degrees Celsius.
    /// @param[in] motorCurrentA Motor current draw in Amperes.
    /// @param[in] voltageV Power rail voltage in Volts.
    /// @param[in] stall Whether motor stall condition is currently detected.
    /// @param[in] dropCount Cumulative communication packet drop count.
    void updateVitalSigns(double tempC, double motorCurrentA, double voltageV,
                          bool stall = false, std::uint32_t dropCount = 0U);

    /// @brief Explicitly overrides PBIT status (useful for simulated fault injection).
    void setPbitStatus(BitResultStatus status);

    // --- Observers & Callbacks ---

    /// @brief Signature for health status change notifications.
    using HealthCallback = std::function<void(const SystemHealthReport& report)>;

    /// @brief Registers an observer callback triggered when health report or device state changes.
    /// @param[in] cb Callback callable.
    void registerHealthCallback(HealthCallback cb);

private:
    void cbitWorkerLoop(std::chrono::milliseconds interval);
    void ibitWorkerLoop(bool intrusive);
    void evaluateOverallStateLocked();
    void dispatchHealthReportLocked();

    mutable std::mutex m_mutex {};

    BitResultStatus m_pbitStatus { BitResultStatus::NotRun };
    BitResultStatus m_cbitStatus { BitResultStatus::Passed };
    BitResultStatus m_ibitStatus { BitResultStatus::NotRun };
    DeviceState m_overallState { DeviceState::Ready };

    double m_enclosureTempC { 25.0 };
    double m_ptuMotorCurrentA { 0.0 };
    double m_ptuSupplyVoltageV { 28.0 };
    bool m_motorStallDetected { false };
    bool m_thermalThrottlingActive { false };
    std::uint32_t m_commPacketDropCount { 0U };

    std::vector<DiagnosticFaultRecord> m_faults {};
    HealthCallback m_healthCb {};

    // CBIT thread control
    std::thread m_cbitThread {};
    std::atomic<bool> m_cbitRunning { false };
    std::condition_variable m_cbitCv {};

    // IBIT thread control
    std::thread m_ibitThread {};
    std::atomic<bool> m_ibitRunning { false };
    std::atomic<bool> m_ibitAbortRequested { false };
    IbitProgress m_ibitProgress {};
};

} // namespace PayloadHal
