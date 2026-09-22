/// @file FujinonSX800Tab.cpp
/// @brief Implementation of dedicated dashboard tab for Fujinon SX800 camera profile.

#include "FujinonSX800Tab.h"

#include <QMessageBox>
#include <QSignalBlocker>

namespace PelcoDApp {

FujinonSX800Tab::FujinonSX800Tab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* FujinonSX800Tab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void FujinonSX800Tab::setupUi()
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
    // Column 1: Optics & Stabilization & Telemetry Card
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    // Group 1: Optics & Stabilization
    auto* grpOptics = new QGroupBox(tr("Optics & Stabilization"), container);
    auto* gridOptics = new QGridLayout(grpOptics);

    gridOptics->addWidget(new QLabel(tr("Stabilization (OIS/EIS):")), 0, 0);
    cmbOis = new QComboBox();
    cmbOis->addItem(tr("Auto (Switching)"), static_cast<int>(PelcoD::FujinonOISMode::Auto));
    cmbOis->addItem(tr("OIS Only (Optical)"), static_cast<int>(PelcoD::FujinonOISMode::OisOn));
    cmbOis->addItem(tr("EIS Only (Electronic)"), static_cast<int>(PelcoD::FujinonOISMode::EisOn));
    cmbOis->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonOISMode::Off));
    gridOptics->addWidget(cmbOis, 0, 1);

    chkVlc = new QCheckBox(tr("Visible Light Cut (VLC) Filter"));
    gridOptics->addWidget(chkVlc, 1, 0, 1, 2);

    gridOptics->addWidget(new QLabel(tr("Defog Level:")), 2, 0);
    cmbDefog = new QComboBox();
    cmbDefog->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonDefogLevel::Off));
    cmbDefog->addItem(tr("Level 1 (Weak)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level1));
    cmbDefog->addItem(tr("Level 2 (Medium)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level2));
    cmbDefog->addItem(tr("Level 3 (Strong)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level3));
    gridOptics->addWidget(cmbDefog, 2, 1);

    gridOptics->addWidget(new QLabel(tr("Heat Haze Reduction:")), 3, 0);
    cmbHeatHaze = new QComboBox();
    cmbHeatHaze->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Off));
    cmbHeatHaze->addItem(tr("Level 1 (Weak)"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Level1));
    cmbHeatHaze->addItem(tr("Level 2 (Strong)"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Level2));
    gridOptics->addWidget(cmbHeatHaze, 3, 1);

    gridOptics->addWidget(new QLabel(tr("Wide Dynamic Range (WDR):")), 4, 0);
    cmbWdr = new QComboBox();
    cmbWdr->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonWDRLevel::Off));
    cmbWdr->addItem(tr("Level 1 (Low)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level1));
    cmbWdr->addItem(tr("Level 2 (Mid)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level2));
    cmbWdr->addItem(tr("Level 3 (High)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level3));
    gridOptics->addWidget(cmbWdr, 4, 1);

    chkAntialiasing = new QCheckBox(tr("Anti-Aliasing Filter"));
    gridOptics->addWidget(chkAntialiasing, 5, 0, 1, 2);

    col1->addWidget(grpOptics);

    // Group 4: Zoom, Focus & Telemetry Card
    auto* grpTelem = new QGroupBox(tr("Zoom, Focus & Telemetry"), container);
    auto* gridTelem = new QGridLayout(grpTelem);

    gridTelem->addWidget(new QLabel(tr("Focal Length:")), 0, 0);
    lblFocalLength = new QLabel(tr("20.0 mm"));
    lblFocalLength->setStyleSheet("font-size: 14pt; font-weight: bold; color: #58a6ff;");
    gridTelem->addWidget(lblFocalLength, 0, 1);

    barFocalLength = new QProgressBar();
    barFocalLength->setRange(20, 800);
    barFocalLength->setValue(20);
    barFocalLength->setFormat(tr("%v mm (40x Optical)"));
    barFocalLength->setTextVisible(true);
    gridTelem->addWidget(barFocalLength, 1, 0, 1, 2);

    gridTelem->addWidget(new QLabel(tr("Zoom Raw Counts:")), 2, 0);
    lblZoomCounts = new QLabel("--");
    lblZoomCounts->setObjectName("lblTelemetry");
    gridTelem->addWidget(lblZoomCounts, 2, 1);

    gridTelem->addWidget(new QLabel(tr("Focus Raw Counts:")), 3, 0);
    lblFocusCounts = new QLabel("--");
    lblFocusCounts->setObjectName("lblTelemetry");
    gridTelem->addWidget(lblFocusCounts, 3, 1);

    gridTelem->addWidget(new QLabel(tr("Zoom Speed (Extended):")), 4, 0);
    cmbZoomSpeedEx = new QComboBox();
    cmbZoomSpeedEx->addItem(tr("Step 1 (4s)"), 1);
    cmbZoomSpeedEx->addItem(tr("Step 2 (6s)"), 2);
    cmbZoomSpeedEx->addItem(tr("Step 3 (8s)"), 3);
    cmbZoomSpeedEx->addItem(tr("Step 4 (10s)"), 4);
    cmbZoomSpeedEx->addItem(tr("Step 5 (15s)"), 5);
    cmbZoomSpeedEx->addItem(tr("Step 6 (20s)"), 6);
    cmbZoomSpeedEx->addItem(tr("Step 7 (30s)"), 7);
    cmbZoomSpeedEx->addItem(tr("Step 8 (60s)"), 8);
    gridTelem->addWidget(cmbZoomSpeedEx, 4, 1);

    gridTelem->addWidget(new QLabel(tr("Focus Speed (Extended):")), 5, 0);
    cmbFocusSpeedEx = new QComboBox();
    cmbFocusSpeedEx->addItem(tr("Step 1 (Fastest)"), 1);
    cmbFocusSpeedEx->addItem(tr("Step 2"), 2);
    cmbFocusSpeedEx->addItem(tr("Step 3 (Default)"), 3);
    cmbFocusSpeedEx->addItem(tr("Step 4"), 4);
    cmbFocusSpeedEx->addItem(tr("Step 5 (Slowest)"), 5);
    cmbFocusSpeedEx->setCurrentIndex(2);
    gridTelem->addWidget(cmbFocusSpeedEx, 5, 1);

    gridTelem->addWidget(new QLabel(tr("Digital Zoom Mode:")), 6, 0);
    cmbDigitalZoomMode = new QComboBox();
    cmbDigitalZoomMode->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::Off));
    cmbDigitalZoomMode->addItem(tr("Digital Zoom"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::DigitalZoom));
    cmbDigitalZoomMode->addItem(tr("Crop Mode"), static_cast<int>(PelcoD::FujinonDigitalZoomMode::CropMode));
    gridTelem->addWidget(cmbDigitalZoomMode, 6, 1);

    gridTelem->addWidget(new QLabel(tr("Digital Zoom Mag:")), 7, 0);
    cmbDigitalZoomMag = new QComboBox();
    cmbDigitalZoomMag->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonDigitalZoom::Off));
    cmbDigitalZoomMag->addItem(tr("1.25x"), static_cast<int>(PelcoD::FujinonDigitalZoom::X1_25));
    cmbDigitalZoomMag->addItem(tr("1.50x"), static_cast<int>(PelcoD::FujinonDigitalZoom::X1_5));
    cmbDigitalZoomMag->addItem(tr("1.75x"), static_cast<int>(PelcoD::FujinonDigitalZoom::X1_75));
    cmbDigitalZoomMag->addItem(tr("2.00x"), static_cast<int>(PelcoD::FujinonDigitalZoom::X2));
    gridTelem->addWidget(cmbDigitalZoomMag, 7, 1);

    btnOnePushAf = new QPushButton(tr("One-Push Auto Focus"));
    btnOnePushAf->setObjectName("btnPrimary");
    gridTelem->addWidget(btnOnePushAf, 8, 0, 1, 2);

    btnQueryAll = new QPushButton(tr("Refresh SX800 Telemetry"));
    gridTelem->addWidget(btnQueryAll, 9, 0, 1, 2);

    col1->addWidget(grpTelem);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: Fine Image Adjustments (1..100) & Day/Night Ex
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    // Group 2: Fine Image Quality
    auto* grpFine = new QGroupBox(tr("Fine Image Quality (1..100)"), container);
    auto* gridFine = new QGridLayout(grpFine);

    auto makeSliderRow = [this, gridFine](int row, const QString& title, QSlider*& slider, QSpinBox*& spin) {
        gridFine->addWidget(new QLabel(title), row, 0);
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(1, 100);
        slider->setValue(50);
        gridFine->addWidget(slider, row, 1);
        spin = new QSpinBox();
        spin->setRange(1, 100);
        spin->setValue(50);
        spin->setFixedWidth(60);
        gridFine->addWidget(spin, row, 2);

        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
    };

    makeSliderRow(0, tr("Brightness:"), sliderBrightnessFine, spinBrightnessFine);
    makeSliderRow(1, tr("Contrast:"), sliderContrastFine, spinContrastFine);
    makeSliderRow(2, tr("Saturation:"), sliderSaturationFine, spinSaturationFine);
    makeSliderRow(3, tr("Sharpness:"), sliderSharpnessFine, spinSharpnessFine);
    makeSliderRow(4, tr("WB Shift Red:"), sliderWbShiftRedFine, spinWbShiftRedFine);
    makeSliderRow(5, tr("WB Shift Blue:"), sliderWbShiftBlueFine, spinWbShiftBlueFine);

    gridFine->addWidget(new QLabel(tr("White Balance:")), 6, 0);
    cmbWbMode = new QComboBox();
    cmbWbMode->addItem(tr("Auto"), static_cast<int>(PelcoD::FujinonWBMode::Auto));
    cmbWbMode->addItem(tr("Custom 1"), static_cast<int>(PelcoD::FujinonWBMode::Custom1));
    cmbWbMode->addItem(tr("Custom 2"), static_cast<int>(PelcoD::FujinonWBMode::Custom2));
    cmbWbMode->addItem(tr("Day"), static_cast<int>(PelcoD::FujinonWBMode::Day));
    cmbWbMode->addItem(tr("Cloud"), static_cast<int>(PelcoD::FujinonWBMode::Cloud));
    cmbWbMode->addItem(tr("Color Temp"), static_cast<int>(PelcoD::FujinonWBMode::ColorTemp));
    gridFine->addWidget(cmbWbMode, 6, 1, 1, 2);

    gridFine->addWidget(new QLabel(tr("Noise Reduction (1..5):")), 7, 0);
    spinNoiseReduction = new QSpinBox();
    spinNoiseReduction->setRange(1, 5);
    spinNoiseReduction->setValue(3);
    gridFine->addWidget(spinNoiseReduction, 7, 1, 1, 2);

    btnQueryFine = new QPushButton(tr("Query Fine Settings"));
    gridFine->addWidget(btnQueryFine, 8, 0, 1, 3);

    col2->addWidget(grpFine);

    // Group 3: Day / Night & Scheduling
    auto* grpDn = new QGroupBox(tr("Day / Night Mode & Scheduling"), container);
    auto* gridDn = new QGridLayout(grpDn);

    gridDn->addWidget(new QLabel(tr("Day/Night Mode Ex:")), 0, 0);
    cmbDayNightEx = new QComboBox();
    cmbDayNightEx->addItem(tr("Auto"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Auto));
    cmbDayNightEx->addItem(tr("Auto & Scheduled"), static_cast<int>(PelcoD::FujinonDayNightModeEx::AutoAndScheduled));
    cmbDayNightEx->addItem(tr("Scheduled"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Scheduled));
    cmbDayNightEx->addItem(tr("Day (Fixed)"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Day));
    cmbDayNightEx->addItem(tr("Night (Fixed)"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Night));
    gridDn->addWidget(cmbDayNightEx, 0, 1, 1, 2);

    gridDn->addWidget(new QLabel(tr("Day -> Night Threshold (0..255):")), 1, 0);
    spinDayToNight = new QSpinBox();
    spinDayToNight->setRange(0, 255);
    spinDayToNight->setValue(60);
    auto* btnSetD2N = new QPushButton(tr("Set"));
    gridDn->addWidget(spinDayToNight, 1, 1);
    gridDn->addWidget(btnSetD2N, 1, 2);

    gridDn->addWidget(new QLabel(tr("Night -> Day Threshold (0..255):")), 2, 0);
    spinNightToDay = new QSpinBox();
    spinNightToDay->setRange(0, 255);
    spinNightToDay->setValue(100);
    auto* btnSetN2D = new QPushButton(tr("Set"));
    gridDn->addWidget(spinNightToDay, 2, 1);
    gridDn->addWidget(btnSetN2D, 2, 2);

    gridDn->addWidget(new QLabel(tr("Auto Switch Delay (0..60s):")), 3, 0);
    spinAutoDelay = new QSpinBox();
    spinAutoDelay->setRange(0, 60);
    spinAutoDelay->setValue(5);
    auto* btnSetDelay = new QPushButton(tr("Set"));
    gridDn->addWidget(spinAutoDelay, 3, 1);
    gridDn->addWidget(btnSetDelay, 3, 2);

    gridDn->addWidget(new QLabel(tr("Day Start Time:")), 4, 0);
    timeDayStart = new QTimeEdit(QTime(6, 0));
    timeDayStart->setDisplayFormat("HH:mm");
    auto* btnSetDayStart = new QPushButton(tr("Set"));
    gridDn->addWidget(timeDayStart, 4, 1);
    gridDn->addWidget(btnSetDayStart, 4, 2);

    gridDn->addWidget(new QLabel(tr("Night Start Time:")), 5, 0);
    timeNightStart = new QTimeEdit(QTime(18, 0));
    timeNightStart->setDisplayFormat("HH:mm");
    auto* btnSetNightStart = new QPushButton(tr("Set"));
    gridDn->addWidget(timeNightStart, 5, 1);
    gridDn->addWidget(btnSetNightStart, 5, 2);

    gridDn->addWidget(new QLabel(tr("Day Optical Filter:")), 6, 0);
    cmbDayOpticalFilter = new QComboBox();
    cmbDayOpticalFilter->addItem(tr("IR Cut Filter (Normal)"), 0);
    cmbDayOpticalFilter->addItem(tr("IR Pass Filter"), 1);
    gridDn->addWidget(cmbDayOpticalFilter, 6, 1, 1, 2);

    gridDn->addWidget(new QLabel(tr("Night Optical Filter:")), 7, 0);
    cmbNightOpticalFilter = new QComboBox();
    cmbNightOpticalFilter->addItem(tr("IR Cut Filter"), 0);
    cmbNightOpticalFilter->addItem(tr("IR Pass Filter (Default)"), 1);
    cmbNightOpticalFilter->setCurrentIndex(1);
    gridDn->addWidget(cmbNightOpticalFilter, 7, 1, 1, 2);

    gridDn->addWidget(new QLabel(tr("IR Wavelength:")), 8, 0);
    cmbIrWavelength = new QComboBox();
    cmbIrWavelength->addItem(tr("Visible"), static_cast<int>(PelcoD::FujinonIRWavelength::Visible));
    cmbIrWavelength->addItem(tr("950 nm"), static_cast<int>(PelcoD::FujinonIRWavelength::W950nm));
    cmbIrWavelength->addItem(tr("940 nm"), static_cast<int>(PelcoD::FujinonIRWavelength::W940nm));
    cmbIrWavelength->addItem(tr("850 nm"), static_cast<int>(PelcoD::FujinonIRWavelength::W850nm));
    cmbIrWavelength->addItem(tr("808 nm"), static_cast<int>(PelcoD::FujinonIRWavelength::W808nm));
    gridDn->addWidget(cmbIrWavelength, 8, 1, 1, 2);

    btnQueryDayNight = new QPushButton(tr("Query Day/Night Settings"));
    gridDn->addWidget(btnQueryDayNight, 9, 0, 1, 3);

    col2->addWidget(grpDn);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    // =========================================================================
    // Column 3: System, OSD Menu & Maintenance
    // =========================================================================
    auto* col3 = new QVBoxLayout();

    // Group 5: System & OSD
    auto* grpOsd = new QGroupBox(tr("OSD Screen & Navigation"), container);
    auto* gridOsd = new QGridLayout(grpOsd);

    gridOsd->addWidget(new QLabel(tr("OSD Language:")), 0, 0);
    cmbLanguage = new QComboBox();
    cmbLanguage->addItem(tr("English"), static_cast<int>(PelcoD::FujinonLanguage::English));
    cmbLanguage->addItem(tr("French"), static_cast<int>(PelcoD::FujinonLanguage::French));
    cmbLanguage->addItem(tr("Japanese"), static_cast<int>(PelcoD::FujinonLanguage::Japanese));
    gridOsd->addWidget(cmbLanguage, 0, 1, 1, 2);

    chkOsdTime = new QCheckBox(tr("Time Overlay"));
    chkOsdTitle = new QCheckBox(tr("Title Overlay"));
    chkOsdId = new QCheckBox(tr("Camera ID"));
    chkOsdReticle = new QCheckBox(tr("Crosshair Reticle"));
    gridOsd->addWidget(chkOsdTime, 1, 0);
    gridOsd->addWidget(chkOsdTitle, 1, 1);
    gridOsd->addWidget(chkOsdId, 2, 0);
    gridOsd->addWidget(chkOsdReticle, 2, 1);

    // Menu Directional Navigation Dpad
    gridOsd->addWidget(new QLabel(tr("OSD Menu Navigation:")), 3, 0, 1, 3);
    auto* menuNavGrid = new QGridLayout();
    btnMenuUp = new QPushButton(QString::fromUtf8("▲"));
    btnMenuDown = new QPushButton(QString::fromUtf8("▼"));
    btnMenuLeft = new QPushButton(QString::fromUtf8("◀"));
    btnMenuRight = new QPushButton(QString::fromUtf8("▶"));
    btnMenuOk = new QPushButton(tr("OK"));
    btnMenuBack = new QPushButton(tr("Back"));

    const auto menuButtons = { btnMenuUp, btnMenuDown, btnMenuLeft, btnMenuRight };
    for (auto* b : menuButtons) {
        b->setFixedSize(40, 40);
    }
    btnMenuOk->setFixedSize(44, 40);
    btnMenuBack->setFixedSize(50, 40);

    menuNavGrid->addWidget(btnMenuUp, 0, 1);
    menuNavGrid->addWidget(btnMenuLeft, 1, 0);
    menuNavGrid->addWidget(btnMenuOk, 1, 1);
    menuNavGrid->addWidget(btnMenuRight, 1, 2);
    menuNavGrid->addWidget(btnMenuDown, 2, 1);
    menuNavGrid->addWidget(btnMenuBack, 2, 2);

    gridOsd->addLayout(menuNavGrid, 4, 0, 1, 3);
    col3->addWidget(grpOsd);

    // System Telemetry Info & RTC
    auto* grpMaint = new QGroupBox(tr("System Telemetry & Maintenance"), container);
    auto* gridMaint = new QGridLayout(grpMaint);

    gridMaint->addWidget(new QLabel(tr("Serial Number:")), 0, 0);
    lblSerial = new QLabel("--");
    lblSerial->setObjectName("lblTelemetry");
    gridMaint->addWidget(lblSerial, 0, 1);

    gridMaint->addWidget(new QLabel(tr("Firmware Version:")), 1, 0);
    lblFirmware = new QLabel("--");
    lblFirmware->setObjectName("lblTelemetry");
    gridMaint->addWidget(lblFirmware, 1, 1);

    gridMaint->addWidget(new QLabel(tr("Lens Diagnostic:")), 2, 0);
    lblLensStatus = new QLabel(tr("Normal (0x00)"));
    lblLensStatus->setObjectName("lblTelemetry");
    gridMaint->addWidget(lblLensStatus, 2, 1);

    btnSyncRtc = new QPushButton(tr("Sync Real-Time Clock with PC"));
    btnSyncRtc->setToolTip(tr("Synchronize camera hardware RTC to current local PC time."));
    gridMaint->addWidget(btnSyncRtc, 3, 0, 1, 2);

    btnFormatSD = new QPushButton(tr("Format SD Card"));
    btnFormatSD->setObjectName("btnDanger");
    gridMaint->addWidget(btnFormatSD, 4, 0, 1, 2);

    btnFactoryReset = new QPushButton(tr("Factory Parameter Reset"));
    btnFactoryReset->setObjectName("btnDanger");
    gridMaint->addWidget(btnFactoryReset, 5, 0, 1, 2);

    btnReboot = new QPushButton(tr("Reboot Camera"));
    btnReboot->setObjectName("btnDanger");
    gridMaint->addWidget(btnReboot, 6, 0, 1, 2);

    col3->addWidget(grpMaint);
    col3->addStretch();
    mainLayout->addLayout(col3, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);

    // Threshold and Delay buttons
    connect(btnSetD2N, &QPushButton::clicked, this, &FujinonSX800Tab::handleSetDayToNightThreshold);
    connect(btnSetN2D, &QPushButton::clicked, this, &FujinonSX800Tab::handleSetNightToDayThreshold);
    connect(btnSetDelay, &QPushButton::clicked, this, &FujinonSX800Tab::handleSetAutoDelay);
    connect(btnSetDayStart, &QPushButton::clicked, this, &FujinonSX800Tab::handleSetDayStartTime);
    connect(btnSetNightStart, &QPushButton::clicked, this, &FujinonSX800Tab::handleSetNightStartTime);
}

void FujinonSX800Tab::setupConnections()
{
    // Optics & Stabilization
    connect(cmbOis, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FujinonSX800Tab::handleOisChanged);
    connect(chkVlc, &QCheckBox::toggled, this, &FujinonSX800Tab::handleVlcToggled);
    connect(cmbDefog, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FujinonSX800Tab::handleDefogChanged);
    connect(cmbHeatHaze, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleHeatHazeChanged);
    connect(cmbWdr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FujinonSX800Tab::handleWdrChanged);
    connect(chkAntialiasing, &QCheckBox::toggled, this, &FujinonSX800Tab::handleAntialiasingToggled);

    // Fine Image Settings
    connect(spinBrightnessFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleBrightnessFineChanged);
    connect(spinContrastFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleContrastFineChanged);
    connect(spinSaturationFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleSaturationFineChanged);
    connect(spinSharpnessFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleSharpnessFineChanged);
    connect(spinWbShiftRedFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleWbShiftRedFineChanged);
    connect(spinWbShiftBlueFine, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleWbShiftBlueFineChanged);
    connect(
        cmbWbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FujinonSX800Tab::handleWbModeChanged);
    connect(spinNoiseReduction, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &FujinonSX800Tab::handleNoiseReductionChanged);
    connect(btnQueryFine, &QPushButton::clicked, this, &FujinonSX800Tab::handleQueryFineSettings);

    // Day / Night
    connect(cmbDayNightEx, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleDayNightExModeChanged);
    connect(cmbDayOpticalFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleDayOpticalFilterChanged);
    connect(cmbNightOpticalFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleNightOpticalFilterChanged);
    connect(cmbIrWavelength, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleIrWavelengthChanged);
    connect(btnQueryDayNight, &QPushButton::clicked, this, &FujinonSX800Tab::handleQueryDayNight);

    // Zoom, Focus & Telemetry
    connect(cmbZoomSpeedEx, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleZoomSpeedExChanged);
    connect(cmbFocusSpeedEx, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleFocusSpeedExChanged);
    connect(cmbDigitalZoomMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleDigitalZoomModeChanged);
    connect(cmbDigitalZoomMag, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleDigitalZoomMagChanged);
    connect(btnOnePushAf, &QPushButton::clicked, this, &FujinonSX800Tab::handleOnePushAF);
    connect(btnQueryAll, &QPushButton::clicked, this, &FujinonSX800Tab::handleQueryAllTelemetry);

    // System & OSD
    connect(cmbLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &FujinonSX800Tab::handleLanguageChanged);
    connect(chkOsdTime, &QCheckBox::toggled, this, &FujinonSX800Tab::handleOsdTimeToggled);
    connect(chkOsdTitle, &QCheckBox::toggled, this, &FujinonSX800Tab::handleOsdTitleToggled);
    connect(chkOsdId, &QCheckBox::toggled, this, &FujinonSX800Tab::handleOsdIdToggled);
    connect(chkOsdReticle, &QCheckBox::toggled, this, &FujinonSX800Tab::handleOsdReticleToggled);

    // Menu Navigation
    connect(btnMenuUp, &QPushButton::clicked, this, [this] { handleMenuNav(PelcoD::FujinonMenuDirection::Up); });
    connect(btnMenuDown, &QPushButton::clicked, this, [this] { handleMenuNav(PelcoD::FujinonMenuDirection::Down); });
    connect(btnMenuLeft, &QPushButton::clicked, this, [this] { handleMenuNav(PelcoD::FujinonMenuDirection::Left); });
    connect(btnMenuRight, &QPushButton::clicked, this, [this] { handleMenuNav(PelcoD::FujinonMenuDirection::Right); });
    connect(btnMenuOk, &QPushButton::clicked, this, &FujinonSX800Tab::handleMenuOk);
    connect(btnMenuBack, &QPushButton::clicked, this, &FujinonSX800Tab::handleMenuBack);

    // Maintenance
    connect(btnSyncRtc, &QPushButton::clicked, this, &FujinonSX800Tab::handleSyncRtc);
    connect(btnFormatSD, &QPushButton::clicked, this, &FujinonSX800Tab::handleFormatSDCard);
    connect(btnFactoryReset, &QPushButton::clicked, this, &FujinonSX800Tab::handleFactoryReset);
    connect(btnReboot, &QPushButton::clicked, this, &FujinonSX800Tab::handleReboot);
}

void FujinonSX800Tab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    // Update Telemetry Badges
    lblFocalLength->setText(tr("%1 mm").arg(status.focalLengthMm, 0, 'f', 1));
    barFocalLength->setValue(static_cast<int>(status.focalLengthMm));
    lblZoomCounts->setText(QString::number(status.absoluteZoomPosition));
    lblFocusCounts->setText(QString::number(status.absoluteFocusPosition));

    if (!status.serialNumber.empty()) {
        lblSerial->setText(QString::fromStdString(status.serialNumber));
    }
    if (!status.firmwareVersion.empty()) {
        lblFirmware->setText(QString::fromStdString(status.firmwareVersion));
    }
    lblLensStatus->setText(status.lensStatus == 0 ? tr("Normal (0x00)")
                                                  : tr("Fault (0x%1)").arg(status.lensStatus, 2, 16, QLatin1Char('0')));

    // Block signals while reflecting received telemetry in UI controls
    {
        const QSignalBlocker b1(cmbOis);
        const int idx = cmbOis->findData(static_cast<int>(status.oisMode));
        if (idx >= 0) {
            cmbOis->setCurrentIndex(idx);
        }
    }
    {
        const QSignalBlocker b2(chkVlc);
        chkVlc->setChecked(status.vlcFilter);
    }
    {
        const QSignalBlocker b3(cmbDefog);
        const int idx = cmbDefog->findData(static_cast<int>(status.defogLevel));
        if (idx >= 0) {
            cmbDefog->setCurrentIndex(idx);
        }
    }
    {
        const QSignalBlocker b4(cmbHeatHaze);
        const int idx = cmbHeatHaze->findData(static_cast<int>(status.heatHazeLevel));
        if (idx >= 0) {
            cmbHeatHaze->setCurrentIndex(idx);
        }
    }
    {
        const QSignalBlocker b5(cmbWdr);
        const int idx = cmbWdr->findData(static_cast<int>(status.wdrLevel));
        if (idx >= 0) {
            cmbWdr->setCurrentIndex(idx);
        }
    }
    {
        const QSignalBlocker b6(chkAntialiasing);
        chkAntialiasing->setChecked(status.antialiasing);
    }
    {
        const QSignalBlocker b7(spinBrightnessFine);
        spinBrightnessFine->setValue(status.fineImageSettings.brightness);
    }
    {
        const QSignalBlocker b8(spinContrastFine);
        spinContrastFine->setValue(status.fineImageSettings.contrast);
    }
    {
        const QSignalBlocker b9(spinSaturationFine);
        spinSaturationFine->setValue(status.fineImageSettings.saturation);
    }
    {
        const QSignalBlocker b10(spinSharpnessFine);
        spinSharpnessFine->setValue(status.fineImageSettings.sharpness);
    }
    {
        const QSignalBlocker b11(spinWbShiftRedFine);
        spinWbShiftRedFine->setValue(status.fineImageSettings.wbShiftRed);
    }
    {
        const QSignalBlocker b12(spinWbShiftBlueFine);
        spinWbShiftBlueFine->setValue(status.fineImageSettings.wbShiftBlue);
    }
    {
        const QSignalBlocker b13(cmbDayNightEx);
        const int idx = cmbDayNightEx->findData(static_cast<int>(status.dayNightExSettings.mode));
        if (idx >= 0) {
            cmbDayNightEx->setCurrentIndex(idx);
        }
    }
    {
        const QSignalBlocker b14(spinDayToNight);
        spinDayToNight->setValue(status.dayNightExSettings.dayToNightThreshold);
    }
    {
        const QSignalBlocker b15(spinNightToDay);
        spinNightToDay->setValue(status.dayNightExSettings.nightToDayThreshold);
    }
    {
        const QSignalBlocker b16(spinAutoDelay);
        spinAutoDelay->setValue(status.dayNightExSettings.autoDelaySeconds);
    }
    {
        const QSignalBlocker b17(timeDayStart);
        timeDayStart->setTime(QTime(status.dayNightExSettings.dayStartHour, status.dayNightExSettings.dayStartMinute));
    }
    {
        const QSignalBlocker b18(timeNightStart);
        timeNightStart->setTime(
            QTime(status.dayNightExSettings.nightStartHour, status.dayNightExSettings.nightStartMinute));
    }
    {
        const QSignalBlocker b19(cmbDayOpticalFilter);
        cmbDayOpticalFilter->setCurrentIndex(status.dayNightExSettings.opticalFilterDayIRPass ? 1 : 0);
    }
    {
        const QSignalBlocker b20(cmbNightOpticalFilter);
        cmbNightOpticalFilter->setCurrentIndex(status.dayNightExSettings.opticalFilterNightIRPass ? 1 : 0);
    }
}

// -----------------------------------------------------------------------------
// Slots: Optics & Stabilization
// -----------------------------------------------------------------------------
void FujinonSX800Tab::handleOisChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonOISMode>(cmbOis->itemData(index).toInt());
        f->setOISMode(mode);
    }
}

