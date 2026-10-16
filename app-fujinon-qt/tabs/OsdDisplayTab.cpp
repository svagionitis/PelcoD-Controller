/// @file OsdDisplayTab.cpp
/// @brief Implementation of OsdDisplayTab for OSD overlays, formatting, and menu navigation.

#include "OsdDisplayTab.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace FujinonApp {

OsdDisplayTab::OsdDisplayTab(PelcoDQt::QPelcoDFujinonDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
    setupConnections();
}

PelcoDQt::QFujinonSX800Device* OsdDisplayTab::fujinon() const noexcept
{
    return m_device ? m_device->fujinonDevice() : nullptr;
}

void OsdDisplayTab::setupUi()
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
    // Column 1: On-Screen Overlays & Positions
    // =========================================================================
    auto* col1 = new QVBoxLayout();

    auto* grpOverlays = new QGroupBox(tr("On-Screen Display Overlays"), container);
    auto* gridOverlays = new QGridLayout(grpOverlays);

    auto addOverlayRow = [&](const QString& label, QCheckBox*& chk, QComboBox*& cmb, int row) {
        chk = new QCheckBox(label, grpOverlays);
        cmb = new QComboBox(grpOverlays);
        cmb->addItem(tr("Top-Left"), static_cast<int>(PelcoD::FujinonOSDPosition::TopLeft));
        cmb->addItem(tr("Top-Right"), static_cast<int>(PelcoD::FujinonOSDPosition::TopRight));
        cmb->addItem(tr("Bottom-Left"), static_cast<int>(PelcoD::FujinonOSDPosition::BottomLeft));
        cmb->addItem(tr("Bottom-Right"), static_cast<int>(PelcoD::FujinonOSDPosition::BottomRight));
        gridOverlays->addWidget(chk, row, 0);
        gridOverlays->addWidget(cmb, row, 1);
    };

    addOverlayRow(tr("Time Overlay"), chkTimeDisplay, cmbTimePos, 0);
    addOverlayRow(tr("Camera Title Overlay"), chkTitleDisplay, cmbTitlePos, 1);
    addOverlayRow(tr("Camera ID Overlay"), chkIdDisplay, cmbIdPos, 2);

    chkReticleDisplay = new QCheckBox(tr("Center Crosshair Reticle"), grpOverlays);
    gridOverlays->addWidget(chkReticleDisplay, 3, 0, 1, 2);
    col1->addWidget(grpOverlays);

    // Group 2: Display & Time Formatting
    auto* grpFmt = new QGroupBox(tr("Display & Time Formats"), container);
    auto* gridFmt = new QGridLayout(grpFmt);

    gridFmt->addWidget(new QLabel(tr("Time Display Format:")), 0, 0);
    cmbTimeFormat = new QComboBox(grpFmt);
    cmbTimeFormat->addItem(tr("24-Hour Format"), static_cast<int>(PelcoD::FujinonTimeFormat::Mode24H));
    cmbTimeFormat->addItem(tr("12-Hour Format (AM/PM)"), static_cast<int>(PelcoD::FujinonTimeFormat::Mode12H));
    gridFmt->addWidget(cmbTimeFormat, 0, 1);

    gridFmt->addWidget(new QLabel(tr("Calendar Date Format:")), 1, 0);
    cmbDateFormat = new QComboBox(grpFmt);
    cmbDateFormat->addItem(tr("YYYY-MM-DD"), static_cast<int>(PelcoD::FujinonDateFormat::YMD_Hyphen));
    cmbDateFormat->addItem(tr("MM-DD-YYYY"), static_cast<int>(PelcoD::FujinonDateFormat::MDY_Hyphen));
    cmbDateFormat->addItem(tr("DD-MM-YYYY"), static_cast<int>(PelcoD::FujinonDateFormat::DMY_Hyphen));
    cmbDateFormat->addItem(tr("YYYY/MM/DD"), static_cast<int>(PelcoD::FujinonDateFormat::YMD_Slash));
    cmbDateFormat->addItem(tr("MM/DD/YYYY"), static_cast<int>(PelcoD::FujinonDateFormat::MDY_Slash));
    cmbDateFormat->addItem(tr("DD/MM/YYYY"), static_cast<int>(PelcoD::FujinonDateFormat::DMY_Slash));
    gridFmt->addWidget(cmbDateFormat, 1, 1);

    gridFmt->addWidget(new QLabel(tr("Video Aspect Framing:")), 2, 0);
    cmbVideoDisplay = new QComboBox(grpFmt);
    cmbVideoDisplay->addItem(tr("Side Cut"), static_cast<int>(PelcoD::FujinonVideoDisplayMode::SideCut));
    cmbVideoDisplay->addItem(tr("LetterBox"), static_cast<int>(PelcoD::FujinonVideoDisplayMode::LetterBox));
    cmbVideoDisplay->addItem(tr("Squeeze"), static_cast<int>(PelcoD::FujinonVideoDisplayMode::Squeeze));
    gridFmt->addWidget(cmbVideoDisplay, 2, 1);

    btnQueryDisplay = new QPushButton(tr("Query OSD Display Settings"), grpFmt);
    gridFmt->addWidget(btnQueryDisplay, 3, 0, 1, 2);
    col1->addWidget(grpFmt);
    col1->addStretch();
    mainLayout->addLayout(col1, 1);

    // =========================================================================
    // Column 2: OSD Menu Navigation D-Pad
    // =========================================================================
    auto* col2 = new QVBoxLayout();

    auto* grpMenu = new QGroupBox(tr("OSD Menu Keypad Navigation"), container);
    auto* gridMenu = new QGridLayout(grpMenu);

    btnMenuUp = new QPushButton(tr("▲ Up"), grpMenu);
    btnMenuDown = new QPushButton(tr("▼ Down"), grpMenu);
    btnMenuLeft = new QPushButton(tr("◀ Left"), grpMenu);
    btnMenuRight = new QPushButton(tr("▶ Right"), grpMenu);
    btnMenuOk = new QPushButton(tr("OK / Enter"), grpMenu);
    btnMenuBack = new QPushButton(tr("Back"), grpMenu);

    gridMenu->addWidget(btnMenuUp, 0, 1);
    gridMenu->addWidget(btnMenuLeft, 1, 0);
    gridMenu->addWidget(btnMenuOk, 1, 1);
    gridMenu->addWidget(btnMenuRight, 1, 2);
    gridMenu->addWidget(btnMenuDown, 2, 1);
    gridMenu->addWidget(btnMenuBack, 3, 1);

    btnQueryKeySetting = new QPushButton(tr("Query Menu Key Setting"), grpMenu);
    gridMenu->addWidget(btnQueryKeySetting, 4, 0, 1, 3);
    col2->addWidget(grpMenu);
    col2->addStretch();
    mainLayout->addLayout(col2, 1);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void OsdDisplayTab::setupConnections()
{
    connect(chkTimeDisplay, &QCheckBox::toggled, this, &OsdDisplayTab::handleTimeDisplayToggled);
    connect(
        cmbTimePos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OsdDisplayTab::handleTimePosChanged);

    connect(chkTitleDisplay, &QCheckBox::toggled, this, &OsdDisplayTab::handleTitleDisplayToggled);
    connect(
        cmbTitlePos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OsdDisplayTab::handleTitlePosChanged);

    connect(chkIdDisplay, &QCheckBox::toggled, this, &OsdDisplayTab::handleIdDisplayToggled);
    connect(cmbIdPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OsdDisplayTab::handleIdPosChanged);

    connect(chkReticleDisplay, &QCheckBox::toggled, this, &OsdDisplayTab::handleReticleDisplayToggled);

    connect(cmbTimeFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &OsdDisplayTab::handleTimeFormatChanged);
    connect(cmbDateFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &OsdDisplayTab::handleDateFormatChanged);
    connect(cmbVideoDisplay, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &OsdDisplayTab::handleVideoDisplayChanged);

    connect(btnMenuUp, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuUp);
    connect(btnMenuDown, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuDown);
    connect(btnMenuLeft, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuLeft);
    connect(btnMenuRight, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuRight);
    connect(btnMenuOk, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuOk);
    connect(btnMenuBack, &QPushButton::clicked, this, &OsdDisplayTab::handleMenuBack);

    connect(btnQueryDisplay, &QPushButton::clicked, this, &OsdDisplayTab::handleQueryDisplay);
    connect(btnQueryKeySetting, &QPushButton::clicked, this, &OsdDisplayTab::handleQueryKeySetting);
}

