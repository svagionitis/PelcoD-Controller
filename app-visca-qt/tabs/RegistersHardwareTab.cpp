/// @file RegistersHardwareTab.cpp
/// @brief Implementation of hardware capability gating and register I/O tab.

#include "RegistersHardwareTab.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ViscaApp {

RegistersHardwareTab::RegistersHardwareTab(QViscaSonyDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device(device)
{
    m_capabilities = Visca::Sony::SonyCameraModel::getCapabilities(Visca::Sony::SonyCameraModelType::FCB_EV9520L);
    setupUi();

    if (m_device) {
        connect(m_device, &QViscaSonyDevice::modelDiscovered, this, &RegistersHardwareTab::updateModelCapabilities);
        connect(m_device, &QViscaSonyDevice::statusUpdated, this, &RegistersHardwareTab::updateTelemetry);
    }
}

void RegistersHardwareTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // ==========================================
    // Left Box: Model Hardware Profile & Gated Features
    // ==========================================
    auto* grpProfile = new QGroupBox(tr("Active Camera Profile & Hardware Capabilities"), this);
    auto* profLayout = new QVBoxLayout(grpProfile);
    profLayout->setSpacing(12);

    lblModelName = new QLabel(this);
    lblModelName->setStyleSheet("background-color: #1f242c; color: #58a6ff; font-size: 14px; font-weight: bold; "
                                "padding: 8px; border-radius: 4px; border: 1px solid #30363d;");
    lblModelName->setAlignment(Qt::AlignCenter);
    profLayout->addWidget(lblModelName);

    lblSensorDesc = new QLabel(this);
    lblSensorDesc->setStyleSheet("color: #8b949e; font-size: 11px;");
    lblSensorDesc->setWordWrap(true);
    profLayout->addWidget(lblSensorDesc);

    lblOpticalRange = new QLabel(this);
    lblOpticalRange->setStyleSheet("color: #e6edf3; font-weight: bold;");
    profLayout->addWidget(lblOpticalRange);

    // Operating Mode (4K vs FHD)
    auto* rowOp = new QVBoxLayout();
    rowOp->addWidget(new QLabel(tr("Video Operating Mode (Reg 0x72):"), grpProfile));

    cmbOperatingMode = new QComboBox(grpProfile);
    cmbOperatingMode->addItem(tr("1080p 59.94 / 60 fps (0x01)"), 0x01);
    cmbOperatingMode->addItem(tr("1080p 50 fps (0x02)"), 0x02);
    cmbOperatingMode->addItem(tr("1080p 29.97 / 30 fps (0x05)"), 0x05);
    cmbOperatingMode->addItem(tr("1080p 25 fps (0x06)"), 0x06);
    cmbOperatingMode->addItem(tr("720p 60 fps (0x0B)"), 0x0B);
    cmbOperatingMode->addItem(tr("4K UHD 2160p 29.97 / 30 fps (0x25) [EW9500H only]"), 0x25);
    cmbOperatingMode->addItem(tr("4K UHD 2160p 25 fps (0x26) [EW9500H only]"), 0x26);
    rowOp->addWidget(cmbOperatingMode);

    lblOperatingModeWarn = new QLabel(grpProfile);
    lblOperatingModeWarn->setStyleSheet("color: #d29922; font-size: 10px;");
    rowOp->addWidget(lblOperatingModeWarn);
    profLayout->addLayout(rowOp);

    // Distortion Compensation
    auto* rowDist = new QHBoxLayout();
    chkDistortionComp = new QCheckBox(tr("Lens Distortion Compensation (Reg 0x57)"), grpProfile);
    lblDistortionCompWarn = new QLabel(grpProfile);
    lblDistortionCompWarn->setStyleSheet("color: #8b949e; font-size: 10px;");
    rowDist->addWidget(chkDistortionComp);
    rowDist->addWidget(lblDistortionCompWarn);
    profLayout->addLayout(rowDist);

    // Optical Axis Gap
    auto* rowGap = new QHBoxLayout();
    chkOpticalAxisGap = new QCheckBox(tr("Optical Axis Gap Compensation (Reg 0x47)"), grpProfile);
    lblOpticalAxisGapWarn = new QLabel(grpProfile);
    lblOpticalAxisGapWarn->setStyleSheet("color: #8b949e; font-size: 10px;");
    rowGap->addWidget(chkOpticalAxisGap);
    rowGap->addWidget(lblOpticalAxisGapWarn);
    profLayout->addLayout(rowGap);

    // Video Output Interface Selectors
    auto* rowLvds = new QHBoxLayout();
    cmbLvdsMode = new QComboBox(grpProfile);
    cmbLvdsMode->addItem(tr("Single LVDS Output"), 0x00);
    cmbLvdsMode->addItem(tr("Dual LVDS Output"), 0x01);
    rowLvds->addWidget(new QLabel(tr("LVDS Mode (Reg 0x74):"), grpProfile));
    rowLvds->addWidget(cmbLvdsMode);
    profLayout->addLayout(rowLvds);

    auto* rowTmds = new QHBoxLayout();
    cmbTmdsMode = new QComboBox(grpProfile);
    cmbTmdsMode->addItem(tr("HDMI / TMDS Output (RGB)"), 0x00);
    cmbTmdsMode->addItem(tr("HDMI / TMDS Output (YCbCr 4:2:2)"), 0x01);
    rowTmds->addWidget(new QLabel(tr("TMDS Digital Output (Reg 0x60):"), grpProfile));
    rowTmds->addWidget(cmbTmdsMode);
    profLayout->addLayout(rowTmds);

    profLayout->addStretch();
    mainLayout->addWidget(grpProfile);

    // ==========================================
    // Right Box: Raw Camera Register R/W Tool
    // ==========================================
    auto* grpReg = new QGroupBox(tr("Direct Camera Register Tool (0x00 to 0x7F)"), this);
    auto* regLayout = new QVBoxLayout(grpReg);
    regLayout->setSpacing(12);

    auto* regDesc = new QLabel(
        tr("Directly inspect and modify internal Sony camera register banks for advanced hardware configuration."),
        grpReg);
    regDesc->setWordWrap(true);
    regDesc->setStyleSheet("color: #8b949e;");
    regLayout->addWidget(regDesc);

    auto* rowRegInputs = new QHBoxLayout();
    spinRegAddress = new QSpinBox(grpReg);
    spinRegAddress->setRange(0x00, 0x7F);
    spinRegAddress->setDisplayIntegerBase(16);
    spinRegAddress->setPrefix("Reg: 0x");

    spinRegValue = new QSpinBox(grpReg);
    spinRegValue->setRange(0x00, 0xFF);
    spinRegValue->setDisplayIntegerBase(16);
    spinRegValue->setPrefix("Val: 0x");

    rowRegInputs->addWidget(spinRegAddress);
    rowRegInputs->addWidget(spinRegValue);
    regLayout->addLayout(rowRegInputs);

    auto* rowRegBtns = new QHBoxLayout();
    btnReadReg = new QPushButton(tr("Read Register"), grpReg);
    btnWriteReg = new QPushButton(tr("Write Register"), grpReg);
    btnWriteReg->setObjectName("btnPrimary");

    rowRegBtns->addWidget(btnReadReg);
    rowRegBtns->addWidget(btnWriteReg);
    regLayout->addLayout(rowRegBtns);

    lblRegResult = new QLabel(tr("Status: Ready"), grpReg);
    lblRegResult->setStyleSheet("color: #58a6ff; font-weight: bold;");
    regLayout->addWidget(lblRegResult);

    regLayout->addStretch();
    mainLayout->addWidget(grpReg);

    // Wire events
    connect(cmbOperatingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &RegistersHardwareTab::handleOperatingModeChanged);
    connect(chkDistortionComp, &QCheckBox::toggled, this, &RegistersHardwareTab::handleDistortionCompToggled);
    connect(chkOpticalAxisGap, &QCheckBox::toggled, this, &RegistersHardwareTab::handleOpticalAxisGapToggled);
    connect(cmbLvdsMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &RegistersHardwareTab::handleLvdsModeChanged);
    connect(cmbTmdsMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &RegistersHardwareTab::handleTmdsModeChanged);

    connect(btnReadReg, &QPushButton::clicked, this, &RegistersHardwareTab::handleReadRegisterClicked);
    connect(btnWriteReg, &QPushButton::clicked, this, &RegistersHardwareTab::handleWriteRegisterClicked);

    applyGatingUI();
}

