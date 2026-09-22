/// @file LensOpticsTab.cpp
/// @brief Implementation of Lens & Optics control tab for Sony FCB cameras.

#include "LensOpticsTab.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ViscaApp {

LensOpticsTab::LensOpticsTab(QViscaSonyDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device(device)
{
    // Default capabilities (FCB-EV9520L)
    m_capabilities = Visca::Sony::SonyCameraModel::getCapabilities(Visca::Sony::SonyCameraModelType::FCB_EV9520L);

    setupUi();

    if (m_device) {
        connect(m_device, &QViscaSonyDevice::statusUpdated, this, &LensOpticsTab::updateTelemetry);
        connect(m_device, &QViscaSonyDevice::modelDiscovered, this, &LensOpticsTab::updateModelCapabilities);
    }
}

void LensOpticsTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // ==========================================
    // Left Box: Zoom Controls & Focal Length
    // ==========================================
    auto* grpZoom = new QGroupBox(tr("Optical & Digital Zoom"), this);
    auto* zoomLayout = new QVBoxLayout(grpZoom);
    zoomLayout->setSpacing(12);

    // Direct Zoom Slider Row
    auto* rowDirect = new QHBoxLayout();
    sliderZoom = new QSlider(Qt::Horizontal, grpZoom);
    sliderZoom->setRange(0x0000, 0x4000);
    sliderZoom->setValue(0x0000);

    spinZoomHex = new QSpinBox(grpZoom);
    spinZoomHex->setRange(0x0000, 0x4000);
    spinZoomHex->setDisplayIntegerBase(16);
    spinZoomHex->setPrefix("0x");

    rowDirect->addWidget(new QLabel(tr("Position:"), grpZoom));
    rowDirect->addWidget(sliderZoom);
    rowDirect->addWidget(spinZoomHex);
    zoomLayout->addLayout(rowDirect);

    // Focal length badge readout
    lblFocalLength = new QLabel(grpZoom);
    lblFocalLength->setStyleSheet("background-color: #1f242c; color: #58a6ff; font-size: 14px; font-weight: bold; "
                                  "padding: 8px; border-radius: 4px; border: 1px solid #30363d;");
    lblFocalLength->setAlignment(Qt::AlignCenter);
    zoomLayout->addWidget(lblFocalLength);

    // Continuous Tele / Wide buttons
    auto* rowButtons = new QHBoxLayout();
    btnZoomWide = new QPushButton(tr("Wide [ ◀ W ]"), grpZoom);
    btnZoomTele = new QPushButton(tr("Tele [ T ▶ ]"), grpZoom);
    btnZoomWide->setMinimumHeight(40);
    btnZoomTele->setMinimumHeight(40);

    rowButtons->addWidget(btnZoomWide);
    rowButtons->addWidget(btnZoomTele);
    zoomLayout->addLayout(rowButtons);

    // Zoom Speed
    auto* rowSpeed = new QHBoxLayout();
    sliderZoomSpeed = new QSlider(Qt::Horizontal, grpZoom);
    sliderZoomSpeed->setRange(0, 7);
    sliderZoomSpeed->setValue(4);
    sliderZoomSpeed->setTickPosition(QSlider::TicksBelow);
    sliderZoomSpeed->setTickInterval(1);

    auto* lblSpeedVal = new QLabel("4", grpZoom);
    connect(sliderZoomSpeed, &QSlider::valueChanged, this,
        [lblSpeedVal](int v) { lblSpeedVal->setText(QString::number(v)); });

    rowSpeed->addWidget(new QLabel(tr("Continuous Speed:"), grpZoom));
    rowSpeed->addWidget(sliderZoomSpeed);
    rowSpeed->addWidget(lblSpeedVal);
    zoomLayout->addLayout(rowSpeed);

    // Digital Zoom Mode
    auto* rowDig = new QHBoxLayout();
    cmbDigitalZoom = new QComboBox(grpZoom);
    cmbDigitalZoom->addItem(tr("Digital Zoom: OFF"), 0);
    cmbDigitalZoom->addItem(tr("Digital Zoom: Combine Mode"), 1);
    cmbDigitalZoom->addItem(tr("Digital Zoom: Separate Mode"), 2);
    rowDig->addWidget(new QLabel(tr("Digital Mode:"), grpZoom));
    rowDig->addWidget(cmbDigitalZoom);
    zoomLayout->addLayout(rowDig);

    zoomLayout->addStretch();
    mainLayout->addWidget(grpZoom);

    // ==========================================
    // Right Box: Auto / Manual Focus & Near Limit
    // ==========================================
    auto* grpFocus = new QGroupBox(tr("Focus System"), this);
    auto* focusLayout = new QVBoxLayout(grpFocus);
    focusLayout->setSpacing(12);

    // Auto Focus Toggle & One Push
    auto* rowAf = new QHBoxLayout();
    chkAutoFocus = new QCheckBox(tr("Auto Focus (AF)"), grpFocus);
    chkAutoFocus->setChecked(true);

    btnOnePushAf = new QPushButton(tr("One-Push AF Trigger"), grpFocus);
    btnOnePushAf->setEnabled(false);

    rowAf->addWidget(chkAutoFocus);
    rowAf->addWidget(btnOnePushAf);
    focusLayout->addLayout(rowAf);

    // Direct Focus Position
    auto* rowFocusDirect = new QHBoxLayout();
    sliderFocus = new QSlider(Qt::Horizontal, grpFocus);
    sliderFocus->setRange(0x1000, 0xF000);
    sliderFocus->setValue(0x8000);

    spinFocusHex = new QSpinBox(grpFocus);
    spinFocusHex->setRange(0x1000, 0xF000);
    spinFocusHex->setDisplayIntegerBase(16);
    spinFocusHex->setPrefix("0x");

    rowFocusDirect->addWidget(new QLabel(tr("Manual Pos:"), grpFocus));
    rowFocusDirect->addWidget(sliderFocus);
    rowFocusDirect->addWidget(spinFocusHex);
    focusLayout->addLayout(rowFocusDirect);

    // Manual Near / Far Buttons
    auto* rowFocusBtns = new QHBoxLayout();
    btnFocusNear = new QPushButton(tr("Near [ N ◀ ]"), grpFocus);
    btnFocusFar = new QPushButton(tr("Far [ ▶ F ]"), grpFocus);
    btnFocusNear->setMinimumHeight(40);
    btnFocusFar->setMinimumHeight(40);

    rowFocusBtns->addWidget(btnFocusNear);
    rowFocusBtns->addWidget(btnFocusFar);
    focusLayout->addLayout(rowFocusBtns);

    // Focus Near Limit Selector
    auto* rowLimit = new QHBoxLayout();
    cmbNearLimit = new QComboBox(grpFocus);
    cmbNearLimit->addItem(tr("Over Infinity (0x0000)"), 0x0000);
    cmbNearLimit->addItem(tr("10 mm / Wide Macro (0x1000)"), 0x1000);
    cmbNearLimit->addItem(tr("80 mm (0x2000)"), 0x2000);
    cmbNearLimit->addItem(tr("300 mm (0x4000)"), 0x4000);
    cmbNearLimit->addItem(tr("1.0 m (0x8000)"), 0x8000);
    cmbNearLimit->addItem(tr("1.2 m (0xB000)"), 0xB000);
    cmbNearLimit->addItem(tr("2.0 m (0xD000)"), 0xD000);

    rowLimit->addWidget(new QLabel(tr("Near Limit:"), grpFocus));
    rowLimit->addWidget(cmbNearLimit);
    focusLayout->addLayout(rowLimit);

    focusLayout->addStretch();
    mainLayout->addWidget(grpFocus);

    // Connect slider/spinbox synchronization
    connect(sliderZoom, &QSlider::valueChanged, spinZoomHex, &QSpinBox::setValue);
    connect(spinZoomHex, QOverload<int>::of(&QSpinBox::valueChanged), sliderZoom, &QSlider::setValue);
    connect(sliderZoom, &QSlider::valueChanged, this, &LensOpticsTab::handleDirectZoomChanged);

    connect(sliderFocus, &QSlider::valueChanged, spinFocusHex, &QSpinBox::setValue);
    connect(spinFocusHex, QOverload<int>::of(&QSpinBox::valueChanged), sliderFocus, &QSlider::setValue);
    connect(sliderFocus, &QSlider::valueChanged, this, &LensOpticsTab::handleDirectFocusChanged);

    // Buttons
    connect(btnZoomTele, &QPushButton::pressed, this, &LensOpticsTab::handleZoomTelePressed);
    connect(btnZoomTele, &QPushButton::released, this, &LensOpticsTab::handleZoomReleased);
    connect(btnZoomWide, &QPushButton::pressed, this, &LensOpticsTab::handleZoomWidePressed);
    connect(btnZoomWide, &QPushButton::released, this, &LensOpticsTab::handleZoomReleased);

    connect(chkAutoFocus, &QCheckBox::toggled, this, &LensOpticsTab::handleFocusAutoToggled);
    connect(btnOnePushAf, &QPushButton::clicked, this, &LensOpticsTab::handleOnePushAf);
    connect(cmbNearLimit, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &LensOpticsTab::handleFocusNearLimitChanged);

    updateFocalLengthDisplay(0x0000);
}