void FujinonSX800Tab::handleVlcToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setVLCFilter(checked);
    }
}

void FujinonSX800Tab::handleDefogChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto level = static_cast<PelcoD::FujinonDefogLevel>(cmbDefog->itemData(index).toInt());
        f->setDefog(level);
    }
}

void FujinonSX800Tab::handleHeatHazeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto level = static_cast<PelcoD::FujinonHeatHazeLevel>(cmbHeatHaze->itemData(index).toInt());
        f->setHeatHaze(level);
    }
}

void FujinonSX800Tab::handleWdrChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto level = static_cast<PelcoD::FujinonWDRLevel>(cmbWdr->itemData(index).toInt());
        f->setWDR(level);
    }
}

void FujinonSX800Tab::handleAntialiasingToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setAntialiasing(checked);
    }
}

// -----------------------------------------------------------------------------
// Slots: Fine Image Quality
// -----------------------------------------------------------------------------
void FujinonSX800Tab::handleBrightnessFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setBrightnessFine(val);
    }
}

void FujinonSX800Tab::handleContrastFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setContrastFine(val);
    }
}

void FujinonSX800Tab::handleSaturationFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setSaturationFine(val);
    }
}

void FujinonSX800Tab::handleSharpnessFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setSharpnessFine(val);
    }
}

