/// @file ExposureTab.cpp
/// @brief Implementation of ExposureTab for Fujinon camera exposure controls.

#include "ExposureTab.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

namespace FujinonApp {

ExposureTab::ExposureTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* ExposureTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void ExposureTab::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* mainLayout = new QHBoxLayout(container);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(14);

    // =========================================================================
    // Column 1: Iris & Shutter Controls
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    // Group 1: Aperture / Iris
    auto* grpIris = new QGroupBox(tr("Aperture & Iris Control"), container);
    auto* gridIris = new QGridLayout(grpIris);

    gridIris->addWidget(new QLabel(tr("Auto Iris Mode:")), 0, 0);
    cmbAutoIris = new QComboBox(grpIris);
    cmbAutoIris->addItem(tr("Manual Iris"), static_cast<int>(PelcoD::FujinonAutoIrisMode::Manual));
    cmbAutoIris->addItem(tr("Auto Iris"), static_cast<int>(PelcoD::FujinonAutoIrisMode::Auto));
    gridIris->addWidget(cmbAutoIris, 0, 1, 1, 3);

    gridIris->addWidget(new QLabel(tr("Manual Aperture (0..255):")), 1, 0);
    spinManualIris = new QSpinBox(grpIris);
    spinManualIris->setRange(0, 255);
    sliderManualIris = new QSlider(Qt::Horizontal, grpIris);
    sliderManualIris->setRange(0, 255);
    btnSetManualIris = new QPushButton(tr("Set Iris"), grpIris);
    btnQueryIris = new QPushButton(tr("Query Iris"), grpIris);

    gridIris->addWidget(spinManualIris, 1, 1);
    gridIris->addWidget(sliderManualIris, 1, 2);
    gridIris->addWidget(btnSetManualIris, 1, 3);
    gridIris->addWidget(btnQueryIris, 1, 4);
    col1->addWidget(grpIris);

    // Group 2: Electronic Shutter
    auto* grpShutter = new QGroupBox(tr("Electronic Shutter Speed"), container);
    auto* gridShutter = new QGridLayout(grpShutter);

