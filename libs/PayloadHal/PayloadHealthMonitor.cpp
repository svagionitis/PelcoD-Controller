#include "PayloadHealthMonitor.h"

#include <algorithm>

namespace PayloadHal {

PayloadHealthMonitor::PayloadHealthMonitor()
{
}

PayloadHealthMonitor::~PayloadHealthMonitor()
{
    stopCbit();
    abortIbit();
}

void PayloadHealthMonitor::reportFault(const DiagnosticFaultRecord& fault)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool found = false;
    for (auto& existing : m_faults) {
        if (existing.code == fault.code) {
            existing = fault;
            existing.active = true;
            found = true;
            break;
        }
    }
    if (!found) {
        m_faults.push_back(fault);
    }
    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

void PayloadHealthMonitor::clearFault(std::uint32_t code)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& f : m_faults) {
        if (f.code == code) {
            f.active = false;
        }
    }
    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

void PayloadHealthMonitor::clearAllFaults()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& f : m_faults) {
        f.active = false;
    }
    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

bool PayloadHealthMonitor::hasFault(std::uint32_t code) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& f : m_faults) {
        if (f.code == code && f.active) {
            return true;
        }
    }
    return false;
}

std::vector<DiagnosticFaultRecord> PayloadHealthMonitor::activeFaults() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DiagnosticFaultRecord> list;
    for (const auto& f : m_faults) {
        if (f.active) {
            list.push_back(f);
        }
    }
    return list;
}

BitResultStatus PayloadHealthMonitor::runPbit()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool fatalOrCritical = false;
    for (const auto& f : m_faults) {
        if (f.active && (f.severity == BitSeverity::Fatal || f.severity == BitSeverity::Critical)) {
            fatalOrCritical = true;
            break;
        }
    }

    if (fatalOrCritical) {
        m_pbitStatus = BitResultStatus::Failed;
    } else {
        m_pbitStatus = BitResultStatus::Passed;
    }

    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
    return m_pbitStatus;
}

BitResultStatus PayloadHealthMonitor::pbitStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pbitStatus;
}

void PayloadHealthMonitor::setPbitStatus(BitResultStatus status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pbitStatus = status;
    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

void PayloadHealthMonitor::startCbit(std::chrono::milliseconds interval)
{
    if (m_cbitRunning.exchange(true)) {
        return; // Already running
    }
    m_cbitThread = std::thread(&PayloadHealthMonitor::cbitWorkerLoop, this, interval);
}

void PayloadHealthMonitor::stopCbit()
{
    if (!m_cbitRunning.exchange(false)) {
        return;
    }
    m_cbitCv.notify_all();
    if (m_cbitThread.joinable()) {
        m_cbitThread.join();
    }
}

bool PayloadHealthMonitor::isCbitRunning() const
{
    return m_cbitRunning.load();
}

BitResultStatus PayloadHealthMonitor::cbitStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cbitStatus;
}

