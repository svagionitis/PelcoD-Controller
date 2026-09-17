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
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::auxiliaryCommandCompleted, this,
            &OnvifCameraTab::handleAuxiliaryCompleted);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::eventReceived, this, &OnvifCameraTab::handleEventReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::osdsUpdated, this, &OnvifCameraTab::handleOsdsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::usersUpdated, this, &OnvifCameraTab::handleUsersUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::networkInterfacesUpdated, this,
            &OnvifCameraTab::handleNetworkUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::networkGatewayUpdated, this,
            &OnvifCameraTab::handleGatewayUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::dnsUpdated, this, &OnvifCameraTab::handleDnsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::ntpUpdated, this, &OnvifCameraTab::handleNtpUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::factoryDefaultCompleted, this,
            &OnvifCameraTab::handleFactoryDefaultCompleted);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::focusStatusUpdated, this,
            &OnvifCameraTab::handleFocusStatusUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::imagingPresetsUpdated, this,
            &OnvifCameraTab::handleImagingPresetsUpdated);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::relayOutputsUpdated, this, &OnvifCameraTab::handleRelaysUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::digitalInputsUpdated, this,
            &OnvifCameraTab::handleDigitalInputsUpdated);
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

    // Auxiliary Commands
    auto* auxLayout = new QHBoxLayout();
    btnWiperOn = new QPushButton(tr("Wiper ON"), groupPtz);
    btnWiperOff = new QPushButton(tr("Wiper OFF"), groupPtz);
    btnWasher = new QPushButton(tr("Washer"), groupPtz);
    btnIrOn = new QPushButton(tr("IR ON"), groupPtz);
    btnIrOff = new QPushButton(tr("IR OFF"), groupPtz);
    editCustomAux = new QLineEdit(groupPtz);
    editCustomAux->setPlaceholderText(tr("e.g. Aux1On"));
    btnSendAux = new QPushButton(tr("Send Aux"), groupPtz);

    connect(btnWiperOn, &QPushButton::clicked, this, &OnvifCameraTab::handleSendWiperOn);
    connect(btnWiperOff, &QPushButton::clicked, this, &OnvifCameraTab::handleSendWiperOff);
    connect(btnWasher, &QPushButton::clicked, this, &OnvifCameraTab::handleSendWasher);
    connect(btnIrOn, &QPushButton::clicked, this, &OnvifCameraTab::handleSendIrOn);
    connect(btnIrOff, &QPushButton::clicked, this, &OnvifCameraTab::handleSendIrOff);
    connect(btnSendAux, &QPushButton::clicked, this, &OnvifCameraTab::handleSendCustomAux);

    auxLayout->addWidget(new QLabel(tr("Aux:"), groupPtz));
    auxLayout->addWidget(btnWiperOn);
    auxLayout->addWidget(btnWiperOff);
    auxLayout->addWidget(btnWasher);
    auxLayout->addWidget(btnIrOn);
    auxLayout->addWidget(btnIrOff);
    auxLayout->addWidget(editCustomAux);
    auxLayout->addWidget(btnSendAux);

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
    ptzLayout->addLayout(auxLayout);
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

    lblFocusStatus = new QLabel(tr("Focus: Idle"), groupImaging);
    lblFocusStatus->setStyleSheet(QStringLiteral("color: #8b949e; font-weight: bold;"));

    focusLayout->addWidget(new QLabel(tr("Focus:"), groupImaging));
    focusLayout->addWidget(cmbAutoFocus);
    focusLayout->addWidget(btnFocusNear);
    focusLayout->addWidget(btnFocusFar);
    focusLayout->addWidget(lblFocusStatus);
    imgLayout->addLayout(focusLayout, 6, 0, 1, 3);

    // Imaging Presets
    auto* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("Optical Preset:"), groupImaging));
    cmbImagingPresets = new QComboBox(groupImaging);
    cmbImagingPresets->setMinimumWidth(180);
    btnRecallImagingPreset = new QPushButton(tr("Recall Preset"), groupImaging);
    connect(btnRecallImagingPreset, &QPushButton::clicked, this, &OnvifCameraTab::handleRecallImagingPreset);
    presetLayout->addWidget(cmbImagingPresets);
    presetLayout->addWidget(btnRecallImagingPreset);
    presetLayout->addStretch();
    imgLayout->addLayout(presetLayout, 7, 0, 1, 3);

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
    imgLayout->addLayout(imgBtnLayout, 8, 0, 1, 3);

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
    // Sub-Tab 5: OSD Overlays
    // -------------------------------------------------------------------------
    auto* osdTabLayout = createScrollTab(tr("OSD Overlays"));

    auto* groupOsd = new QGroupBox(tr("On-Screen Display (OSD) Overlays"), this);
    auto* osdLayout = new QVBoxLayout(groupOsd);
    osdLayout->setSpacing(6);

    tableOsds = new QTableWidget(0, 5, groupOsd);
    tableOsds->setHorizontalHeaderLabels({ tr("Token"), tr("Type"), tr("Position"), tr("Font Size"), tr("Content") });
    tableOsds->horizontalHeader()->setStretchLastSection(true);
    tableOsds->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableOsds->setSelectionMode(QAbstractItemView::SingleSelection);
    tableOsds->setMinimumHeight(180);
    osdLayout->addWidget(tableOsds);

    auto* editOsdGrid = new QGridLayout();
    editOsdGrid->addWidget(new QLabel(tr("Text Content:"), groupOsd), 0, 0);
    editOsdText = new QLineEdit(groupOsd);
    editOsdText->setPlaceholderText(tr("Text overlay or label..."));
    editOsdGrid->addWidget(editOsdText, 0, 1, 1, 3);

    editOsdGrid->addWidget(new QLabel(tr("Position:"), groupOsd), 1, 0);
    cmbOsdPosition = new QComboBox(groupOsd);
    cmbOsdPosition->addItems({ tr("UpperLeft"), tr("UpperRight"), tr("LowerLeft"), tr("LowerRight"), tr("Custom") });
    editOsdGrid->addWidget(cmbOsdPosition, 1, 1);

    editOsdGrid->addWidget(new QLabel(tr("Font Size:"), groupOsd), 1, 2);
    spinOsdFontSize = new QSpinBox(groupOsd);
    spinOsdFontSize->setRange(12, 72);
    spinOsdFontSize->setValue(24);
    editOsdGrid->addWidget(spinOsdFontSize, 1, 3);

    chkOsdDateTime = new QCheckBox(tr("Dynamic Date & Time Overlay"), groupOsd);
    editOsdGrid->addWidget(chkOsdDateTime, 2, 0, 1, 4);

    osdLayout->addLayout(editOsdGrid);

    auto* osdBtnLayout = new QHBoxLayout();
    btnRefreshOsds = new QPushButton(tr("Refresh"), groupOsd);
    btnAddOsd = new QPushButton(tr("Add OSD"), groupOsd);
    btnUpdateOsd = new QPushButton(tr("Update Selected"), groupOsd);
    btnDeleteOsd = new QPushButton(tr("Delete Selected"), groupOsd);
    osdBtnLayout->addWidget(btnRefreshOsds);
    osdBtnLayout->addWidget(btnAddOsd);
    osdBtnLayout->addWidget(btnUpdateOsd);
    osdBtnLayout->addWidget(btnDeleteOsd);
    osdBtnLayout->addStretch();
    osdLayout->addLayout(osdBtnLayout);

    osdTabLayout->addWidget(groupOsd);
    osdTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 6: Device & Streams
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

    // -------------------------------------------------------------------------
    // Sub-Tab 7: Users & Security
    // -------------------------------------------------------------------------
    auto* usersTabLayout = createScrollTab(tr("Users & Security"));

    auto* groupUsers = new QGroupBox(tr("ONVIF User Accounts"), this);
    auto* usersLayout = new QVBoxLayout(groupUsers);
    usersLayout->setSpacing(6);

    tableUsers = new QTableWidget(groupUsers);
    tableUsers->setColumnCount(3);
    tableUsers->setHorizontalHeaderLabels({ tr("Username"), tr("User Level"), tr("Password Status") });
    tableUsers->horizontalHeader()->setStretchLastSection(true);
    tableUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableUsers->setSelectionMode(QAbstractItemView::SingleSelection);
    tableUsers->setMinimumHeight(140);
    usersLayout->addWidget(tableUsers);

    auto* userForm = new QGridLayout();
    userForm->setSpacing(6);

    userForm->addWidget(new QLabel(tr("Username:"), groupUsers), 0, 0);
    editUserUsername = new QLineEdit(groupUsers);
    userForm->addWidget(editUserUsername, 0, 1);

    userForm->addWidget(new QLabel(tr("Password:"), groupUsers), 0, 2);
    editUserPassword = new QLineEdit(groupUsers);
    editUserPassword->setEchoMode(QLineEdit::Password);
    userForm->addWidget(editUserPassword, 0, 3);

    userForm->addWidget(new QLabel(tr("User Level:"), groupUsers), 1, 0);
    cmbUserLevel = new QComboBox(groupUsers);
    cmbUserLevel->addItems({ QStringLiteral("Administrator"), QStringLiteral("Operator"), QStringLiteral("User"),
        QStringLiteral("Anonymous") });
    userForm->addWidget(cmbUserLevel, 1, 1);

    auto* userBtnLayout = new QHBoxLayout();
    btnAddUser = new QPushButton(tr("＋ Add User"), groupUsers);
    btnUpdateUser = new QPushButton(tr("✎ Update User"), groupUsers);
    btnDeleteUser = new QPushButton(tr("🗑 Delete User"), groupUsers);
    btnRefreshUsers = new QPushButton(tr("⟳ Refresh"), groupUsers);

    userBtnLayout->addWidget(btnAddUser);
    userBtnLayout->addWidget(btnUpdateUser);
    userBtnLayout->addWidget(btnDeleteUser);
    userBtnLayout->addWidget(btnRefreshUsers);
    userBtnLayout->addStretch();
    userForm->addLayout(userBtnLayout, 1, 2, 1, 2);

    usersLayout->addLayout(userForm);
    usersTabLayout->addWidget(groupUsers);
    usersTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 8: Network & Maintenance
    // -------------------------------------------------------------------------
    auto* netTabLayout = createScrollTab(tr("Network & Maintenance"));

    auto* groupNet = new QGroupBox(tr("Network Interface Configuration"), this);
    auto* netGrid = new QGridLayout(groupNet);
    netGrid->setSpacing(6);

    netGrid->addWidget(new QLabel(tr("Interface:"), groupNet), 0, 0);
    editNetToken = new QLineEdit(groupNet);
    editNetToken->setReadOnly(true);
    netGrid->addWidget(editNetToken, 0, 1);

    chkNetEnabled = new QCheckBox(tr("Enabled"), groupNet);
    chkNetEnabled->setChecked(true);
    netGrid->addWidget(chkNetEnabled, 0, 2);

    chkNetDhcp = new QCheckBox(tr("DHCP (Auto IPv4)"), groupNet);
    netGrid->addWidget(chkNetDhcp, 0, 3);

    netGrid->addWidget(new QLabel(tr("IP Address:"), groupNet), 1, 0);
    editNetIp = new QLineEdit(groupNet);
    netGrid->addWidget(editNetIp, 1, 1);

    netGrid->addWidget(new QLabel(tr("Prefix Length:"), groupNet), 1, 2);
    spinNetPrefix = new QSpinBox(groupNet);
    spinNetPrefix->setRange(1, 32);
    spinNetPrefix->setValue(24);
    netGrid->addWidget(spinNetPrefix, 1, 3);

    netGrid->addWidget(new QLabel(tr("Gateway:"), groupNet), 2, 0);
    editNetGateway = new QLineEdit(groupNet);
    netGrid->addWidget(editNetGateway, 2, 1);

    auto* netBtnLayout = new QHBoxLayout();
    btnRefreshNetwork = new QPushButton(tr("⟳ Refresh Network"), groupNet);
    btnApplyNetwork = new QPushButton(tr("Apply Network"), groupNet);
    btnApplyNetwork->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; }"));
    netBtnLayout->addWidget(btnRefreshNetwork);
    netBtnLayout->addWidget(btnApplyNetwork);
    netBtnLayout->addStretch();
    netGrid->addLayout(netBtnLayout, 2, 2, 1, 2);

    netTabLayout->addWidget(groupNet);

    auto* groupDnsNtp = new QGroupBox(tr("DNS & NTP Configuration"), this);
    auto* dnsNtpGrid = new QGridLayout(groupDnsNtp);
    dnsNtpGrid->setSpacing(6);

    dnsNtpGrid->addWidget(new QLabel(tr("DNS Servers:"), groupDnsNtp), 0, 0);
    editDnsServers = new QLineEdit(groupDnsNtp);
    editDnsServers->setPlaceholderText(tr("e.g. 8.8.8.8, 1.1.1.1"));
    dnsNtpGrid->addWidget(editDnsServers, 0, 1);

    chkDnsDhcp = new QCheckBox(tr("DNS from DHCP"), groupDnsNtp);
    dnsNtpGrid->addWidget(chkDnsDhcp, 0, 2);

    auto* dnsBtnLayout = new QHBoxLayout();
    btnRefreshDns = new QPushButton(tr("⟳ Refresh DNS"), groupDnsNtp);
    btnApplyDns = new QPushButton(tr("Apply DNS"), groupDnsNtp);
    dnsBtnLayout->addWidget(btnRefreshDns);
    dnsBtnLayout->addWidget(btnApplyDns);
    dnsNtpGrid->addLayout(dnsBtnLayout, 0, 3);

    dnsNtpGrid->addWidget(new QLabel(tr("NTP Servers:"), groupDnsNtp), 1, 0);
    editNtpServers = new QLineEdit(groupDnsNtp);
    editNtpServers->setPlaceholderText(tr("e.g. pool.ntp.org"));
    dnsNtpGrid->addWidget(editNtpServers, 1, 1);

    chkNtpDhcp = new QCheckBox(tr("NTP from DHCP"), groupDnsNtp);
    dnsNtpGrid->addWidget(chkNtpDhcp, 1, 2);

    auto* ntpBtnLayout = new QHBoxLayout();
    btnRefreshNtp = new QPushButton(tr("⟳ Refresh NTP"), groupDnsNtp);
    btnApplyNtp = new QPushButton(tr("Apply NTP"), groupDnsNtp);
    ntpBtnLayout->addWidget(btnRefreshNtp);
    ntpBtnLayout->addWidget(btnApplyNtp);
    dnsNtpGrid->addLayout(ntpBtnLayout, 1, 3);

    netTabLayout->addWidget(groupDnsNtp);

    auto* groupMaint = new QGroupBox(tr("System Lifecycle & Time"), this);
    auto* maintLayout = new QHBoxLayout(groupMaint);
    maintLayout->setSpacing(8);

    btnSyncPcTime = new QPushButton(tr("⏱ Sync Time with PC"), groupMaint);
    btnFactoryDefaultSoft = new QPushButton(tr("Soft Factory Reset"), groupMaint);
    btnFactoryDefaultHard = new QPushButton(tr("⚠ Hard Factory Reset"), groupMaint);
    btnFactoryDefaultHard->setStyleSheet(QStringLiteral("QPushButton { color: #f85149; }"));

    maintLayout->addWidget(btnSyncPcTime);
    maintLayout->addWidget(btnFactoryDefaultSoft);
    maintLayout->addWidget(btnFactoryDefaultHard);
    maintLayout->addStretch();

    netTabLayout->addWidget(groupMaint);
    netTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 9: Relays & I/O (Profile S & T)
    // -------------------------------------------------------------------------
    auto* relayTabLayout = createScrollTab(tr("Relays & I/O"));

    auto* groupRelays = new QGroupBox(tr("Profile S/T: Relay Outputs & Actuators"));
    auto* relaysLayout = new QVBoxLayout(groupRelays);
    relaysLayout->setSpacing(6);

    tableRelays = new QTableWidget(0, 5, groupRelays);
    tableRelays->setHorizontalHeaderLabels(
        { tr("Token"), tr("Mode"), tr("Delay (s)"), tr("Idle State"), tr("Logical State") });
    tableRelays->horizontalHeader()->setStretchLastSection(true);
    tableRelays->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableRelays->setSelectionMode(QAbstractItemView::SingleSelection);
    tableRelays->setMinimumHeight(120);

    auto* relayEditLayout = new QGridLayout();
    relayEditLayout->setSpacing(6);

    relayEditLayout->addWidget(new QLabel(tr("Relay Token:"), groupRelays), 0, 0);
    editRelayToken = new QLineEdit(groupRelays);
    editRelayToken->setReadOnly(true);
    relayEditLayout->addWidget(editRelayToken, 0, 1);

    relayEditLayout->addWidget(new QLabel(tr("Mode:"), groupRelays), 0, 2);
    cmbRelayMode = new QComboBox(groupRelays);
    cmbRelayMode->addItems({ QStringLiteral("Bistable"), QStringLiteral("Monostable") });
    relayEditLayout->addWidget(cmbRelayMode, 0, 3);

    relayEditLayout->addWidget(new QLabel(tr("Delay Time (s):"), groupRelays), 1, 0);
    spinRelayDelay = new QDoubleSpinBox(groupRelays);
    spinRelayDelay->setRange(0.0, 300.0);
    spinRelayDelay->setDecimals(1);
    relayEditLayout->addWidget(spinRelayDelay, 1, 1);

    relayEditLayout->addWidget(new QLabel(tr("Idle State:"), groupRelays), 1, 2);
    cmbRelayIdleState = new QComboBox(groupRelays);
    cmbRelayIdleState->addItems({ QStringLiteral("open"), QStringLiteral("closed") });
    relayEditLayout->addWidget(cmbRelayIdleState, 1, 3);

    auto* relayBtnLayout = new QHBoxLayout();
    btnRefreshRelays = new QPushButton(tr("Refresh Relays"), groupRelays);
    btnActivateRelay = new QPushButton(tr("⚡ Activate (Aux ON)"), groupRelays);
    btnActivateRelay->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #238636; color: white;"));
    btnDeactivateRelay = new QPushButton(tr("⭕ Deactivate (Aux OFF)"), groupRelays);
    btnApplyRelaySettings = new QPushButton(tr("Save Settings"), groupRelays);

    relayBtnLayout->addWidget(btnRefreshRelays);
    relayBtnLayout->addWidget(btnActivateRelay);
    relayBtnLayout->addWidget(btnDeactivateRelay);
    relayBtnLayout->addWidget(btnApplyRelaySettings);
    relayBtnLayout->addStretch();

    relaysLayout->addWidget(tableRelays);
    relaysLayout->addLayout(relayEditLayout);
    relaysLayout->addLayout(relayBtnLayout);

    // Digital Inputs Group
    auto* groupInputs = new QGroupBox(tr("Profile S/T: Digital Inputs & Sensors"));
    auto* inputsLayout = new QVBoxLayout(groupInputs);
    inputsLayout->setSpacing(6);

    tableDigitalInputs = new QTableWidget(0, 4, groupInputs);
    tableDigitalInputs->setHorizontalHeaderLabels({ tr("Token"), tr("Idle State"), tr("Sensor Type"), tr("State") });
    tableDigitalInputs->horizontalHeader()->setStretchLastSection(true);
    tableDigitalInputs->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableDigitalInputs->setSelectionMode(QAbstractItemView::SingleSelection);
    tableDigitalInputs->setMinimumHeight(100);

    auto* inputBtnLayout = new QHBoxLayout();
    btnRefreshInputs = new QPushButton(tr("Refresh Inputs"), groupInputs);
    inputBtnLayout->addWidget(btnRefreshInputs);
    inputBtnLayout->addStretch();

    inputsLayout->addWidget(tableDigitalInputs);
    inputsLayout->addLayout(inputBtnLayout);

    relayTabLayout->addWidget(groupRelays);
    relayTabLayout->addWidget(groupInputs);
    relayTabLayout->addStretch();

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

    // OSD Connections
    connect(btnRefreshOsds, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshOsds);
    connect(btnAddOsd, &QPushButton::clicked, this, &OnvifCameraTab::handleCreateOsd);
    connect(btnUpdateOsd, &QPushButton::clicked, this, &OnvifCameraTab::handleSetOsd);
    connect(btnDeleteOsd, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteOsd);
    connect(tableOsds, &QTableWidget::itemSelectionChanged, this, &OnvifCameraTab::handleOsdSelectionChanged);

    // Users & Security connections
    connect(btnRefreshUsers, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshUsers);
    connect(btnAddUser, &QPushButton::clicked, this, &OnvifCameraTab::handleAddUser);
    connect(btnUpdateUser, &QPushButton::clicked, this, &OnvifCameraTab::handleUpdateUser);
    connect(btnDeleteUser, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteUser);
    connect(tableUsers, &QTableWidget::itemSelectionChanged, this, &OnvifCameraTab::handleUserSelectionChanged);

    // Network & Maintenance connections
    connect(btnRefreshNetwork, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshNetwork);
    connect(btnApplyNetwork, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyNetwork);
    connect(btnRefreshDns, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshDns);
    connect(btnApplyDns, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyDns);
    connect(btnRefreshNtp, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshNtp);
    connect(btnApplyNtp, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyNtp);
    connect(btnSyncPcTime, &QPushButton::clicked, this, &OnvifCameraTab::handleSyncPcTime);
    connect(btnFactoryDefaultSoft, &QPushButton::clicked, this, &OnvifCameraTab::handleFactoryDefaultSoft);
    connect(btnFactoryDefaultHard, &QPushButton::clicked, this, &OnvifCameraTab::handleFactoryDefaultHard);

    // Relay & I/O connections
    connect(btnRefreshRelays, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRelays);
    connect(btnActivateRelay, &QPushButton::clicked, this, &OnvifCameraTab::handleActivateRelay);
    connect(btnDeactivateRelay, &QPushButton::clicked, this, &OnvifCameraTab::handleDeactivateRelay);
    connect(btnApplyRelaySettings, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyRelaySettings);
    connect(btnRefreshInputs, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshInputs);
    connect(tableRelays, &QTableWidget::itemSelectionChanged, this, &OnvifCameraTab::handleRelaySelectionChanged);
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
    btnWiperOn->setEnabled(connected);
    btnWiperOff->setEnabled(connected);
    btnWasher->setEnabled(connected);
    btnIrOn->setEnabled(connected);
    btnIrOff->setEnabled(connected);
    editCustomAux->setEnabled(connected);
    btnSendAux->setEnabled(connected);
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
    cmbImagingPresets->setEnabled(connected);
    btnRecallImagingPreset->setEnabled(connected);
    btnRefreshImaging->setEnabled(connected);
    btnApplyImaging->setEnabled(connected);
    btnToggleEvents->setEnabled(connected);
    btnClearEvents->setEnabled(connected);

    // Profile T OSD widgets
    tableOsds->setEnabled(connected);
    editOsdText->setEnabled(connected);
    cmbOsdPosition->setEnabled(connected);
    chkOsdDateTime->setEnabled(connected);
    spinOsdFontSize->setEnabled(connected);
    btnRefreshOsds->setEnabled(connected);
    btnAddOsd->setEnabled(connected);
    btnUpdateOsd->setEnabled(connected);
    btnDeleteOsd->setEnabled(connected);

    // Device Management: Users & Security widgets
    tableUsers->setEnabled(connected);
    editUserUsername->setEnabled(connected);
    editUserPassword->setEnabled(connected);
    cmbUserLevel->setEnabled(connected);
    btnAddUser->setEnabled(connected);
    btnUpdateUser->setEnabled(connected);
    btnDeleteUser->setEnabled(connected);
    btnRefreshUsers->setEnabled(connected);

    // Device Management: Network & Maintenance widgets
    chkNetEnabled->setEnabled(connected);
    chkNetDhcp->setEnabled(connected);
    editNetIp->setEnabled(connected);
    spinNetPrefix->setEnabled(connected);
    editNetGateway->setEnabled(connected);
    btnRefreshNetwork->setEnabled(connected);
    btnApplyNetwork->setEnabled(connected);

    chkDnsDhcp->setEnabled(connected);
    editDnsServers->setEnabled(connected);
    btnRefreshDns->setEnabled(connected);
    btnApplyDns->setEnabled(connected);

    chkNtpDhcp->setEnabled(connected);
    editNtpServers->setEnabled(connected);
    btnRefreshNtp->setEnabled(connected);
    btnApplyNtp->setEnabled(connected);

    btnSyncPcTime->setEnabled(connected);
    btnFactoryDefaultSoft->setEnabled(connected);
    btnFactoryDefaultHard->setEnabled(connected);

    // Relay & I/O widgets
    tableRelays->setEnabled(connected);
    editRelayToken->setEnabled(connected);
    cmbRelayMode->setEnabled(connected);
    spinRelayDelay->setEnabled(connected);
    cmbRelayIdleState->setEnabled(connected);
    btnRefreshRelays->setEnabled(connected);
    btnActivateRelay->setEnabled(connected);
    btnDeactivateRelay->setEnabled(connected);
    btnApplyRelaySettings->setEnabled(connected);
    tableDigitalInputs->setEnabled(connected);
    btnRefreshInputs->setEnabled(connected);

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
        tableOsds->setRowCount(0);
        editOsdText->clear();
        tableUsers->setRowCount(0);
        editUserUsername->clear();
        editUserPassword->clear();
        editNetToken->clear();
        editNetIp->clear();
        editNetGateway->clear();
        editDnsServers->clear();
        editNtpServers->clear();
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

