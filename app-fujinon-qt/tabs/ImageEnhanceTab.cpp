/// @file ImageEnhanceTab.cpp
/// @brief Implementation of ImageEnhanceTab for optical stabilization, defog, and fine tuning.

#include "ImageEnhanceTab.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace FujinonApp {

ImageEnhanceTab::ImageEnhanceTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* ImageEnhanceTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void ImageEnhanceTab::setupUi()
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
    // Column 1: Stabilization, Atmospheric & Color Balance
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    // Group 1: Stabilization & Optical Filters
    auto* grpStab = new QGroupBox(tr("Stabilization & Optical Filters"), container);
    auto* gridStab = new QGridLayout(grpStab);

    gridStab->addWidget(new QLabel(tr("Stabilization Mode:")), 0, 0);
    cmbOis = new QComboBox(grpStab);
    cmbOis->addItem(tr("Auto (Switching)"), static_cast<int>(PelcoD::FujinonOISMode::Auto));
    cmbOis->addItem(tr("OIS Only (Optical)"), static_cast<int>(PelcoD::FujinonOISMode::OisOn));
    cmbOis->addItem(tr("EIS Only (Electronic)"), static_cast<int>(PelcoD::FujinonOISMode::EisOn));
    cmbOis->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonOISMode::Off));
    gridStab->addWidget(cmbOis, 0, 1);

    chkVlc = new QCheckBox(tr("Visible Light Cut (VLC) Optical Filter"), grpStab);
    chkAntialiasing = new QCheckBox(tr("Anti-Aliasing Filter"), grpStab);
    gridStab->addWidget(chkVlc, 1, 0, 1, 2);
    gridStab->addWidget(chkAntialiasing, 2, 0, 1, 2);
    col1->addWidget(grpStab);

    // Group 2: Atmospheric Processing & Noise Reduction
    auto* grpAtmos = new QGroupBox(tr("Atmospheric Processing & NR"), container);
    auto* gridAtmos = new QGridLayout(grpAtmos);

    gridAtmos->addWidget(new QLabel(tr("Defog Level:")), 0, 0);
    cmbDefog = new QComboBox(grpAtmos);
    cmbDefog->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonDefogLevel::Off));
    cmbDefog->addItem(tr("Level 1 (Low)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level1));
    cmbDefog->addItem(tr("Level 2 (Mid)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level2));
    cmbDefog->addItem(tr("Level 3 (High)"), static_cast<int>(PelcoD::FujinonDefogLevel::Level3));
    gridAtmos->addWidget(cmbDefog, 0, 1);

    gridAtmos->addWidget(new QLabel(tr("Heat Haze Reduction:")), 1, 0);
    cmbHeatHaze = new QComboBox(grpAtmos);
    cmbHeatHaze->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Off));
    cmbHeatHaze->addItem(tr("Level 1 (Low)"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Level1));
    cmbHeatHaze->addItem(tr("Level 2 (High)"), static_cast<int>(PelcoD::FujinonHeatHazeLevel::Level2));
    gridAtmos->addWidget(cmbHeatHaze, 1, 1);

    gridAtmos->addWidget(new QLabel(tr("Wide Dynamic Range (WDR):")), 2, 0);
    cmbWdr = new QComboBox(grpAtmos);
    cmbWdr->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonWDRLevel::Off));
    cmbWdr->addItem(tr("Level 1 (Low)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level1));
    cmbWdr->addItem(tr("Level 2 (Mid)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level2));
    cmbWdr->addItem(tr("Level 3 (High)"), static_cast<int>(PelcoD::FujinonWDRLevel::Level3));
    gridAtmos->addWidget(cmbWdr, 2, 1);

    gridAtmos->addWidget(new QLabel(tr("Noise Reduction (1..5):")), 3, 0);
    spinNoiseReduction = new QSpinBox(grpAtmos);
    spinNoiseReduction->setRange(1, 5);
    spinNoiseReduction->setValue(3);
    gridAtmos->addWidget(spinNoiseReduction, 3, 1);
    col1->addWidget(grpAtmos);

    // Group 3: White Balance
    auto* grpWb = new QGroupBox(tr("White Balance"), container);
    auto* gridWb = new QGridLayout(grpWb);

    gridWb->addWidget(new QLabel(tr("White Balance Mode:")), 0, 0);
    cmbWbMode = new QComboBox(grpWb);
    cmbWbMode->addItem(tr("Auto"), static_cast<int>(PelcoD::FujinonWBMode::Auto));
    cmbWbMode->addItem(tr("Custom 1"), static_cast<int>(PelcoD::FujinonWBMode::Custom1));
    cmbWbMode->addItem(tr("Custom 2"), static_cast<int>(PelcoD::FujinonWBMode::Custom2));
    cmbWbMode->addItem(tr("Day"), static_cast<int>(PelcoD::FujinonWBMode::Day));
    cmbWbMode->addItem(tr("Cloud"), static_cast<int>(PelcoD::FujinonWBMode::Cloud));
    cmbWbMode->addItem(tr("Color Temperature"), static_cast<int>(PelcoD::FujinonWBMode::ColorTemp));
    gridWb->addWidget(cmbWbMode, 0, 1);

    gridWb->addWidget(new QLabel(tr("Color Temperature:")), 1, 0);
    cmbColorTemp = new QComboBox(grpWb);
    cmbColorTemp->addItem(tr("3000 K"), static_cast<int>(PelcoD::FujinonColorTemp::K3000));
    cmbColorTemp->addItem(tr("5000 K"), static_cast<int>(PelcoD::FujinonColorTemp::K5000));
    cmbColorTemp->addItem(tr("9000 K"), static_cast<int>(PelcoD::FujinonColorTemp::K9000));
    gridWb->addWidget(cmbColorTemp, 1, 1);
    col1->addWidget(grpWb);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: Fine 1..100 Picture Parameter Sliders
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    auto* grpFine = new QGroupBox(tr("Fine Picture Quality (1..100)"), container);
    auto* gridFine = new QGridLayout(grpFine);

    auto addSliderRow = [&](const QString& label, QSlider*& slider, QSpinBox*& spin, int row) {
        gridFine->addWidget(new QLabel(label), row, 0);
        slider = new QSlider(Qt::Horizontal, grpFine);
        slider->setRange(1, 100);
        slider->setValue(50);
        spin = new QSpinBox(grpFine);
        spin->setRange(1, 100);
        spin->setValue(50);
        gridFine->addWidget(slider, row, 1);
        gridFine->addWidget(spin, row, 2);
    };

    addSliderRow(tr("Brightness:"), sliderBrightness, spinBrightness, 0);
    addSliderRow(tr("Contrast:"), sliderContrast, spinContrast, 1);
    addSliderRow(tr("Saturation:"), sliderSaturation, spinSaturation, 2);
    addSliderRow(tr("Sharpness:"), sliderSharpness, spinSharpness, 3);
    addSliderRow(tr("WB Shift (Red):"), sliderWbRed, spinWbRed, 4);
    addSliderRow(tr("WB Shift (Blue):"), sliderWbBlue, spinWbBlue, 5);

    auto* hQuery = new QHBoxLayout();
    btnQueryFine = new QPushButton(tr("Query Fine Settings"), grpFine);
    btnQueryQuality = new QPushButton(tr("Query Standard Quality"), grpFine);
    hQuery->addWidget(btnQueryFine);
    hQuery->addWidget(btnQueryQuality);
    gridFine->addLayout(hQuery, 6, 0, 1, 3);

    col2->addWidget(grpFine);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void ImageEnhanceTab::setupConnections()
{
    connect(cmbOis, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImageEnhanceTab::handleOisChanged);
    connect(chkVlc, &QCheckBox::toggled, this, &ImageEnhanceTab::handleVlcToggled);
    connect(chkAntialiasing, &QCheckBox::toggled, this, &ImageEnhanceTab::handleAntialiasingToggled);

    connect(cmbDefog, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImageEnhanceTab::handleDefogChanged);
    connect(cmbHeatHaze, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ImageEnhanceTab::handleHeatHazeChanged);
    connect(cmbWdr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImageEnhanceTab::handleWdrChanged);
    connect(spinNoiseReduction, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &ImageEnhanceTab::handleNoiseReductionChanged);

    connect(
        cmbWbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ImageEnhanceTab::handleWbModeChanged);
    connect(cmbColorTemp, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ImageEnhanceTab::handleColorTempChanged);

    // Sync fine sliders and spinboxes
    auto linkSlider = [this](QSlider* s, QSpinBox* sp, auto func) {
        connect(s, &QSlider::valueChanged, sp, &QSpinBox::setValue);
        connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), s, &QSlider::setValue);
        connect(s, &QSlider::valueChanged, this, func);
    };

    linkSlider(sliderBrightness, spinBrightness, &ImageEnhanceTab::handleBrightnessChanged);
    linkSlider(sliderContrast, spinContrast, &ImageEnhanceTab::handleContrastChanged);
    linkSlider(sliderSaturation, spinSaturation, &ImageEnhanceTab::handleSaturationChanged);
    linkSlider(sliderSharpness, spinSharpness, &ImageEnhanceTab::handleSharpnessChanged);
    linkSlider(sliderWbRed, spinWbRed, &ImageEnhanceTab::handleWbRedChanged);
    linkSlider(sliderWbBlue, spinWbBlue, &ImageEnhanceTab::handleWbBlueChanged);

    connect(btnQueryFine, &QPushButton::clicked, this, &ImageEnhanceTab::handleQueryFineQuality);
    connect(btnQueryQuality, &QPushButton::clicked, this, &ImageEnhanceTab::handleQueryImageQuality);
}

void ImageEnhanceTab::handleOisChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonOISMode>(cmbOis->itemData(index).toInt());
        f->setOISMode(mode);
    }
}

