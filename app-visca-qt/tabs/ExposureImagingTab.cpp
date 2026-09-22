/// @file ExposureImagingTab.cpp
/// @brief Implementation of Exposure, White Balance, and Image Enhancement tab.

#include "ExposureImagingTab.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ViscaApp {

ExposureImagingTab::ExposureImagingTab(QViscaSonyDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device(device)
{
    setupUi();

    if (m_device) {
        connect(m_device, &QViscaSonyDevice::statusUpdated, this, &ExposureImagingTab::updateTelemetry);
    }
}

void ExposureImagingTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // ==========================================
    // Left Box: Exposure System
    // ==========================================
    auto* grpExp = new QGroupBox(tr("Exposure & Sensor Controls"), this);
    auto* expLayout = new QVBoxLayout(grpExp);
    expLayout->setSpacing(10);

    // Mode
    auto* rowMode = new QHBoxLayout();
    cmbExpMode = new QComboBox(grpExp);
    cmbExpMode->addItem(tr("Full Auto"), static_cast<int>(Visca::Sony::SonyExposureMode::FullAuto));
    cmbExpMode->addItem(tr("Manual"), static_cast<int>(Visca::Sony::SonyExposureMode::Manual));
    cmbExpMode->addItem(tr("Shutter Priority"), static_cast<int>(Visca::Sony::SonyExposureMode::ShutterPriority));
    cmbExpMode->addItem(tr("Iris Priority"), static_cast<int>(Visca::Sony::SonyExposureMode::IrisPriority));
    cmbExpMode->addItem(tr("Bright Mode"), static_cast<int>(Visca::Sony::SonyExposureMode::Bright));
    rowMode->addWidget(new QLabel(tr("AE Mode:"), grpExp));
    rowMode->addWidget(cmbExpMode);
    expLayout->addLayout(rowMode);

    // Shutter Speed
    auto* rowShutter = new QHBoxLayout();
    cmbShutter = new QComboBox(grpExp);
    const struct {
        const char* name;
        uint8_t code;
    } shutters[] = {
        { "1/1s", 0x01 },
        { "1/2s", 0x02 },
        { "1/4s", 0x04 },
        { "1/8s", 0x07 },
        { "1/15s", 0x0A },
        { "1/30s", 0x0D },
        { "1/60s", 0x10 },
        { "1/100s", 0x12 },
        { "1/125s", 0x13 },
        { "1/250s", 0x15 },
        { "1/500s", 0x17 },
        { "1/1000s", 0x19 },
        { "1/2000s", 0x1B },
        { "1/4000s", 0x1D },
        { "1/10000s", 0x20 },
    };
    for (const auto& s : shutters) {
        cmbShutter->addItem(s.name, s.code);
    }
    cmbShutter->setCurrentIndex(6); // 1/60s
    rowShutter->addWidget(new QLabel(tr("Shutter Speed:"), grpExp));
    rowShutter->addWidget(cmbShutter);
    expLayout->addLayout(rowShutter);

    // Iris
    auto* rowIris = new QHBoxLayout();
    cmbIris = new QComboBox(grpExp);
    const struct {
        const char* name;
        uint8_t code;
    } irises[] = {
        { "CLOSE", 0x00 },
        { "F28", 0x01 },
        { "F22", 0x03 },
        { "F16", 0x05 },
        { "F11", 0x07 },
        { "F8.0", 0x09 },
        { "F5.6", 0x0B },
        { "F4.0", 0x0D },
        { "F2.8", 0x0F },
        { "F2.0", 0x11 },
        { "F1.6", 0x13 },
    };
    for (const auto& i : irises) {
        cmbIris->addItem(i.name, i.code);
    }
    cmbIris->setCurrentIndex(10); // F1.6
    rowIris->addWidget(new QLabel(tr("Iris Aperture:"), grpExp));
    rowIris->addWidget(cmbIris);
    expLayout->addLayout(rowIris);

    // Gain Slider
    auto* rowGain = new QHBoxLayout();
    sliderGain = new QSlider(Qt::Horizontal, grpExp);
    sliderGain->setRange(0, 14); // 0 to +42 dB steps
    sliderGain->setValue(0);
    lblGainVal = new QLabel("0 dB", grpExp);
    lblGainVal->setFixedWidth(50);
    rowGain->addWidget(new QLabel(tr("Gain:"), grpExp));
    rowGain->addWidget(sliderGain);
    rowGain->addWidget(lblGainVal);
    expLayout->addLayout(rowGain);

    // Exposure Compensation
    auto* rowExpComp = new QHBoxLayout();
    chkExpComp = new QCheckBox(tr("Exposure Comp"), grpExp);
    sliderExpComp = new QSlider(Qt::Horizontal, grpExp);
    sliderExpComp->setRange(0, 14);
    sliderExpComp->setValue(7);
    sliderExpComp->setEnabled(false);
    rowExpComp->addWidget(chkExpComp);
    rowExpComp->addWidget(sliderExpComp);
    expLayout->addLayout(rowExpComp);

    expLayout->addStretch();
    mainLayout->addWidget(grpExp);

    // ==========================================
    // Middle Box: White Balance
    // ==========================================
    auto* grpWb = new QGroupBox(tr("White Balance (WB)"), this);
    auto* wbLayout = new QVBoxLayout(grpWb);
    wbLayout->setSpacing(10);

    // Mode
    auto* rowWbMode = new QHBoxLayout();
    cmbWbMode = new QComboBox(grpWb);
    cmbWbMode->addItem(tr("Auto (ATW)"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::Auto));
    cmbWbMode->addItem(tr("Indoor (3200K)"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::Indoor));
    cmbWbMode->addItem(tr("Outdoor (5800K)"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::Outdoor));
    cmbWbMode->addItem(tr("One-Push WB"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::OnePush));
    cmbWbMode->addItem(tr("Sodium Lamp"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::SodiumAuto));
    cmbWbMode->addItem(tr("Manual"), static_cast<int>(Visca::Sony::SonyWhiteBalanceMode::Manual));
    rowWbMode->addWidget(new QLabel(tr("WB Mode:"), grpWb));
    rowWbMode->addWidget(cmbWbMode);
    wbLayout->addLayout(rowWbMode);

    btnOnePushWb = new QPushButton(tr("One-Push Trigger"), grpWb);
    btnOnePushWb->setEnabled(false);
    wbLayout->addWidget(btnOnePushWb);

    // R-Gain
    auto* rowRgain = new QHBoxLayout();
    sliderRgain = new QSlider(Qt::Horizontal, grpWb);
    sliderRgain->setRange(0, 255);
    sliderRgain->setValue(128);
    lblRgainVal = new QLabel("128", grpWb);
    lblRgainVal->setFixedWidth(35);
    rowRgain->addWidget(new QLabel(tr("R-Gain:"), grpWb));
    rowRgain->addWidget(sliderRgain);
    rowRgain->addWidget(lblRgainVal);
    wbLayout->addLayout(rowRgain);

    // B-Gain
    auto* rowBgain = new QHBoxLayout();
    sliderBgain = new QSlider(Qt::Horizontal, grpWb);
    sliderBgain->setRange(0, 255);
    sliderBgain->setValue(128);
    lblBgainVal = new QLabel("128", grpWb);
    lblBgainVal->setFixedWidth(35);
    rowBgain->addWidget(new QLabel(tr("B-Gain:"), grpWb));
    rowBgain->addWidget(sliderBgain);
    rowBgain->addWidget(lblBgainVal);
    wbLayout->addLayout(rowBgain);

    wbLayout->addStretch();
    mainLayout->addWidget(grpWb);

    // ==========================================
    // Right Box: Image Enhancements & Defog
    // ==========================================
    auto* grpEnhance = new QGroupBox(tr("Enhancements & Stabilizer"), this);
    auto* enhLayout = new QVBoxLayout(grpEnhance);
    enhLayout->setSpacing(10);

    // Image Stabilizer
    auto* rowStab = new QHBoxLayout();
    cmbStabilizer = new QComboBox(grpEnhance);
    cmbStabilizer->addItem(tr("Stabilizer: OFF"), static_cast<int>(Visca::Sony::SonyStabilizerMode::Off));
    cmbStabilizer->addItem(tr("Normal EIS"), static_cast<int>(Visca::Sony::SonyStabilizerMode::Normal));
    cmbStabilizer->addItem(tr("Super Mode"), static_cast<int>(Visca::Sony::SonyStabilizerMode::Super));
    cmbStabilizer->addItem(tr("Super+ Mode (High Wind)"), static_cast<int>(Visca::Sony::SonyStabilizerMode::SuperPlus));
    rowStab->addWidget(new QLabel(tr("Stabilizer:"), grpEnhance));
    rowStab->addWidget(cmbStabilizer);
    enhLayout->addLayout(rowStab);

    // Defog
    auto* rowDefog = new QHBoxLayout();
    cmbDefog = new QComboBox(grpEnhance);
    cmbDefog->addItem(tr("Defog: OFF"), static_cast<int>(Visca::Sony::SonyDefogMode::Off));
    cmbDefog->addItem(tr("Defog: Low"), static_cast<int>(Visca::Sony::SonyDefogMode::Low));
    cmbDefog->addItem(tr("Defog: Mid"), static_cast<int>(Visca::Sony::SonyDefogMode::Mid));
    cmbDefog->addItem(tr("Defog: High"), static_cast<int>(Visca::Sony::SonyDefogMode::High));
    rowDefog->addWidget(new QLabel(tr("Defog:"), grpEnhance));
    rowDefog->addWidget(cmbDefog);
    enhLayout->addLayout(rowDefog);

    // ICR / Night Filter
    chkIcr = new QCheckBox(tr("IR Cut Filter (Night / B&W Mode)"), grpEnhance);
    chkAutoIcr = new QCheckBox(tr("Auto ICR Day/Night Switching"), grpEnhance);
    enhLayout->addWidget(chkIcr);
    enhLayout->addWidget(chkAutoIcr);

    enhLayout->addStretch();
    mainLayout->addWidget(grpEnhance);

    // Wire events
    connect(cmbExpMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ExposureImagingTab::handleExposureModeChanged);
    connect(cmbShutter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ExposureImagingTab::handleShutterChanged);
    connect(cmbIris, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureImagingTab::handleIrisChanged);
    connect(sliderGain, &QSlider::valueChanged, this, &ExposureImagingTab::handleGainChanged);
    connect(chkExpComp, &QCheckBox::toggled, this, &ExposureImagingTab::handleExpCompToggled);

    connect(
        cmbWbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureImagingTab::handleWbModeChanged);
    connect(btnOnePushWb, &QPushButton::clicked, this, &ExposureImagingTab::handleOnePushWb);
    connect(sliderRgain, &QSlider::valueChanged, this, &ExposureImagingTab::handleRgainChanged);
    connect(sliderBgain, &QSlider::valueChanged, this, &ExposureImagingTab::handleBgainChanged);

    connect(cmbStabilizer, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ExposureImagingTab::handleStabilizerChanged);
    connect(
        cmbDefog, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureImagingTab::handleDefogChanged);
    connect(chkIcr, &QCheckBox::toggled, this, &ExposureImagingTab::handleIcrToggled);
}

