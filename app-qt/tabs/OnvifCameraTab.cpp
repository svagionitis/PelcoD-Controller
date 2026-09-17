/// @file OnvifCameraTab.cpp
/// @brief Dashboard tab for discovering, connecting to, and controlling ONVIF Profile S IP cameras.

#include "OnvifCameraTab.h"
#include "VideoStreamTab.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSplitter>

namespace PelcoDApp {

OnvifCameraTab::OnvifCameraTab(PelcoD::Qt::QOnvifDevice* onvifDevice, VideoStreamTab* videoTab, QWidget* parent)
    : QWidget(parent)
    , m_onvifDevice(onvifDevice)
    , m_videoTab(videoTab)
{
    setupUi();

    if (m_onvifDevice != nullptr) {
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::discoveryFinished, this,
            &OnvifCameraTab::handleDiscoveryFinished);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::connected, this, &OnvifCameraTab::handleDeviceConnected);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::disconnected, this, &OnvifCameraTab::handleDeviceDisconnected);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::errorOccurred, this, &OnvifCameraTab::handleErrorOccurred);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::streamUriResolved, this,
            &OnvifCameraTab::handleStreamUriResolved);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::snapshotUriResolved, this,
            &OnvifCameraTab::handleSnapshotUriResolved);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::presetsUpdated, this, &OnvifCameraTab::handlePresetsUpdated);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::presetToursUpdated, this, &OnvifCameraTab::handleToursUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::statusUpdated, this, &OnvifCameraTab::handleStatusUpdated);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::rebootCompleted, this, &OnvifCameraTab::handleRebootCompleted);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::imagingSettingsUpdated, this,
            &OnvifCameraTab::handleImagingSettingsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::eventReceived, this, &OnvifCameraTab::handleEventReceived);
    }

    updateConnectionUi(false);
}

void OnvifCameraTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // =========================================================================
    // Top Section: Compact Discovery & Authentication Banner
    // =========================================================================
    auto* groupDiscovery = new QGroupBox(tr("ONVIF Discovery & Authentication"), this);
    auto* gridDisc = new QGridLayout(groupDiscovery);
    gridDisc->setContentsMargins(8, 6, 8, 6);
    gridDisc->setSpacing(6);

    btnDiscover = new QPushButton(tr("🔍 Discover Cameras"), groupDiscovery);
    btnDiscover->setToolTip(tr("Send WS-Discovery multicast probe on LAN (port 3702)"));
    cmbDiscovered = new QComboBox(groupDiscovery);
    cmbDiscovered->addItem(tr("-- Select Discovered Device --"), -1);

    lblConnectionStatus = new QLabel(tr("Disconnected"), groupDiscovery);
    lblConnectionStatus->setStyleSheet(QStringLiteral("color: #8b949e; font-weight: bold; padding: 2px 8px;"));

    gridDisc->addWidget(btnDiscover, 0, 0);
    gridDisc->addWidget(cmbDiscovered, 0, 1, 1, 2);
    gridDisc->addWidget(lblConnectionStatus, 0, 3, Qt::AlignRight);

    auto* connRow = new QHBoxLayout();
    connRow->setSpacing(6);
    connRow->addWidget(new QLabel(tr("Endpoint:"), groupDiscovery));
    editEndpoint = new QLineEdit(groupDiscovery);
    editEndpoint->setPlaceholderText(QStringLiteral("http://192.168.1.100/onvif/device_service"));
    editEndpoint->setText(QStringLiteral("http://192.168.1.100/onvif/device_service"));
    connRow->addWidget(editEndpoint, 1);

    connRow->addWidget(new QLabel(tr("User:"), groupDiscovery));
    editUsername = new QLineEdit(groupDiscovery);
    editUsername->setText(QStringLiteral("admin"));
    editUsername->setMaximumWidth(110);
    connRow->addWidget(editUsername);

    connRow->addWidget(new QLabel(tr("Pass:"), groupDiscovery));
    editPassword = new QLineEdit(groupDiscovery);
    editPassword->setEchoMode(QLineEdit::Password);
    editPassword->setMaximumWidth(110);
    connRow->addWidget(editPassword);

    btnConnect = new QPushButton(tr("Connect"), groupDiscovery);
    btnConnect->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #238636; color: white; padding: 4px 12px; }"));
    btnDisconnect = new QPushButton(tr("Disconnect"), groupDiscovery);
    connRow->addWidget(btnConnect);
    connRow->addWidget(btnDisconnect);

    gridDisc->addLayout(connRow, 1, 0, 1, 4);

    mainLayout->addWidget(groupDiscovery);

    // =========================================================================
    // Central Categorized Sub-Tabs
    // =========================================================================
    m_cameraTabs = new QTabWidget(this);
    m_cameraTabs->setObjectName(QStringLiteral("onvifCameraTabs"));

    auto createScrollTab = [this](const QString& title) -> QVBoxLayout* {
        auto* pageContainer = new QWidget();
        auto* pageLayout = new QVBoxLayout(pageContainer);
        pageLayout->setContentsMargins(8, 8, 8, 8);
        pageLayout->setSpacing(6);

        auto* scrollArea = new QScrollArea(m_cameraTabs);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setWidget(pageContainer);

        m_cameraTabs->addTab(scrollArea, title);
        return pageLayout;
    };

    // -------------------------------------------------------------------------
    // Sub-Tab 1: PTZ Control
    // -------------------------------------------------------------------------
    auto* ptzTabLayout = createScrollTab(tr("PTZ Control"));

    auto* groupPtz = new QGroupBox(tr("PTZ Motion & Positioning"));
    auto* ptzLayout = new QVBoxLayout(groupPtz);
    ptzLayout->setSpacing(6);

    // Continuous Jog Grid
    auto* jogGrid = new QGridLayout();
    jogGrid->setSpacing(4);

    auto makeJogBtn = [this](const QString& text, double pan, double tilt) -> QPushButton* {
        auto* b = new QPushButton(text, this);
        b->setFixedSize(50, 40);
        connect(b, &QPushButton::pressed, this, [this, pan, tilt]() { handleContinuousPanTilt(pan, tilt); });
        connect(b, &QPushButton::released, this, &OnvifCameraTab::handleStopMotion);
        return b;
    };

    jogGrid->addWidget(makeJogBtn(QStringLiteral("↖"), -1.0, 1.0), 0, 0);
    jogGrid->addWidget(makeJogBtn(QStringLiteral("▲"), 0.0, 1.0), 0, 1);
    jogGrid->addWidget(makeJogBtn(QStringLiteral("↗"), 1.0, 1.0), 0, 2);

    jogGrid->addWidget(makeJogBtn(QStringLiteral("◀"), -1.0, 0.0), 1, 0);
    auto* btnStop = new QPushButton(tr("STOP"), groupPtz);
    btnStop->setFixedSize(50, 40);
    btnStop->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #da3633; color: white; }"));
    connect(btnStop, &QPushButton::clicked, this, &OnvifCameraTab::handleStopMotion);
    jogGrid->addWidget(btnStop, 1, 1);
    jogGrid->addWidget(makeJogBtn(QStringLiteral("▶"), 1.0, 0.0), 1, 2);

    jogGrid->addWidget(makeJogBtn(QStringLiteral("↙"), -1.0, -1.0), 2, 0);
    jogGrid->addWidget(makeJogBtn(QStringLiteral("▼"), 0.0, -1.0), 2, 1);
    jogGrid->addWidget(makeJogBtn(QStringLiteral("↘"), 1.0, -1.0), 2, 2);

    // Zoom buttons
    auto* btnZoomIn = new QPushButton(tr("Zoom Tele (+)"), groupPtz);
    auto* btnZoomOut = new QPushButton(tr("Zoom Wide (-)"), groupPtz);
    connect(btnZoomIn, &QPushButton::pressed, this, [this]() { handleContinuousZoom(1); });
    connect(btnZoomIn, &QPushButton::released, this, &OnvifCameraTab::handleStopMotion);
    connect(btnZoomOut, &QPushButton::pressed, this, [this]() { handleContinuousZoom(-1); });
    connect(btnZoomOut, &QPushButton::released, this, &OnvifCameraTab::handleStopMotion);

    auto* zoomLayout = new QHBoxLayout();
    zoomLayout->addWidget(btnZoomIn);
    zoomLayout->addWidget(btnZoomOut);

    // Speed Slider
    auto* speedLayout = new QHBoxLayout();
    speedLayout->addWidget(new QLabel(tr("Velocity:"), groupPtz));
    sliderSpeed = new QSlider(Qt::Horizontal, groupPtz);
    sliderSpeed->setRange(1, 10);
    sliderSpeed->setValue(5);
    lblSpeedVal = new QLabel(QStringLiteral("0.5"), groupPtz);
    connect(sliderSpeed, &QSlider::valueChanged, this,
        [this](int val) { lblSpeedVal->setText(QString::number(val / 10.0, 'f', 1)); });
    speedLayout->addWidget(sliderSpeed);
    speedLayout->addWidget(lblSpeedVal);

    // Relative Step Move
    auto* relLayout = new QHBoxLayout();
    relLayout->addWidget(new QLabel(tr("Step Pan:"), groupPtz));
    spinRelPan = new QDoubleSpinBox(groupPtz);
    spinRelPan->setRange(-1.0, 1.0);
    spinRelPan->setSingleStep(0.05);
    spinRelPan->setValue(0.1);

    relLayout->addWidget(spinRelPan);
    relLayout->addWidget(new QLabel(tr("Tilt:"), groupPtz));
    spinRelTilt = new QDoubleSpinBox(groupPtz);
    spinRelTilt->setRange(-1.0, 1.0);
    spinRelTilt->setSingleStep(0.05);
    spinRelTilt->setValue(0.05);
    relLayout->addWidget(spinRelTilt);

    relLayout->addWidget(new QLabel(tr("Zoom:"), groupPtz));
    spinRelZoom = new QDoubleSpinBox(groupPtz);
    spinRelZoom->setRange(-1.0, 1.0);
    spinRelZoom->setSingleStep(0.05);
    spinRelZoom->setValue(0.0);
    relLayout->addWidget(spinRelZoom);

    btnRelMove = new QPushButton(tr("Step"), groupPtz);
    connect(btnRelMove, &QPushButton::clicked, this, &OnvifCameraTab::handleRelativeMove);
    relLayout->addWidget(btnRelMove);

    // Home Position
    auto* homeLayout = new QHBoxLayout();
    btnGotoHome = new QPushButton(tr("⌂ Go To Home"), groupPtz);
    btnSetHome = new QPushButton(tr("Set As Home"), groupPtz);
    connect(btnGotoHome, &QPushButton::clicked, this, &OnvifCameraTab::handleGotoHome);
    connect(btnSetHome, &QPushButton::clicked, this, &OnvifCameraTab::handleSetHome);
    homeLayout->addWidget(btnGotoHome);
    homeLayout->addWidget(btnSetHome);

    // Telemetry display
    auto* telemLayout = new QHBoxLayout();
    lblTelemetryPanTilt = new QLabel(tr("Pan/Tilt: (0.00, 0.00)"), groupPtz);
    lblTelemetryZoom = new QLabel(tr("Zoom: 0.00"), groupPtz);
    lblTelemetryMoving = new QLabel(tr("Status: IDLE"), groupPtz);
    telemLayout->addWidget(lblTelemetryPanTilt);
    telemLayout->addWidget(lblTelemetryZoom);
    telemLayout->addWidget(lblTelemetryMoving);

    ptzLayout->addLayout(jogGrid);
    ptzLayout->addLayout(zoomLayout);
    ptzLayout->addLayout(speedLayout);
    ptzLayout->addLayout(relLayout);
    ptzLayout->addLayout(homeLayout);
    ptzLayout->addLayout(telemLayout);

    ptzTabLayout->addWidget(groupPtz);
    ptzTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 2: Presets
    // -------------------------------------------------------------------------
    auto* presetsTabLayout = createScrollTab(tr("Presets"));

    auto* groupPresets = new QGroupBox(tr("Stored Camera Presets"));
    auto* presetsLayout = new QVBoxLayout(groupPresets);
    presetsLayout->setSpacing(6);

    tablePresets = new QTableWidget(0, 2, groupPresets);
    tablePresets->setHorizontalHeaderLabels({ tr("Token"), tr("Label / Name") });
    tablePresets->horizontalHeader()->setStretchLastSection(true);
    tablePresets->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePresets->setSelectionMode(QAbstractItemView::SingleSelection);
    tablePresets->setMinimumHeight(200);

    auto* presetControls = new QHBoxLayout();
    btnRefreshPresets = new QPushButton(tr("Refresh"), groupPresets);
    btnGotoPreset = new QPushButton(tr("Go To Preset"), groupPresets);
    editPresetName = new QLineEdit(groupPresets);
    editPresetName->setPlaceholderText(tr("New preset name..."));
    btnSavePreset = new QPushButton(tr("Save Preset"), groupPresets);
    btnDeletePreset = new QPushButton(tr("Delete"), groupPresets);

    connect(btnRefreshPresets, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshPresets);
    connect(btnGotoPreset, &QPushButton::clicked, this, &OnvifCameraTab::handleGotoPreset);
    connect(btnSavePreset, &QPushButton::clicked, this, &OnvifCameraTab::handleSavePreset);
    connect(btnDeletePreset, &QPushButton::clicked, this, &OnvifCameraTab::handleDeletePreset);

    presetControls->addWidget(btnRefreshPresets);
    presetControls->addWidget(btnGotoPreset);
    presetControls->addWidget(editPresetName);
    presetControls->addWidget(btnSavePreset);
    presetControls->addWidget(btnDeletePreset);

    presetsLayout->addWidget(tablePresets);
    presetsLayout->addLayout(presetControls);

    // Preset Tours / Patrols Group
    auto* groupTours = new QGroupBox(tr("Preset Tours / Patrols (Profile S)"), groupPresets);
    auto* toursLayout = new QVBoxLayout(groupTours);
    toursLayout->setSpacing(6);

    auto* tourSelectorLayout = new QHBoxLayout();
    cmbPresetTours = new QComboBox(groupTours);
    cmbPresetTours->setMinimumWidth(180);
    btnRefreshTours = new QPushButton(tr("Refresh Tours"), groupTours);
    btnStartTour = new QPushButton(tr("▶ Start Tour"), groupTours);
    btnPauseTour = new QPushButton(tr("⏸ Pause"), groupTours);
    btnStopTour = new QPushButton(tr("⏹ Stop"), groupTours);
    lblTourStatus = new QLabel(tr("Status: Idle"), groupTours);
    lblTourStatus->setStyleSheet("font-weight: bold; color: #8b949e;");

    tourSelectorLayout->addWidget(new QLabel(tr("Tour:"), groupTours));
    tourSelectorLayout->addWidget(cmbPresetTours);
    tourSelectorLayout->addWidget(btnRefreshTours);
    tourSelectorLayout->addWidget(btnStartTour);
    tourSelectorLayout->addWidget(btnPauseTour);
    tourSelectorLayout->addWidget(btnStopTour);
    tourSelectorLayout->addWidget(lblTourStatus);
    tourSelectorLayout->addStretch();

    tableTourSpots = new QTableWidget(0, 3, groupTours);
    tableTourSpots->setHorizontalHeaderLabels({ tr("Preset Token"), tr("Speed (0.0 - 1.0)"), tr("Stay Time (sec)") });
    tableTourSpots->horizontalHeader()->setStretchLastSection(true);
    tableTourSpots->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableTourSpots->setSelectionMode(QAbstractItemView::SingleSelection);
    tableTourSpots->setMinimumHeight(150);

    auto* tourStepControls = new QHBoxLayout();
    btnAddTourStep = new QPushButton(tr("Add Step"), groupTours);
    btnRemoveTourStep = new QPushButton(tr("Remove Step"), groupTours);
    btnSaveTour = new QPushButton(tr("Save Tour Modifications"), groupTours);

    tourStepControls->addWidget(btnAddTourStep);
    tourStepControls->addWidget(btnRemoveTourStep);
    tourStepControls->addWidget(btnSaveTour);
    tourStepControls->addStretch();

    toursLayout->addLayout(tourSelectorLayout);
    toursLayout->addWidget(tableTourSpots);
    toursLayout->addLayout(tourStepControls);

    connect(btnRefreshTours, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshTours);
    connect(
        cmbPresetTours, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OnvifCameraTab::handleTourSelected);
    connect(btnStartTour, &QPushButton::clicked, this, &OnvifCameraTab::handleStartTour);
    connect(btnPauseTour, &QPushButton::clicked, this, &OnvifCameraTab::handlePauseTour);
    connect(btnStopTour, &QPushButton::clicked, this, &OnvifCameraTab::handleStopTour);
    connect(btnAddTourStep, &QPushButton::clicked, this, &OnvifCameraTab::handleAddTourStep);
    connect(btnRemoveTourStep, &QPushButton::clicked, this, &OnvifCameraTab::handleRemoveTourStep);
    connect(btnSaveTour, &QPushButton::clicked, this, &OnvifCameraTab::handleSaveTour);

    presetsTabLayout->addWidget(groupPresets);
    presetsTabLayout->addWidget(groupTours);
    presetsTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 3: Imaging (Profile T)
    // -------------------------------------------------------------------------
    auto* imagingTabLayout = createScrollTab(tr("Imaging"));

    auto* groupImaging = new QGroupBox(tr("Profile T: Optical & Imaging Controls"));
    auto* imgLayout = new QGridLayout(groupImaging);
    imgLayout->setSpacing(6);

    // Brightness
    imgLayout->addWidget(new QLabel(tr("Brightness:"), groupImaging), 0, 0);
    sliderBrightness = new QSlider(Qt::Horizontal, groupImaging);
    sliderBrightness->setRange(0, 100);
    sliderBrightness->setValue(50);
    lblBrightnessVal = new QLabel(QStringLiteral("50"), groupImaging);
    lblBrightnessVal->setFixedWidth(28);
    connect(sliderBrightness, &QSlider::valueChanged, this,
        [this](int v) { lblBrightnessVal->setText(QString::number(v)); });
    imgLayout->addWidget(sliderBrightness, 0, 1);
    imgLayout->addWidget(lblBrightnessVal, 0, 2);

    // Contrast
    imgLayout->addWidget(new QLabel(tr("Contrast:"), groupImaging), 1, 0);
    sliderContrast = new QSlider(Qt::Horizontal, groupImaging);
    sliderContrast->setRange(0, 100);
    sliderContrast->setValue(50);
    lblContrastVal = new QLabel(QStringLiteral("50"), groupImaging);
    lblContrastVal->setFixedWidth(28);
    connect(
        sliderContrast, &QSlider::valueChanged, this, [this](int v) { lblContrastVal->setText(QString::number(v)); });
    imgLayout->addWidget(sliderContrast, 1, 1);
    imgLayout->addWidget(lblContrastVal, 1, 2);

    // Saturation
    imgLayout->addWidget(new QLabel(tr("Saturation:"), groupImaging), 2, 0);
    sliderSaturation = new QSlider(Qt::Horizontal, groupImaging);
    sliderSaturation->setRange(0, 100);
    sliderSaturation->setValue(50);
    lblSaturationVal = new QLabel(QStringLiteral("50"), groupImaging);
    lblSaturationVal->setFixedWidth(28);
    connect(sliderSaturation, &QSlider::valueChanged, this,
        [this](int v) { lblSaturationVal->setText(QString::number(v)); });
    imgLayout->addWidget(sliderSaturation, 2, 1);
    imgLayout->addWidget(lblSaturationVal, 2, 2);

    // Sharpness
    imgLayout->addWidget(new QLabel(tr("Sharpness:"), groupImaging), 3, 0);
    sliderSharpness = new QSlider(Qt::Horizontal, groupImaging);
    sliderSharpness->setRange(0, 100);
    sliderSharpness->setValue(50);
    lblSharpnessVal = new QLabel(QStringLiteral("50"), groupImaging);
    lblSharpnessVal->setFixedWidth(28);
    connect(
        sliderSharpness, &QSlider::valueChanged, this, [this](int v) { lblSharpnessVal->setText(QString::number(v)); });
    imgLayout->addWidget(sliderSharpness, 3, 1);
    imgLayout->addWidget(lblSharpnessVal, 3, 2);

    // IR Filter
    imgLayout->addWidget(new QLabel(tr("IR Filter:"), groupImaging), 4, 0);
    cmbIrFilter = new QComboBox(groupImaging);
    cmbIrFilter->addItems({ QStringLiteral("AUTO"), QStringLiteral("ON"), QStringLiteral("OFF") });
    imgLayout->addWidget(cmbIrFilter, 4, 1, 1, 2);

    // BLC / WDR
    auto* chkLayout = new QHBoxLayout();
    chkBacklight = new QCheckBox(tr("Backlight Compensation (BLC)"), groupImaging);
    chkWdr = new QCheckBox(tr("Wide Dynamic Range (WDR)"), groupImaging);
    chkLayout->addWidget(chkBacklight);
    chkLayout->addWidget(chkWdr);
    imgLayout->addLayout(chkLayout, 5, 0, 1, 3);

    // Focus controls
    auto* focusLayout = new QHBoxLayout();
    cmbAutoFocus = new QComboBox(groupImaging);
    cmbAutoFocus->addItems({ QStringLiteral("AUTO"), QStringLiteral("MANUAL") });
    btnFocusNear = new QPushButton(tr("Focus Near"), groupImaging);
    btnFocusFar = new QPushButton(tr("Focus Far"), groupImaging);
    connect(btnFocusNear, &QPushButton::pressed, this, &OnvifCameraTab::handleFocusNear);
    connect(btnFocusNear, &QPushButton::released, this, &OnvifCameraTab::handleFocusStop);
    connect(btnFocusFar, &QPushButton::pressed, this, &OnvifCameraTab::handleFocusFar);
    connect(btnFocusFar, &QPushButton::released, this, &OnvifCameraTab::handleFocusStop);

    focusLayout->addWidget(new QLabel(tr("Focus:"), groupImaging));
    focusLayout->addWidget(cmbAutoFocus);
    focusLayout->addWidget(btnFocusNear);
    focusLayout->addWidget(btnFocusFar);
    imgLayout->addLayout(focusLayout, 6, 0, 1, 3);

    // Action buttons
    auto* imgBtnLayout = new QHBoxLayout();
    btnRefreshImaging = new QPushButton(tr("Refresh"), groupImaging);
    btnApplyImaging = new QPushButton(tr("Apply Settings"), groupImaging);
    btnApplyImaging->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #238636; color: white; }"));
    connect(btnRefreshImaging, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshImaging);
    connect(btnApplyImaging, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyImaging);
    imgBtnLayout->addWidget(btnRefreshImaging);
    imgBtnLayout->addWidget(btnApplyImaging);
    imgLayout->addLayout(imgBtnLayout, 7, 0, 1, 3);

    imagingTabLayout->addWidget(groupImaging);
    imagingTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 4: Events (Profile T)
    // -------------------------------------------------------------------------
    auto* eventsTabLayout = createScrollTab(tr("Events"));

    auto* groupEvents = new QGroupBox(tr("Profile T: Live Event Monitor"));
    auto* eventsLayout = new QVBoxLayout(groupEvents);
    eventsLayout->setSpacing(6);

    auto* eventHeaderLayout = new QHBoxLayout();
    btnToggleEvents = new QPushButton(tr("▶ Subscribe Events"), groupEvents);
    btnClearEvents = new QPushButton(tr("Clear"), groupEvents);
    connect(btnToggleEvents, &QPushButton::clicked, this, [this]() {
        const bool active = m_onvifDevice ? m_onvifDevice->isEventSubscriptionActive() : false;
        handleToggleEvents(!active);
    });
    connect(btnClearEvents, &QPushButton::clicked, this, &OnvifCameraTab::handleClearEvents);
    eventHeaderLayout->addWidget(btnToggleEvents);
    eventHeaderLayout->addWidget(btnClearEvents);
    eventHeaderLayout->addStretch();
    eventsLayout->addLayout(eventHeaderLayout);

    tableEvents = new QTableWidget(0, 4, groupEvents);
    tableEvents->setHorizontalHeaderLabels({ tr("Time"), tr("Topic"), tr("Item"), tr("Value") });
    tableEvents->horizontalHeader()->setStretchLastSection(true);
    tableEvents->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableEvents->setMinimumHeight(200);
    eventsLayout->addWidget(tableEvents);

    eventsTabLayout->addWidget(groupEvents);
    eventsTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 5: Device & Streams
    // -------------------------------------------------------------------------
    auto* devTabLayout = createScrollTab(tr("Device & Streams"));

    auto* groupStreams = new QGroupBox(tr("Media Profiles & Streaming"));
    auto* streamGrid = new QGridLayout(groupStreams);
    streamGrid->setSpacing(6);

    streamGrid->addWidget(new QLabel(tr("Media Profile:"), groupStreams), 0, 0);
    cmbProfiles = new QComboBox(groupStreams);
    streamGrid->addWidget(cmbProfiles, 0, 1);

    streamGrid->addWidget(new QLabel(tr("RTSP URI:"), groupStreams), 1, 0);
    editRtspUri = new QLineEdit(groupStreams);
    editRtspUri->setReadOnly(true);
    streamGrid->addWidget(editRtspUri, 1, 1);

    auto* rtspBtnLayout = new QHBoxLayout();
    btnCopyRtsp = new QPushButton(tr("Copy RTSP"), groupStreams);
    btnStreamInVideoTab = new QPushButton(tr("▶ Open in Video Tab"), groupStreams);
    btnStreamInVideoTab->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #1f6feb; color: white; }"));
    rtspBtnLayout->addWidget(btnCopyRtsp);
    rtspBtnLayout->addWidget(btnStreamInVideoTab);
    rtspBtnLayout->addStretch();
    streamGrid->addLayout(rtspBtnLayout, 2, 1);

    streamGrid->addWidget(new QLabel(tr("Snapshot URI:"), groupStreams), 3, 0);
    editSnapshotUri = new QLineEdit(groupStreams);
    editSnapshotUri->setReadOnly(true);
    streamGrid->addWidget(editSnapshotUri, 3, 1);

    devTabLayout->addWidget(groupStreams);

    auto* groupInfo = new QGroupBox(tr("Device Identification"));
    auto* infoGrid = new QGridLayout(groupInfo);
    infoGrid->setSpacing(6);

    infoGrid->addWidget(new QLabel(tr("Manufacturer:"), groupInfo), 0, 0);
    lblManufacturer = new QLabel(tr("-"), groupInfo);
    infoGrid->addWidget(lblManufacturer, 0, 1);

    infoGrid->addWidget(new QLabel(tr("Model:"), groupInfo), 1, 0);
    lblModel = new QLabel(tr("-"), groupInfo);
    infoGrid->addWidget(lblModel, 1, 1);

    infoGrid->addWidget(new QLabel(tr("Firmware:"), groupInfo), 2, 0);
    lblFirmware = new QLabel(tr("-"), groupInfo);
    infoGrid->addWidget(lblFirmware, 2, 1);

    infoGrid->addWidget(new QLabel(tr("Serial No:"), groupInfo), 3, 0);
    lblSerial = new QLabel(tr("-"), groupInfo);
    infoGrid->addWidget(lblSerial, 3, 1);

    infoGrid->addWidget(new QLabel(tr("Hardware ID:"), groupInfo), 4, 0);
    lblHardwareId = new QLabel(tr("-"), groupInfo);
    infoGrid->addWidget(lblHardwareId, 4, 1);

    btnReboot = new QPushButton(tr("⚠ Reboot Camera"), groupInfo);
    btnReboot->setStyleSheet(QStringLiteral("QPushButton { color: #f85149; }"));
    btnReboot->setMaximumWidth(160);
    infoGrid->addWidget(btnReboot, 5, 0, 1, 2);

    devTabLayout->addWidget(groupInfo);
    devTabLayout->addStretch();

    // Add tab widget to main layout
    mainLayout->addWidget(m_cameraTabs, 1);

    // Connections
    connect(btnDiscover, &QPushButton::clicked, this, &OnvifCameraTab::handleStartDiscovery);
    connect(cmbDiscovered, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &OnvifCameraTab::handleSelectDiscovered);
    connect(btnConnect, &QPushButton::clicked, this, &OnvifCameraTab::handleConnect);
    connect(btnDisconnect, &QPushButton::clicked, this, &OnvifCameraTab::handleDisconnect);
    connect(
        cmbProfiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OnvifCameraTab::handleProfileSelected);
    connect(btnCopyRtsp, &QPushButton::clicked, this, &OnvifCameraTab::handleCopyRtsp);
    connect(btnStreamInVideoTab, &QPushButton::clicked, this, &OnvifCameraTab::handleSendToVideoTab);
    connect(btnReboot, &QPushButton::clicked, this, &OnvifCameraTab::handleRebootCamera);
}