void FujinonSX800Tab::handleWbShiftRedFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setWBShiftRedFine(val);
    }
}

void FujinonSX800Tab::handleWbShiftBlueFineChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setWBShiftBlueFine(val);
    }
}

void FujinonSX800Tab::handleWbModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonWBMode>(cmbWbMode->itemData(index).toInt());
        f->setWhiteBalance(mode);
    }
}

void FujinonSX800Tab::handleNoiseReductionChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setNoiseReduction(val);
    }
}

void FujinonSX800Tab::handleQueryFineSettings()
{
    if (auto* f = fujinon()) {
        f->queryImageQualityFine(0);
    }
}

// -----------------------------------------------------------------------------
// Slots: Day / Night & Scheduling
// -----------------------------------------------------------------------------
void FujinonSX800Tab::handleDayNightExModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonDayNightModeEx>(cmbDayNightEx->itemData(index).toInt());
        f->setDayNightModeEx(mode);
    }
}

void FujinonSX800Tab::handleSetDayToNightThreshold()
{
    if (auto* f = fujinon()) {
        f->setDayToNightThreshold(spinDayToNight->value());
    }
}

void FujinonSX800Tab::handleSetNightToDayThreshold()
{
    if (auto* f = fujinon()) {
        f->setNightToDayThreshold(spinNightToDay->value());
    }
}