void PayloadHealthMonitor::cbitWorkerLoop(std::chrono::milliseconds interval)
{
    while (m_cbitRunning.load()) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cbitCv.wait_for(lock, interval, [this]() { return !m_cbitRunning.load(); });
            if (!m_cbitRunning.load()) {
                break;
            }

            // Thermal checks
            if (m_enclosureTempC > 75.0) {
                m_thermalThrottlingActive = true;
                DiagnosticFaultRecord rec {};
                rec.code = DiagnosticFaultCode::OverTemperatureCritical;
                rec.component = SubsystemComponent::SystemMaster;
                rec.severity = BitSeverity::Critical;
                rec.message = "Critical enclosure over-temperature exceeding 75C";
                rec.timestamp = std::chrono::system_clock::now();
                rec.active = true;

                bool exists = false;
                for (auto& f : m_faults) {
                    if (f.code == rec.code) {
                        f = rec;
                        exists = true;
                        break;
                    }
                }
                if (!exists) m_faults.push_back(rec);
            } else if (m_enclosureTempC > 60.0) {
                m_thermalThrottlingActive = true;
                DiagnosticFaultRecord rec {};
                rec.code = DiagnosticFaultCode::OverTemperatureWarning;
                rec.component = SubsystemComponent::SystemMaster;
                rec.severity = BitSeverity::Warning;
                rec.message = "High enclosure temperature exceeding 60C";
                rec.timestamp = std::chrono::system_clock::now();
                rec.active = true;

                bool exists = false;
                for (auto& f : m_faults) {
                    if (f.code == rec.code) {
                        f = rec;
                        exists = true;
                        break;
                    }
                }
                if (!exists) m_faults.push_back(rec);
            } else if (m_enclosureTempC <= 55.0) {
                m_thermalThrottlingActive = false;
                for (auto& f : m_faults) {
                    if (f.code == DiagnosticFaultCode::OverTemperatureWarning ||
                        f.code == DiagnosticFaultCode::OverTemperatureCritical) {
                        f.active = false;
                    }
                }
            }

            // Motor stall checks
            if (m_motorStallDetected) {
                DiagnosticFaultRecord rec {};
                rec.code = DiagnosticFaultCode::MotorStallPan;
                rec.component = SubsystemComponent::GimbalPtu;
                rec.severity = BitSeverity::Fatal;
                rec.message = "Motor stall detected on pan/tilt drive";
                rec.timestamp = std::chrono::system_clock::now();
                rec.active = true;

                bool exists = false;
                for (auto& f : m_faults) {
                    if (f.code == rec.code) {
                        f = rec;
                        exists = true;
                        break;
                    }
                }
                if (!exists) m_faults.push_back(rec);
            } else {
                for (auto& f : m_faults) {
                    if (f.code == DiagnosticFaultCode::MotorStallPan ||
                        f.code == DiagnosticFaultCode::MotorStallTilt) {
                        f.active = false;
                    }
                }
            }

            evaluateOverallStateLocked();
            m_cbitStatus = (m_overallState == DeviceState::Fault)
                ? BitResultStatus::Failed
                : ((m_overallState == DeviceState::Degraded) ? BitResultStatus::Degraded : BitResultStatus::Passed);

            dispatchHealthReportLocked();
        }
    }
}

bool PayloadHealthMonitor::startIbit(bool intrusive)
{
    if (m_ibitRunning.exchange(true)) {
        return false; // Already in progress
    }
    if (m_ibitThread.joinable()) {
        m_ibitThread.join();
    }
    m_ibitAbortRequested = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ibitStatus = BitResultStatus::Running;
        m_ibitProgress.running = true;
        m_ibitProgress.progress01 = 0.0f;
        m_ibitProgress.currentStepName = "Initiating System IBIT";
        m_ibitProgress.currentStatus = BitResultStatus::Running;
    }

    m_ibitThread = std::thread(&PayloadHealthMonitor::ibitWorkerLoop, this, intrusive);
    return true;
}

void PayloadHealthMonitor::abortIbit()
{
    m_ibitAbortRequested = true;
    if (m_ibitThread.joinable()) {
        m_ibitThread.join();
    }
    m_ibitRunning = false;
}

IbitProgress PayloadHealthMonitor::ibitProgress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ibitProgress;
}

BitResultStatus PayloadHealthMonitor::ibitStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ibitStatus;
}