void OnvifCameraTab::updateConnectionUi(bool connected)
{
    btnConnect->setEnabled(!connected);
    btnDisconnect->setEnabled(connected);
    btnReboot->setEnabled(connected);
    cmbProfiles->setEnabled(connected);
    btnGotoHome->setEnabled(connected);
    btnSetHome->setEnabled(connected);
    btnRelMove->setEnabled(connected);
    btnRefreshPresets->setEnabled(connected);
    btnGotoPreset->setEnabled(connected);
    btnSavePreset->setEnabled(connected);
    btnDeletePreset->setEnabled(connected);

    // Tour widgets
    cmbPresetTours->setEnabled(connected);
    btnRefreshTours->setEnabled(connected);
    btnStartTour->setEnabled(connected);
    btnPauseTour->setEnabled(connected);
    btnStopTour->setEnabled(connected);
    tableTourSpots->setEnabled(connected);
    btnAddTourStep->setEnabled(connected);
    btnRemoveTourStep->setEnabled(connected);
    btnSaveTour->setEnabled(connected);

    // Profile T widgets
    sliderBrightness->setEnabled(connected);
    sliderContrast->setEnabled(connected);
    sliderSaturation->setEnabled(connected);
    sliderSharpness->setEnabled(connected);
    cmbIrFilter->setEnabled(connected);
    chkBacklight->setEnabled(connected);
    chkWdr->setEnabled(connected);
    cmbAutoFocus->setEnabled(connected);
    btnFocusNear->setEnabled(connected);
    btnFocusFar->setEnabled(connected);
    btnRefreshImaging->setEnabled(connected);
    btnApplyImaging->setEnabled(connected);
    btnToggleEvents->setEnabled(connected);
    btnClearEvents->setEnabled(connected);

    if (connected) {
        lblConnectionStatus->setText(tr("Connected"));
        lblConnectionStatus->setStyleSheet("color: #7ee787; font-weight: bold;");
    } else {
        lblConnectionStatus->setText(tr("Disconnected"));
        lblConnectionStatus->setStyleSheet("color: #8b949e; font-weight: bold;");
        lblManufacturer->setText("-");
        lblModel->setText("-");
        lblFirmware->setText("-");
        lblSerial->setText("-");
        lblHardwareId->setText("-");
        cmbProfiles->clear();
        editRtspUri->clear();
        editSnapshotUri->clear();
        tablePresets->setRowCount(0);
        tableTourSpots->setRowCount(0);
        cmbPresetTours->clear();
        lblTourStatus->setText(tr("Status: Idle"));
        lblTourStatus->setStyleSheet("font-weight: bold; color: #8b949e;");
        tableEvents->setRowCount(0);
        btnToggleEvents->setText(tr("▶ Subscribe Events"));
        btnToggleEvents->setStyleSheet("");
        lblTelemetryPanTilt->setText(tr("Pan/Tilt: (0.00, 0.00)"));
        lblTelemetryZoom->setText(tr("Zoom: 0.00"));
        lblTelemetryMoving->setText(tr("Status: IDLE"));
    }
}