void OsdDisplayTab::handleTimeDisplayToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setTimeDisplay(checked);
    }
}

void OsdDisplayTab::handleTimePosChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto pos = static_cast<PelcoD::FujinonOSDPosition>(cmbTimePos->itemData(index).toInt());
        f->setTimePosition(pos);
    }
}

void OsdDisplayTab::handleTitleDisplayToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setTitleDisplay(checked);
    }
}

void OsdDisplayTab::handleTitlePosChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto pos = static_cast<PelcoD::FujinonOSDPosition>(cmbTitlePos->itemData(index).toInt());
        f->setTitlePosition(pos);
    }
}

void OsdDisplayTab::handleIdDisplayToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setIdDisplay(checked);
    }
}

void OsdDisplayTab::handleIdPosChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto pos = static_cast<PelcoD::FujinonOSDPosition>(cmbIdPos->itemData(index).toInt());
        f->setIdPosition(pos);
    }
}

void OsdDisplayTab::handleReticleDisplayToggled(bool checked)
{
    if (auto* f = fujinon()) {
        f->setReticleDisplay(checked);
    }
}

void OsdDisplayTab::handleTimeFormatChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto fmt = static_cast<PelcoD::FujinonTimeFormat>(cmbTimeFormat->itemData(index).toInt());
        f->setTimeFormat(fmt);
    }
}

