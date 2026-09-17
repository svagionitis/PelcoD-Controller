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
    // Top Row: Discovery & Authentication
    // =========================================================================
    auto* groupDiscovery = new QGroupBox(tr("ONVIF Discovery & Authentication"), this);
    auto* gridDisc = new QGridLayout(groupDiscovery);
    gridDisc->setContentsMargins(8, 8, 8, 8);
    gridDisc->setSpacing(6);

    btnDiscover = new QPushButton(tr("🔍 Discover Cameras"), groupDiscovery);
    btnDiscover->setToolTip(tr("Send WS-Discovery multicast probe on LAN (port 3702)"));
    cmbDiscovered = new QComboBox(groupDiscovery);
    cmbDiscovered->addItem(tr("-- Select Discovered Device --"), -1);

    gridDisc->addWidget(btnDiscover, 0, 0);
    gridDisc->addWidget(cmbDiscovered, 0, 1, 1, 3);

    gridDisc->addWidget(new QLabel(tr("Endpoint URL:"), groupDiscovery), 1, 0);
    editEndpoint = new QLineEdit(groupDiscovery);
    editEndpoint->setPlaceholderText("http://192.168.1.100/onvif/device_service");
    editEndpoint->setText("http://192.168.1.100/onvif/device_service");
    gridDisc->addWidget(editEndpoint, 1, 1, 1, 3);

    gridDisc->addWidget(new QLabel(tr("User:"), groupDiscovery), 2, 0);
    editUsername = new QLineEdit(groupDiscovery);
    editUsername->setText("admin");
    gridDisc->addWidget(editUsername, 2, 1);

    gridDisc->addWidget(new QLabel(tr("Pass:"), groupDiscovery), 2, 2);
    editPassword = new QLineEdit(groupDiscovery);
    editPassword->setEchoMode(QLineEdit::Password);
    gridDisc->addWidget(editPassword, 2, 3);

    auto* connBtnLayout = new QHBoxLayout();
    btnConnect = new QPushButton(tr("Connect"), groupDiscovery);
    btnConnect->setStyleSheet("QPushButton { font-weight: bold; background-color: #238636; color: white; }");
    btnDisconnect = new QPushButton(tr("Disconnect"), groupDiscovery);
    lblConnectionStatus = new QLabel(tr("Disconnected"), groupDiscovery);
    lblConnectionStatus->setStyleSheet("color: #8b949e; font-weight: bold;");

    connBtnLayout->addWidget(btnConnect);
    connBtnLayout->addWidget(btnDisconnect);
    connBtnLayout->addWidget(lblConnectionStatus);
    connBtnLayout->addStretch();
    gridDisc->addLayout(connBtnLayout, 3, 0, 1, 4);

    mainLayout->addWidget(groupDiscovery);

    // =========================================================================
    // Middle Splitter: Device Info / Video & PTZ / Presets
    // =========================================================================
    auto* midSplitter = new QSplitter(Qt::Horizontal, this);

    // Panel Left: Device Information & Media Profiles
    auto* leftContainer = new QWidget(midSplitter);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    auto* groupInfo = new QGroupBox(tr("Device Identification & Streams"), leftContainer);
    auto* infoGrid = new QGridLayout(groupInfo);
    infoGrid->setContentsMargins(6, 6, 6, 6);
    infoGrid->setSpacing(4);

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

    infoGrid->addWidget(new QLabel(tr("Media Profile:"), groupInfo), 5, 0);
    cmbProfiles = new QComboBox(groupInfo);
    infoGrid->addWidget(cmbProfiles, 5, 1);

    infoGrid->addWidget(new QLabel(tr("RTSP URI:"), groupInfo), 6, 0);
    editRtspUri = new QLineEdit(groupInfo);
    editRtspUri->setReadOnly(true);
    infoGrid->addWidget(editRtspUri, 6, 1);

    auto* rtspBtnLayout = new QHBoxLayout();
    btnCopyRtsp = new QPushButton(tr("Copy RTSP"), groupInfo);
    btnStreamInVideoTab = new QPushButton(tr("▶ Open in Video Tab"), groupInfo);
    btnStreamInVideoTab->setStyleSheet("QPushButton { font-weight: bold; background-color: #1f6feb; color: white; }");
    rtspBtnLayout->addWidget(btnCopyRtsp);
    rtspBtnLayout->addWidget(btnStreamInVideoTab);
    infoGrid->addLayout(rtspBtnLayout, 7, 0, 1, 2);

    infoGrid->addWidget(new QLabel(tr("Snapshot URI:"), groupInfo), 8, 0);
    editSnapshotUri = new QLineEdit(groupInfo);
    editSnapshotUri->setReadOnly(true);
    infoGrid->addWidget(editSnapshotUri, 8, 1);

    // Maintenance button
    btnReboot = new QPushButton(tr("⚠ Reboot Camera"), groupInfo);
    btnReboot->setStyleSheet("QPushButton { color: #f85149; }");
    infoGrid->addWidget(btnReboot, 9, 0, 1, 2);

    leftLayout->addWidget(groupInfo);

    // =========================================================================
    // Profile T: Optical & Imaging Controls
    // =========================================================================
    auto* groupImaging = new QGroupBox(tr("Profile T: Optical & Imaging Controls"), leftContainer);
    auto* imgLayout = new QGridLayout(groupImaging);
    imgLayout->setSpacing(4);

    // Brightness
    imgLayout->addWidget(new QLabel(tr("Brightness:"), groupImaging), 0, 0);
    sliderBrightness = new QSlider(Qt::Horizontal, groupImaging);
    sliderBrightness->setRange(0, 100);
    sliderBrightness->setValue(50);
    lblBrightnessVal = new QLabel("50", groupImaging);
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
    lblContrastVal = new QLabel("50", groupImaging);
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
    lblSaturationVal = new QLabel("50", groupImaging);
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
    lblSharpnessVal = new QLabel("50", groupImaging);
    lblSharpnessVal->setFixedWidth(28);
    connect(
        sliderSharpness, &QSlider::valueChanged, this, [this](int v) { lblSharpnessVal->setText(QString::number(v)); });
    imgLayout->addWidget(sliderSharpness, 3, 1);
    imgLayout->addWidget(lblSharpnessVal, 3, 2);

    // IR Filter & WDR/BLC
    imgLayout->addWidget(new QLabel(tr("IR Filter:"), groupImaging), 4, 0);
    cmbIrFilter = new QComboBox(groupImaging);
    cmbIrFilter->addItems({ "AUTO", "ON", "OFF" });
    imgLayout->addWidget(cmbIrFilter, 4, 1, 1, 2);

    auto* chkLayout = new QHBoxLayout();
    chkBacklight = new QCheckBox(tr("BLC"), groupImaging);
    chkWdr = new QCheckBox(tr("WDR"), groupImaging);
    chkLayout->addWidget(chkBacklight);
    chkLayout->addWidget(chkWdr);
    imgLayout->addLayout(chkLayout, 5, 0, 1, 3);

    // Focus controls
    auto* focusLayout = new QHBoxLayout();
    cmbAutoFocus = new QComboBox(groupImaging);
    cmbAutoFocus->addItems({ "AUTO", "MANUAL" });
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
    btnApplyImaging->setStyleSheet("QPushButton { font-weight: bold; background-color: #238636; color: white; }");
    connect(btnRefreshImaging, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshImaging);
    connect(btnApplyImaging, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyImaging);
    imgBtnLayout->addWidget(btnRefreshImaging);
    imgBtnLayout->addWidget(btnApplyImaging);
    imgLayout->addLayout(imgBtnLayout, 7, 0, 1, 3);

    leftLayout->addWidget(groupImaging);
    leftLayout->addStretch();
    midSplitter->addWidget(leftContainer);

    // Panel Right: PTZ Controls & Presets
    auto* rightContainer = new QWidget(midSplitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto* groupPtz = new QGroupBox(tr("PTZ Motion & Positioning"), rightContainer);
    auto* ptzLayout = new QVBoxLayout(groupPtz);

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

    jogGrid->addWidget(makeJogBtn("↖", -1.0, 1.0), 0, 0);
    jogGrid->addWidget(makeJogBtn("▲", 0.0, 1.0), 0, 1);
    jogGrid->addWidget(makeJogBtn("↗", 1.0, 1.0), 0, 2);

    jogGrid->addWidget(makeJogBtn("◀", -1.0, 0.0), 1, 0);
    auto* btnStop = new QPushButton(tr("STOP"), groupPtz);
    btnStop->setFixedSize(50, 40);
    btnStop->setStyleSheet("QPushButton { font-weight: bold; background-color: #da3633; color: white; }");
    connect(btnStop, &QPushButton::clicked, this, &OnvifCameraTab::handleStopMotion);
    jogGrid->addWidget(btnStop, 1, 1);
    jogGrid->addWidget(makeJogBtn("▶", 1.0, 0.0), 1, 2);

    jogGrid->addWidget(makeJogBtn("↙", -1.0, -1.0), 2, 0);
    jogGrid->addWidget(makeJogBtn("▼", 0.0, -1.0), 2, 1);
    jogGrid->addWidget(makeJogBtn("↘", 1.0, -1.0), 2, 2);

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
    lblSpeedVal = new QLabel("0.5", groupPtz);
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

    rightLayout->addWidget(groupPtz);

    // Presets GroupBox
    auto* groupPresets = new QGroupBox(tr("Stored Presets"), rightContainer);
    auto* presetsLayout = new QVBoxLayout(groupPresets);

    tablePresets = new QTableWidget(0, 2, groupPresets);
    tablePresets->setHorizontalHeaderLabels({ tr("Token"), tr("Label / Name") });
    tablePresets->horizontalHeader()->setStretchLastSection(true);
    tablePresets->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePresets->setSelectionMode(QAbstractItemView::SingleSelection);
    tablePresets->setFixedHeight(120);

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

    rightLayout->addWidget(groupPresets);

    // =========================================================================
    // Profile T: Live Event Monitor
    // =========================================================================
    auto* groupEvents = new QGroupBox(tr("Profile T: Live Event Monitor"), rightContainer);
    auto* eventsLayout = new QVBoxLayout(groupEvents);

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
    tableEvents->setFixedHeight(120);
    eventsLayout->addWidget(tableEvents);

    rightLayout->addWidget(groupEvents);
    rightLayout->addStretch();
    midSplitter->addWidget(rightContainer);

    mainLayout->addWidget(midSplitter, 1);

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
