/// @file SystemMediaTab.cpp
/// @brief Implementation of SystemMediaTab for camera system maintenance and SD media storage.

#include "SystemMediaTab.h"

#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace FujinonApp {

SystemMediaTab::SystemMediaTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* SystemMediaTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void SystemMediaTab::setupUi()
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
    // Column 1: System, Communication & Video Standards
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    auto* grpSys = new QGroupBox(tr("System & Video Standards"), container);
    auto* gridSys = new QGridLayout(grpSys);

    gridSys->addWidget(new QLabel(tr("Video Standard:")), 0, 0);
    cmbVideoMode = new QComboBox(grpSys);
    cmbVideoMode->addItem(tr("NTSC"), static_cast<int>(PelcoD::FujinonVideoMode::NTSC));
    cmbVideoMode->addItem(tr("PAL"), static_cast<int>(PelcoD::FujinonVideoMode::PAL));
    gridSys->addWidget(cmbVideoMode, 0, 1);

    gridSys->addWidget(new QLabel(tr("Full HD SDI / HDMI Format:")), 1, 0);
    cmbHdFormat = new QComboBox(grpSys);
    cmbHdFormat->addItem(tr("1080p 60fps"), static_cast<int>(PelcoD::FujinonHDFormat::F1080p60));
    cmbHdFormat->addItem(tr("1080p 50fps"), static_cast<int>(PelcoD::FujinonHDFormat::F1080p50));
    cmbHdFormat->addItem(tr("1080p 30fps"), static_cast<int>(PelcoD::FujinonHDFormat::F1080p30));
    cmbHdFormat->addItem(tr("1080p 25fps"), static_cast<int>(PelcoD::FujinonHDFormat::F1080p25));
    gridSys->addWidget(cmbHdFormat, 1, 1);

    gridSys->addWidget(new QLabel(tr("Reprogram RS-485 Address (1..31):")), 2, 0);
    auto* hAddr = new QHBoxLayout();
    spinNewAddress = new QSpinBox(grpSys);
    spinNewAddress->setRange(1, 31);
    spinNewAddress->setValue(1);
    btnReprogramAddress = new QPushButton(tr("Reprogram Address"), grpSys);
    hAddr->addWidget(spinNewAddress);
    hAddr->addWidget(btnReprogramAddress);
    gridSys->addLayout(hAddr, 2, 1);

    chkTermination = new QCheckBox(tr("RS-485 Internal 120Ω Termination"), grpSys);
    gridSys->addWidget(chkTermination, 3, 0, 1, 2);

    gridSys->addWidget(new QLabel(tr("Remote Baud Rate:")), 4, 0);
    cmbBaudRate = new QComboBox(grpSys);
    cmbBaudRate->addItem(tr("2400 bps"), 0);
    cmbBaudRate->addItem(tr("4800 bps"), 1);
    cmbBaudRate->addItem(tr("9600 bps"), 2);
    cmbBaudRate->addItem(tr("19200 bps"), 3);
    cmbBaudRate->addItem(tr("38400 bps"), 4);
    cmbBaudRate->addItem(tr("115200 bps"), 5);
    gridSys->addWidget(cmbBaudRate, 4, 1);

    gridSys->addWidget(new QLabel(tr("OSD Language:")), 5, 0);
    cmbLanguage = new QComboBox(grpSys);
    cmbLanguage->addItem(tr("English"), static_cast<int>(PelcoD::FujinonLanguage::English));
    cmbLanguage->addItem(tr("French"), static_cast<int>(PelcoD::FujinonLanguage::French));
    cmbLanguage->addItem(tr("Japanese"), static_cast<int>(PelcoD::FujinonLanguage::Japanese));
    gridSys->addWidget(cmbLanguage, 5, 1);

    auto* hRtc = new QHBoxLayout();
    btnSyncRtc = new QPushButton(tr("Sync Real-Time Clock With PC"), grpSys);
    btnQueryRtc = new QPushButton(tr("Query RTC"), grpSys);
    hRtc->addWidget(btnSyncRtc);
    hRtc->addWidget(btnQueryRtc);
    gridSys->addLayout(hRtc, 6, 0, 1, 2);

    auto* hPower = new QHBoxLayout();
    btnReboot = new QPushButton(tr("Reboot Camera"), grpSys);
    btnFactoryReset = new QPushButton(tr("Factory Reset"), grpSys);
    hPower->addWidget(btnReboot);
    hPower->addWidget(btnFactoryReset);
    gridSys->addLayout(hPower, 7, 0, 1, 2);

    col1->addWidget(grpSys);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: SD Card Recording, Playback & Diagnostics
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    // Group 2: SD Card & LiveView Recording
    auto* grpSd = new QGroupBox(tr("SD Card Storage & Recording"), container);
    auto* gridSd = new QGridLayout(grpSd);

    btnFormatSD = new QPushButton(tr("Format SD Card"), grpSd);
    btnRecordLive = new QPushButton(tr("Start LiveView Recording"), grpSd);
    btnRecordLive->setCheckable(true);
    gridSd->addWidget(btnFormatSD, 0, 0);
    gridSd->addWidget(btnRecordLive, 0, 1);

    gridSd->addWidget(new QLabel(tr("Recording Video Codec:")), 1, 0);
    cmbEncodeMode = new QComboBox(grpSd);
    cmbEncodeMode->addItem(tr("H.264"), static_cast<int>(PelcoD::FujinonVideoEncoding::H264));
    cmbEncodeMode->addItem(tr("Motion JPEG (MJPEG)"), static_cast<int>(PelcoD::FujinonVideoEncoding::MJPEG));
    cmbEncodeMode->addItem(tr("MPEG-4"), static_cast<int>(PelcoD::FujinonVideoEncoding::MPEG4));
    gridSd->addWidget(cmbEncodeMode, 1, 1);

    gridSd->addWidget(new QLabel(tr("Full SD Action:")), 2, 0);
    cmbSdRecordMode = new QComboBox(grpSd);
    cmbSdRecordMode->addItem(tr("Overwrite Oldest"), static_cast<int>(PelcoD::FujinonSDRecordMode::Overwrite));
    cmbSdRecordMode->addItem(tr("Stop Recording"), static_cast<int>(PelcoD::FujinonSDRecordMode::Stop));
    gridSd->addWidget(cmbSdRecordMode, 2, 1);

    chkSdCapacityDisp = new QCheckBox(tr("Display Remaining SD Capacity on OSD"), grpSd);
    gridSd->addWidget(chkSdCapacityDisp, 3, 0, 1, 2);

    btnRecordLog = new QPushButton(tr("Record Diagnostic Log to SD"), grpSd);
    gridSd->addWidget(btnRecordLog, 4, 0);

    cmbLogType = new QComboBox(grpSd);
    cmbLogType->addItem(tr("Zoom Log"), static_cast<int>(PelcoD::FujinonLogType::Zoom));
    cmbLogType->addItem(tr("Focus Log"), static_cast<int>(PelcoD::FujinonLogType::Focus));
    cmbLogType->addItem(tr("Power-On Time Log"), static_cast<int>(PelcoD::FujinonLogType::PowerOnTime));
    cmbLogType->addItem(tr("OIS Log"), static_cast<int>(PelcoD::FujinonLogType::OIS));
    btnQueryLog = new QPushButton(tr("Query Log"), grpSd);

    auto* hLog = new QHBoxLayout();
    hLog->addWidget(cmbLogType);
    hLog->addWidget(btnQueryLog);
    gridSd->addLayout(hLog, 4, 1);
    col2->addWidget(grpSd);

    // Group 3: Movie Playback Controls
    auto* grpPlay = new QGroupBox(tr("Recorded Movie Playback"), container);
    auto* gridPlay = new QGridLayout(grpPlay);

    gridPlay->addWidget(new QLabel(tr("Target File Index (1..65535, 0=All):")), 0, 0);
    spinMovieFileIndex = new QSpinBox(grpPlay);
    spinMovieFileIndex->setRange(0, 65535);
    spinMovieFileIndex->setValue(1);
    btnSetFirstMovie = new QPushButton(tr("Set Initial File"), grpPlay);
    gridPlay->addWidget(spinMovieFileIndex, 0, 1);
    gridPlay->addWidget(btnSetFirstMovie, 0, 2);

    gridPlay->addWidget(new QLabel(tr("Playback Mode:")), 1, 0);
    cmbPlayMovieMode = new QComboBox(grpPlay);
    cmbPlayMovieMode->addItem(tr("Stop"), static_cast<int>(PelcoD::FujinonMoviePlayMode::Stop));
    cmbPlayMovieMode->addItem(tr("Step (Frame by Frame)"), static_cast<int>(PelcoD::FujinonMoviePlayMode::Step));
    cmbPlayMovieMode->addItem(tr("Continuous"), static_cast<int>(PelcoD::FujinonMoviePlayMode::Continuous));
    btnPlayMovie = new QPushButton(tr("Play Movie"), grpPlay);
    btnPlayMovie->setCheckable(true);
    btnDeleteMovie = new QPushButton(tr("Delete Movie"), grpPlay);
    gridPlay->addWidget(cmbPlayMovieMode, 1, 1);
    gridPlay->addWidget(btnPlayMovie, 1, 2);
    gridPlay->addWidget(btnDeleteMovie, 1, 3);

    auto* hQueries = new QHBoxLayout();
    btnQueryMovieCount = new QPushButton(tr("Query Count"), grpPlay);
    btnQueryMovieYear = new QPushButton(tr("Query Year"), grpPlay);
    btnQueryMovieDate = new QPushButton(tr("Query Date"), grpPlay);
    btnQueryMovieTime = new QPushButton(tr("Query Time"), grpPlay);
    hQueries->addWidget(btnQueryMovieCount);
    hQueries->addWidget(btnQueryMovieYear);
    hQueries->addWidget(btnQueryMovieDate);
    hQueries->addWidget(btnQueryMovieTime);
    gridPlay->addLayout(hQueries, 2, 0, 1, 4);

    lblMovieInfo = new QLabel(tr("Movie Inventory: Ready"), grpPlay);
    gridPlay->addWidget(lblMovieInfo, 3, 0, 1, 4);
    col2->addWidget(grpPlay);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void SystemMediaTab::setupConnections()
{
    connect(cmbVideoMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &SystemMediaTab::handleVideoModeChanged);
    connect(
        cmbHdFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SystemMediaTab::handleHdFormatChanged);
    connect(btnReprogramAddress, &QPushButton::clicked, this, &SystemMediaTab::handleReprogramAddress);
    connect(chkTermination, &QCheckBox::toggled, this, &SystemMediaTab::handleTerminationToggled);
    connect(
        cmbBaudRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SystemMediaTab::handleBaudRateChanged);
    connect(
        cmbLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SystemMediaTab::handleLanguageChanged);
    connect(btnSyncRtc, &QPushButton::clicked, this, &SystemMediaTab::handleSyncRtc);
    connect(btnQueryRtc, &QPushButton::clicked, this, &SystemMediaTab::handleQueryRtc);
    connect(btnReboot, &QPushButton::clicked, this, &SystemMediaTab::handleReboot);
    connect(btnFactoryReset, &QPushButton::clicked, this, &SystemMediaTab::handleFactoryReset);

    connect(btnFormatSD, &QPushButton::clicked, this, &SystemMediaTab::handleFormatSD);
    connect(btnRecordLive, &QPushButton::toggled, this, &SystemMediaTab::handleRecordLiveToggled);
    connect(cmbEncodeMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &SystemMediaTab::handleEncodeModeChanged);
    connect(cmbSdRecordMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &SystemMediaTab::handleSdRecordModeChanged);
    connect(chkSdCapacityDisp, &QCheckBox::toggled, this, &SystemMediaTab::handleSdCapDispToggled);
    connect(btnRecordLog, &QPushButton::clicked, this, &SystemMediaTab::handleRecordLogData);
    connect(btnQueryLog, &QPushButton::clicked, this, &SystemMediaTab::handleQueryLogData);

    connect(btnSetFirstMovie, &QPushButton::clicked, this, &SystemMediaTab::handleSetFirstMovieFile);
    connect(cmbPlayMovieMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &SystemMediaTab::handlePlayMovieModeChanged);
    connect(btnPlayMovie, &QPushButton::toggled, this, &SystemMediaTab::handlePlayMovieToggled);
    connect(btnDeleteMovie, &QPushButton::clicked, this, &SystemMediaTab::handleDeleteMovie);
    connect(btnQueryMovieCount, &QPushButton::clicked, this, &SystemMediaTab::handleQueryMovieCount);
    connect(btnQueryMovieYear, &QPushButton::clicked, this, &SystemMediaTab::handleQueryMovieYear);
    connect(btnQueryMovieDate, &QPushButton::clicked, this, &SystemMediaTab::handleQueryMovieDate);
    connect(btnQueryMovieTime, &QPushButton::clicked, this, &SystemMediaTab::handleQueryMovieTime);
}