void RegistersHardwareTab::updateModelCapabilities(
    Visca::Sony::SonyCameraModelType /*modelType*/, const Visca::Sony::CameraCapabilities& caps)
{
    m_capabilities = caps;
    applyGatingUI();
}

void RegistersHardwareTab::applyGatingUI()
{
    lblModelName->setText(QString::fromStdString(m_capabilities.modelName));
    lblSensorDesc->setText(QString::fromStdString(m_capabilities.sensorDescription));
    lblOpticalRange->setText(tr("Focal Range: %1 mm – %2 mm (%3x Optical, %4x Digital)")
                                 .arg(m_capabilities.minFocalLengthMm, 0, 'f', 1)
                                 .arg(m_capabilities.maxFocalLengthMm, 0, 'f', 1)
                                 .arg(m_capabilities.maxOpticalZoomRatio, 0, 'f', 0)
                                 .arg(m_capabilities.maxDigitalZoomRatio, 0, 'f', 0));

    // 4K Operating Mode Gating
    if (!m_capabilities.supports4K) {
        lblOperatingModeWarn->setText(tr("⚠ 4K modes disabled: active sensor is 1080p Full HD (FCB-EV9520L)."));
    } else {
        lblOperatingModeWarn->setText(tr("✓ 4K UHD resolutions active (FCB-EW9500H)."));
    }

    // Distortion Compensation Gating
    chkDistortionComp->setEnabled(m_capabilities.supportsDistortionCompensation);
    lblDistortionCompWarn->setText(
        m_capabilities.supportsDistortionCompensation ? tr("(Supported on EV9520L)") : tr("(Unsupported on EW9500H)"));

    // Optical Axis Gap Gating
    chkOpticalAxisGap->setEnabled(m_capabilities.supportsOpticalAxisGapCompensation);
    lblOpticalAxisGapWarn->setText(m_capabilities.supportsOpticalAxisGapCompensation ? tr("(Supported on EV9520L)")
                                                                                     : tr("(Unsupported on EW9500H)"));

    // LVDS vs TMDS
    cmbLvdsMode->setEnabled(m_capabilities.supportsLvdsMode);
    cmbTmdsMode->setEnabled(m_capabilities.supportsTmdsOutput);
}