void ImageEnhanceTab::handleVlcToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setVLCFilter(checked);
    }
}

void ImageEnhanceTab::handleAntialiasingToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setAntialiasing(checked);
    }
}

void ImageEnhanceTab::handleDefogChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto lvl = static_cast<PelcoD::FujinonDefogLevel>(cmbDefog->itemData(index).toInt());
        f->setDefog(lvl);
    }
}

void ImageEnhanceTab::handleHeatHazeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto lvl = static_cast<PelcoD::FujinonHeatHazeLevel>(cmbHeatHaze->itemData(index).toInt());
        f->setHeatHaze(lvl);
    }
}

void ImageEnhanceTab::handleWdrChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto lvl = static_cast<PelcoD::FujinonWDRLevel>(cmbWdr->itemData(index).toInt());
        f->setWDR(lvl);
    }
}

void ImageEnhanceTab::handleNoiseReductionChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setNoiseReduction(val);
    }
}

void ImageEnhanceTab::handleWbModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonWBMode>(cmbWbMode->itemData(index).toInt());
        f->setWhiteBalance(mode);
    }
}

void ImageEnhanceTab::handleColorTempChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto temp = static_cast<PelcoD::FujinonColorTemp>(cmbColorTemp->itemData(index).toInt());
        f->setColorTemperature(temp);
    }
}