void SystemMediaTab::handleVideoModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonVideoMode>(cmbVideoMode->itemData(index).toInt());
        f->setVideoMode(mode);
    }
}

void SystemMediaTab::handleHdFormatChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto fmt = static_cast<PelcoD::FujinonHDFormat>(cmbHdFormat->itemData(index).toInt());
        f->setHDFormat(fmt);
    }
}

void SystemMediaTab::handleReprogramAddress()
{
    if (auto* f = fujinon()) {
        const int newId = spinNewAddress->value();
        f->setRS485Address(newId);
    }
}

void SystemMediaTab::handleTerminationToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setRS485Termination(checked);
    }
}

void SystemMediaTab::handleBaudRateChanged(int index)
{
    if (auto* f = fujinon()) {
        f->setBaudRate(cmbBaudRate->itemData(index).toInt());
    }
}

void SystemMediaTab::handleLanguageChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto lang = static_cast<PelcoD::FujinonLanguage>(cmbLanguage->itemData(index).toInt());
        f->setLanguage(lang);
    }
}

void SystemMediaTab::handleSyncRtc()
{
    if (auto* f = fujinon()) {
        const auto now = QDateTime::currentDateTime();
        f->setRTCYear(now.date().year());
        f->setRTCMonthDay(now.date().month(), now.date().day());
        f->setRTCHourMinute(now.time().hour(), now.time().minute());
        f->setRTCSecond(now.time().second());
    }
}

