/// @file DayNightTab.cpp
/// @brief Implementation of DayNightTab for Day/Night and infrared filtering.

#include "DayNightTab.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace FujinonApp {

DayNightTab::DayNightTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* DayNightTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void DayNightTab::setupUi()
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
    // Column 1: Day / Night Modes, Thresholds & Delay
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    // Group 1: Modes
    auto* grpModes = new QGroupBox(tr("Day / Night Operation Mode"), container);
    auto* gridModes = new QGridLayout(grpModes);

    gridModes->addWidget(new QLabel(tr("Standard Mode:")), 0, 0);
    cmbDayNight = new QComboBox(grpModes);
    cmbDayNight->addItem(tr("Auto"), static_cast<int>(PelcoD::FujinonDayNightMode::Auto));
    cmbDayNight->addItem(tr("Day Mode"), static_cast<int>(PelcoD::FujinonDayNightMode::Day));
    cmbDayNight->addItem(tr("Night Mode"), static_cast<int>(PelcoD::FujinonDayNightMode::Night));
    gridModes->addWidget(cmbDayNight, 0, 1);

    gridModes->addWidget(new QLabel(tr("Extended Mode (v2.12.0):")), 1, 0);
    cmbDayNightEx = new QComboBox(grpModes);
    cmbDayNightEx->addItem(tr("Auto"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Auto));
    cmbDayNightEx->addItem(tr("Auto & Scheduled"), static_cast<int>(PelcoD::FujinonDayNightModeEx::AutoAndScheduled));
    cmbDayNightEx->addItem(tr("Scheduled"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Scheduled));
    cmbDayNightEx->addItem(tr("Day Fixed"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Day));
    cmbDayNightEx->addItem(tr("Night Fixed"), static_cast<int>(PelcoD::FujinonDayNightModeEx::Night));
    gridModes->addWidget(cmbDayNightEx, 1, 1);
    col1->addWidget(grpModes);

    // Group 2: Thresholds & Delay
    auto* grpThresh = new QGroupBox(tr("Switching Thresholds & Delay"), container);
    auto* gridThresh = new QGridLayout(grpThresh);

    gridThresh->addWidget(new QLabel(tr("Day to Night Threshold (0..255):")), 0, 0);
    spinDayToNight = new QSpinBox(grpThresh);
    spinDayToNight->setRange(0, 255);
    btnSetDayToNight = new QPushButton(tr("Set"), grpThresh);
    gridThresh->addWidget(spinDayToNight, 0, 1);
    gridThresh->addWidget(btnSetDayToNight, 0, 2);

    gridThresh->addWidget(new QLabel(tr("Night to Day Threshold (0..255):")), 1, 0);
    spinNightToDay = new QSpinBox(grpThresh);
    spinNightToDay->setRange(0, 255);
    btnSetNightToDay = new QPushButton(tr("Set"), grpThresh);
    gridThresh->addWidget(spinNightToDay, 1, 1);
    gridThresh->addWidget(btnSetNightToDay, 1, 2);

    gridThresh->addWidget(new QLabel(tr("Auto Switch Delay (0..60s):")), 2, 0);
    spinAutoDelay = new QSpinBox(grpThresh);
    spinAutoDelay->setRange(0, 60);
    btnSetAutoDelay = new QPushButton(tr("Set"), grpThresh);
    gridThresh->addWidget(spinAutoDelay, 2, 1);
    gridThresh->addWidget(btnSetAutoDelay, 2, 2);
    col1->addWidget(grpThresh);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: Schedules, Optical Filters & External Trigger
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    // Group 3: Scheduled Switching
    auto* grpSched = new QGroupBox(tr("Scheduled Switching Times"), container);
    auto* gridSched = new QGridLayout(grpSched);

    gridSched->addWidget(new QLabel(tr("Day Mode Start Time:")), 0, 0);
    timeDayStart = new QTimeEdit(QTime(6, 0), grpSched);
    btnSetDayStart = new QPushButton(tr("Set Day Start"), grpSched);
    gridSched->addWidget(timeDayStart, 0, 1);
    gridSched->addWidget(btnSetDayStart, 0, 2);

    gridSched->addWidget(new QLabel(tr("Night Mode Start Time:")), 1, 0);
    timeNightStart = new QTimeEdit(QTime(18, 0), grpSched);
    btnSetNightStart = new QPushButton(tr("Set Night Start"), grpSched);
    gridSched->addWidget(timeNightStart, 1, 1);
    gridSched->addWidget(btnSetNightStart, 1, 2);
    col2->addWidget(grpSched);

    // Group 4: Optical Filters & External Trigger
    auto* grpFilters = new QGroupBox(tr("Optical Filters & External Contact"), container);
    auto* gridFilters = new QGridLayout(grpFilters);

    gridFilters->addWidget(new QLabel(tr("Day Optical Filter:")), 0, 0);
    cmbDayFilter = new QComboBox(grpFilters);
    cmbDayFilter->addItem(tr("IR Cut Filter (Standard Day)"), 0);
    cmbDayFilter->addItem(tr("IR Pass Filter"), 1);
    gridFilters->addWidget(cmbDayFilter, 0, 1);

    gridFilters->addWidget(new QLabel(tr("Night Optical Filter:")), 1, 0);
    cmbNightFilter = new QComboBox(grpFilters);
    cmbNightFilter->addItem(tr("IR Pass Filter (Standard Night)"), 1);
    cmbNightFilter->addItem(tr("IR Cut Filter"), 0);
    gridFilters->addWidget(cmbNightFilter, 1, 1);

    gridFilters->addWidget(new QLabel(tr("IR Bandpass Wavelength:")), 2, 0);
    cmbIrWavelength = new QComboBox(grpFilters);
    cmbIrWavelength->addItem(tr("Visible Light Band"), static_cast<int>(PelcoD::FujinonIRWavelength::Visible));
    cmbIrWavelength->addItem(tr("950 nm Bandpass"), static_cast<int>(PelcoD::FujinonIRWavelength::W950nm));
    cmbIrWavelength->addItem(tr("940 nm Bandpass"), static_cast<int>(PelcoD::FujinonIRWavelength::W940nm));
    cmbIrWavelength->addItem(tr("850 nm Bandpass"), static_cast<int>(PelcoD::FujinonIRWavelength::W850nm));
    cmbIrWavelength->addItem(tr("808 nm Bandpass"), static_cast<int>(PelcoD::FujinonIRWavelength::W808nm));
    gridFilters->addWidget(cmbIrWavelength, 2, 1);

    gridFilters->addWidget(new QLabel(tr("External Contact Trigger:")), 3, 0);
    cmbExternalDn = new QComboBox(grpFilters);
    cmbExternalDn->addItem(tr("On (VLC Filter)"), static_cast<int>(PelcoD::FujinonExternalDayNight::OnVLC));
    cmbExternalDn->addItem(tr("On (Clear)"), static_cast<int>(PelcoD::FujinonExternalDayNight::OnClear));
    cmbExternalDn->addItem(tr("Off"), static_cast<int>(PelcoD::FujinonExternalDayNight::Off));
    gridFilters->addWidget(cmbExternalDn, 3, 1);

    btnQueryDayNight = new QPushButton(tr("Query Extended Day/Night Settings"), grpFilters);
    gridFilters->addWidget(btnQueryDayNight, 4, 0, 1, 2);
    col2->addWidget(grpFilters);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void DayNightTab::setupConnections()
{
    connect(cmbDayNight, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DayNightTab::handleDayNightModeChanged);
    connect(cmbDayNightEx, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DayNightTab::handleDayNightExChanged);

    connect(btnSetDayToNight, &QPushButton::clicked, this, &DayNightTab::handleSetDayToNightThreshold);
    connect(btnSetNightToDay, &QPushButton::clicked, this, &DayNightTab::handleSetNightToDayThreshold);
    connect(btnSetAutoDelay, &QPushButton::clicked, this, &DayNightTab::handleSetAutoDelay);

    connect(btnSetDayStart, &QPushButton::clicked, this, &DayNightTab::handleSetDayStartTime);
    connect(btnSetNightStart, &QPushButton::clicked, this, &DayNightTab::handleSetNightStartTime);

    connect(
        cmbDayFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DayNightTab::handleDayFilterChanged);
    connect(cmbNightFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DayNightTab::handleNightFilterChanged);
    connect(cmbIrWavelength, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DayNightTab::handleIrWavelengthChanged);
    connect(cmbExternalDn, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &DayNightTab::handleExternalDnChanged);

    connect(btnQueryDayNight, &QPushButton::clicked, this, &DayNightTab::handleQueryDayNight);
}

