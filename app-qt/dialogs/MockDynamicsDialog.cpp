/// @file MockDynamicsDialog.cpp
/// @brief Dialog for configuring simulated PTZ physical kinematics, link latency, and packet loss.

#include "MockDynamicsDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace PelcoDApp {

MockDynamicsDialog::MockDynamicsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void MockDynamicsDialog::setupUi()
{
    setWindowTitle(tr("Mock Device Dynamics & Link Simulation"));
    resize(460, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- 1. Kinematics Group ---
    grpKinematics = new QGroupBox(tr("PTZ Physical Kinematics"), this);
    grpKinematics->setCheckable(true);
    grpKinematics->setChecked(false);
    auto* kinForm = new QFormLayout(grpKinematics);
    kinForm->setSpacing(8);

    cmbKinematicsPreset = new QComboBox(grpKinematics);
    cmbKinematicsPreset->addItem(tr("Custom Dynamics"), 0);
    cmbKinematicsPreset->addItem(tr("Agile PTZ Dome (90°/s pan, 45°/s tilt)"), 1);
    cmbKinematicsPreset->addItem(tr("Standard Speed Dome (60°/s pan, 30°/s tilt)"), 2);
    cmbKinematicsPreset->addItem(tr("Heavy Surveillance Gimbal (20°/s pan, 10°/s tilt)"), 3);
    cmbKinematicsPreset->addItem(tr("Slow Satellite Dish (5°/s pan, 2°/s tilt)"), 4);
    kinForm->addRow(tr("Preset:"), cmbKinematicsPreset);

    spinMaxPanSpeed = new QDoubleSpinBox(grpKinematics);
    spinMaxPanSpeed->setRange(1.0, 360.0);
    spinMaxPanSpeed->setValue(60.0);
    spinMaxPanSpeed->setSuffix(tr(" °/s"));
    kinForm->addRow(tr("Max Pan Speed:"), spinMaxPanSpeed);

    spinMaxTiltSpeed = new QDoubleSpinBox(grpKinematics);
    spinMaxTiltSpeed->setRange(1.0, 180.0);
    spinMaxTiltSpeed->setValue(30.0);
    spinMaxTiltSpeed->setSuffix(tr(" °/s"));
    kinForm->addRow(tr("Max Tilt Speed:"), spinMaxTiltSpeed);

    spinPanAccel = new QDoubleSpinBox(grpKinematics);
    spinPanAccel->setRange(5.0, 720.0);
    spinPanAccel->setValue(180.0);
    spinPanAccel->setSuffix(tr(" °/s²"));
    kinForm->addRow(tr("Pan Acceleration:"), spinPanAccel);

    spinTiltAccel = new QDoubleSpinBox(grpKinematics);
    spinTiltAccel->setRange(5.0, 360.0);
    spinTiltAccel->setValue(90.0);
    spinTiltAccel->setSuffix(tr(" °/s²"));
    kinForm->addRow(tr("Tilt Acceleration:"), spinTiltAccel);

    spinZoomTransit = new QDoubleSpinBox(grpKinematics);
    spinZoomTransit->setRange(0.2, 10.0);
    spinZoomTransit->setValue(2.0);
    spinZoomTransit->setSingleStep(0.5);
    spinZoomTransit->setSuffix(tr(" s"));
    kinForm->addRow(tr("Zoom Transit Time:"), spinZoomTransit);

    mainLayout->addWidget(grpKinematics);

    // --- 2. Latency & Link Group ---
    grpLatency = new QGroupBox(tr("Transport Delay & Loss Simulation"), this);
    grpLatency->setCheckable(true);
    grpLatency->setChecked(false);
    auto* latForm = new QFormLayout(grpLatency);
    latForm->setSpacing(8);

    cmbLatencyPreset = new QComboBox(grpLatency);
    cmbLatencyPreset->addItem(tr("Custom Delay"), 0);
    cmbLatencyPreset->addItem(tr("RS-485 Half-Duplex (25ms delay, 5ms jitter)"), 1);
    cmbLatencyPreset->addItem(tr("Ethernet TCP Socket (10ms delay, 2ms jitter)"), 2);
    cmbLatencyPreset->addItem(tr("Lossy Cellular Bridge (80ms delay, 25ms jitter, 2% loss)"), 3);
    cmbLatencyPreset->addItem(tr("Satellite / Remote Link (300ms delay, 50ms jitter, 5% loss)"), 4);
    latForm->addRow(tr("Preset:"), cmbLatencyPreset);

    spinBaseLatency = new QSpinBox(grpLatency);
    spinBaseLatency->setRange(0, 2000);
    spinBaseLatency->setValue(25);
    spinBaseLatency->setSuffix(tr(" ms"));
    latForm->addRow(tr("Base Latency:"), spinBaseLatency);

    spinJitter = new QSpinBox(grpLatency);
    spinJitter->setRange(0, 500);
    spinJitter->setValue(5);
    spinJitter->setSuffix(tr(" ms"));
    latForm->addRow(tr("Latency Jitter (±):"), spinJitter);

    spinPacketDrop = new QDoubleSpinBox(grpLatency);
    spinPacketDrop->setRange(0.0, 50.0);
    spinPacketDrop->setValue(0.0);
    spinPacketDrop->setSingleStep(0.5);
    spinPacketDrop->setSuffix(tr(" %"));
    latForm->addRow(tr("Packet Drop Rate:"), spinPacketDrop);

    mainLayout->addWidget(grpLatency);

    // --- 3. Bottom Action Buttons ---
    auto* btnLayout = new QHBoxLayout();
    btnResetIdeal = new QPushButton(tr("Reset to Ideal (Instant)"), this);
    btnApply = new QPushButton(tr("Apply"), this);
    btnApply->setObjectName("btnPrimary");
    btnCancel = new QPushButton(tr("Cancel"), this);

    btnLayout->addWidget(btnResetIdeal);
    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(cmbKinematicsPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MockDynamicsDialog::onKinematicsPresetChanged);
    connect(cmbLatencyPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MockDynamicsDialog::onLatencyPresetChanged);
    connect(btnResetIdeal, &QPushButton::clicked, this, &MockDynamicsDialog::onResetToIdeal);
    connect(btnApply, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void MockDynamicsDialog::onKinematicsPresetChanged(int index)
{
    switch (index) {
    case 1: // Agile PTZ Dome
        spinMaxPanSpeed->setValue(90.0);
        spinMaxTiltSpeed->setValue(45.0);
        spinPanAccel->setValue(240.0);
        spinTiltAccel->setValue(120.0);
        spinZoomTransit->setValue(1.5);
        break;
    case 2: // Standard Speed Dome
        spinMaxPanSpeed->setValue(60.0);
        spinMaxTiltSpeed->setValue(30.0);
        spinPanAccel->setValue(180.0);
        spinTiltAccel->setValue(90.0);
        spinZoomTransit->setValue(2.0);
        break;
    case 3: // Heavy Surveillance Gimbal
        spinMaxPanSpeed->setValue(20.0);
        spinMaxTiltSpeed->setValue(10.0);
        spinPanAccel->setValue(40.0);
        spinTiltAccel->setValue(20.0);
        spinZoomTransit->setValue(3.5);
        break;
    case 4: // Slow Satellite Dish
        spinMaxPanSpeed->setValue(5.0);
        spinMaxTiltSpeed->setValue(2.0);
        spinPanAccel->setValue(5.0);
        spinTiltAccel->setValue(2.0);
        spinZoomTransit->setValue(5.0);
        break;
    default:
        break;
    }
}

void MockDynamicsDialog::onLatencyPresetChanged(int index)
{
    switch (index) {
    case 1: // RS-485 Half-Duplex
        spinBaseLatency->setValue(25);
        spinJitter->setValue(5);
        spinPacketDrop->setValue(0.0);
        break;
    case 2: // Ethernet TCP Socket
        spinBaseLatency->setValue(10);
        spinJitter->setValue(2);
        spinPacketDrop->setValue(0.0);
        break;
    case 3: // Lossy Cellular Bridge
        spinBaseLatency->setValue(80);
        spinJitter->setValue(25);
        spinPacketDrop->setValue(2.0);
        break;
    case 4: // Satellite / Remote Link
        spinBaseLatency->setValue(300);
        spinJitter->setValue(50);
        spinPacketDrop->setValue(5.0);
        break;
    default:
        break;
    }
}

void MockDynamicsDialog::onResetToIdeal()
{
    grpKinematics->setChecked(false);
    cmbKinematicsPreset->setCurrentIndex(0);
    spinMaxPanSpeed->setValue(60.0);
    spinMaxTiltSpeed->setValue(30.0);
    spinPanAccel->setValue(180.0);
    spinTiltAccel->setValue(90.0);
    spinZoomTransit->setValue(2.0);

    grpLatency->setChecked(false);
    cmbLatencyPreset->setCurrentIndex(0);
    spinBaseLatency->setValue(0);
    spinJitter->setValue(0);
    spinPacketDrop->setValue(0.0);
}

void MockDynamicsDialog::setKinematicsConfig(const PelcoD::KinematicsConfig& config)
{
    grpKinematics->setChecked(config.enabled);
    spinMaxPanSpeed->setValue(config.maxPanSpeedDegPerSec);
    spinMaxTiltSpeed->setValue(config.maxTiltSpeedDegPerSec);
    spinPanAccel->setValue(config.panAccelerationDegPerSec2);
    spinTiltAccel->setValue(config.tiltAccelerationDegPerSec2);
    spinZoomTransit->setValue(config.zoomTransitTimeSeconds);
}

PelcoD::KinematicsConfig MockDynamicsDialog::kinematicsConfig() const
{
    PelcoD::KinematicsConfig cfg;
    cfg.enabled = grpKinematics->isChecked();
    cfg.maxPanSpeedDegPerSec = spinMaxPanSpeed->value();
    cfg.maxTiltSpeedDegPerSec = spinMaxTiltSpeed->value();
    cfg.panAccelerationDegPerSec2 = spinPanAccel->value();
    cfg.tiltAccelerationDegPerSec2 = spinTiltAccel->value();
    cfg.zoomTransitTimeSeconds = spinZoomTransit->value();
    return cfg;
}

void MockDynamicsDialog::setLatencyConfig(const PelcoD::LatencyConfig& config)
{
    grpLatency->setChecked(config.enabled);
    spinBaseLatency->setValue(static_cast<int>(config.baseLatencyMs));
    spinJitter->setValue(static_cast<int>(config.jitterMs));
    spinPacketDrop->setValue(config.packetDropPercent);
}

PelcoD::LatencyConfig MockDynamicsDialog::latencyConfig() const
{
    PelcoD::LatencyConfig cfg;
    cfg.enabled = grpLatency->isChecked();
    cfg.baseLatencyMs = static_cast<std::uint32_t>(spinBaseLatency->value());
    cfg.jitterMs = static_cast<std::uint32_t>(spinJitter->value());
    cfg.packetDropPercent = spinPacketDrop->value();
    return cfg;
}

} // namespace PelcoDApp
