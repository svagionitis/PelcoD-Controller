/// @file DeviceSettingsTab.cpp
/// @brief Implementation of device optical parameters and settings tab.

#include "DeviceSettingsTab.h"

namespace PelcoDApp {

DeviceSettingsTab::DeviceSettingsTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void DeviceSettingsTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // Group 1: Automated Modes
    auto* grpAuto = new QGroupBox(tr("Automated Optical Modes"), this);
    auto* gridAuto = new QGridLayout(grpAuto);

    const QStringList triModes { tr("Off (Manual)"), tr("On"), tr("Auto") };
    const QStringList biModes { tr("Disabled"), tr("Enabled") };

    // Auto Focus
    gridAuto->addWidget(new QLabel(tr("Auto-Focus:")), 0, 0);
    cmbAutoFocus = new QComboBox();
    cmbAutoFocus->addItems(triModes);
    gridAuto->addWidget(cmbAutoFocus, 0, 1);

    // Auto Iris
    gridAuto->addWidget(new QLabel(tr("Auto-Iris:")), 0, 2);
    cmbAutoIris = new QComboBox();
    cmbAutoIris->addItems(triModes);
    gridAuto->addWidget(cmbAutoIris, 0, 3);

    // AGC
    gridAuto->addWidget(new QLabel(tr("AGC (Gain Control):")), 1, 0);
    cmbAgc = new QComboBox();
    cmbAgc->addItems(triModes);
    gridAuto->addWidget(cmbAgc, 1, 1);

    // Backlight Comp
    gridAuto->addWidget(new QLabel(tr("Backlight Comp (BLC):")), 1, 2);
    cmbBlc = new QComboBox();
    cmbBlc->addItems(biModes);
    gridAuto->addWidget(cmbBlc, 1, 3);

    // Auto White Balance
    gridAuto->addWidget(new QLabel(tr("Auto White Balance (AWB):")), 2, 0);
    cmbAwb = new QComboBox();
    cmbAwb->addItems(biModes);
    gridAuto->addWidget(cmbAwb, 2, 1);

    mainLayout->addWidget(grpAuto);

    // Group 2: Manual Levels & Speeds
    auto* grpManual = new QGroupBox(tr("Manual Exposure & Speed Adjustments"), this);
    auto* gridManual = new QGridLayout(grpManual);

    // Shutter Speed
    gridManual->addWidget(new QLabel(tr("Shutter Speed (Raw Value):")), 0, 0);
    spinShutter = new QSpinBox();
    spinShutter->setRange(0, 65535);
    spinShutter->setValue(100);
    auto* btnSetShutter = new QPushButton(tr("Set Shutter"));
    gridManual->addWidget(spinShutter, 0, 1);
    gridManual->addWidget(btnSetShutter, 0, 2);

    // Gain
    gridManual->addWidget(new QLabel(tr("Gain (Raw Value):")), 1, 0);
    spinGain = new QSpinBox();
    spinGain->setRange(0, 65535);
    spinGain->setValue(0);
    auto* btnSetGain = new QPushButton(tr("Set Gain"));
    gridManual->addWidget(spinGain, 1, 1);
    gridManual->addWidget(btnSetGain, 1, 2);

    // Auto-Iris Level
    gridManual->addWidget(new QLabel(tr("Auto-Iris Level (0 - 255):")), 2, 0);
    sliderIrisLevel = new QSlider(Qt::Horizontal);
    sliderIrisLevel->setRange(0, 255);
    sliderIrisLevel->setValue(128);
    auto* btnSetIrisLevel = new QPushButton(tr("Apply Level"));
    gridManual->addWidget(sliderIrisLevel, 2, 1);
    gridManual->addWidget(btnSetIrisLevel, 2, 2);