void ImageEnhanceTab::handleBrightnessChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setBrightnessFine(val);
    }
}

void ImageEnhanceTab::handleContrastChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setContrastFine(val);
    }
}

void ImageEnhanceTab::handleSaturationChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setSaturationFine(val);
    }
}

void ImageEnhanceTab::handleSharpnessChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setSharpnessFine(val);
    }
}

void ImageEnhanceTab::handleWbRedChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setWBShiftRedFine(val);
    }
}

void ImageEnhanceTab::handleWbBlueChanged(int val)
{
    if (auto* f = fujinon()) {
        f->setWBShiftBlueFine(val);
    }
}

void ImageEnhanceTab::handleQueryFineQuality()
{
    if (auto* f = fujinon()) {
        f->queryImageQualityFine(0x00);
    }
}

void ImageEnhanceTab::handleQueryImageQuality()
{
    if (auto* f = fujinon()) {
        f->queryImageQuality();
    }
}

void ImageEnhanceTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    const QSignalBlocker bVlc(chkVlc);
    chkVlc->setChecked(status.vlcFilter);

    const QSignalBlocker bAa(chkAntialiasing);
    chkAntialiasing->setChecked(status.antialiasing);

    if (status.fineImageSettings.brightness > 0U) {
        const QSignalBlocker bBri(sliderBrightness);
        sliderBrightness->setValue(static_cast<int>(status.fineImageSettings.brightness));
    }
    if (status.fineImageSettings.contrast > 0U) {
        const QSignalBlocker bCon(sliderContrast);
        sliderContrast->setValue(static_cast<int>(status.fineImageSettings.contrast));
    }
    if (status.fineImageSettings.saturation > 0U) {
        const QSignalBlocker bSat(sliderSaturation);
        sliderSaturation->setValue(static_cast<int>(status.fineImageSettings.saturation));
    }
    if (status.fineImageSettings.sharpness > 0U) {
        const QSignalBlocker bShp(sliderSharpness);
        sliderSharpness->setValue(static_cast<int>(status.fineImageSettings.sharpness));
    }
}

} // namespace FujinonApp