void PayloadHealthMonitor::ibitWorkerLoop(bool intrusive)
{
    struct Step {
        const char* name;
        float progress;
    };

    const std::vector<Step> steps = {
        { "Transport Comm Bus Ping & Latency Verification", 0.20f },
        { "Gimbal Pan/Tilt Mechanical Travel & Encoder Limits", 0.45f },
        { "Daylight Camera Optical Zoom & Autofocus Drive", 0.70f },
        { "Thermal Sensor NUC Calibration Shutter", 0.85f },
        { "Laser Capacitor Charge & Safety Interlock Loop", 1.00f }
    };

    (void)intrusive;

    for (const auto& step : steps) {
        if (m_ibitAbortRequested.load()) {
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ibitProgress.currentStepName = step.name;
            m_ibitProgress.progress01 = step.progress;
        }

        // Simulate multi-stage hardware diagnostic processing
        for (int i = 0; i < 5; ++i) {
            if (m_ibitAbortRequested.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_ibitAbortRequested.load()) {
        m_ibitStatus = BitResultStatus::Aborted;
        m_ibitProgress.running = false;
        m_ibitProgress.currentStatus = BitResultStatus::Aborted;
        m_ibitProgress.currentStepName = "Aborted by Operator";
    } else {
        m_ibitStatus = BitResultStatus::Passed;
        m_ibitProgress.running = false;
        m_ibitProgress.progress01 = 1.0f;
        m_ibitProgress.currentStatus = BitResultStatus::Passed;
        m_ibitProgress.currentStepName = "Complete";
    }
    m_ibitRunning = false;
    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

void PayloadHealthMonitor::updateVitalSigns(double tempC, double motorCurrentA, double voltageV,
                                            bool stall, std::uint32_t dropCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enclosureTempC = tempC;
    m_ptuMotorCurrentA = motorCurrentA;
    m_ptuSupplyVoltageV = voltageV;
    m_motorStallDetected = stall;
    m_commPacketDropCount = dropCount;

    evaluateOverallStateLocked();
    dispatchHealthReportLocked();
}

void PayloadHealthMonitor::evaluateOverallStateLocked()
{
    bool fatal = m_motorStallDetected;
    bool critical = m_thermalThrottlingActive || (m_pbitStatus == BitResultStatus::Failed);

    for (const auto& f : m_faults) {
        if (!f.active) {
            continue;
        }
        if (f.severity == BitSeverity::Fatal) {
            fatal = true;
        } else if (f.severity == BitSeverity::Critical) {
            critical = true;
        }
    }

    if (fatal) {
        m_overallState = DeviceState::Fault;
    } else if (critical) {
        m_overallState = DeviceState::Degraded;
    } else {
        m_overallState = DeviceState::Ready;
    }
}

SystemHealthReport PayloadHealthMonitor::healthReport() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    SystemHealthReport rep {};
    rep.overallState = m_overallState;
    rep.pbitStatus = m_pbitStatus;
    rep.cbitStatus = m_cbitStatus;
    rep.ibitStatus = m_ibitStatus;
    rep.enclosureTempC = m_enclosureTempC;
    rep.ptuMotorCurrentA = m_ptuMotorCurrentA;
    rep.ptuSupplyVoltageV = m_ptuSupplyVoltageV;
    rep.motorStallDetected = m_motorStallDetected;
    rep.thermalThrottlingActive = m_thermalThrottlingActive;
    rep.commPacketDropCount = m_commPacketDropCount;
    rep.timestamp = std::chrono::system_clock::now();

    for (const auto& f : m_faults) {
        if (f.active) {
            rep.activeFaults.push_back(f);
        }
    }
    return rep;
}

void PayloadHealthMonitor::dispatchHealthReportLocked()
{
    if (!m_healthCb) {
        return;
    }
    SystemHealthReport rep {};
    rep.overallState = m_overallState;
    rep.pbitStatus = m_pbitStatus;
    rep.cbitStatus = m_cbitStatus;
    rep.ibitStatus = m_ibitStatus;
    rep.enclosureTempC = m_enclosureTempC;
    rep.ptuMotorCurrentA = m_ptuMotorCurrentA;
    rep.ptuSupplyVoltageV = m_ptuSupplyVoltageV;
    rep.motorStallDetected = m_motorStallDetected;
    rep.thermalThrottlingActive = m_thermalThrottlingActive;
    rep.commPacketDropCount = m_commPacketDropCount;
    rep.timestamp = std::chrono::system_clock::now();

    for (const auto& f : m_faults) {
        if (f.active) {
            rep.activeFaults.push_back(f);
        }
    }
    m_healthCb(rep);
}

void PayloadHealthMonitor::registerHealthCallback(HealthCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_healthCb = std::move(cb);
}

} // namespace PayloadHal