void FujinonSX800Tab::handleSetAutoDelay()
{
    if (auto* f = fujinon()) {
        f->setDayNightAutoDelay(spinAutoDelay->value());
    }
}

void FujinonSX800Tab::handleSetDayStartTime()
{
    if (auto* f = fujinon()) {
        const QTime t = timeDayStart->time();
        f->setDayStartTime(t.hour(), t.minute());
    }
}

void FujinonSX800Tab::handleSetNightStartTime()
{
    if (auto* f = fujinon()) {
        const QTime t = timeNightStart->time();
        f->setNightStartTime(t.hour(), t.minute());
    }
}

void FujinonSX800Tab::handleDayOpticalFilterChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setOpticalFilterDay(index == 1);
    }
}

void FujinonSX800Tab::handleNightOpticalFilterChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setOpticalFilterNight(index == 1);
    }
}

void FujinonSX800Tab::handleIrWavelengthChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto wl = static_cast<PelcoD::FujinonIRWavelength>(cmbIrWavelength->itemData(index).toInt());
        f->setIRWavelength(wl);
    }
}

void FujinonSX800Tab::handleQueryDayNight()
{
    if (auto* f = fujinon()) {
        f->queryDayNightEx(0);
    }
}

// -----------------------------------------------------------------------------
// Slots: Zoom, Focus & Telemetry
// -----------------------------------------------------------------------------
void FujinonSX800Tab::handleZoomSpeedExChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setZoomSpeedEx(cmbZoomSpeedEx->itemData(index).toInt());
    }
}