void OnvifCameraTab::handleStartDiscovery()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    btnDiscover->setEnabled(false);
    btnDiscover->setText(tr("Discovering..."));
    m_onvifDevice->discoverCamerasAsync(2000);
}

void OnvifCameraTab::handleDiscoveryFinished(const QList<PelcoD::Onvif::DiscoveredDevice>& devices)
{
    btnDiscover->setEnabled(true);
    btnDiscover->setText(tr("🔍 Discover Cameras"));

    m_discoveredList = devices;
    cmbDiscovered->clear();
    cmbDiscovered->addItem(tr("-- Select Discovered Device (%1 found) --").arg(devices.size()), -1);

    for (qsizetype i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        QString label = QString::fromStdString(d.ip);
        if (!d.hardware.empty()) {
            label += QString(" (%1)").arg(QString::fromStdString(d.hardware));
        } else if (!d.name.empty()) {
            label += QString(" (%1)").arg(QString::fromStdString(d.name));
        }
        cmbDiscovered->addItem(label, static_cast<int>(i));
    }

    if (!devices.empty()) {
        cmbDiscovered->setCurrentIndex(1);
    }
}

void OnvifCameraTab::handleSelectDiscovered(int index)
{
    Q_UNUSED(index);
    const int devIdx = cmbDiscovered->currentData().toInt();
    if (devIdx >= 0 && devIdx < m_discoveredList.size()) {
        const auto& d = m_discoveredList[devIdx];
        editEndpoint->setText(QString::fromStdString(d.endpoint));
    }
}