void LensOpticsTab::updateModelCapabilities(
    Visca::Sony::SonyCameraModelType /*modelType*/, const Visca::Sony::CameraCapabilities& caps)
{
    m_capabilities = caps;
    updateFocalLengthDisplay(static_cast<uint16_t>(sliderZoom->value()));
}

void LensOpticsTab::updateFocalLengthDisplay(uint16_t zoomPos)
{
    const double focalMm = m_capabilities.calculateFocalLength(zoomPos);
    const double zoomRatio = 1.0 + (static_cast<double>(zoomPos) / 0x4000) * (m_capabilities.maxOpticalZoomRatio - 1.0);

    lblFocalLength->setText(tr("Focal Length: %1 mm  |  Optical Zoom: %2x\n(%3)")
                                .arg(focalMm, 0, 'f', 1)
                                .arg(zoomRatio, 0, 'f', 1)
                                .arg(QString::fromStdString(m_capabilities.modelName)));
}

void LensOpticsTab::handleDirectZoomChanged(int value)
{
    updateFocalLengthDisplay(static_cast<uint16_t>(value));
    if (!m_updatingFromTelemetry && m_device) {
        m_device->setZoomDirect(static_cast<uint16_t>(value));
    }
}

void LensOpticsTab::handleZoomTelePressed()
{
    if (m_device) {
        m_device->zoomTele(static_cast<uint8_t>(sliderZoomSpeed->value()));
    }
}