void FujinonSX800Tab::handleFocusSpeedExChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setFocusSpeedEx(cmbFocusSpeedEx->itemData(index).toInt());
    }
}

void FujinonSX800Tab::handleDigitalZoomModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonDigitalZoomMode>(cmbDigitalZoomMode->itemData(index).toInt());
        f->setDigitalZoomMode(mode);
    }
}

void FujinonSX800Tab::handleDigitalZoomMagChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto zoom = static_cast<PelcoD::FujinonDigitalZoom>(cmbDigitalZoomMag->itemData(index).toInt());
        f->setDigitalZoom(zoom);
    }
}

void FujinonSX800Tab::handleOnePushAF()
{
    if (auto* f = fujinon()) {
        f->onePushAF();
    }
}

void FujinonSX800Tab::handleQueryAllTelemetry()
{
    if (auto* f = fujinon()) {
        f->queryPhotoSettings();
        f->queryImageQuality();
        f->queryManualSettings();
        f->queryFirmwareVersion();
        f->querySerialNumber();
        f->queryLensStatus();
        f->queryZoomStandard();
    }
}

// -----------------------------------------------------------------------------
// Slots: System, OSD & Maintenance
// -----------------------------------------------------------------------------
void FujinonSX800Tab::handleLanguageChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto lang = static_cast<PelcoD::FujinonLanguage>(cmbLanguage->itemData(index).toInt());
        f->setLanguage(lang);
    }
}

