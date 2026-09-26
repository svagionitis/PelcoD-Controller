#include "PayloadHal.h"
#include "PayloadHealthMonitor.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace PayloadHal {
namespace {

TEST(TestPayloadHealthMonitor, PbitExecution)
{
    PayloadHealthMonitor monitor;
    EXPECT_EQ(monitor.pbitStatus(), BitResultStatus::NotRun);

    // Initial clean boot PBIT
    EXPECT_EQ(monitor.runPbit(), BitResultStatus::Passed);
    EXPECT_EQ(monitor.pbitStatus(), BitResultStatus::Passed);

    // Inject a fatal fault and verify PBIT fails
    DiagnosticFaultRecord fatalFault {};
    fatalFault.code = DiagnosticFaultCode::PbitFailed;
    fatalFault.component = SubsystemComponent::SystemMaster;
    fatalFault.severity = BitSeverity::Fatal;
    fatalFault.message = "Hardware self-check ROM checksum failure";
    monitor.reportFault(fatalFault);

    EXPECT_EQ(monitor.runPbit(), BitResultStatus::Failed);
    EXPECT_EQ(monitor.pbitStatus(), BitResultStatus::Failed);

    // Clear fault and verify PBIT passes again
    monitor.clearFault(DiagnosticFaultCode::PbitFailed);
    EXPECT_EQ(monitor.runPbit(), BitResultStatus::Passed);
}

TEST(TestPayloadHealthMonitor, CbitContinuousMonitoring)
{
    PayloadHealthMonitor monitor;
    EXPECT_FALSE(monitor.isCbitRunning());

    monitor.startCbit(std::chrono::milliseconds(50));
    EXPECT_TRUE(monitor.isCbitRunning());

    monitor.updateVitalSigns(25.0, 1.2, 28.0, false, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    const auto report = monitor.healthReport();
    EXPECT_EQ(report.cbitStatus, BitResultStatus::Passed);
    EXPECT_EQ(report.overallState, DeviceState::Ready);
    EXPECT_NEAR(report.enclosureTempC, 25.0, 0.1);
    EXPECT_NEAR(report.ptuSupplyVoltageV, 28.0, 0.1);

    monitor.stopCbit();
    EXPECT_FALSE(monitor.isCbitRunning());
}

TEST(TestPayloadHealthMonitor, FaultReportingAndLifecycle)
{
    PayloadHealthMonitor monitor;
    EXPECT_TRUE(monitor.activeFaults().empty());
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Ready);

    // 1. Report a Warning severity fault -> System stays Ready
    DiagnosticFaultRecord warnFault {};
    warnFault.code = DiagnosticFaultCode::CommPacketDrop;
    warnFault.component = SubsystemComponent::TransportLink;
    warnFault.severity = BitSeverity::Warning;
    warnFault.message = "Occasional packet drop on RS-422";
    monitor.reportFault(warnFault);

    EXPECT_TRUE(monitor.hasFault(DiagnosticFaultCode::CommPacketDrop));
    EXPECT_EQ(monitor.activeFaults().size(), 1U);
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Ready);

    // 2. Report a Critical severity fault -> Escalates to Degraded
    DiagnosticFaultRecord critFault {};
    critFault.code = DiagnosticFaultCode::SensorStreamLost;
    critFault.component = SubsystemComponent::DaylightCamera;
    critFault.severity = BitSeverity::Critical;
    critFault.message = "Primary RTSP stream drop";
    monitor.reportFault(critFault);

    EXPECT_EQ(monitor.activeFaults().size(), 2U);
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Degraded);

    // 3. Report a Fatal severity fault -> Escalates to Fault
    DiagnosticFaultRecord fatalFault {};
    fatalFault.code = DiagnosticFaultCode::LaserInterlockFailure;
    fatalFault.component = SubsystemComponent::LaserRangeFinder;
    fatalFault.severity = BitSeverity::Fatal;
    fatalFault.message = "Laser discharge capacitor safety relay short";
    monitor.reportFault(fatalFault);

    EXPECT_EQ(monitor.activeFaults().size(), 3U);
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Fault);

    // 4. Clear the fatal fault -> De-escalates to Degraded
    monitor.clearFault(DiagnosticFaultCode::LaserInterlockFailure);
    EXPECT_FALSE(monitor.hasFault(DiagnosticFaultCode::LaserInterlockFailure));
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Degraded);

    // 5. Clear all faults -> Returns to Ready
    monitor.clearAllFaults();
    EXPECT_TRUE(monitor.activeFaults().empty());
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Ready);
}