    // Auto-Iris Peak
    gridManual->addWidget(new QLabel(tr("Auto-Iris Peak (0 - 255):")), 3, 0);
    sliderIrisPeak = new QSlider(Qt::Horizontal);
    sliderIrisPeak->setRange(0, 255);
    sliderIrisPeak->setValue(128);
    auto* btnSetIrisPeak = new QPushButton(tr("Apply Peak"));
    gridManual->addWidget(sliderIrisPeak, 3, 1);
    gridManual->addWidget(btnSetIrisPeak, 3, 2);

    mainLayout->addWidget(grpManual);

    // Group 3: Motor Speeds
    auto* grpSpeeds = new QGroupBox(tr("Motor Step Speeds"), this);
    auto* gridSpeeds = new QGridLayout(grpSpeeds);

    const QStringList speedOpts { tr("Speed 0 (Slowest)"), tr("Speed 1"), tr("Speed 2"), tr("Speed 3 (Fastest)") };

    gridSpeeds->addWidget(new QLabel(tr("Zoom Speed:")), 0, 0);
    cmbZoomSpeed = new QComboBox();
    cmbZoomSpeed->addItems(speedOpts);
    gridSpeeds->addWidget(cmbZoomSpeed, 0, 1);

    gridSpeeds->addWidget(new QLabel(tr("Focus Speed:")), 0, 2);
    cmbFocusSpeed = new QComboBox();
    cmbFocusSpeed->addItems(speedOpts);
    gridSpeeds->addWidget(cmbFocusSpeed, 0, 3);

    mainLayout->addWidget(grpSpeeds);
    mainLayout->addStretch();

    // Signal Connections
    connect(cmbAutoFocus, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DeviceSettingsTab::handleAutoFocusChanged);
    connect(cmbAutoIris, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DeviceSettingsTab::handleAutoIrisChanged);
    connect(cmbAgc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceSettingsTab::handleAgcChanged);
    connect(cmbBlc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceSettingsTab::handleBlcChanged);
    connect(cmbAwb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceSettingsTab::handleAwbChanged);

    connect(btnSetShutter, &QPushButton::clicked, this, &DeviceSettingsTab::handleSetShutter);
    connect(btnSetGain, &QPushButton::clicked, this, &DeviceSettingsTab::handleSetGain);
    connect(btnSetIrisLevel, &QPushButton::clicked, this, &DeviceSettingsTab::handleSetIrisLevel);
    connect(btnSetIrisPeak, &QPushButton::clicked, this, &DeviceSettingsTab::handleSetIrisPeak);

    connect(cmbZoomSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DeviceSettingsTab::handleZoomSpeedChanged);
    connect(cmbFocusSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DeviceSettingsTab::handleFocusSpeedChanged);
}

void DeviceSettingsTab::handleAutoFocusChanged(int index)
{
    m_device->setAutoFocus(index);
}

void DeviceSettingsTab::handleAutoIrisChanged(int index)
{
    m_device->setAutoIris(index);
}

void DeviceSettingsTab::handleAgcChanged(int index)
{
    m_device->setAgc(index);
}

void DeviceSettingsTab::handleBlcChanged(int index)
{
    m_device->setBacklightComp(index == 1);
}

void DeviceSettingsTab::handleAwbChanged(int index)
{
    m_device->setAutoWhiteBalance(index == 1);
}

void DeviceSettingsTab::handleSetShutter()
{
    m_device->setShutterSpeed(spinShutter->value());
}

void DeviceSettingsTab::handleSetGain()
{
    m_device->setGain(spinGain->value());
}

void DeviceSettingsTab::handleSetIrisLevel()
{
    m_device->setAutoIrisLevel(sliderIrisLevel->value());
}

void DeviceSettingsTab::handleSetIrisPeak()
{
    m_device->setAutoIrisPeak(sliderIrisPeak->value());
}

void DeviceSettingsTab::handleZoomSpeedChanged(int speed)
{
    m_device->setZoomSpeed(speed);
}

void DeviceSettingsTab::handleFocusSpeedChanged(int speed)
{
    m_device->setFocusSpeed(speed);
}

} // namespace PelcoDApp