void OnvifCameraTab::handleConnect()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString endpoint = editEndpoint->text().trimmed();
    const QString user = editUsername->text().trimmed();
    const QString pass = editPassword->text();

    if (endpoint.isEmpty()) {
        QMessageBox::warning(this, tr("Connection Error"), tr("Please enter an ONVIF device service endpoint URL."));
        return;
    }

    btnConnect->setEnabled(false);
    lblConnectionStatus->setText(tr("Connecting..."));
    lblConnectionStatus->setStyleSheet("color: #d29922; font-weight: bold;");

    const bool ok = m_onvifDevice->connectToCamera(endpoint, user, pass);
    if (!ok) {
        updateConnectionUi(false);
    }
}

void OnvifCameraTab::handleDisconnect()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->disconnectFromCamera();
    }
}

void OnvifCameraTab::handleDeviceConnected(const QString& endpoint, const QString& model)
{
    Q_UNUSED(endpoint)
    Q_UNUSED(model)

    updateConnectionUi(true);

    if (m_onvifDevice != nullptr) {
        const auto info = m_onvifDevice->deviceInformation();
        lblManufacturer->setText(QString::fromStdString(info.manufacturer));
        lblModel->setText(QString::fromStdString(info.model));
        lblFirmware->setText(QString::fromStdString(info.firmwareVersion));
        lblSerial->setText(QString::fromStdString(info.serialNumber));
        lblHardwareId->setText(QString::fromStdString(info.hardwareId));

        cmbProfiles->clear();
        const auto profiles = m_onvifDevice->profiles();
        for (const auto& p : profiles) {
            const QString label = QString("%1 (%2x%3 %4)")
                                      .arg(QString::fromStdString(p.name))
                                      .arg(p.videoWidth)
                                      .arg(p.videoHeight)
                                      .arg(QString::fromStdString(p.videoEncoding));
            cmbProfiles->addItem(label, QString::fromStdString(p.token));
        }

        if (!profiles.empty()) {
            cmbProfiles->setCurrentIndex(0);
        }
    }
}