void OnvifCameraTab::handleSendWiperOn()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->sendAuxiliaryCommand("tt:Wiper|On");
    }
}

void OnvifCameraTab::handleSendWiperOff()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->sendAuxiliaryCommand("tt:Wiper|Off");
    }
}

void OnvifCameraTab::handleSendWasher()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->sendAuxiliaryCommand("tt:Washer|On");
    }
}

void OnvifCameraTab::handleSendIrOn()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->sendAuxiliaryCommand("tt:IR|On");
    }
}

void OnvifCameraTab::handleSendIrOff()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->sendAuxiliaryCommand("tt:IR|Off");
    }
}

void OnvifCameraTab::handleSendCustomAux()
{
    if (m_onvifDevice != nullptr && editCustomAux != nullptr) {
        const QString cmd = editCustomAux->text().trimmed();
        if (!cmd.isEmpty()) {
            m_onvifDevice->sendAuxiliaryCommand(cmd);
        }
    }
}

void OnvifCameraTab::handleAuxiliaryCompleted(bool success, const QString& /*response*/)
{
    if (!success) {
        QMessageBox::warning(this, tr("Auxiliary Command"), tr("Auxiliary command execution failed."));
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

void OnvifCameraTab::handleRefreshOsds()
{
    if (m_onvifDevice) {
        m_onvifDevice->refreshOSDs();
    }
}

void OnvifCameraTab::handleCreateOsd()
{
    if (!m_onvifDevice) {
        return;
    }
    PelcoD::Onvif::OsdConfig osd;
    osd.plainText = editOsdText->text().toStdString();
    osd.fontSize = static_cast<uint32_t>(spinOsdFontSize->value());
    osd.isDateAndTime = chkOsdDateTime->isChecked();

    const int posIdx = cmbOsdPosition->currentIndex();
    if (posIdx == 1) {
        osd.position = PelcoD::Onvif::OsdPositionType::UpperRight;
    } else if (posIdx == 2) {
        osd.position = PelcoD::Onvif::OsdPositionType::LowerLeft;
    } else if (posIdx == 3) {
        osd.position = PelcoD::Onvif::OsdPositionType::LowerRight;
    } else if (posIdx == 4) {
        osd.position = PelcoD::Onvif::OsdPositionType::Custom;
    } else {
        osd.position = PelcoD::Onvif::OsdPositionType::UpperLeft;
    }

    m_onvifDevice->createOSD(osd);
}

void OnvifCameraTab::handleSetOsd()
{
    if (!m_onvifDevice || tableOsds == nullptr) {
        return;
    }
    const int row = tableOsds->currentRow();
    if (row < 0 || row >= tableOsds->rowCount()) {
        return;
    }
    const QString token = tableOsds->item(row, 0)->text();
    PelcoD::Onvif::OsdConfig osd;
    osd.token = token.toStdString();
    osd.plainText = editOsdText->text().toStdString();
    osd.fontSize = static_cast<uint32_t>(spinOsdFontSize->value());
    osd.isDateAndTime = chkOsdDateTime->isChecked();

    const int posIdx = cmbOsdPosition->currentIndex();
    if (posIdx == 1) {
        osd.position = PelcoD::Onvif::OsdPositionType::UpperRight;
    } else if (posIdx == 2) {
        osd.position = PelcoD::Onvif::OsdPositionType::LowerLeft;
    } else if (posIdx == 3) {
        osd.position = PelcoD::Onvif::OsdPositionType::LowerRight;
    } else if (posIdx == 4) {
        osd.position = PelcoD::Onvif::OsdPositionType::Custom;
    } else {
        osd.position = PelcoD::Onvif::OsdPositionType::UpperLeft;
    }

    m_onvifDevice->setOSD(osd);
}

void OnvifCameraTab::handleDeleteOsd()
{
    if (!m_onvifDevice || tableOsds == nullptr) {
        return;
    }
    const int row = tableOsds->currentRow();
    if (row < 0 || row >= tableOsds->rowCount()) {
        return;
    }
    const QString token = tableOsds->item(row, 0)->text();
    m_onvifDevice->deleteOSD(token);
}

void OnvifCameraTab::handleOsdsUpdated(const std::vector<PelcoD::Onvif::OsdConfig>& osds)
{
    if (tableOsds == nullptr) {
        return;
    }
    tableOsds->setRowCount(0);
    for (const auto& osd : osds) {
        const int row = tableOsds->rowCount();
        tableOsds->insertRow(row);
        tableOsds->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(osd.token)));
        tableOsds->setItem(row, 1, new QTableWidgetItem(osd.isDateAndTime ? tr("Date/Time") : tr("Plain Text")));

        QString posStr = tr("UpperLeft");
        if (osd.position == PelcoD::Onvif::OsdPositionType::UpperRight) {
            posStr = tr("UpperRight");
        } else if (osd.position == PelcoD::Onvif::OsdPositionType::LowerLeft) {
            posStr = tr("LowerLeft");
        } else if (osd.position == PelcoD::Onvif::OsdPositionType::LowerRight) {
            posStr = tr("LowerRight");
        } else if (osd.position == PelcoD::Onvif::OsdPositionType::Custom) {
            posStr = tr("Custom");
        }
        tableOsds->setItem(row, 2, new QTableWidgetItem(posStr));

        tableOsds->setItem(row, 3, new QTableWidgetItem(QString::number(osd.fontSize)));
        tableOsds->setItem(row, 4,
            new QTableWidgetItem(osd.isDateAndTime ? QString("%1 %2").arg(
                                     QString::fromStdString(osd.dateFormat), QString::fromStdString(osd.timeFormat))
                                                   : QString::fromStdString(osd.plainText)));
    }
}