void SystemMediaTab::handleQueryRtc()
{
    if (auto* f = fujinon()) {
        f->queryRTC(0x01);
    }
}

void SystemMediaTab::handleReboot()
{
    if (auto* f = fujinon()) {
        f->reboot();
    }
}

void SystemMediaTab::handleFactoryReset()
{
    if (QMessageBox::warning(this, tr("Confirm Factory Reset"),
            tr("Are you sure you want to restore factory default parameters?"), QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->factoryReset();
        }
    }
}

void SystemMediaTab::handleFormatSD()
{
    if (QMessageBox::warning(this, tr("Confirm SD Format"),
            tr("All recorded movies and logs on the SD card will be lost. Format?"), QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->formatSDCard();
        }
    }
}

void SystemMediaTab::handleRecordLiveToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->recordLiveView(checked);
        btnRecordLive->setText(checked ? tr("Stop LiveView Recording") : tr("Start LiveView Recording"));
    }
}

void SystemMediaTab::handleEncodeModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto enc = static_cast<PelcoD::FujinonVideoEncoding>(cmbEncodeMode->itemData(index).toInt());
        f->setEncodeMode(enc);
    }
}

void SystemMediaTab::handleSdRecordModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonSDRecordMode>(cmbSdRecordMode->itemData(index).toInt());
        f->setSDRecordingMode(mode);
    }
}