void OnvifCameraTab::handleDeviceDisconnected()
{
    updateConnectionUi(false);
}

void OnvifCameraTab::handleErrorOccurred(const QString& message)
{
    lblConnectionStatus->setText(tr("Error"));
    lblConnectionStatus->setStyleSheet("color: #f85149; font-weight: bold;");
    QMessageBox::warning(this, tr("ONVIF Error"), message);
}

void OnvifCameraTab::handleProfileSelected(int index)
{
    if (m_onvifDevice == nullptr || index < 0) {
        return;
    }
    const QString token = cmbProfiles->currentData().toString();
    if (!token.isEmpty()) {
        m_onvifDevice->setActiveProfile(token);
    }
}

void OnvifCameraTab::handleStreamUriResolved(const QString& uri)
{
    editRtspUri->setText(uri);
}

void OnvifCameraTab::handleSnapshotUriResolved(const QString& uri)
{
    editSnapshotUri->setText(uri);
}

void OnvifCameraTab::handleCopyRtsp()
{
    const QString uri = editRtspUri->text().trimmed();
    if (!uri.isEmpty()) {
        QApplication::clipboard()->setText(uri);
    }
}

void OnvifCameraTab::handleSendToVideoTab()
{
    const QString uri = editRtspUri->text().trimmed();
    if (!uri.isEmpty()) {
        emit streamUriSelected(uri);
    }
}

void OnvifCameraTab::handleContinuousPanTilt(double pan, double tilt)
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const double speed = sliderSpeed->value() / 10.0;
    m_onvifDevice->move(pan * speed, tilt * speed);
}

void OnvifCameraTab::handleContinuousZoom(int dir)
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const double speed = sliderSpeed->value() / 10.0;
    m_onvifDevice->zoom(dir, speed);
}

void OnvifCameraTab::handleStopMotion()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->stopMotion(true, true);
    }
}

void OnvifCameraTab::handleRelativeMove()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    m_onvifDevice->relativeMove(spinRelPan->value(), spinRelTilt->value(), spinRelZoom->value());
}

void OnvifCameraTab::handleGotoHome()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->gotoHomePosition();
    }
}

void OnvifCameraTab::handleSetHome()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->setHomePosition();
    }
}

void OnvifCameraTab::handleRefreshPresets()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshPresets();
    }
}

void OnvifCameraTab::handleGotoPreset()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const int row = tablePresets->currentRow();
    if (row >= 0) {
        const QString token = tablePresets->item(row, 0)->text();
        m_onvifDevice->gotoPreset(token);
    }
}

void OnvifCameraTab::handleSavePreset()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString name = editPresetName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::information(this, tr("Preset Name"), tr("Please enter a name for the new preset."));
        return;
    }
    m_onvifDevice->setPreset(name);
    editPresetName->clear();
}

void OnvifCameraTab::handleDeletePreset()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const int row = tablePresets->currentRow();
    if (row >= 0) {
        const QString token = tablePresets->item(row, 0)->text();
        m_onvifDevice->removePreset(token);
    }
}

void OnvifCameraTab::handlePresetsUpdated(const std::vector<PelcoD::Onvif::PtzPreset>& presets)
{
    tablePresets->setRowCount(static_cast<int>(presets.size()));
    for (size_t i = 0; i < presets.size(); ++i) {
        const auto& p = presets[i];
        const int row = static_cast<int>(i);
        tablePresets->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(p.token)));
        tablePresets->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(p.name)));
    }
}

void OnvifCameraTab::handleRefreshTours()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshPresetTours();
    }
}