void OnvifCameraTab::handleOsdSelectionChanged()
{
    if (tableOsds == nullptr) {
        return;
    }
    const int row = tableOsds->currentRow();
    if (row < 0 || row >= tableOsds->rowCount()) {
        return;
    }
    const QString type = tableOsds->item(row, 1)->text();
    chkOsdDateTime->setChecked(type == tr("Date/Time"));

    const QString pos = tableOsds->item(row, 2)->text();
    const int posIdx = cmbOsdPosition->findText(pos);
    if (posIdx >= 0) {
        cmbOsdPosition->setCurrentIndex(posIdx);
    }

    if (tableOsds->item(row, 3)) {
        spinOsdFontSize->setValue(tableOsds->item(row, 3)->text().toInt());
    }

    if (tableOsds->item(row, 4) && !chkOsdDateTime->isChecked()) {
        editOsdText->setText(tableOsds->item(row, 4)->text());
    }
}

void OnvifCameraTab::handleRefreshUsers()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshUsers();
    }
}

void OnvifCameraTab::handleAddUser()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString username = editUserUsername->text().trimmed();
    const QString password = editUserPassword->text();
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("User Management"), tr("Username cannot be empty."));
        return;
    }

    PelcoD::Onvif::OnvifUser user {};
    user.username = username.toStdString();
    user.password = password.toStdString();
    user.level = PelcoD::Onvif::userLevelFromString(cmbUserLevel->currentText().toStdString());

    if (m_onvifDevice->createUser(user)) {
        editUserUsername->clear();
        editUserPassword->clear();
    } else {
        QMessageBox::critical(this, tr("User Management"), tr("Failed to create user."));
    }
}