void DayNightTab::handleDayNightModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonDayNightMode>(cmbDayNight->itemData(index).toInt());
        f->setDayNightMode(mode);
    }
}

void DayNightTab::handleDayNightExChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonDayNightModeEx>(cmbDayNightEx->itemData(index).toInt());
        f->setDayNightModeEx(mode);
    }
}

void DayNightTab::handleSetDayToNightThreshold()
{
    if (auto* f = fujinon()) {
        f->setDayToNightThreshold(spinDayToNight->value());
    }
}

void DayNightTab::handleSetNightToDayThreshold()
{
    if (auto* f = fujinon()) {
        f->setNightToDayThreshold(spinNightToDay->value());
    }
}

void DayNightTab::handleSetAutoDelay()
{
    if (auto* f = fujinon()) {
        f->setDayNightAutoDelay(spinAutoDelay->value());
    }
}

void DayNightTab::handleSetDayStartTime()
{
    if (auto* f = fujinon()) {
        const auto t = timeDayStart->time();
        f->setDayStartTime(t.hour(), t.minute());
    }
}

void DayNightTab::handleSetNightStartTime()
{
    if (auto* f = fujinon()) {
        const auto t = timeNightStart->time();
        f->setNightStartTime(t.hour(), t.minute());
    }
}

void DayNightTab::handleDayFilterChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setOpticalFilterDay(cmbDayFilter->itemData(index).toInt() == 1);
    }
}

void DayNightTab::handleNightFilterChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setOpticalFilterNight(cmbNightFilter->itemData(index).toInt() == 1);
    }
}

void DayNightTab::handleIrWavelengthChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto wl = static_cast<PelcoD::FujinonIRWavelength>(cmbIrWavelength->itemData(index).toInt());
        f->setIRWavelength(wl);
    }
}

void DayNightTab::handleExternalDnChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto ext = static_cast<PelcoD::FujinonExternalDayNight>(cmbExternalDn->itemData(index).toInt());
        f->setExternalDayNight(ext);
    }
}

void DayNightTab::handleQueryDayNight()
{
    if (auto* f = fujinon()) {
        f->queryDayNightEx(0x00);
    }
}

void DayNightTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    spinDayToNight->setValue(status.dayNightExSettings.dayToNightThreshold);
    spinNightToDay->setValue(status.dayNightExSettings.nightToDayThreshold);
    spinAutoDelay->setValue(status.dayNightExSettings.autoDelaySeconds);
    timeDayStart->setTime(QTime(status.dayNightExSettings.dayStartHour, status.dayNightExSettings.dayStartMinute));
    timeNightStart->setTime(
        QTime(status.dayNightExSettings.nightStartHour, status.dayNightExSettings.nightStartMinute));
}

} // namespace FujinonApp