void OnvifCameraTab::handleTourSelected(int index)
{
    if (m_onvifDevice == nullptr || index < 0 || cmbPresetTours == nullptr) {
        if (tableTourSpots != nullptr) {
            tableTourSpots->setRowCount(0);
        }
        return;
    }

    const QString token = cmbPresetTours->itemData(index).toString();
    const auto tours = m_onvifDevice->presetTours();
    for (const auto& tour : tours) {
        if (tour.token == token.toStdString()) {
            tableTourSpots->setRowCount(static_cast<int>(tour.spots.size()));
            for (size_t i = 0; i < tour.spots.size(); ++i) {
                const auto& spot = tour.spots[i];
                const int row = static_cast<int>(i);
                tableTourSpots->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(spot.presetToken)));
                tableTourSpots->setItem(row, 1, new QTableWidgetItem(QString::number(spot.speed, 'f', 2)));
                tableTourSpots->setItem(row, 2, new QTableWidgetItem(QString::number(spot.stayTimeSeconds)));
            }

            QString statusStr = tr("Idle");
            QString colorStr = "#8b949e";
            if (tour.status == PelcoD::Onvif::PresetTourState::Touring) {
                statusStr = tr("Touring");
                colorStr = "#7ee787";
            } else if (tour.status == PelcoD::Onvif::PresetTourState::Paused) {
                statusStr = tr("Paused");
                colorStr = "#d29922";
            }
            lblTourStatus->setText(tr("Status: %1").arg(statusStr));
            lblTourStatus->setStyleSheet(QString("font-weight: bold; color: %1;").arg(colorStr));
            return;
        }
    }
}

void OnvifCameraTab::handleStartTour()
{
    if (m_onvifDevice == nullptr || cmbPresetTours == nullptr || cmbPresetTours->currentIndex() < 0) {
        return;
    }
    const QString token = cmbPresetTours->currentData().toString();
    if (!token.isEmpty()) {
        m_onvifDevice->operatePresetTour(token, PelcoD::Onvif::PresetTourOperation::Start);
        lblTourStatus->setText(tr("Status: Touring"));
        lblTourStatus->setStyleSheet("font-weight: bold; color: #7ee787;");
    }
}

void OnvifCameraTab::handlePauseTour()
{
    if (m_onvifDevice == nullptr || cmbPresetTours == nullptr || cmbPresetTours->currentIndex() < 0) {
        return;
    }
    const QString token = cmbPresetTours->currentData().toString();
    if (!token.isEmpty()) {
        m_onvifDevice->operatePresetTour(token, PelcoD::Onvif::PresetTourOperation::Pause);
        lblTourStatus->setText(tr("Status: Paused"));
        lblTourStatus->setStyleSheet("font-weight: bold; color: #d29922;");
    }
}

void OnvifCameraTab::handleStopTour()
{
    if (m_onvifDevice == nullptr || cmbPresetTours == nullptr || cmbPresetTours->currentIndex() < 0) {
        return;
    }
    const QString token = cmbPresetTours->currentData().toString();
    if (!token.isEmpty()) {
        m_onvifDevice->operatePresetTour(token, PelcoD::Onvif::PresetTourOperation::Stop);
        lblTourStatus->setText(tr("Status: Idle"));
        lblTourStatus->setStyleSheet("font-weight: bold; color: #8b949e;");
    }
}

void OnvifCameraTab::handleAddTourStep()
{
    if (tableTourSpots == nullptr) {
        return;
    }
    QString presetToken = "1";
    if (tablePresets != nullptr && tablePresets->currentRow() >= 0) {
        presetToken = tablePresets->item(tablePresets->currentRow(), 0)->text();
    } else if (tablePresets != nullptr && tablePresets->rowCount() > 0) {
        presetToken = tablePresets->item(0, 0)->text();
    }

    const int row = tableTourSpots->rowCount();
    tableTourSpots->insertRow(row);
    tableTourSpots->setItem(row, 0, new QTableWidgetItem(presetToken));
    tableTourSpots->setItem(row, 1, new QTableWidgetItem("1.00"));
    tableTourSpots->setItem(row, 2, new QTableWidgetItem("5"));
    tableTourSpots->selectRow(row);
}

void OnvifCameraTab::handleRemoveTourStep()
{
    if (tableTourSpots == nullptr) {
        return;
    }
    const int row = tableTourSpots->currentRow();
    if (row >= 0) {
        tableTourSpots->removeRow(row);
    }
}

void OnvifCameraTab::handleSaveTour()
{
    if (m_onvifDevice == nullptr || cmbPresetTours == nullptr || cmbPresetTours->currentIndex() < 0
        || tableTourSpots == nullptr) {
        return;
    }
    const QString token = cmbPresetTours->currentData().toString();
    if (token.isEmpty()) {
        return;
    }

    PelcoD::Onvif::PresetTour tour;
    tour.token = token.toStdString();
    tour.name = cmbPresetTours->currentText().toStdString();

    for (int r = 0; r < tableTourSpots->rowCount(); ++r) {
        PelcoD::Onvif::PresetTourSpot spot;
        if (tableTourSpots->item(r, 0) != nullptr) {
            spot.presetToken = tableTourSpots->item(r, 0)->text().trimmed().toStdString();
        }
        if (tableTourSpots->item(r, 1) != nullptr) {
            spot.speed = tableTourSpots->item(r, 1)->text().toFloat();
        }
        if (tableTourSpots->item(r, 2) != nullptr) {
            spot.stayTimeSeconds = tableTourSpots->item(r, 2)->text().toUInt();
        }
        if (!spot.presetToken.empty()) {
            tour.spots.push_back(spot);
        }
    }

    m_onvifDevice->modifyPresetTour(tour);
    QMessageBox::information(
        this, tr("Tour Saved"), tr("Preset tour '%1' updated successfully.").arg(QString::fromStdString(tour.name)));
}

void OnvifCameraTab::handleToursUpdated(const std::vector<PelcoD::Onvif::PresetTour>& tours)
{
    if (cmbPresetTours == nullptr) {
        return;
    }
    const QString previousToken = cmbPresetTours->currentData().toString();
    const QSignalBlocker blocker(cmbPresetTours);
    cmbPresetTours->clear();

    for (const auto& tour : tours) {
        QString label = QString::fromStdString(tour.name.empty() ? tour.token : tour.name);
        cmbPresetTours->addItem(label, QString::fromStdString(tour.token));
    }

    int selectIndex = cmbPresetTours->findData(previousToken);
    if (selectIndex < 0 && cmbPresetTours->count() > 0) {
        selectIndex = 0;
    }
    if (selectIndex >= 0) {
        cmbPresetTours->setCurrentIndex(selectIndex);
        handleTourSelected(selectIndex);
    } else {
        if (tableTourSpots != nullptr) {
            tableTourSpots->setRowCount(0);
        }
        if (lblTourStatus != nullptr) {
            lblTourStatus->setText(tr("Status: Idle"));
            lblTourStatus->setStyleSheet("font-weight: bold; color: #8b949e;");
        }
    }
}