    gridShutter->addWidget(new QLabel(tr("Auto Shutter Limit:")), 0, 0);
    cmbShutterLimit = new QComboBox(grpShutter);
    cmbShutterLimit->addItem(tr("Manual Limit"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::Manual));
    cmbShutterLimit->addItem(tr("1/8 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_8));
    cmbShutterLimit->addItem(tr("1/15 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_15));
    cmbShutterLimit->addItem(tr("1/30 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_30));
    cmbShutterLimit->addItem(tr("1/60 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_60));
    cmbShutterLimit->addItem(tr("1/125 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_125));
    cmbShutterLimit->addItem(tr("1/250 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_250));
    cmbShutterLimit->addItem(tr("1/500 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_500));
    cmbShutterLimit->addItem(tr("1/1000 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_1000));
    cmbShutterLimit->addItem(tr("1/2000 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_2000));
    cmbShutterLimit->addItem(tr("1/4000 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_4000));
    cmbShutterLimit->addItem(tr("1/10000 sec"), static_cast<int>(PelcoD::FujinonShutterLimitAuto::S1_10000));
    gridShutter->addWidget(cmbShutterLimit, 0, 1, 1, 3);

    gridShutter->addWidget(new QLabel(tr("Manual Shutter Index (1..255):")), 1, 0);
    spinManualShutter = new QSpinBox(grpShutter);
    spinManualShutter->setRange(1, 255);
    sliderManualShutter = new QSlider(Qt::Horizontal, grpShutter);
    sliderManualShutter->setRange(1, 255);
    btnSetManualShutter = new QPushButton(tr("Set Shutter"), grpShutter);
    btnQueryShutter = new QPushButton(tr("Query Shutter"), grpShutter);

    gridShutter->addWidget(spinManualShutter, 1, 1);
    gridShutter->addWidget(sliderManualShutter, 1, 2);
    gridShutter->addWidget(btnSetManualShutter, 1, 3);
    gridShutter->addWidget(btnQueryShutter, 1, 4);
    col1->addWidget(grpShutter);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: Sensor Gain (ISO), AGC, BLC & Photo Preset
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    // Group 3: Sensor Gain / ISO
    auto* grpIso = new QGroupBox(tr("Sensor Gain & ISO Sensitivity"), container);
    auto* gridIso = new QGridLayout(grpIso);

    gridIso->addWidget(new QLabel(tr("Manual Gain / ISO Index (1..255):")), 0, 0);
    spinManualIso = new QSpinBox(grpIso);
    spinManualIso->setRange(1, 255);
    sliderManualIso = new QSlider(Qt::Horizontal, grpIso);
    sliderManualIso->setRange(1, 255);
    btnSetManualIso = new QPushButton(tr("Set ISO"), grpIso);
    btnQueryIso = new QPushButton(tr("Query ISO"), grpIso);

    gridIso->addWidget(spinManualIso, 0, 1);
    gridIso->addWidget(sliderManualIso, 0, 2);
    gridIso->addWidget(btnSetManualIso, 0, 3);
    gridIso->addWidget(btnQueryIso, 0, 4);
    col2->addWidget(grpIso);

    // Group 4: Exposure Modes & Presets
    auto* grpModes = new QGroupBox(tr("Exposure Compensation & Scene Presets"), container);
    auto* gridModes = new QGridLayout(grpModes);

    gridModes->addWidget(new QLabel(tr("Automatic Gain Control (AGC):")), 0, 0);
    cmbAgc = new QComboBox(grpModes);
    cmbAgc->addItem(tr("Standard AGC"), static_cast<int>(PelcoD::FujinonAGCMode::Standard));
    cmbAgc->addItem(tr("AGC Off"), static_cast<int>(PelcoD::FujinonAGCMode::Off));
    cmbAgc->addItem(tr("Hyper AGC"), static_cast<int>(PelcoD::FujinonAGCMode::Hyper));
    gridModes->addWidget(cmbAgc, 0, 1);

    gridModes->addWidget(new QLabel(tr("Backlight Compensation (BLC):")), 1, 0);
    cmbBlc = new QComboBox(grpModes);
    cmbBlc->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonBLCMode::Off));
    cmbBlc->addItem(tr("Soft BLC"), static_cast<int>(PelcoD::FujinonBLCMode::Soft));
    cmbBlc->addItem(tr("Hard BLC"), static_cast<int>(PelcoD::FujinonBLCMode::Hard));
    gridModes->addWidget(cmbBlc, 1, 1);

    gridModes->addWidget(new QLabel(tr("Photo Scene Preset:")), 2, 0);
    cmbPhotoMode = new QComboBox(grpModes);
    cmbPhotoMode->addItem(tr("Surveillance Mode"), static_cast<int>(PelcoD::FujinonPhotoModePreset::Surveillance));
    cmbPhotoMode->addItem(tr("Movie Mode"), static_cast<int>(PelcoD::FujinonPhotoModePreset::Movie));
    gridModes->addWidget(cmbPhotoMode, 2, 1);
    col2->addWidget(grpModes);

    // Group 5: Telemetry & General Queries
    auto* grpTelem = new QGroupBox(tr("Exposure Telemetry Queries"), container);
    auto* gridTelem = new QGridLayout(grpTelem);

    btnQueryManual = new QPushButton(tr("Query All Manual Settings"), grpTelem);
    btnQueryPhoto = new QPushButton(tr("Query Photo / Scene Settings"), grpTelem);
    lblCurrentExposure = new QLabel(tr("Current: Auto Iris | Shutter: Auto | ISO: Auto"), grpTelem);

    gridTelem->addWidget(btnQueryManual, 0, 0);
    gridTelem->addWidget(btnQueryPhoto, 0, 1);
    gridTelem->addWidget(lblCurrentExposure, 1, 0, 1, 2);
    col2->addWidget(grpTelem);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void ExposureTab::setupConnections()
{
    // Sliders & Spinboxes sync
    connect(spinManualIris, QOverload<int>::of(&QSpinBox::valueChanged), sliderManualIris, &QSlider::setValue);
    connect(sliderManualIris, &QSlider::valueChanged, spinManualIris, &QSpinBox::setValue);

    connect(spinManualShutter, QOverload<int>::of(&QSpinBox::valueChanged), sliderManualShutter, &QSlider::setValue);
    connect(sliderManualShutter, &QSlider::valueChanged, spinManualShutter, &QSpinBox::setValue);

    connect(spinManualIso, QOverload<int>::of(&QSpinBox::valueChanged), sliderManualIso, &QSlider::setValue);
    connect(sliderManualIso, &QSlider::valueChanged, spinManualIso, &QSpinBox::setValue);

    // Iris
    connect(
        cmbAutoIris, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureTab::handleAutoIrisChanged);
    connect(btnSetManualIris, &QPushButton::clicked, this, &ExposureTab::handleSetManualIris);
    connect(btnQueryIris, &QPushButton::clicked, this, &ExposureTab::handleQueryIris);

    // Shutter
    connect(cmbShutterLimit, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ExposureTab::handleShutterLimitChanged);
    connect(btnSetManualShutter, &QPushButton::clicked, this, &ExposureTab::handleSetManualShutter);
    connect(btnQueryShutter, &QPushButton::clicked, this, &ExposureTab::handleQueryShutter);

    // ISO
    connect(btnSetManualIso, &QPushButton::clicked, this, &ExposureTab::handleSetManualIso);
    connect(btnQueryIso, &QPushButton::clicked, this, &ExposureTab::handleQueryIso);

    // AGC & BLC & Photo Mode
    connect(cmbAgc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureTab::handleAgcChanged);
    connect(cmbBlc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureTab::handleBlcChanged);
    connect(
        cmbPhotoMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExposureTab::handlePhotoModeChanged);

    // Queries
    connect(btnQueryManual, &QPushButton::clicked, this, &ExposureTab::handleQueryManualSettings);
    connect(btnQueryPhoto, &QPushButton::clicked, this, &ExposureTab::handleQueryPhotoSettings);
}

void ExposureTab::handleAutoIrisChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonAutoIrisMode>(cmbAutoIris->itemData(index).toInt());
        f->setAutoIris(mode);
    }
}

void ExposureTab::handleSetManualIris()
{
    if (auto* f = fujinon()) {
        f->setManualIris(spinManualIris->value());
    }
}

void ExposureTab::handleQueryIris()
{
    if (auto* f = fujinon()) {
        f->queryManualIris();
    }
}

void ExposureTab::handleShutterLimitChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto limit = static_cast<PelcoD::FujinonShutterLimitAuto>(cmbShutterLimit->itemData(index).toInt());
        f->setShutterLimitAuto(limit);
    }
}

void ExposureTab::handleSetManualShutter()
{
    if (auto* f = fujinon()) {
        f->setManualShutter(spinManualShutter->value());
    }
}

void ExposureTab::handleQueryShutter()
{
    if (auto* f = fujinon()) {
        f->queryManualShutter();
    }
}

void ExposureTab::handleSetManualIso()
{
    if (auto* f = fujinon()) {
        f->setManualISO(spinManualIso->value());
    }
}

void ExposureTab::handleQueryIso()
{
    if (auto* f = fujinon()) {
        f->queryManualISO();
    }
}

void ExposureTab::handleAgcChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonAGCMode>(cmbAgc->itemData(index).toInt());
        f->setAGC(mode);
    }
}

void ExposureTab::handleBlcChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonBLCMode>(cmbBlc->itemData(index).toInt());
        f->setBLC(mode);
    }
}

void ExposureTab::handlePhotoModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto preset = static_cast<PelcoD::FujinonPhotoModePreset>(cmbPhotoMode->itemData(index).toInt());
        f->setPhotoModePreset(preset);
    }
}

void ExposureTab::handleQueryManualSettings()
{
    if (auto* f = fujinon()) {
        f->queryManualSettings();
    }
}

void ExposureTab::handleQueryPhotoSettings()
{
    if (auto* f = fujinon()) {
        f->queryPhotoSettings();
    }
}

void ExposureTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    lblCurrentExposure->setText(
        tr("Iris: %1 | Shutter: %2 | ISO: %3").arg(status.manualIris).arg(status.manualShutter).arg(status.manualISO));
}

} // namespace FujinonApp