void SystemMediaTab::handleSdCapDispToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setSDCapacityDisp(checked);
    }
}

void SystemMediaTab::handleRecordLogData()
{
    if (auto* f = fujinon()) {
        f->recordLogData();
    }
}

void SystemMediaTab::handleQueryLogData()
{
    if (auto* f = fujinon()) {
        const auto logType = static_cast<PelcoD::FujinonLogType>(cmbLogType->currentData().toInt());
        f->queryLogData(logType);
    }
}

void SystemMediaTab::handleSetFirstMovieFile()
{
    if (auto* f = fujinon()) {
        f->setFirstMovieFile(spinMovieFileIndex->value());
    }
}

void SystemMediaTab::handlePlayMovieModeChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonMoviePlayMode>(cmbPlayMovieMode->itemData(index).toInt());
        f->playMovieMode(mode);
    }
}

void SystemMediaTab::handlePlayMovieToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setMovieMode(checked);
        f->playMovie(checked);
        btnPlayMovie->setText(checked ? tr("Stop Playback") : tr("Play Movie"));
    }
}

void SystemMediaTab::handleDeleteMovie()
{
    const int idx = spinMovieFileIndex->value();
    const QString targetStr = (idx == 0) ? tr("ALL movies") : tr("movie file %1").arg(idx);
    if (QMessageBox::warning(this, tr("Confirm Delete"), tr("Are you sure you want to delete %1?").arg(targetStr),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
        if (auto* f = fujinon()) {
            f->deleteMovie(idx);
        }
    }
}

void SystemMediaTab::handleQueryMovieCount()
{
    if (auto* f = fujinon()) {
        f->queryMovieCount();
    }
}

void SystemMediaTab::handleQueryMovieYear()
{
    if (auto* f = fujinon()) {
        f->queryMovieYear(spinMovieFileIndex->value());
    }
}

void SystemMediaTab::handleQueryMovieDate()
{
    if (auto* f = fujinon()) {
        f->queryMovieDate(spinMovieFileIndex->value());
    }
}

void SystemMediaTab::handleQueryMovieTime()
{
    if (auto* f = fujinon()) {
        f->queryMovieTime(spinMovieFileIndex->value());
    }
}

void SystemMediaTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    const QSignalBlocker bTerm(chkTermination);
    chkTermination->setChecked(status.rs485Termination);

    const int langIdx = cmbLanguage->findData(static_cast<int>(status.language));
    if (langIdx >= 0) {
        const QSignalBlocker bLang(cmbLanguage);
        cmbLanguage->setCurrentIndex(langIdx);
    }
}

} // namespace FujinonApp