void LensOpticsTab::handleZoomWidePressed()
{
    if (m_device) {
        m_device->zoomWide(static_cast<uint8_t>(sliderZoomSpeed->value()));
    }
}

void LensOpticsTab::handleZoomReleased()
{
    if (m_device) {
        m_device->zoomStop();
    }
}

void LensOpticsTab::handleFocusAutoToggled(bool checked)
{
    btnOnePushAf->setEnabled(!checked);
    sliderFocus->setEnabled(!checked);
    spinFocusHex->setEnabled(!checked);
    btnFocusNear->setEnabled(!checked);
    btnFocusFar->setEnabled(!checked);

    if (m_device) {
        m_device->setFocusAuto(checked);
    }
}

void LensOpticsTab::handleDirectFocusChanged(int value)
{
    if (!m_updatingFromTelemetry && m_device && !chkAutoFocus->isChecked()) {
        m_device->setFocusDirect(static_cast<uint16_t>(value));
    }
}

void LensOpticsTab::handleFocusNearLimitChanged(int index)
{
    const uint16_t limit = static_cast<uint16_t>(cmbNearLimit->itemData(index).toUInt());
    if (m_device) {
        m_device->setFocusNearLimit(limit);
    }
}

void LensOpticsTab::handleOnePushAf()
{
    if (m_device) {
        m_device->focusOnePush();
    }
}

void LensOpticsTab::updateTelemetry(const Visca::Sony::SonyFCBStatus& status)
{
    m_updatingFromTelemetry = true;

    // Zoom
    if (sliderZoom->value() != status.zoomPosition) {
        sliderZoom->setValue(status.zoomPosition);
    }

    // Focus
    if (chkAutoFocus->isChecked() != status.focusAuto) {
        chkAutoFocus->setChecked(status.focusAuto);
    }
    if (sliderFocus->value() != status.focusPosition) {
        sliderFocus->setValue(status.focusPosition);
    }

    m_updatingFromTelemetry = false;
}

} // namespace ViscaApp