void ExposureImagingTab::handleExposureModeChanged(int index)
{
    const auto mode = static_cast<Visca::Sony::SonyExposureMode>(cmbExpMode->itemData(index).toInt());
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setExposureMode(mode);
    }
}

void ExposureImagingTab::handleShutterChanged(int index)
{
    const uint8_t code = static_cast<uint8_t>(cmbShutter->itemData(index).toUInt());
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setShutter(code);
    }
}

void ExposureImagingTab::handleIrisChanged(int index)
{
    const uint8_t code = static_cast<uint8_t>(cmbIris->itemData(index).toUInt());
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setIris(code);
    }
}

void ExposureImagingTab::handleGainChanged(int value)
{
    lblGainVal->setText(tr("+%1 dB").arg(value * 3));
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setGain(static_cast<uint8_t>(value));
    }
}

void ExposureImagingTab::handleExpCompToggled(bool checked)
{
    sliderExpComp->setEnabled(checked);
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setExposureCompensation(checked, static_cast<uint8_t>(sliderExpComp->value()));
    }
}

void ExposureImagingTab::handleWbModeChanged(int index)
{
    const auto mode = static_cast<Visca::Sony::SonyWhiteBalanceMode>(cmbWbMode->itemData(index).toInt());
    btnOnePushWb->setEnabled(mode == Visca::Sony::SonyWhiteBalanceMode::OnePush);
    sliderRgain->setEnabled(mode == Visca::Sony::SonyWhiteBalanceMode::Manual);
    sliderBgain->setEnabled(mode == Visca::Sony::SonyWhiteBalanceMode::Manual);

    if (!m_updatingFromTelemetry && m_device) {
        m_device->setWhiteBalance(mode);
    }
}