void OnvifCameraTab::handleUpdateUser()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString username = editUserUsername->text().trimmed();
    const QString password = editUserPassword->text();
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("User Management"), tr("Please select or enter a username to update."));
        return;
    }

    PelcoD::Onvif::OnvifUser user {};
    user.username = username.toStdString();
    user.password = password.toStdString();
    user.level = PelcoD::Onvif::userLevelFromString(cmbUserLevel->currentText().toStdString());

    if (!m_onvifDevice->setUser(user)) {
        QMessageBox::critical(this, tr("User Management"), tr("Failed to update user."));
    }
}

void OnvifCameraTab::handleDeleteUser()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString username = editUserUsername->text().trimmed();
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("User Management"), tr("Please select a user to delete."));
        return;
    }

    if (QMessageBox::question(
            this, tr("Confirm Delete"), tr("Are you sure you want to delete user '%1'?").arg(username))
        != QMessageBox::Yes) {
        return;
    }

    if (m_onvifDevice->deleteUser(username)) {
        editUserUsername->clear();
        editUserPassword->clear();
    } else {
        QMessageBox::critical(this, tr("User Management"), tr("Failed to delete user."));
    }
}

void OnvifCameraTab::handleUsersUpdated(const std::vector<PelcoD::Onvif::OnvifUser>& users)
{
    tableUsers->setRowCount(0);
    for (const auto& u : users) {
        const int row = tableUsers->rowCount();
        tableUsers->insertRow(row);
        tableUsers->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(u.username)));
        tableUsers->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(PelcoD::Onvif::userLevelToString(u.level))));
        tableUsers->setItem(
            row, 2, new QTableWidgetItem(u.password.empty() ? tr("Not Set / Hidden") : tr("Configured")));
    }
}