void RegistersHardwareTab::handleOperatingModeChanged(int index)
{
    const uint8_t mode = static_cast<uint8_t>(cmbOperatingMode->itemData(index).toUInt());
    if (m_device) {
        m_device->setOperatingMode(mode);
    }
}

void RegistersHardwareTab::handleDistortionCompToggled(bool checked)
{
    if (m_device) {
        m_device->setDistortionCompensation(checked);
    }
}

void RegistersHardwareTab::handleOpticalAxisGapToggled(bool checked)
{
    if (m_device) {
        m_device->setOpticalAxisGapCompensation(checked);
    }
}

void RegistersHardwareTab::handleLvdsModeChanged(int index)
{
    const uint8_t mode = static_cast<uint8_t>(cmbLvdsMode->itemData(index).toUInt());
    if (m_device) {
        m_device->setLvdsMode(mode);
    }
}

void RegistersHardwareTab::handleTmdsModeChanged(int index)
{
    const uint8_t mode = static_cast<uint8_t>(cmbTmdsMode->itemData(index).toUInt());
    if (m_device) {
        m_device->setDigitalOutputMode(mode);
    }
}

void RegistersHardwareTab::handleReadRegisterClicked()
{
    const uint8_t reg = static_cast<uint8_t>(spinRegAddress->value());
    lblRegResult->setText(tr("Querying register 0x%1...").arg(reg, 2, 16, QLatin1Char('0')));
    if (m_device) {
        m_device->readRegister(reg);
    }
}

void RegistersHardwareTab::handleWriteRegisterClicked()
{
    const uint8_t reg = static_cast<uint8_t>(spinRegAddress->value());
    const uint8_t val = static_cast<uint8_t>(spinRegValue->value());
    lblRegResult->setText(
        tr("Writing 0x%1 to register 0x%2...").arg(val, 2, 16, QLatin1Char('0')).arg(reg, 2, 16, QLatin1Char('0')));
    if (m_device) {
        m_device->writeRegister(reg, val);
    }
}

void RegistersHardwareTab::updateTelemetry(const Visca::Sony::SonyFCBStatus& /*status*/)
{
    // Specific register states (0x57, 0x47) are updated via direct inquiry or UI toggles
}

} // namespace ViscaApp