TEST(TestPayloadHealthMonitor, MotorStallDetection)
{
    PayloadHealthMonitor monitor;
    monitor.startCbit(std::chrono::milliseconds(40));

    // Simulate motor stall
    monitor.updateVitalSigns(30.0, 5.8, 27.5, true, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto report = monitor.healthReport();
    EXPECT_TRUE(report.motorStallDetected);
    EXPECT_EQ(report.overallState, DeviceState::Fault);
    EXPECT_TRUE(monitor.hasFault(DiagnosticFaultCode::MotorStallPan));

    // Clear motor stall condition
    monitor.updateVitalSigns(30.0, 0.8, 28.0, false, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    report = monitor.healthReport();
    EXPECT_FALSE(report.motorStallDetected);
    EXPECT_EQ(report.overallState, DeviceState::Ready);
    EXPECT_FALSE(monitor.hasFault(DiagnosticFaultCode::MotorStallPan));

    monitor.stopCbit();
}

TEST(TestPayloadHealthMonitor, ThermalThrottlingAlerts)
{
    PayloadHealthMonitor monitor;
    monitor.startCbit(std::chrono::milliseconds(40));

    // Normal temp
    monitor.updateVitalSigns(45.0, 1.0, 28.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_FALSE(monitor.healthReport().thermalThrottlingActive);

    // Warning temp: 65C (> 60C)
    monitor.updateVitalSigns(65.0, 1.0, 28.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_TRUE(monitor.healthReport().thermalThrottlingActive);
    EXPECT_TRUE(monitor.hasFault(DiagnosticFaultCode::OverTemperatureWarning));

    // Critical temp: 80C (> 75C) -> Escalates to Degraded
    monitor.updateVitalSigns(80.0, 1.0, 28.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_TRUE(monitor.healthReport().thermalThrottlingActive);
    EXPECT_TRUE(monitor.hasFault(DiagnosticFaultCode::OverTemperatureCritical));
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Degraded);

    // Cool down to 50C (<= 55C) -> Clears throttling
    monitor.updateVitalSigns(50.0, 1.0, 28.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_FALSE(monitor.healthReport().thermalThrottlingActive);
    EXPECT_FALSE(monitor.hasFault(DiagnosticFaultCode::OverTemperatureCritical));
    EXPECT_EQ(monitor.healthReport().overallState, DeviceState::Ready);

    monitor.stopCbit();
}

TEST(TestPayloadHealthMonitor, IbitExecutionAndProgress)
{
    PayloadHealthMonitor monitor;
    EXPECT_EQ(monitor.ibitStatus(), BitResultStatus::NotRun);

    EXPECT_TRUE(monitor.startIbit(true));
    // Cannot launch a second IBIT concurrently
    EXPECT_FALSE(monitor.startIbit(true));

    // Check intermediate progress
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    const auto prog = monitor.ibitProgress();
    EXPECT_TRUE(prog.running);
    EXPECT_GT(prog.progress01, 0.0f);

    // Wait for IBIT to complete (5 steps * 50ms = ~250ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    EXPECT_EQ(monitor.ibitStatus(), BitResultStatus::Passed);
    const auto finalProg = monitor.ibitProgress();
    EXPECT_FALSE(finalProg.running);
    EXPECT_FLOAT_EQ(finalProg.progress01, 1.0f);
    EXPECT_EQ(finalProg.currentStatus, BitResultStatus::Passed);
}

TEST(TestPayloadHealthMonitor, IbitAbort)
{
    PayloadHealthMonitor monitor;
    EXPECT_TRUE(monitor.startIbit(true));

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    monitor.abortIbit();

    EXPECT_EQ(monitor.ibitStatus(), BitResultStatus::Aborted);
    const auto prog = monitor.ibitProgress();
    EXPECT_FALSE(prog.running);
    EXPECT_EQ(prog.currentStatus, BitResultStatus::Aborted);
}

TEST(TestPayloadHealthMonitor, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto health = payload->healthMonitor();
    ASSERT_NE(health, nullptr);

    // Initial state of simulated payload is connected
    EXPECT_TRUE(payload->isConnected());

    // Connect runs PBIT and starts CBIT
    EXPECT_TRUE(payload->connect());
    EXPECT_EQ(payload->state(), DeviceState::Ready);
    EXPECT_EQ(health->pbitStatus(), BitResultStatus::Passed);
    EXPECT_TRUE(health->isCbitRunning());

    // Inject fatal fault
    DiagnosticFaultRecord fault {};
    fault.code = DiagnosticFaultCode::MotorOverCurrent;
    fault.component = SubsystemComponent::GimbalPtu;
    fault.severity = BitSeverity::Fatal;
    fault.message = "Severe over-current trip on azimuth drive";
    health->reportFault(fault);

    // SimulatedPayload state must escalate to Fault!
    EXPECT_EQ(payload->state(), DeviceState::Fault);

    // Clear fault -> reverts to Ready
    health->clearFault(DiagnosticFaultCode::MotorOverCurrent);
    EXPECT_EQ(payload->state(), DeviceState::Ready);

    // Disconnect stops CBIT and transitions to Disconnected
    payload->disconnect();
    EXPECT_EQ(payload->state(), DeviceState::Disconnected);
    EXPECT_FALSE(health->isCbitRunning());
}

} // namespace
} // namespace PayloadHal