void OnvifCameraTab::handleUserSelectionChanged()
{
    const auto selected = tableUsers->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const int row = selected.first()->row();
    if (row >= 0 && row < tableUsers->rowCount()) {
        editUserUsername->setText(tableUsers->item(row, 0)->text());
        cmbUserLevel->setCurrentText(tableUsers->item(row, 1)->text());
        editUserPassword->clear();
    }
}

void OnvifCameraTab::handleRefreshNetwork()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshNetworkInterfaces();
        m_onvifDevice->refreshNetworkGateway();
    }
}

void OnvifCameraTab::handleApplyNetwork()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    PelcoD::Onvif::NetworkInterfaceConfig cfg {};
    cfg.token = editNetToken->text().isEmpty() ? "eth0" : editNetToken->text().toStdString();
    cfg.enabled = chkNetEnabled->isChecked();
    cfg.ipv4.enabled = chkNetEnabled->isChecked();
    cfg.ipv4.dhcp = chkNetDhcp->isChecked();
    cfg.ipv4.manualAddress = editNetIp->text().trimmed().toStdString();
    cfg.ipv4.prefixLength = spinNetPrefix->value();

    const bool netOk = m_onvifDevice->setNetworkInterface(cfg);
    const QString gw = editNetGateway->text().trimmed();
    bool gwOk = true;
    if (!gw.isEmpty()) {
        gwOk = m_onvifDevice->setNetworkGateway(gw);
    }
    if (netOk && gwOk) {
        QMessageBox::information(this, tr("Network Configuration"), tr("Network settings applied successfully."));
    } else {
        QMessageBox::critical(this, tr("Network Configuration"), tr("Failed to apply some network settings."));
    }
}