void OnvifCameraTab::handleStatusUpdated(const PelcoD::Onvif::PtzStatus& status)
{
    lblTelemetryPanTilt->setText(
        tr("Pan/Tilt: (%1, %2)").arg(QString::number(status.pan, 'f', 2)).arg(QString::number(status.tilt, 'f', 2)));
    lblTelemetryZoom->setText(tr("Zoom: %1").arg(QString::number(status.zoom, 'f', 2)));
    lblTelemetryMoving->setText(status.isMoving ? tr("Status: MOVING") : tr("Status: IDLE"));
    lblTelemetryMoving->setStyleSheet(
        status.isMoving ? "color: #d29922; font-weight: bold;" : "color: #7ee787; font-weight: bold;");
}

void OnvifCameraTab::handleRebootCamera()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const auto ans = QMessageBox::question(this, tr("Reboot Camera Confirmation"),
        tr("Are you sure you want to reboot this ONVIF camera?"), QMessageBox::Yes | QMessageBox::No);
    if (ans == QMessageBox::Yes) {
        m_onvifDevice->rebootCamera();
    }
}

void OnvifCameraTab::handleRebootCompleted(bool success)
{
    if (success) {
        QMessageBox::information(
            this, tr("Reboot Initiated"), tr("Camera accepted the reboot command and is restarting."));
        handleDisconnect();
    } else {
        QMessageBox::warning(
            this, tr("Reboot Failed"), tr("Camera rejected the reboot command or service is unavailable."));
    }
}

void OnvifCameraTab::handleRefreshImaging()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshImagingSettings();
    }
}

void OnvifCameraTab::handleApplyImaging()
{
    if (m_onvifDevice == nullptr) {
        return;
    }

    PelcoD::Onvif::ImagingSettings settings {};
    settings.brightness = static_cast<float>(sliderBrightness->value());
    settings.contrast = static_cast<float>(sliderContrast->value());
    settings.colorSaturation = static_cast<float>(sliderSaturation->value());
    settings.sharpness = static_cast<float>(sliderSharpness->value());
    settings.irCutFilter = cmbIrFilter->currentText().toStdString();
    settings.backlightCompensation = chkBacklight->isChecked();
    settings.wideDynamicRange = chkWdr->isChecked();
    settings.autoFocusMode = cmbAutoFocus->currentText().toStdString();

    const bool ok = m_onvifDevice->setImagingSettings(settings);
    if (!ok) {
        QMessageBox::warning(this, tr("Imaging Error"), tr("Failed to apply imaging settings to camera."));
    }
}

void OnvifCameraTab::handleFocusNear()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->focusContinuous(-1.0f);
    }
}

void OnvifCameraTab::handleFocusFar()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->focusContinuous(1.0f);
    }
}

void OnvifCameraTab::handleFocusStop()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->focusStop();
    }
}

void OnvifCameraTab::handleToggleEvents(bool enable)
{
    if (m_onvifDevice == nullptr) {
        return;
    }

    if (enable) {
        m_onvifDevice->startEventSubscription(2000);
        btnToggleEvents->setText(tr("⏹ Stop Events"));
        btnToggleEvents->setStyleSheet("QPushButton { font-weight: bold; color: #f85149; }");
    } else {
        m_onvifDevice->stopEventSubscription();
        btnToggleEvents->setText(tr("▶ Subscribe Events"));
        btnToggleEvents->setStyleSheet("");
    }
}

void OnvifCameraTab::handleClearEvents()
{
    if (tableEvents != nullptr) {
        tableEvents->setRowCount(0);
    }
}

void OnvifCameraTab::handleImagingSettingsUpdated(const PelcoD::Onvif::ImagingSettings& settings)
{
    const QSignalBlocker b1(sliderBrightness);
    const QSignalBlocker b2(sliderContrast);
    const QSignalBlocker b3(sliderSaturation);
    const QSignalBlocker b4(sliderSharpness);
    const QSignalBlocker b5(cmbIrFilter);
    const QSignalBlocker b6(chkBacklight);
    const QSignalBlocker b7(chkWdr);
    const QSignalBlocker b8(cmbAutoFocus);

    sliderBrightness->setValue(static_cast<int>(settings.brightness));
    lblBrightnessVal->setText(QString::number(static_cast<int>(settings.brightness)));

    sliderContrast->setValue(static_cast<int>(settings.contrast));
    lblContrastVal->setText(QString::number(static_cast<int>(settings.contrast)));

    sliderSaturation->setValue(static_cast<int>(settings.colorSaturation));
    lblSaturationVal->setText(QString::number(static_cast<int>(settings.colorSaturation)));

    sliderSharpness->setValue(static_cast<int>(settings.sharpness));
    lblSharpnessVal->setText(QString::number(static_cast<int>(settings.sharpness)));

    const int irIdx = cmbIrFilter->findText(QString::fromStdString(settings.irCutFilter));
    if (irIdx >= 0) {
        cmbIrFilter->setCurrentIndex(irIdx);
    }

    chkBacklight->setChecked(settings.backlightCompensation);
    chkWdr->setChecked(settings.wideDynamicRange);

    const int focusIdx = cmbAutoFocus->findText(QString::fromStdString(settings.autoFocusMode));
    if (focusIdx >= 0) {
        cmbAutoFocus->setCurrentIndex(focusIdx);
    }
}

void OnvifCameraTab::handleEventReceived(const PelcoD::Onvif::OnvifEvent& event)
{
    if (tableEvents == nullptr) {
        return;
    }

    constexpr int kMaxRows = 100;
    if (tableEvents->rowCount() >= kMaxRows) {
        tableEvents->removeRow(0);
    }

    const int row = tableEvents->rowCount();
    tableEvents->insertRow(row);

    const QString timeStr = !event.utcTime.empty() ? QString::fromStdString(event.utcTime)
                                                   : QDateTime::currentDateTime().toString("hh:mm:ss");
    tableEvents->setItem(row, 0, new QTableWidgetItem(timeStr));
    tableEvents->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(event.topic)));
    tableEvents->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(event.dataName)));
    tableEvents->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(event.dataValue)));

    tableEvents->scrollToBottom();
}

} // namespace PelcoDApp