void FujinonSX800Tab::handleOsdTimeToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setTimeDisplay(checked);
    }
}

void FujinonSX800Tab::handleOsdTitleToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setTitleDisplay(checked);
    }
}

void FujinonSX800Tab::handleOsdIdToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setIdDisplay(checked);
    }
}

void FujinonSX800Tab::handleOsdReticleToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setReticleDisplay(checked);
    }
}

void FujinonSX800Tab::handleMenuNav(PelcoD::FujinonMenuDirection dir)
{
    if (auto* f = fujinon()) {
        f->menuDirection(dir);
    }
}

void FujinonSX800Tab::handleMenuOk()
{
    if (auto* f = fujinon()) {
        f->menuOk();
    }
}

void FujinonSX800Tab::handleMenuBack()
{
    if (auto* f = fujinon()) {
        f->menuBack();
    }
}

void FujinonSX800Tab::handleSyncRtc()
{
    if (auto* f = fujinon()) {
        const QDateTime now = QDateTime::currentDateTime();
        const QDate d = now.date();
        const QTime t = now.time();
        f->setRTCYear(d.year());
        f->setRTCMonthDay(d.month(), d.day());
        f->setRTCHourMinute(t.hour(), t.minute());
        f->setRTCSecond(t.second());
    }
}

void FujinonSX800Tab::handleFormatSDCard()
{
    const auto res = QMessageBox::warning(this, tr("Confirm SD Card Format"),
        tr("Are you sure you want to format the internal SD card storage?\nAll recorded media will be permanently "
           "lost!"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->formatSDCard();
        }
    }
}

void FujinonSX800Tab::handleFactoryReset()
{
    const auto res = QMessageBox::warning(this, tr("Confirm Factory Reset"),
        tr("Are you sure you want to reset the Fujinon SX800 camera to factory default parameters?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->factoryReset();
        }
    }
}

void FujinonSX800Tab::handleReboot()
{
    const auto res = QMessageBox::question(this, tr("Confirm Camera Reboot"),
        tr("Are you sure you want to trigger a software reboot of the Fujinon SX800 camera?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->reboot();
        }
    }
}

} // namespace PelcoDApp