void OnvifCameraTab::handleRefreshDns()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshDNS();
    }
}

void OnvifCameraTab::handleApplyDns()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    PelcoD::Onvif::DnsConfig dns {};
    dns.fromDhcp = chkDnsDhcp->isChecked();
    const QStringList parts = editDnsServers->text().split(',', Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        dns.dnsServers.push_back(p.trimmed().toStdString());
    }
    if (m_onvifDevice->setDNS(dns)) {
        QMessageBox::information(this, tr("DNS Configuration"), tr("DNS settings applied successfully."));
    } else {
        QMessageBox::critical(this, tr("DNS Configuration"), tr("Failed to apply DNS settings."));
    }
}

void OnvifCameraTab::handleRefreshNtp()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshNTP();
    }
}

void OnvifCameraTab::handleApplyNtp()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    PelcoD::Onvif::NtpConfig ntp {};
    ntp.fromDhcp = chkNtpDhcp->isChecked();
    const QStringList parts = editNtpServers->text().split(',', Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        ntp.manualServers.push_back(p.trimmed().toStdString());
    }
    if (m_onvifDevice->setNTP(ntp)) {
        QMessageBox::information(this, tr("NTP Configuration"), tr("NTP settings applied successfully."));
    } else {
        QMessageBox::critical(this, tr("NTP Configuration"), tr("Failed to apply NTP settings."));
    }
}