void ExposureImagingTab::handleOnePushWb()
{
    if (m_device) {
        m_device->triggerOnePushWb();
    }
}

void ExposureImagingTab::handleRgainChanged(int value)
{
    lblRgainVal->setText(QString::number(value));
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setRgainDirect(static_cast<uint8_t>(value));
    }
}

void ExposureImagingTab::handleBgainChanged(int value)
{
    lblBgainVal->setText(QString::number(value));
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setBgainDirect(static_cast<uint8_t>(value));
    }
}

void ExposureImagingTab::handleStabilizerChanged(int index)
{
    const auto mode = static_cast<Visca::Sony::SonyStabilizerMode>(cmbStabilizer->itemData(index).toInt());
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setStabilizer(mode);
    }
}

void ExposureImagingTab::handleDefogChanged(int index)
{
    const auto mode = static_cast<Visca::Sony::SonyDefogMode>(cmbDefog->itemData(index).toInt());
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setDefog(mode);
    }
}

void ExposureImagingTab::handleIcrToggled(bool checked)
{
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setIcr(checked);
    }
}

void ExposureImagingTab::updateTelemetry(const Visca::Sony::SonyFCBStatus& status)
{
    m_updatingFromTelemetry = true;

    // Exposure mode
    for (int i = 0; i < cmbExpMode->count(); ++i) {
        if (cmbExpMode->itemData(i).toInt() == static_cast<int>(status.exposureMode)) {
            cmbExpMode->setCurrentIndex(i);
            break;
        }
    }

    // Shutter
    for (int i = 0; i < cmbShutter->count(); ++i) {
        if (cmbShutter->itemData(i).toUInt() == status.shutterPosition) {
            cmbShutter->setCurrentIndex(i);
            break;
        }
    }

    // Iris
    for (int i = 0; i < cmbIris->count(); ++i) {
        if (cmbIris->itemData(i).toUInt() == status.irisPosition) {
            cmbIris->setCurrentIndex(i);
            break;
        }
    }

    // Gain
    if (sliderGain->value() != status.gainPosition) {
        sliderGain->setValue(status.gainPosition);
    }

    // Stabilizer
    for (int i = 0; i < cmbStabilizer->count(); ++i) {
        if (cmbStabilizer->itemData(i).toInt() == static_cast<int>(status.stabilizerLevel)) {
            cmbStabilizer->setCurrentIndex(i);
            break;
        }
    }

    // Defog
    for (int i = 0; i < cmbDefog->count(); ++i) {
        if (cmbDefog->itemData(i).toInt() == static_cast<int>(status.defogLevel)) {
            cmbDefog->setCurrentIndex(i);
            break;
        }
    }

    // ICR
    if (chkIcr->isChecked() != status.icrOn) {
        chkIcr->setChecked(status.icrOn);
    }

    m_updatingFromTelemetry = false;
}

} // namespace ViscaApp