void OsdDisplayTab::handleDateFormatChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto fmt = static_cast<PelcoD::FujinonDateFormat>(cmbDateFormat->itemData(index).toInt());
        f->setDateFormat(fmt);
    }
}

void OsdDisplayTab::handleVideoDisplayChanged(int index)
{
    if (auto* f = fujinon()) {
        const auto mode = static_cast<PelcoD::FujinonVideoDisplayMode>(cmbVideoDisplay->itemData(index).toInt());
        f->setVideoDisplay(mode);
    }
}

void OsdDisplayTab::handleMenuUp()
{
    if (auto* f = fujinon()) {
        f->menuDirection(PelcoD::FujinonMenuDirection::Up);
    }
}

void OsdDisplayTab::handleMenuDown()
{
    if (auto* f = fujinon()) {
        f->menuDirection(PelcoD::FujinonMenuDirection::Down);
    }
}

void OsdDisplayTab::handleMenuLeft()
{
    if (auto* f = fujinon()) {
        f->menuDirection(PelcoD::FujinonMenuDirection::Left);
    }
}

void OsdDisplayTab::handleMenuRight()
{
    if (auto* f = fujinon()) {
        f->menuDirection(PelcoD::FujinonMenuDirection::Right);
    }
}

void OsdDisplayTab::handleMenuOk()
{
    if (auto* f = fujinon()) {
        f->menuOk();
    }
}

void OsdDisplayTab::handleMenuBack()
{
    if (auto* f = fujinon()) {
        f->menuBack();
    }
}

void OsdDisplayTab::handleQueryDisplay()
{
    if (auto* f = fujinon()) {
        f->queryDisplay(0x00);
    }
}

void OsdDisplayTab::handleQueryKeySetting()
{
    if (auto* f = fujinon()) {
        f->queryKeySetting(0x00);
    }
}

void OsdDisplayTab::updateFujinonStatus(const PelcoD::FujinonStatus& status)
{
    const QSignalBlocker bTime(chkTimeDisplay);
    chkTimeDisplay->setChecked(status.timeDisplay);

    const QSignalBlocker bTitle(chkTitleDisplay);
    chkTitleDisplay->setChecked(status.titleDisplay);

    const QSignalBlocker bId(chkIdDisplay);
    chkIdDisplay->setChecked(status.idDisplay);

    const QSignalBlocker bRet(chkReticleDisplay);
    chkReticleDisplay->setChecked(status.reticleDisplay);
}

} // namespace FujinonApp