void OnvifCameraTab::handleSyncPcTime()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const auto utcNow = QDateTime::currentDateTimeUtc();
    const auto dt = utcNow.date();
    const auto tm = utcNow.time();

    PelcoD::Onvif::SystemDateTimeConfig cfg {};
    cfg.dateTimeType = "Manual";
    cfg.daylightSavings = false;
    cfg.timeZone = "UTC";
    cfg.year = dt.year();
    cfg.month = dt.month();
    cfg.day = dt.day();
    cfg.hour = tm.hour();
    cfg.minute = tm.minute();
    cfg.second = tm.second();

    if (m_onvifDevice->setSystemDateAndTime(cfg)) {
        QMessageBox::information(this, tr("System Time"), tr("Camera time synchronized with PC UTC clock."));
    } else {
        QMessageBox::critical(this, tr("System Time"), tr("Failed to set camera date and time."));
    }
}

void OnvifCameraTab::handleFactoryDefaultSoft()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    if (QMessageBox::warning(this, tr("Soft Factory Reset"),
            tr("Are you sure you want to perform a soft factory reset? Network parameters will be preserved."),
            QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    m_onvifDevice->setSystemFactoryDefault(false);
}

void OnvifCameraTab::handleFactoryDefaultHard()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    if (QMessageBox::critical(this, tr("Hard Factory Reset"),
            tr("WARNING: Are you sure you want to perform a hard factory reset? ALL settings and accounts will be "
               "wiped!"),
            QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    m_onvifDevice->setSystemFactoryDefault(true);
}

void OnvifCameraTab::handleNetworkUpdated(const std::vector<PelcoD::Onvif::NetworkInterfaceConfig>& ifaces)
{
    if (ifaces.empty()) {
        return;
    }
    const auto& iface = ifaces.front();
    editNetToken->setText(QString::fromStdString(iface.token));
    chkNetEnabled->setChecked(iface.enabled);
    chkNetDhcp->setChecked(iface.ipv4.dhcp);
    editNetIp->setText(QString::fromStdString(iface.ipv4.manualAddress));
    spinNetPrefix->setValue(iface.ipv4.prefixLength > 0 ? iface.ipv4.prefixLength : 24);
}

void OnvifCameraTab::handleGatewayUpdated(const QString& gateway)
{
    editNetGateway->setText(gateway);
}

void OnvifCameraTab::handleDnsUpdated(const PelcoD::Onvif::DnsConfig& dns)
{
    chkDnsDhcp->setChecked(dns.fromDhcp);
    QStringList servers {};
    for (const auto& s : dns.dnsServers) {
        servers << QString::fromStdString(s);
    }
    editDnsServers->setText(servers.join(QStringLiteral(", ")));
}

void OnvifCameraTab::handleNtpUpdated(const PelcoD::Onvif::NtpConfig& ntp)
{
    chkNtpDhcp->setChecked(ntp.fromDhcp);
    QStringList servers {};
    for (const auto& s : ntp.manualServers) {
        servers << QString::fromStdString(s);
    }
    editNtpServers->setText(servers.join(QStringLiteral(", ")));
}

void OnvifCameraTab::handleFactoryDefaultCompleted(bool success)
{
    if (success) {
        QMessageBox::information(this, tr("Factory Reset"), tr("Factory reset successfully accepted by camera."));
    } else {
        QMessageBox::critical(this, tr("Factory Reset"), tr("Factory reset rejected or failed."));
    }
}

void OnvifCameraTab::handleRecallImagingPreset()
{
    if (m_onvifDevice == nullptr || cmbImagingPresets == nullptr) {
        return;
    }
    const QString token = cmbImagingPresets->currentData().toString();
    if (!token.isEmpty()) {
        m_onvifDevice->setCurrentImagingPreset(token);
    }
}

void OnvifCameraTab::handleFocusStatusUpdated(const PelcoD::Onvif::FocusStatus20& status)
{
    if (lblFocusStatus == nullptr) {
        return;
    }
    const bool isMoving = (status.moveStatus == "MOVING");
    const QString moveStr = QString::fromStdString(status.moveStatus.empty() ? "IDLE" : status.moveStatus);
    const QString color = isMoving ? QStringLiteral("#d29922") : QStringLiteral("#7ee787");
    lblFocusStatus->setText(tr("Focus: %1 | Pos: %2").arg(moveStr).arg(QString::number(status.position, 'f', 2)));
    lblFocusStatus->setStyleSheet(QString("font-weight: bold; color: %1;").arg(color));
}

void OnvifCameraTab::handleImagingPresetsUpdated(const std::vector<PelcoD::Onvif::ImagingPreset>& presets)
{
    if (cmbImagingPresets == nullptr) {
        return;
    }
    const QString previousToken = cmbImagingPresets->currentData().toString();
    const QSignalBlocker blocker(cmbImagingPresets);
    cmbImagingPresets->clear();

    for (const auto& p : presets) {
        const QString label = QString::fromStdString(p.name.empty() ? p.token : p.name);
        cmbImagingPresets->addItem(label, QString::fromStdString(p.token));
    }

    int selectIndex = cmbImagingPresets->findData(previousToken);
    if (selectIndex < 0 && cmbImagingPresets->count() > 0) {
        selectIndex = 0;
    }
    if (selectIndex >= 0) {
        cmbImagingPresets->setCurrentIndex(selectIndex);
    }
}

void OnvifCameraTab::handleRefreshRelays()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRelayOutputs();
    }
}

void OnvifCameraTab::handleActivateRelay()
{
    if (m_onvifDevice == nullptr || editRelayToken == nullptr) {
        return;
    }
    const QString token = editRelayToken->text().trimmed();
    if (!token.isEmpty()) {
        m_onvifDevice->setRelayOutputState(token, true);
    }
}

void OnvifCameraTab::handleDeactivateRelay()
{
    if (m_onvifDevice == nullptr || editRelayToken == nullptr) {
        return;
    }
    const QString token = editRelayToken->text().trimmed();
    if (!token.isEmpty()) {
        m_onvifDevice->setRelayOutputState(token, false);
    }
}

void OnvifCameraTab::handleApplyRelaySettings()
{
    if (m_onvifDevice == nullptr || editRelayToken == nullptr || cmbRelayMode == nullptr || spinRelayDelay == nullptr
        || cmbRelayIdleState == nullptr) {
        return;
    }
    const QString token = editRelayToken->text().trimmed();
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Relay Settings"), tr("Please select or enter a relay token."));
        return;
    }
    PelcoD::Onvif::RelayOutputConfig cfg {};
    cfg.token = token.toStdString();
    cfg.mode = PelcoD::Onvif::relayModeFromString(cmbRelayMode->currentText().toStdString());
    cfg.delayTimeSeconds = static_cast<float>(spinRelayDelay->value());
    cfg.idleState = PelcoD::Onvif::relayIdleStateFromString(cmbRelayIdleState->currentText().toStdString());

    if (m_onvifDevice->setRelayOutputSettings(token, cfg)) {
        QMessageBox::information(this, tr("Relay Settings"), tr("Relay settings updated successfully."));
    } else {
        QMessageBox::warning(this, tr("Relay Settings"), tr("Failed to update relay settings."));
    }
}

void OnvifCameraTab::handleRefreshInputs()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshDigitalInputs();
    }
}

void OnvifCameraTab::handleRelaysUpdated(const std::vector<PelcoD::Onvif::RelayOutputConfig>& relays)
{
    if (tableRelays == nullptr) {
        return;
    }
    tableRelays->setRowCount(0);
    for (const auto& r : relays) {
        const int row = tableRelays->rowCount();
        tableRelays->insertRow(row);
        tableRelays->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.token)));
        tableRelays->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(PelcoD::Onvif::relayModeToString(r.mode))));
        tableRelays->setItem(row, 2, new QTableWidgetItem(QString::number(r.delayTimeSeconds, 'f', 1)));
        tableRelays->setItem(
            row, 3, new QTableWidgetItem(QString::fromStdString(PelcoD::Onvif::relayIdleStateToString(r.idleState))));
        const QString stateStr = QString::fromStdString(PelcoD::Onvif::relayLogicalStateToString(r.logicalState));
        auto* itemState = new QTableWidgetItem(stateStr);
        if (r.logicalState == PelcoD::Onvif::RelayLogicalState::Active) {
            itemState->setForeground(QBrush(QColor("#7ee787")));
        } else {
            itemState->setForeground(QBrush(QColor("#8b949e")));
        }
        tableRelays->setItem(row, 4, itemState);
    }
}

void OnvifCameraTab::handleDigitalInputsUpdated(const std::vector<PelcoD::Onvif::DigitalInputConfig>& inputs)
{
    if (tableDigitalInputs == nullptr) {
        return;
    }
    tableDigitalInputs->setRowCount(0);
    for (const auto& in : inputs) {
        const int row = tableDigitalInputs->rowCount();
        tableDigitalInputs->insertRow(row);
        tableDigitalInputs->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(in.token)));
        tableDigitalInputs->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(PelcoD::Onvif::relayIdleStateToString(in.idleState))));
        tableDigitalInputs->setItem(row, 2, new QTableWidgetItem(in.active ? tr("Active") : tr("Inactive")));
    }
}

void OnvifCameraTab::handleRelaySelectionChanged()
{
    if (tableRelays == nullptr || editRelayToken == nullptr || cmbRelayMode == nullptr || spinRelayDelay == nullptr
        || cmbRelayIdleState == nullptr) {
        return;
    }
    const int row = tableRelays->currentRow();
    if (row < 0 || row >= tableRelays->rowCount()) {
        return;
    }
    if (tableRelays->item(row, 0) != nullptr) {
        editRelayToken->setText(tableRelays->item(row, 0)->text());
    }
    if (tableRelays->item(row, 1) != nullptr) {
        const int idx = cmbRelayMode->findText(tableRelays->item(row, 1)->text());
        if (idx >= 0) {
            cmbRelayMode->setCurrentIndex(idx);
        }
    }
    if (tableRelays->item(row, 2) != nullptr) {
        spinRelayDelay->setValue(tableRelays->item(row, 2)->text().toDouble());
    }
    if (tableRelays->item(row, 3) != nullptr) {
        const int idx = cmbRelayIdleState->findText(tableRelays->item(row, 3)->text());
        if (idx >= 0) {
            cmbRelayIdleState->setCurrentIndex(idx);
        }
    }
}

} // namespace PelcoDApp
