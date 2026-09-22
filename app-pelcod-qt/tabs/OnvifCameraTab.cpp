/// @file OnvifCameraTab.cpp
/// @brief Dashboard tab for discovering, connecting to, and controlling ONVIF Profile S IP cameras.

#include "OnvifCameraTab.h"
#include "VideoStreamTab.h"
#include <Onvif/GeodesyUtils.h>
#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFontDatabase>
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
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::metadataConfigurationsUpdated, this,
            &OnvifCameraTab::handleMetadataConfigsUpdated);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::metadataReceived, this, &OnvifCameraTab::handleMetadataReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::systemLogReceived, this,
            &OnvifCameraTab::handleSystemLogReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::systemSupportInfoReceived, this,
            &OnvifCameraTab::handleSystemSupportInfoReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::systemBackupReceived, this,
            &OnvifCameraTab::handleSystemBackupReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::systemRestoreCompleted, this,
            &OnvifCameraTab::handleSystemRestoreCompleted);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::endpointReferenceReceived, this,
            &OnvifCameraTab::handleEndpointReferenceReceived);

        // PKI Certificates & TLS Security
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::certificatesUpdated, this,
            &OnvifCameraTab::handleCertificatesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::certificateInfoReceived, this,
            &OnvifCameraTab::handleCertificateInfoReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::pkcs10CsrReceived, this,
            &OnvifCameraTab::handlePkcs10CsrReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::clientCertificateModeUpdated, this,
            &OnvifCameraTab::handleClientCertModeUpdated);

        // Profile G Recordings & Replay
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::recordingsUpdated, this,
            &OnvifCameraTab::handleRecordingsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::recordingJobsUpdated, this,
            &OnvifCameraTab::handleRecordingJobsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::recordingSummaryUpdated, this,
            &OnvifCameraTab::handleRecordingSummaryUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::recordingSearchResultsReceived, this,
            &OnvifCameraTab::handleRecordingSearchResultsReceived);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::eventSearchResultsReceived, this,
            &OnvifCameraTab::handleEventSearchResultsReceived);

        // Profile M & T Analytics Rules & Modules
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::rulesUpdated, this, &OnvifCameraTab::handleRulesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::supportedRulesUpdated, this,
            &OnvifCameraTab::handleSupportedRulesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::analyticsModulesUpdated, this,
            &OnvifCameraTab::handleAnalyticsModulesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::replayUriResolved, this,
            &OnvifCameraTab::handleReplayUriResolved);

        // Geolocation & GeoMove
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::geoLocationUpdated, this,
            &OnvifCameraTab::handleGeoLocationUpdated);
        connect(
            m_onvifDevice, &PelcoD::Qt::QOnvifDevice::geoMoveCompleted, this, &OnvifCameraTab::handleGeoMoveCompleted);

        // Profile T: Privacy Masks & Video Source Modes
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::masksUpdated, this, &OnvifCameraTab::handleMasksUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::videoSourceModesUpdated, this,
            &OnvifCameraTab::handleVideoSourceModesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::videoSourceModeChanged, this,
            &OnvifCameraTab::handleVideoSourceModeChanged);

        // Thermal & Radiometry Service
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::radiometryConfigurationUpdated, this,
            &OnvifCameraTab::handleRadiometryConfigUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::radiometrySpotsUpdated, this,
            &OnvifCameraTab::handleRadiometrySpotsUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::radiometryBoxesUpdated, this,
            &OnvifCameraTab::handleRadiometryBoxesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::colorPalettesUpdated, this,
            &OnvifCameraTab::handleColorPalettesUpdated);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::nucTriggered, this, &OnvifCameraTab::handleNucTriggered);
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

    // -------------------------------------------------------------------------
    // Camera Geolocation & Mounting (WGS84)
    // -------------------------------------------------------------------------
    auto* groupGeoLoc = new QGroupBox(tr("Camera Geolocation & Mounting (WGS84)"), groupPtz);
    auto* gridGeoLoc = new QGridLayout(groupGeoLoc);
    gridGeoLoc->setSpacing(6);

    gridGeoLoc->addWidget(new QLabel(tr("Latitude:"), groupGeoLoc), 0, 0);
    spinCameraLat = new QDoubleSpinBox(groupGeoLoc);
    spinCameraLat->setRange(-90.0, 90.0);
    spinCameraLat->setDecimals(6);
    spinCameraLat->setSingleStep(0.0001);
    spinCameraLat->setValue(37.7749);
    gridGeoLoc->addWidget(spinCameraLat, 0, 1);

    gridGeoLoc->addWidget(new QLabel(tr("Longitude:"), groupGeoLoc), 0, 2);
    spinCameraLon = new QDoubleSpinBox(groupGeoLoc);
    spinCameraLon->setRange(-180.0, 180.0);
    spinCameraLon->setDecimals(6);
    spinCameraLon->setSingleStep(0.0001);
    spinCameraLon->setValue(-122.4194);
    gridGeoLoc->addWidget(spinCameraLon, 0, 3);

    gridGeoLoc->addWidget(new QLabel(tr("Altitude (m):"), groupGeoLoc), 0, 4);
    spinCameraElev = new QDoubleSpinBox(groupGeoLoc);
    spinCameraElev->setRange(-1000.0, 10000.0);
    spinCameraElev->setDecimals(2);
    spinCameraElev->setValue(10.0);
    gridGeoLoc->addWidget(spinCameraElev, 0, 5);

    gridGeoLoc->addWidget(new QLabel(tr("Heading / Yaw (°):"), groupGeoLoc), 1, 0);
    spinCameraYaw = new QDoubleSpinBox(groupGeoLoc);
    spinCameraYaw->setRange(0.0, 359.99);
    spinCameraYaw->setDecimals(2);
    spinCameraYaw->setValue(0.0);
    gridGeoLoc->addWidget(spinCameraYaw, 1, 1);

    gridGeoLoc->addWidget(new QLabel(tr("Pitch (°):"), groupGeoLoc), 1, 2);
    spinCameraPitch = new QDoubleSpinBox(groupGeoLoc);
    spinCameraPitch->setRange(-90.0, 90.0);
    spinCameraPitch->setDecimals(2);
    spinCameraPitch->setValue(0.0);
    gridGeoLoc->addWidget(spinCameraPitch, 1, 3);

    btnRefreshGeoLoc = new QPushButton(tr("⟳ Query Location"), groupGeoLoc);
    btnSaveGeoLoc = new QPushButton(tr("💾 Save Location"), groupGeoLoc);
    btnSaveGeoLoc->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #238636; color: white; }"));
    gridGeoLoc->addWidget(btnRefreshGeoLoc, 1, 4);
    gridGeoLoc->addWidget(btnSaveGeoLoc, 1, 5);

    ptzLayout->addWidget(groupGeoLoc);

    // -------------------------------------------------------------------------
    // PTZ GeoMove & Spherical Coordinate Spaces
    // -------------------------------------------------------------------------
    auto* groupGeoMove = new QGroupBox(tr("PTZ GeoMove & Spherical Coordinate Spaces"), groupPtz);
    auto* gridGeoMove = new QGridLayout(groupGeoMove);
    gridGeoMove->setSpacing(6);

    gridGeoMove->addWidget(new QLabel(tr("Target Lat:"), groupGeoMove), 0, 0);
    spinTargetLat = new QDoubleSpinBox(groupGeoMove);
    spinTargetLat->setRange(-90.0, 90.0);
    spinTargetLat->setDecimals(6);
    spinTargetLat->setSingleStep(0.0001);
    spinTargetLat->setValue(37.7780);
    gridGeoMove->addWidget(spinTargetLat, 0, 1);

    gridGeoMove->addWidget(new QLabel(tr("Target Lon:"), groupGeoMove), 0, 2);
    spinTargetLon = new QDoubleSpinBox(groupGeoMove);
    spinTargetLon->setRange(-180.0, 180.0);
    spinTargetLon->setDecimals(6);
    spinTargetLon->setSingleStep(0.0001);
    spinTargetLon->setValue(-122.4150);
    gridGeoMove->addWidget(spinTargetLon, 0, 3);

    gridGeoMove->addWidget(new QLabel(tr("Target Alt (m):"), groupGeoMove), 0, 4);
    spinTargetElev = new QDoubleSpinBox(groupGeoMove);
    spinTargetElev->setRange(-1000.0, 10000.0);
    spinTargetElev->setDecimals(2);
    spinTargetElev->setValue(5.0);
    gridGeoMove->addWidget(spinTargetElev, 0, 5);

    gridGeoMove->addWidget(new QLabel(tr("Area Width (m):"), groupGeoMove), 1, 0);
    spinTargetWidth = new QDoubleSpinBox(groupGeoMove);
    spinTargetWidth->setRange(0.0, 1000.0);
    spinTargetWidth->setDecimals(1);
    spinTargetWidth->setValue(0.0);
    gridGeoMove->addWidget(spinTargetWidth, 1, 1);

    gridGeoMove->addWidget(new QLabel(tr("Area Height (m):"), groupGeoMove), 1, 2);
    spinTargetHeight = new QDoubleSpinBox(groupGeoMove);
    spinTargetHeight->setRange(0.0, 1000.0);
    spinTargetHeight->setDecimals(1);
    spinTargetHeight->setValue(0.0);
    gridGeoMove->addWidget(spinTargetHeight, 1, 3);

    btnExecuteGeoMove = new QPushButton(tr("🎯 GeoMove to Target"), groupGeoMove);
    btnExecuteGeoMove->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #1f6feb; color: white; padding: 4px 10px; }"));
    gridGeoMove->addWidget(btnExecuteGeoMove, 1, 4, 1, 2);

    // Live Readout Row
    auto* readoutLayout = new QHBoxLayout();
    lblComputedGeoBearing = new QLabel(tr("Bearing: 0.00°"), groupGeoMove);
    lblComputedGeoBearing->setStyleSheet(QStringLiteral("color: #58a6ff; font-weight: bold;"));
    lblComputedGeoTilt = new QLabel(tr("Tilt: 0.00°"), groupGeoMove);
    lblComputedGeoTilt->setStyleSheet(QStringLiteral("color: #58a6ff; font-weight: bold;"));
    lblComputedGeoDistance = new QLabel(tr("Slant Dist: 0.0 m"), groupGeoMove);
    lblComputedGeoDistance->setStyleSheet(QStringLiteral("color: #58a6ff; font-weight: bold;"));
    readoutLayout->addWidget(lblComputedGeoBearing);
    readoutLayout->addWidget(lblComputedGeoTilt);
    readoutLayout->addWidget(lblComputedGeoDistance);
    readoutLayout->addStretch();
    gridGeoMove->addLayout(readoutLayout, 2, 0, 1, 6);

    // Direct Spherical Row
    gridGeoMove->addWidget(new QLabel(tr("Azimuth (°):"), groupGeoMove), 3, 0);
    spinSphericalAzimuth = new QDoubleSpinBox(groupGeoMove);
    spinSphericalAzimuth->setRange(0.0, 359.99);
    spinSphericalAzimuth->setDecimals(2);
    spinSphericalAzimuth->setValue(0.0);
    gridGeoMove->addWidget(spinSphericalAzimuth, 3, 1);

    gridGeoMove->addWidget(new QLabel(tr("Elevation (°):"), groupGeoMove), 3, 2);
    spinSphericalElevation = new QDoubleSpinBox(groupGeoMove);
    spinSphericalElevation->setRange(-90.0, 90.0);
    spinSphericalElevation->setDecimals(2);
    spinSphericalElevation->setValue(0.0);
    gridGeoMove->addWidget(spinSphericalElevation, 3, 3);

    gridGeoMove->addWidget(new QLabel(tr("Zoom [0-1]:"), groupGeoMove), 3, 4);
    spinSphericalZoom = new QDoubleSpinBox(groupGeoMove);
    spinSphericalZoom->setRange(0.0, 1.0);
    spinSphericalZoom->setSingleStep(0.05);
    spinSphericalZoom->setDecimals(2);
    spinSphericalZoom->setValue(0.0);
    gridGeoMove->addWidget(spinSphericalZoom, 3, 5);

    btnExecuteSphericalMove = new QPushButton(tr("Move to Spherical (deg)"), groupGeoMove);
    gridGeoMove->addWidget(btnExecuteSphericalMove, 4, 4, 1, 2);

    ptzLayout->addWidget(groupGeoMove);

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
    configureTable(tablePresets, { tr("Token"), tr("Label / Name") });
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
    configureTable(tableTourSpots, { tr("Preset Token"), tr("Speed (0.0 - 1.0)"), tr("Stay Time (sec)") });
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
    configureTable(tableEvents, { tr("Time"), tr("Topic"), tr("Item"), tr("Value") });
    tableEvents->setMinimumHeight(200);
    eventsLayout->addWidget(tableEvents);

    eventsTabLayout->addWidget(groupEvents);
    eventsTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 5: OSD Overlays
    // -------------------------------------------------------------------------
    auto* osdTabLayout = createScrollTab(tr("OSD & Privacy Masks"));

    auto* groupOsd = new QGroupBox(tr("On-Screen Display (OSD) Overlays"), this);
    auto* osdLayout = new QVBoxLayout(groupOsd);
    osdLayout->setSpacing(6);

    tableOsds = new QTableWidget(0, 5, groupOsd);
    configureTable(tableOsds, { tr("Token"), tr("Type"), tr("Position"), tr("Font Size"), tr("Content") });
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

    auto* groupMasks = new QGroupBox(tr("Profile T: Privacy Masks"), this);
    auto* maskLayout = new QVBoxLayout(groupMasks);
    maskLayout->setSpacing(6);

    tableMasks = new QTableWidget(0, 5, groupMasks);
    configureTable(tableMasks, { tr("Token"), tr("Config Token"), tr("Type"), tr("Color (RGB)"), tr("Enabled") });
    tableMasks->setMinimumHeight(160);
    maskLayout->addWidget(tableMasks);

    auto* editMaskGrid = new QGridLayout();
    editMaskGrid->addWidget(new QLabel(tr("Mask Token:"), groupMasks), 0, 0);
    editMaskToken = new QLineEdit(groupMasks);
    editMaskToken->setPlaceholderText(tr("e.g. Mask_1 (auto-generated if empty)"));
    editMaskGrid->addWidget(editMaskToken, 0, 1);

    editMaskGrid->addWidget(new QLabel(tr("Type:"), groupMasks), 0, 2);
    cmbMaskType = new QComboBox(groupMasks);
    cmbMaskType->addItems({ tr("Color"), tr("Pixelated"), tr("Blurred") });
    editMaskGrid->addWidget(cmbMaskType, 0, 3);

    auto* colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel(tr("R:"), groupMasks));
    spinMaskColorR = new QSpinBox(groupMasks);
    spinMaskColorR->setRange(0, 255);
    spinMaskColorR->setValue(0);
    colorLayout->addWidget(spinMaskColorR);

    colorLayout->addWidget(new QLabel(tr("G:"), groupMasks));
    spinMaskColorG = new QSpinBox(groupMasks);
    spinMaskColorG->setRange(0, 255);
    spinMaskColorG->setValue(0);
    colorLayout->addWidget(spinMaskColorG);

    colorLayout->addWidget(new QLabel(tr("B:"), groupMasks));
    spinMaskColorB = new QSpinBox(groupMasks);
    spinMaskColorB->setRange(0, 255);
    spinMaskColorB->setValue(0);
    colorLayout->addWidget(spinMaskColorB);

    editMaskGrid->addWidget(new QLabel(tr("Color:"), groupMasks), 1, 0);
    editMaskGrid->addLayout(colorLayout, 1, 1);

    chkMaskEnabled = new QCheckBox(tr("Mask Enabled"), groupMasks);
    chkMaskEnabled->setChecked(true);
    editMaskGrid->addWidget(chkMaskEnabled, 1, 2, 1, 2);

    maskLayout->addLayout(editMaskGrid);

    auto* maskBtnLayout = new QHBoxLayout();
    btnRefreshMasks = new QPushButton(tr("Refresh"), groupMasks);
    btnAddMask = new QPushButton(tr("Add Mask"), groupMasks);
    btnUpdateMask = new QPushButton(tr("Update Selected"), groupMasks);
    btnDeleteMask = new QPushButton(tr("Delete Selected"), groupMasks);
    maskBtnLayout->addWidget(btnRefreshMasks);
    maskBtnLayout->addWidget(btnAddMask);
    maskBtnLayout->addWidget(btnUpdateMask);
    maskBtnLayout->addWidget(btnDeleteMask);
    maskBtnLayout->addStretch();
    maskLayout->addLayout(maskBtnLayout);

    connect(btnRefreshMasks, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshMasks);
    connect(btnAddMask, &QPushButton::clicked, this, &OnvifCameraTab::handleAddMask);
    connect(btnUpdateMask, &QPushButton::clicked, this, &OnvifCameraTab::handleUpdateMask);
    connect(btnDeleteMask, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteMask);
    connect(tableMasks, &QTableWidget::itemSelectionChanged, this, &OnvifCameraTab::handleMaskSelectionChanged);

    osdTabLayout->addWidget(groupOsd);
    osdTabLayout->addWidget(groupMasks);
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

    auto* groupModes
        = new QGroupBox(tr("Profile T: Video Source Modes (Sensor Capture Modes)"), devTabLayout->parentWidget());
    auto* modesGrid = new QGridLayout(groupModes);
    modesGrid->setSpacing(6);

    modesGrid->addWidget(new QLabel(tr("Capture Mode:"), groupModes), 0, 0);
    cmbVideoSourceModes = new QComboBox(groupModes);
    modesGrid->addWidget(cmbVideoSourceModes, 0, 1);

    lblVideoSourceModeInfo = new QLabel(tr("-"), groupModes);
    lblVideoSourceModeInfo->setStyleSheet(QStringLiteral("color: #8b949e;"));
    modesGrid->addWidget(lblVideoSourceModeInfo, 1, 0, 1, 2);

    auto* modeBtnLayout = new QHBoxLayout();
    btnRefreshVideoSourceModes = new QPushButton(tr("Refresh Modes"), groupModes);
    btnApplyVideoSourceMode = new QPushButton(tr("Apply Mode"), groupModes);
    modeBtnLayout->addWidget(btnRefreshVideoSourceModes);
    modeBtnLayout->addWidget(btnApplyVideoSourceMode);
    modeBtnLayout->addStretch();
    modesGrid->addLayout(modeBtnLayout, 2, 0, 1, 2);

    connect(btnRefreshVideoSourceModes, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshVideoSourceModes);
    connect(btnApplyVideoSourceMode, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyVideoSourceMode);

    devTabLayout->addWidget(groupModes);

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
    configureTable(tableUsers, { tr("Username"), tr("User Level"), tr("Password Status") });
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

    // Maintenance Extensions: System Logs & Support Diagnostics
    auto* groupLogs = new QGroupBox(tr("System Logs & Diagnostics"), this);
    auto* logsLayout = new QVBoxLayout(groupLogs);
    logsLayout->setSpacing(6);

    auto* logBtnLayout = new QHBoxLayout();
    btnFetchSystemLog = new QPushButton(tr("📄 Fetch System Log"), groupLogs);
    btnFetchAccessLog = new QPushButton(tr("🔒 Fetch Access Log"), groupLogs);
    btnFetchSupportInfo = new QPushButton(tr("🛠 Diagnostics / Support Info"), groupLogs);
    btnFetchEndpointRef = new QPushButton(tr("🆔 Endpoint UUID"), groupLogs);
    lblEndpointRef = new QLabel(tr("UUID: N/A"), groupLogs);
    lblEndpointRef->setStyleSheet(QStringLiteral("color: #58a6ff; font-family: monospace; font-weight: bold;"));

    logBtnLayout->addWidget(btnFetchSystemLog);
    logBtnLayout->addWidget(btnFetchAccessLog);
    logBtnLayout->addWidget(btnFetchSupportInfo);
    logBtnLayout->addWidget(btnFetchEndpointRef);
    logBtnLayout->addWidget(lblEndpointRef);
    logBtnLayout->addStretch();
    logsLayout->addLayout(logBtnLayout);

    txtSystemLogs = new QTextEdit(groupLogs);
    txtSystemLogs->setReadOnly(true);
    txtSystemLogs->setMinimumHeight(140);
    txtSystemLogs->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    txtSystemLogs->setPlaceholderText(tr("System and access diagnostic log output will appear here..."));
    logsLayout->addWidget(txtSystemLogs);

    netTabLayout->addWidget(groupLogs);

    // Maintenance Extensions: Configuration Backup & Restore
    auto* groupBackup = new QGroupBox(tr("Configuration Backup & Restore"), this);
    auto* backupLayout = new QHBoxLayout(groupBackup);
    backupLayout->setSpacing(6);

    btnDownloadBackup = new QPushButton(tr("📥 Download Backup XML"), groupBackup);
    editBackupPayload = new QLineEdit(groupBackup);
    editBackupPayload->setPlaceholderText(tr("Base64 or XML configuration backup payload"));
    btnRestoreBackup = new QPushButton(tr("📤 Restore Camera Configuration"), groupBackup);

    backupLayout->addWidget(btnDownloadBackup);
    backupLayout->addWidget(editBackupPayload, 1);
    backupLayout->addWidget(btnRestoreBackup);

    netTabLayout->addWidget(groupBackup);

    // Maintenance Extensions: PKI Certificates & HTTPS/TLS Security
    auto* groupCerts = new QGroupBox(tr("PKI Certificates & HTTPS/TLS Security"), this);
    auto* certsLayout = new QVBoxLayout(groupCerts);
    certsLayout->setSpacing(6);

    tableCertificates = new QTableWidget(0, 6, groupCerts);
    configureTable(tableCertificates,
        { tr("Certificate ID"), tr("Subject DN"), tr("Issuer DN"), tr("Valid From"), tr("Valid Until"),
            tr("Key Usage") });
    tableCertificates->setMinimumHeight(120);
    certsLayout->addWidget(tableCertificates);

    auto* certActionLayout = new QHBoxLayout();
    btnRefreshCerts = new QPushButton(tr("🔄 Refresh Certificates"), groupCerts);
    btnDeleteCert = new QPushButton(tr("🗑 Delete Certificate"), groupCerts);
    certActionLayout->addWidget(btnRefreshCerts);
    certActionLayout->addWidget(btnDeleteCert);
    certActionLayout->addStretch();
    certsLayout->addLayout(certActionLayout);

    // Create / CSR generation form
    auto* genFormLayout = new QHBoxLayout();
    editNewCertId = new QLineEdit(groupCerts);
    editNewCertId->setPlaceholderText(tr("Cert ID (e.g. Cert_1)"));
    editNewCertSubject = new QLineEdit(groupCerts);
    editNewCertSubject->setPlaceholderText(tr("Subject (e.g. CN=Camera1)"));
    spinNewCertDays = new QSpinBox(groupCerts);
    spinNewCertDays->setRange(1, 3650);
    spinNewCertDays->setValue(365);
    spinNewCertDays->setSuffix(tr(" days"));
    btnCreateSelfSignedCert = new QPushButton(tr("🔏 Create Self-Signed"), groupCerts);
    btnGenerateCsr = new QPushButton(tr("📜 Generate PKCS#10 CSR"), groupCerts);

    genFormLayout->addWidget(new QLabel(tr("ID:"), groupCerts));
    genFormLayout->addWidget(editNewCertId);
    genFormLayout->addWidget(new QLabel(tr("Subject:"), groupCerts));
    genFormLayout->addWidget(editNewCertSubject, 1);
    genFormLayout->addWidget(spinNewCertDays);
    genFormLayout->addWidget(btnCreateSelfSignedCert);
    genFormLayout->addWidget(btnGenerateCsr);
    certsLayout->addLayout(genFormLayout);

    // Client certificate mode
    auto* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(tr("TLS Client Certificate Authentication:"), groupCerts));
    cmbClientCertMode = new QComboBox(groupCerts);
    cmbClientCertMode->addItem(tr("Off / Disabled"), static_cast<int>(Onvif::ClientCertificateMode::Off));
    cmbClientCertMode->addItem(tr("Optional"), static_cast<int>(Onvif::ClientCertificateMode::Optional));
    cmbClientCertMode->addItem(tr("Required (mTLS)"), static_cast<int>(Onvif::ClientCertificateMode::Required));
    btnApplyClientCertMode = new QPushButton(tr("Apply Mode"), groupCerts);
    modeLayout->addWidget(cmbClientCertMode);
    modeLayout->addWidget(btnApplyClientCertMode);
    modeLayout->addStretch();
    certsLayout->addLayout(modeLayout);

    netTabLayout->addWidget(groupCerts);
    netTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 9: Relays & I/O (Profile S & T)
    // -------------------------------------------------------------------------
    auto* relayTabLayout = createScrollTab(tr("Relays & I/O"));

    auto* groupRelays = new QGroupBox(tr("Profile S/T: Relay Outputs & Actuators"));
    auto* relaysLayout = new QVBoxLayout(groupRelays);
    relaysLayout->setSpacing(6);

    tableRelays = new QTableWidget(0, 5, groupRelays);
    configureTable(tableRelays, { tr("Token"), tr("Mode"), tr("Delay (s)"), tr("Idle State"), tr("Logical State") });
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
    configureTable(tableDigitalInputs, { tr("Token"), tr("Idle State"), tr("Sensor Type"), tr("State") });
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

    // -------------------------------------------------------------------------
    // Sub-Tab 10: Metadata & Analytics (Profile T & M)
    // -------------------------------------------------------------------------
    auto* metaTabLayout = createScrollTab(tr("Metadata & Analytics"));

    auto* groupMetaConfig = new QGroupBox(tr("Profile M/T: Metadata Stream Configuration"), this);
    auto* metaConfigLayout = new QVBoxLayout(groupMetaConfig);
    metaConfigLayout->setSpacing(6);

    auto* metaCfgRow1 = new QHBoxLayout();
    metaCfgRow1->addWidget(new QLabel(tr("Configuration:"), groupMetaConfig));
    cmbMetaConfigs = new QComboBox(groupMetaConfig);
    metaCfgRow1->addWidget(cmbMetaConfigs, 1);
    btnRefreshMetaConfigs = new QPushButton(tr("Refresh Configs"), groupMetaConfig);
    metaCfgRow1->addWidget(btnRefreshMetaConfigs);
    metaConfigLayout->addLayout(metaCfgRow1);

    auto* metaChkRow = new QHBoxLayout();
    chkMetaPtzStatus = new QCheckBox(tr("PTZ Status Stream"), groupMetaConfig);
    chkMetaAnalytics = new QCheckBox(tr("Analytics (Objects / Bounding Boxes)"), groupMetaConfig);
    chkMetaEvents = new QCheckBox(tr("Event Notifications"), groupMetaConfig);
    chkMetaGeo = new QCheckBox(tr("Geo-Location Coordinates"), groupMetaConfig);
    chkMetaPtzStatus->setChecked(true);
    chkMetaAnalytics->setChecked(true);
    metaChkRow->addWidget(chkMetaPtzStatus);
    metaChkRow->addWidget(chkMetaAnalytics);
    metaChkRow->addWidget(chkMetaEvents);
    metaChkRow->addWidget(chkMetaGeo);
    metaConfigLayout->addLayout(metaChkRow);

    auto* metaBtnRow = new QHBoxLayout();
    btnApplyMetaConfig = new QPushButton(tr("Apply Metadata Config"), groupMetaConfig);
    metaBtnRow->addWidget(btnApplyMetaConfig);
    metaBtnRow->addStretch();
    metaConfigLayout->addLayout(metaBtnRow);

    metaTabLayout->addWidget(groupMetaConfig);

    auto* groupMetaStream = new QGroupBox(tr("Live Metadata & Analytics Stream"), this);
    auto* metaStreamLayout = new QVBoxLayout(groupMetaStream);
    metaStreamLayout->setSpacing(6);

    auto* streamCtrlRow = new QHBoxLayout();
    btnToggleMetaStream = new QPushButton(tr("▶ Start Metadata Streaming (5 Hz)"), groupMetaStream);
    btnToggleMetaStream->setCheckable(true);
    btnPollMetaOnce = new QPushButton(tr("🔄 Poll Once"), groupMetaStream);
    lblMetaTelemetry = new QLabel(tr("PTZ: Pan=0.000 Tilt=0.000 Zoom=0.000 | Moving: No | Frame: -"), groupMetaStream);
    lblMetaTelemetry->setStyleSheet(QStringLiteral("color: #58a6ff; font-family: monospace; font-weight: bold;"));

    streamCtrlRow->addWidget(btnToggleMetaStream);
    streamCtrlRow->addWidget(btnPollMetaOnce);
    streamCtrlRow->addWidget(lblMetaTelemetry, 1);
    metaStreamLayout->addLayout(streamCtrlRow);

    tableMetaObjects = new QTableWidget(0, 6, groupMetaStream);
    configureTable(tableMetaObjects,
        { tr("Object ID"), tr("Class"), tr("Confidence"), tr("Bounding Box (L,T,R,B)"), tr("Geo Location"),
            tr("Timestamp") });
    tableMetaObjects->setMinimumHeight(160);
    metaStreamLayout->addWidget(tableMetaObjects);

    metaTabLayout->addWidget(groupMetaStream);

    auto* groupRules = new QGroupBox(tr("Profile M & T: Video Analytics Rule Engine"), this);
    auto* rulesLayout = new QVBoxLayout(groupRules);
    rulesLayout->setSpacing(6);

    tableRules = new QTableWidget(0, 5, groupRules);
    configureTable(tableRules, { tr("Rule Name"), tr("Type"), tr("Classes"), tr("Parameters / Dwell"), tr("Status") });
    tableRules->setMinimumHeight(130);
    rulesLayout->addWidget(tableRules);

    auto* ruleInputRow = new QHBoxLayout();
    editRuleName = new QLineEdit(groupRules);
    editRuleName->setPlaceholderText(tr("Rule Name (e.g. PerimeterTripwire)"));
    cmbRuleType = new QComboBox(groupRules);
    cmbRuleType->addItem(tr("Tripwire (tt:LineDetector)"), QStringLiteral("tt:LineDetector"));
    cmbRuleType->addItem(tr("Field Intrusion (tt:FieldDetector)"), QStringLiteral("tt:FieldDetector"));
    cmbRuleType->addItem(tr("Loitering (tt:LoiteringDetector)"), QStringLiteral("tt:LoiteringDetector"));
    cmbRuleType->addItem(tr("Cell Motion (tt:CellMotionDetector)"), QStringLiteral("tt:CellMotionDetector"));

    editRuleClasses = new QLineEdit(groupRules);
    editRuleClasses->setPlaceholderText(tr("Classes (e.g. Human,Vehicle)"));
    editRuleClasses->setText(QStringLiteral("Human,Vehicle"));

    spinRuleMinConf = new QDoubleSpinBox(groupRules);
    spinRuleMinConf->setRange(0.0, 1.0);
    spinRuleMinConf->setSingleStep(0.05);
    spinRuleMinConf->setValue(0.5);
    spinRuleMinConf->setPrefix(tr("Conf: "));

    spinRuleDwellTime = new QDoubleSpinBox(groupRules);
    spinRuleDwellTime->setRange(0.0, 300.0);
    spinRuleDwellTime->setValue(5.0);
    spinRuleDwellTime->setSuffix(tr(" s"));

    btnAddRule = new QPushButton(tr("➕ Add Rule"), groupRules);
    btnDeleteRule = new QPushButton(tr("🗑 Delete Rule"), groupRules);
    btnRefreshRules = new QPushButton(tr("🔄 Refresh Rules"), groupRules);

    ruleInputRow->addWidget(editRuleName, 2);
    ruleInputRow->addWidget(cmbRuleType, 2);
    ruleInputRow->addWidget(editRuleClasses, 2);
    ruleInputRow->addWidget(spinRuleMinConf, 1);
    ruleInputRow->addWidget(spinRuleDwellTime, 1);
    ruleInputRow->addWidget(btnAddRule);
    ruleInputRow->addWidget(btnDeleteRule);
    ruleInputRow->addWidget(btnRefreshRules);
    rulesLayout->addLayout(ruleInputRow);

    metaTabLayout->addWidget(groupRules);
    metaTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 11: Recordings & Replay (Profile G)
    // -------------------------------------------------------------------------
    auto* recTabLayout = createScrollTab(tr("Recordings & Replay"));

    // Storage Summary & Edge Recordings
    auto* groupRecordings = new QGroupBox(tr("Profile G: Edge Recordings & Storage"), this);
    auto* recLayout = new QVBoxLayout(groupRecordings);
    recLayout->setSpacing(6);

    auto* sumLayout = new QHBoxLayout();
    lblRecordingSummary = new QLabel(tr("Storage Summary: Unknown"), groupRecordings);
    btnRefreshRecordingSummary = new QPushButton(tr("🔄 Summary"), groupRecordings);
    sumLayout->addWidget(lblRecordingSummary, 1);
    sumLayout->addWidget(btnRefreshRecordingSummary);
    recLayout->addLayout(sumLayout);

    tableRecordings = new QTableWidget(0, 5, groupRecordings);
    configureTable(
        tableRecordings, { tr("Recording Token"), tr("Source"), tr("Content"), tr("Retention"), tr("Tracks") });
    tableRecordings->setMinimumHeight(110);
    recLayout->addWidget(tableRecordings);

    auto* recActionLayout = new QHBoxLayout();
    editNewRecordingSource = new QLineEdit(groupRecordings);
    editNewRecordingSource->setPlaceholderText(tr("Source Token (e.g. VideoSource_1)"));
    editNewRecordingContent = new QLineEdit(groupRecordings);
    editNewRecordingContent->setPlaceholderText(tr("Content (e.g. MainStream)"));
    btnCreateRecording = new QPushButton(tr("➕ Create Recording"), groupRecordings);
    btnDeleteRecording = new QPushButton(tr("🗑 Delete Recording"), groupRecordings);
    btnRefreshRecordings = new QPushButton(tr("🔄 Refresh"), groupRecordings);

    recActionLayout->addWidget(editNewRecordingSource);
    recActionLayout->addWidget(editNewRecordingContent);
    recActionLayout->addWidget(btnCreateRecording);
    recActionLayout->addWidget(btnDeleteRecording);
    recActionLayout->addWidget(btnRefreshRecordings);
    recLayout->addLayout(recActionLayout);

    // Track Management
    auto* trackActionLayout = new QHBoxLayout();
    cmbTrackType = new QComboBox(groupRecordings);
    cmbTrackType->addItem(tr("Video"), static_cast<int>(Onvif::RecordingTrackType::Video));
    cmbTrackType->addItem(tr("Audio"), static_cast<int>(Onvif::RecordingTrackType::Audio));
    cmbTrackType->addItem(tr("Metadata"), static_cast<int>(Onvif::RecordingTrackType::Metadata));
    editTrackDesc = new QLineEdit(groupRecordings);
    editTrackDesc->setPlaceholderText(tr("Track Description"));
    btnCreateTrack = new QPushButton(tr("➕ Add Track"), groupRecordings);
    btnDeleteTrack = new QPushButton(tr("🗑 Remove Track"), groupRecordings);

    trackActionLayout->addWidget(new QLabel(tr("Track:"), groupRecordings));
    trackActionLayout->addWidget(cmbTrackType);
    trackActionLayout->addWidget(editTrackDesc, 1);
    trackActionLayout->addWidget(btnCreateTrack);
    trackActionLayout->addWidget(btnDeleteTrack);
    recLayout->addLayout(trackActionLayout);

    recTabLayout->addWidget(groupRecordings);

    // Automated Recording Jobs
    auto* groupJobs = new QGroupBox(tr("Automated Recording Jobs"), this);
    auto* jobsLayout = new QVBoxLayout(groupJobs);
    jobsLayout->setSpacing(6);

    tableRecordingJobs = new QTableWidget(0, 5, groupJobs);
    configureTable(
        tableRecordingJobs, { tr("Job Token"), tr("Recording Token"), tr("Source Token"), tr("Priority"), tr("Mode") });
    tableRecordingJobs->setMinimumHeight(100);
    jobsLayout->addWidget(tableRecordingJobs);

    auto* jobActionLayout = new QHBoxLayout();
    editJobRecordingToken = new QLineEdit(groupJobs);
    editJobRecordingToken->setPlaceholderText(tr("Rec Token"));
    editJobSourceToken = new QLineEdit(groupJobs);
    editJobSourceToken->setPlaceholderText(tr("Src Token"));
    spinJobPriority = new QSpinBox(groupJobs);
    spinJobPriority->setRange(1, 10);
    spinJobPriority->setValue(5);
    cmbJobMode = new QComboBox(groupJobs);
    cmbJobMode->addItem(tr("Active"), static_cast<int>(Onvif::RecordingJobMode::Active));
    cmbJobMode->addItem(tr("Idle"), static_cast<int>(Onvif::RecordingJobMode::Idle));

    btnCreateJob = new QPushButton(tr("➕ Create Job"), groupJobs);
    btnToggleJobMode = new QPushButton(tr("⚡ Toggle Active/Idle"), groupJobs);
    btnDeleteJob = new QPushButton(tr("🗑 Delete Job"), groupJobs);
    btnRefreshRecordingJobs = new QPushButton(tr("🔄 Refresh Jobs"), groupJobs);

    jobActionLayout->addWidget(editJobRecordingToken);
    jobActionLayout->addWidget(editJobSourceToken);
    jobActionLayout->addWidget(spinJobPriority);
    jobActionLayout->addWidget(cmbJobMode);
    jobActionLayout->addWidget(btnCreateJob);
    jobActionLayout->addWidget(btnToggleJobMode);
    jobActionLayout->addWidget(btnDeleteJob);
    jobActionLayout->addWidget(btnRefreshRecordingJobs);
    jobsLayout->addLayout(jobActionLayout);

    recTabLayout->addWidget(groupJobs);

    // Historical Search & Replay
    auto* groupSearch = new QGroupBox(tr("Historical Search & RTSP Replay"), this);
    auto* searchLayout = new QVBoxLayout(groupSearch);
    searchLayout->setSpacing(6);

    auto* recSearchForm = new QHBoxLayout();
    editSearchScope = new QLineEdit(groupSearch);
    editSearchScope->setPlaceholderText(tr("Search Scope / Sources (or empty for all)"));
    btnFindRecordings = new QPushButton(tr("🔍 Find Recordings"), groupSearch);
    recSearchForm->addWidget(editSearchScope, 1);
    recSearchForm->addWidget(btnFindRecordings);
    searchLayout->addLayout(recSearchForm);

    tableSearchResults = new QTableWidget(0, 5, groupSearch);
    configureTable(tableSearchResults, { tr("Recording"), tr("Track"), tr("Earliest"), tr("Latest"), tr("State") });
    tableSearchResults->setMinimumHeight(100);
    searchLayout->addWidget(tableSearchResults);

    // Replay Stream Controls
    auto* replayForm = new QHBoxLayout();
    editReplayUri = new QLineEdit(groupSearch);
    editReplayUri->setPlaceholderText(tr("RTSP Replay URI will appear here..."));
    btnResolveReplayUri = new QPushButton(tr("🎬 Get Replay URI"), groupSearch);
    btnPlayReplayUri = new QPushButton(tr("▶ Play in Video Stream Tab"), groupSearch);
    btnPlayReplayUri->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #238636; color: white;"));

    replayForm->addWidget(editReplayUri, 1);
    replayForm->addWidget(btnResolveReplayUri);
    replayForm->addWidget(btnPlayReplayUri);
    searchLayout->addLayout(replayForm);

    // Event Search
    auto* evSearchForm = new QHBoxLayout();
    editEventStartUtc = new QLineEdit(groupSearch);
    editEventStartUtc->setPlaceholderText(tr("Start UTC (e.g. 2026-01-01T00:00:00Z)"));
    editEventEndUtc = new QLineEdit(groupSearch);
    editEventEndUtc->setPlaceholderText(tr("End UTC (optional)"));
    btnFindEvents = new QPushButton(tr("🔍 Find Recorded Events"), groupSearch);

    evSearchForm->addWidget(editEventStartUtc);
    evSearchForm->addWidget(editEventEndUtc);
    evSearchForm->addWidget(btnFindEvents);
    searchLayout->addLayout(evSearchForm);

    tableEventSearchResults = new QTableWidget(0, 5, groupSearch);
    configureTable(tableEventSearchResults, { tr("Recording"), tr("UTC Time"), tr("Topic"), tr("Source"), tr("Data") });
    tableEventSearchResults->setMinimumHeight(100);
    searchLayout->addWidget(tableEventSearchResults);

    recTabLayout->addWidget(groupSearch);
    recTabLayout->addStretch();

    // -------------------------------------------------------------------------
    // Sub-Tab 12: Thermal & Radiometry (ONVIF Thermal Service)
    // -------------------------------------------------------------------------
    auto* thermalTabLayout = createScrollTab(tr("Thermal & Radiometry"));

    // Group 1: Radiometry Environmental Parameters
    auto* groupRadParams = new QGroupBox(tr("Radiometry Environmental Parameters"), this);
    auto* gridRadParams = new QGridLayout(groupRadParams);
    gridRadParams->setSpacing(6);

    gridRadParams->addWidget(new QLabel(tr("Emissivity:"), groupRadParams), 0, 0);
    spinEmissivity = new QDoubleSpinBox(groupRadParams);
    spinEmissivity->setRange(0.01, 1.0);
    spinEmissivity->setSingleStep(0.01);
    spinEmissivity->setValue(0.95);
    gridRadParams->addWidget(spinEmissivity, 0, 1);

    gridRadParams->addWidget(new QLabel(tr("Target Distance (m):"), groupRadParams), 0, 2);
    spinTargetDistance = new QDoubleSpinBox(groupRadParams);
    spinTargetDistance->setRange(0.1, 10000.0);
    spinTargetDistance->setSingleStep(0.5);
    spinTargetDistance->setValue(5.0);
    gridRadParams->addWidget(spinTargetDistance, 0, 3);

    gridRadParams->addWidget(new QLabel(tr("Reflected Temp (°C):"), groupRadParams), 0, 4);
    spinReflectedTemp = new QDoubleSpinBox(groupRadParams);
    spinReflectedTemp->setRange(-50.0, 500.0);
    spinReflectedTemp->setValue(20.0);
    gridRadParams->addWidget(spinReflectedTemp, 0, 5);

    gridRadParams->addWidget(new QLabel(tr("Atmospheric Temp (°C):"), groupRadParams), 1, 0);
    spinAtmosphericTemp = new QDoubleSpinBox(groupRadParams);
    spinAtmosphericTemp->setRange(-50.0, 100.0);
    spinAtmosphericTemp->setValue(20.0);
    gridRadParams->addWidget(spinAtmosphericTemp, 1, 1);

    gridRadParams->addWidget(new QLabel(tr("Relative Humidity (%):"), groupRadParams), 1, 2);
    spinRelativeHumidity = new QDoubleSpinBox(groupRadParams);
    spinRelativeHumidity->setRange(0.0, 100.0);
    spinRelativeHumidity->setValue(50.0);
    gridRadParams->addWidget(spinRelativeHumidity, 1, 3);

    gridRadParams->addWidget(new QLabel(tr("Window Transmittance:"), groupRadParams), 1, 4);
    spinWindowTransmission = new QDoubleSpinBox(groupRadParams);
    spinWindowTransmission->setRange(0.01, 1.0);
    spinWindowTransmission->setSingleStep(0.01);
    spinWindowTransmission->setValue(1.0);
    gridRadParams->addWidget(spinWindowTransmission, 1, 5);

    auto* radParamBtnRow = new QHBoxLayout();
    btnRefreshRadiometry = new QPushButton(tr("🔄 Refresh Radiometry"), groupRadParams);
    btnApplyRadiometry = new QPushButton(tr("💾 Apply Radiometry"), groupRadParams);
    btnApplyRadiometry->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #238636; color: white; }"));
    radParamBtnRow->addWidget(btnRefreshRadiometry);
    radParamBtnRow->addWidget(btnApplyRadiometry);
    radParamBtnRow->addStretch();
    gridRadParams->addLayout(radParamBtnRow, 2, 0, 1, 6);

    thermalTabLayout->addWidget(groupRadParams);

    // Group 2: Color Palettes & Non-Uniformity Correction (NUC)
    auto* groupPaletteNuc = new QGroupBox(tr("Thermal Color Palettes & NUC Calibration"), this);
    auto* paletteNucLayout = new QHBoxLayout(groupPaletteNuc);
    paletteNucLayout->setSpacing(8);

    paletteNucLayout->addWidget(new QLabel(tr("Color Palette:"), groupPaletteNuc));
    cmbThermalPalettes = new QComboBox(groupPaletteNuc);
    cmbThermalPalettes->setMinimumWidth(160);
    paletteNucLayout->addWidget(cmbThermalPalettes);

    btnSetPalette = new QPushButton(tr("🎨 Apply Palette"), groupPaletteNuc);
    btnRefreshPalettes = new QPushButton(tr("🔄 Refresh Palettes"), groupPaletteNuc);
    paletteNucLayout->addWidget(btnSetPalette);
    paletteNucLayout->addWidget(btnRefreshPalettes);

    paletteNucLayout->addSpacing(20);

    btnTriggerNuc = new QPushButton(tr("⚡ Calibrate NUC"), groupPaletteNuc);
    btnTriggerNuc->setStyleSheet(
        QStringLiteral("QPushButton { font-weight: bold; background-color: #1f6feb; color: white; }"));
    lblNucStatus = new QLabel(tr("NUC: Ready"), groupPaletteNuc);
    lblNucStatus->setStyleSheet(QStringLiteral("color: #8b949e; font-weight: bold;"));
    paletteNucLayout->addWidget(btnTriggerNuc);
    paletteNucLayout->addWidget(lblNucStatus);
    paletteNucLayout->addStretch();

    thermalTabLayout->addWidget(groupPaletteNuc);

    // Group 3: Spots & Boxes Measurement Table
    auto* groupMeasurements = new QGroupBox(tr("Radiometry Temperature Measurements (Spots & Boxes)"), this);
    auto* measLayout = new QVBoxLayout(groupMeasurements);
    measLayout->setSpacing(6);

    tableRadiometry = new QTableWidget(0, 7, groupMeasurements);
    configureTable(tableRadiometry,
        { tr("Token"), tr("Type"), tr("Label"), tr("Coordinates"), tr("Celsius"), tr("Fahrenheit"), tr("Alarm") });
    tableRadiometry->setMinimumHeight(150);
    measLayout->addWidget(tableRadiometry);

    // Measurement controls
    auto* measEditGrid = new QGridLayout();
    measEditGrid->addWidget(new QLabel(tr("Type:"), groupMeasurements), 0, 0);
    cmbRadType = new QComboBox(groupMeasurements);
    cmbRadType->addItem(tr("Spotmeter (Point)"), QStringLiteral("Spot"));
    cmbRadType->addItem(tr("Box (ROI)"), QStringLiteral("Box"));
    measEditGrid->addWidget(cmbRadType, 0, 1);

    measEditGrid->addWidget(new QLabel(tr("Token:"), groupMeasurements), 0, 2);
    editRadToken = new QLineEdit(groupMeasurements);
    editRadToken->setPlaceholderText(tr("e.g. Spot1 or Box1"));
    measEditGrid->addWidget(editRadToken, 0, 3);

    measEditGrid->addWidget(new QLabel(tr("Label:"), groupMeasurements), 0, 4);
    editRadLabel = new QLineEdit(groupMeasurements);
    editRadLabel->setPlaceholderText(tr("e.g. Bearing Check"));
    measEditGrid->addWidget(editRadLabel, 0, 5);

    measEditGrid->addWidget(new QLabel(tr("X1 / Y1:"), groupMeasurements), 1, 0);
    auto* p1Layout = new QHBoxLayout();
    spinRadX1 = new QDoubleSpinBox(groupMeasurements);
    spinRadX1->setRange(-1.0, 1.0);
    spinRadX1->setSingleStep(0.05);
    spinRadX1->setValue(0.0);
    spinRadY1 = new QDoubleSpinBox(groupMeasurements);
    spinRadY1->setRange(-1.0, 1.0);
    spinRadY1->setSingleStep(0.05);
    spinRadY1->setValue(0.0);
    p1Layout->addWidget(spinRadX1);
    p1Layout->addWidget(spinRadY1);
    measEditGrid->addLayout(p1Layout, 1, 1);

    measEditGrid->addWidget(new QLabel(tr("X2 / Y2:"), groupMeasurements), 1, 2);
    auto* p2Layout = new QHBoxLayout();
    spinRadX2 = new QDoubleSpinBox(groupMeasurements);
    spinRadX2->setRange(-1.0, 1.0);
    spinRadX2->setSingleStep(0.05);
    spinRadX2->setValue(0.2);
    spinRadY2 = new QDoubleSpinBox(groupMeasurements);
    spinRadY2->setRange(-1.0, 1.0);
    spinRadY2->setSingleStep(0.05);
    spinRadY2->setValue(0.2);
    p2Layout->addWidget(spinRadX2);
    p2Layout->addWidget(spinRadY2);
    measEditGrid->addLayout(p2Layout, 1, 3);

    measEditGrid->addWidget(new QLabel(tr("Alarm Thresh (°C):"), groupMeasurements), 1, 4);
    spinAlarmThreshold = new QDoubleSpinBox(groupMeasurements);
    spinAlarmThreshold->setRange(-50.0, 500.0);
    spinAlarmThreshold->setValue(75.0);
    measEditGrid->addWidget(spinAlarmThreshold, 1, 5);

    auto* measBtnRow = new QHBoxLayout();
    btnAddMeasurement = new QPushButton(tr("➕ Add / Save Measurement"), groupMeasurements);
    btnDeleteMeasurement = new QPushButton(tr("🗑 Delete Measurement"), groupMeasurements);
    btnRefreshMeasurements = new QPushButton(tr("🔄 Refresh Measurements"), groupMeasurements);
    lblThermalAlarmStatus = new QLabel(tr("Alarm: Normal"), groupMeasurements);
    lblThermalAlarmStatus->setStyleSheet(QStringLiteral("font-weight: bold; color: #238636; padding: 2px 8px;"));

    measBtnRow->addWidget(btnAddMeasurement);
    measBtnRow->addWidget(btnDeleteMeasurement);
    measBtnRow->addWidget(btnRefreshMeasurements);
    measBtnRow->addSpacing(16);
    measBtnRow->addWidget(lblThermalAlarmStatus);
    measBtnRow->addStretch();

    measLayout->addLayout(measEditGrid);
    measLayout->addLayout(measBtnRow);

    thermalTabLayout->addWidget(groupMeasurements);
    thermalTabLayout->addStretch();

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

    // Maintenance Extensions connections
    connect(btnFetchSystemLog, &QPushButton::clicked, this, &OnvifCameraTab::handleFetchSystemLog);
    connect(btnFetchAccessLog, &QPushButton::clicked, this, &OnvifCameraTab::handleFetchAccessLog);
    connect(btnFetchSupportInfo, &QPushButton::clicked, this, &OnvifCameraTab::handleFetchSupportInfo);
    connect(btnDownloadBackup, &QPushButton::clicked, this, &OnvifCameraTab::handleDownloadBackup);
    connect(btnRestoreBackup, &QPushButton::clicked, this, &OnvifCameraTab::handleRestoreBackup);
    connect(btnFetchEndpointRef, &QPushButton::clicked, this, &OnvifCameraTab::handleFetchEndpointReference);

    // Profile T/M: Metadata & Analytics connections
    connect(btnRefreshMetaConfigs, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshMetadataConfigs);
    connect(btnApplyMetaConfig, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyMetadataConfig);
    connect(btnToggleMetaStream, &QPushButton::toggled, this, &OnvifCameraTab::handleToggleMetadataStream);
    connect(btnPollMetaOnce, &QPushButton::clicked, this, &OnvifCameraTab::handlePollMetadataOnce);

    // PKI Certificates & TLS Security connections
    connect(btnRefreshCerts, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshCertificates);
    connect(btnCreateSelfSignedCert, &QPushButton::clicked, this, &OnvifCameraTab::handleCreateSelfSignedCert);
    connect(btnGenerateCsr, &QPushButton::clicked, this, &OnvifCameraTab::handleGenerateCsr);
    connect(btnDeleteCert, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteCertificate);
    connect(btnApplyClientCertMode, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyClientCertMode);

    // Profile G: Recordings & Replay connections
    connect(btnRefreshRecordingSummary, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRecordingSummary);
    connect(btnRefreshRecordings, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRecordings);
    connect(btnCreateRecording, &QPushButton::clicked, this, &OnvifCameraTab::handleCreateRecording);
    connect(btnDeleteRecording, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteRecording);
    connect(btnCreateTrack, &QPushButton::clicked, this, &OnvifCameraTab::handleCreateTrack);
    connect(btnDeleteTrack, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteTrack);
    connect(btnRefreshRecordingJobs, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRecordingJobs);
    connect(btnCreateJob, &QPushButton::clicked, this, &OnvifCameraTab::handleCreateRecordingJob);
    connect(btnToggleJobMode, &QPushButton::clicked, this, &OnvifCameraTab::handleToggleJobMode);
    connect(btnDeleteJob, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteRecordingJob);
    connect(btnFindRecordings, &QPushButton::clicked, this, &OnvifCameraTab::handleFindRecordings);
    connect(btnFindEvents, &QPushButton::clicked, this, &OnvifCameraTab::handleFindEvents);
    connect(btnResolveReplayUri, &QPushButton::clicked, this, &OnvifCameraTab::handleResolveReplayUri);
    connect(btnPlayReplayUri, &QPushButton::clicked, this, &OnvifCameraTab::handlePlayInVideoStreamTab);

    // Profile M & T: Analytics Rules connections
    connect(btnRefreshRules, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRules);
    connect(btnAddRule, &QPushButton::clicked, this, &OnvifCameraTab::handleAddRule);
    connect(btnDeleteRule, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteRule);

    // Geolocation & GeoMove connections
    connect(btnRefreshGeoLoc, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshGeoLocation);
    connect(btnSaveGeoLoc, &QPushButton::clicked, this, &OnvifCameraTab::handleSaveGeoLocation);
    connect(btnExecuteGeoMove, &QPushButton::clicked, this, &OnvifCameraTab::handleExecuteGeoMove);
    connect(btnExecuteSphericalMove, &QPushButton::clicked, this, &OnvifCameraTab::handleExecuteAbsoluteSpherical);

    auto updateGeoTargetLambda = [this]() { handleUpdateLiveGeoTargetReadout(); };
    connect(spinTargetLat, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinTargetLon, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinTargetElev, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinCameraLat, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinCameraLon, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinCameraElev, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinCameraYaw, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);
    connect(spinCameraPitch, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateGeoTargetLambda);

    // Thermal & Radiometry connections
    connect(btnRefreshRadiometry, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshRadiometry);
    connect(btnApplyRadiometry, &QPushButton::clicked, this, &OnvifCameraTab::handleApplyRadiometry);
    connect(btnRefreshPalettes, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshPalettes);
    connect(btnSetPalette, &QPushButton::clicked, this, &OnvifCameraTab::handleSetPalette);
    connect(btnTriggerNuc, &QPushButton::clicked, this, &OnvifCameraTab::handleTriggerNuc);
    connect(btnRefreshMeasurements, &QPushButton::clicked, this, &OnvifCameraTab::handleRefreshMeasurements);
    connect(btnAddMeasurement, &QPushButton::clicked, this, &OnvifCameraTab::handleAddMeasurement);
    connect(btnDeleteMeasurement, &QPushButton::clicked, this, &OnvifCameraTab::handleDeleteMeasurement);
    initConnectionWidgets();
}

// ---------------------------------------------------------------------------
// Helper: configureTable
// ---------------------------------------------------------------------------
void OnvifCameraTab::configureTable(QTableWidget* table, const QStringList& headers)
{
    table->setColumnCount(static_cast<int>(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

// ---------------------------------------------------------------------------
// Helper: osdPositionFromIndex
// ---------------------------------------------------------------------------
Onvif::OsdPositionType OnvifCameraTab::osdPositionFromIndex(int index) noexcept
{
    switch (index) {
    case 1:
        return Onvif::OsdPositionType::UpperRight;
    case 2:
        return Onvif::OsdPositionType::LowerLeft;
    case 3:
        return Onvif::OsdPositionType::LowerRight;
    case 4:
        return Onvif::OsdPositionType::Custom;
    default:
        return Onvif::OsdPositionType::UpperLeft;
    }
}

// ---------------------------------------------------------------------------
// Helper: registerConnectionWidget / initConnectionWidgets
// ---------------------------------------------------------------------------
void OnvifCameraTab::registerConnectionWidget(QWidget* w)
{
    if (w != nullptr) {
        m_connectionWidgets.append(w);
    }
}

void OnvifCameraTab::initConnectionWidgets()
{
    // PTZ
    registerConnectionWidget(cmbProfiles);
    registerConnectionWidget(btnGotoHome);
    registerConnectionWidget(btnSetHome);
    registerConnectionWidget(btnRelMove);
    registerConnectionWidget(btnWiperOn);
    registerConnectionWidget(btnWiperOff);
    registerConnectionWidget(btnWasher);
    registerConnectionWidget(btnIrOn);
    registerConnectionWidget(btnIrOff);
    registerConnectionWidget(editCustomAux);
    registerConnectionWidget(btnSendAux);

    // Presets
    registerConnectionWidget(btnRefreshPresets);
    registerConnectionWidget(btnGotoPreset);
    registerConnectionWidget(btnSavePreset);
    registerConnectionWidget(btnDeletePreset);

    // Geolocation
    registerConnectionWidget(btnRefreshGeoLoc);
    registerConnectionWidget(btnSaveGeoLoc);
    registerConnectionWidget(btnExecuteGeoMove);
    registerConnectionWidget(btnExecuteSphericalMove);

    // Tours
    registerConnectionWidget(cmbPresetTours);
    registerConnectionWidget(btnRefreshTours);
    registerConnectionWidget(btnStartTour);
    registerConnectionWidget(btnPauseTour);
    registerConnectionWidget(btnStopTour);
    registerConnectionWidget(tableTourSpots);
    registerConnectionWidget(btnAddTourStep);
    registerConnectionWidget(btnRemoveTourStep);
    registerConnectionWidget(btnSaveTour);

    // Profile T: Imaging
    registerConnectionWidget(sliderBrightness);
    registerConnectionWidget(sliderContrast);
    registerConnectionWidget(sliderSaturation);
    registerConnectionWidget(sliderSharpness);
    registerConnectionWidget(cmbIrFilter);
    registerConnectionWidget(chkBacklight);
    registerConnectionWidget(chkWdr);
    registerConnectionWidget(cmbAutoFocus);
    registerConnectionWidget(btnFocusNear);
    registerConnectionWidget(btnFocusFar);
    registerConnectionWidget(cmbImagingPresets);
    registerConnectionWidget(btnRecallImagingPreset);
    registerConnectionWidget(btnRefreshImaging);
    registerConnectionWidget(btnApplyImaging);
    registerConnectionWidget(btnToggleEvents);
    registerConnectionWidget(btnClearEvents);

    // Profile T: OSD
    registerConnectionWidget(tableOsds);
    registerConnectionWidget(editOsdText);
    registerConnectionWidget(cmbOsdPosition);
    registerConnectionWidget(chkOsdDateTime);
    registerConnectionWidget(spinOsdFontSize);
    registerConnectionWidget(btnRefreshOsds);
    registerConnectionWidget(btnAddOsd);
    registerConnectionWidget(btnUpdateOsd);
    registerConnectionWidget(btnDeleteOsd);

    // Profile T: Privacy Masks
    registerConnectionWidget(tableMasks);
    registerConnectionWidget(editMaskToken);
    registerConnectionWidget(cmbMaskType);
    registerConnectionWidget(spinMaskColorR);
    registerConnectionWidget(spinMaskColorG);
    registerConnectionWidget(spinMaskColorB);
    registerConnectionWidget(chkMaskEnabled);
    registerConnectionWidget(btnRefreshMasks);
    registerConnectionWidget(btnAddMask);
    registerConnectionWidget(btnUpdateMask);
    registerConnectionWidget(btnDeleteMask);

    // Profile T: Video Source Modes
    registerConnectionWidget(cmbVideoSourceModes);
    registerConnectionWidget(btnRefreshVideoSourceModes);
    registerConnectionWidget(btnApplyVideoSourceMode);

    // Maintenance
    registerConnectionWidget(btnReboot);

    // Device Management: Users
    registerConnectionWidget(tableUsers);
    registerConnectionWidget(editUserUsername);
    registerConnectionWidget(editUserPassword);
    registerConnectionWidget(cmbUserLevel);
    registerConnectionWidget(btnAddUser);
    registerConnectionWidget(btnUpdateUser);
    registerConnectionWidget(btnDeleteUser);
    registerConnectionWidget(btnRefreshUsers);

    // Device Management: Network
    registerConnectionWidget(chkNetEnabled);
    registerConnectionWidget(chkNetDhcp);
    registerConnectionWidget(editNetIp);
    registerConnectionWidget(spinNetPrefix);
    registerConnectionWidget(editNetGateway);
    registerConnectionWidget(btnRefreshNetwork);
    registerConnectionWidget(btnApplyNetwork);
    registerConnectionWidget(chkDnsDhcp);
    registerConnectionWidget(editDnsServers);
    registerConnectionWidget(btnRefreshDns);
    registerConnectionWidget(btnApplyDns);
    registerConnectionWidget(chkNtpDhcp);
    registerConnectionWidget(editNtpServers);
    registerConnectionWidget(btnRefreshNtp);
    registerConnectionWidget(btnApplyNtp);
    registerConnectionWidget(btnSyncPcTime);
    registerConnectionWidget(btnFactoryDefaultSoft);
    registerConnectionWidget(btnFactoryDefaultHard);

    // Maintenance Extensions
    registerConnectionWidget(btnFetchSystemLog);
    registerConnectionWidget(btnFetchAccessLog);
    registerConnectionWidget(txtSystemLogs);
    registerConnectionWidget(btnFetchSupportInfo);
    registerConnectionWidget(btnDownloadBackup);
    registerConnectionWidget(btnRestoreBackup);
    registerConnectionWidget(editBackupPayload);
    registerConnectionWidget(btnFetchEndpointRef);

    // Relays & I/O
    registerConnectionWidget(tableRelays);
    registerConnectionWidget(editRelayToken);
    registerConnectionWidget(cmbRelayMode);
    registerConnectionWidget(spinRelayDelay);
    registerConnectionWidget(cmbRelayIdleState);
    registerConnectionWidget(btnRefreshRelays);
    registerConnectionWidget(btnActivateRelay);
    registerConnectionWidget(btnDeactivateRelay);
    registerConnectionWidget(btnApplyRelaySettings);
    registerConnectionWidget(tableDigitalInputs);
    registerConnectionWidget(btnRefreshInputs);

    // Metadata & Analytics
    registerConnectionWidget(cmbMetaConfigs);
    registerConnectionWidget(chkMetaPtzStatus);
    registerConnectionWidget(chkMetaAnalytics);
    registerConnectionWidget(chkMetaEvents);
    registerConnectionWidget(chkMetaGeo);
    registerConnectionWidget(btnRefreshMetaConfigs);
    registerConnectionWidget(btnApplyMetaConfig);
    registerConnectionWidget(btnToggleMetaStream);
    registerConnectionWidget(btnPollMetaOnce);
    registerConnectionWidget(tableMetaObjects);

    // Analytics Rules
    registerConnectionWidget(tableRules);
    registerConnectionWidget(editRuleName);
    registerConnectionWidget(cmbRuleType);
    registerConnectionWidget(editRuleClasses);
    registerConnectionWidget(spinRuleMinConf);
    registerConnectionWidget(spinRuleDwellTime);
    registerConnectionWidget(btnAddRule);
    registerConnectionWidget(btnDeleteRule);
    registerConnectionWidget(btnRefreshRules);

    // PKI Certificates
    registerConnectionWidget(tableCertificates);
    registerConnectionWidget(btnRefreshCerts);
    registerConnectionWidget(editNewCertId);
    registerConnectionWidget(editNewCertSubject);
    registerConnectionWidget(spinNewCertDays);
    registerConnectionWidget(btnCreateSelfSignedCert);
    registerConnectionWidget(btnGenerateCsr);
    registerConnectionWidget(btnDeleteCert);
    registerConnectionWidget(cmbClientCertMode);
    registerConnectionWidget(btnApplyClientCertMode);

    // Profile G: Recordings & Replay
    registerConnectionWidget(btnRefreshRecordingSummary);
    registerConnectionWidget(tableRecordings);
    registerConnectionWidget(editNewRecordingSource);
    registerConnectionWidget(editNewRecordingContent);
    registerConnectionWidget(btnCreateRecording);
    registerConnectionWidget(btnDeleteRecording);
    registerConnectionWidget(btnRefreshRecordings);
    registerConnectionWidget(cmbTrackType);
    registerConnectionWidget(editTrackDesc);
    registerConnectionWidget(btnCreateTrack);
    registerConnectionWidget(btnDeleteTrack);
    registerConnectionWidget(tableRecordingJobs);
    registerConnectionWidget(editJobRecordingToken);
    registerConnectionWidget(editJobSourceToken);
    registerConnectionWidget(spinJobPriority);
    registerConnectionWidget(cmbJobMode);
    registerConnectionWidget(btnCreateJob);
    registerConnectionWidget(btnToggleJobMode);
    registerConnectionWidget(btnDeleteJob);
    registerConnectionWidget(btnRefreshRecordingJobs);
    registerConnectionWidget(editSearchScope);
    registerConnectionWidget(btnFindRecordings);
    registerConnectionWidget(tableSearchResults);
    registerConnectionWidget(editEventStartUtc);
    registerConnectionWidget(editEventEndUtc);
    registerConnectionWidget(btnFindEvents);
    registerConnectionWidget(tableEventSearchResults);
    registerConnectionWidget(editReplayUri);
    registerConnectionWidget(btnResolveReplayUri);
    registerConnectionWidget(btnPlayReplayUri);

    // Thermal & Radiometry
    registerConnectionWidget(spinEmissivity);
    registerConnectionWidget(spinTargetDistance);
    registerConnectionWidget(spinReflectedTemp);
    registerConnectionWidget(spinAtmosphericTemp);
    registerConnectionWidget(spinRelativeHumidity);
    registerConnectionWidget(spinWindowTransmission);
    registerConnectionWidget(btnRefreshRadiometry);
    registerConnectionWidget(btnApplyRadiometry);
    registerConnectionWidget(cmbThermalPalettes);
    registerConnectionWidget(btnSetPalette);
    registerConnectionWidget(btnRefreshPalettes);
    registerConnectionWidget(btnTriggerNuc);
    registerConnectionWidget(tableRadiometry);
    registerConnectionWidget(cmbRadType);
    registerConnectionWidget(editRadToken);
    registerConnectionWidget(editRadLabel);
    registerConnectionWidget(spinRadX1);
    registerConnectionWidget(spinRadY1);
    registerConnectionWidget(spinRadX2);
    registerConnectionWidget(spinRadY2);
    registerConnectionWidget(btnAddMeasurement);
    registerConnectionWidget(btnDeleteMeasurement);
    registerConnectionWidget(btnRefreshMeasurements);
    registerConnectionWidget(spinAlarmThreshold);
}

void OnvifCameraTab::updateConnectionUi(bool connected)
{
    btnConnect->setEnabled(!connected);
    btnDisconnect->setEnabled(connected);
    for (QWidget* w : std::as_const(m_connectionWidgets)) {
        w->setEnabled(connected);
    }

    if (!connected && btnToggleMetaStream->isChecked()) {
        btnToggleMetaStream->setChecked(false);
    }

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

        tableCertificates->setRowCount(0);
        editNewCertId->clear();
        editNewCertSubject->clear();
        tableRecordings->setRowCount(0);
        editNewRecordingSource->clear();
        editNewRecordingContent->clear();
        editTrackDesc->clear();
        tableRecordingJobs->setRowCount(0);
        editJobRecordingToken->clear();
        editJobSourceToken->clear();
        tableSearchResults->setRowCount(0);
        editSearchScope->clear();
        tableEventSearchResults->setRowCount(0);
        editEventStartUtc->clear();
        editEventEndUtc->clear();
        editReplayUri->clear();
        lblRecordingSummary->setText(tr("Storage Summary: Disconnected"));

        tableRadiometry->setRowCount(0);
        cmbThermalPalettes->clear();
        lblNucStatus->setText(tr("NUC: Ready"));
        lblThermalAlarmStatus->setText(tr("Alarm: Normal"));
        lblThermalAlarmStatus->setStyleSheet(QStringLiteral("font-weight: bold; color: #238636; padding: 2px 8px;"));
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

void OnvifCameraTab::handleDiscoveryFinished(const QList<Onvif::DiscoveredDevice>& devices)
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
        btnConnect->setEnabled(true);
        lblConnectionStatus->setText(tr("Failed"));
        lblConnectionStatus->setStyleSheet("color: #da3633; font-weight: bold;");
        QMessageBox::critical(this, tr("Connection Failed"),
            tr("Failed to connect to ONVIF camera at:\n%1\nPlease verify IP, port, and credentials.").arg(endpoint));
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

        handleRefreshCertificates();
        handleRefreshRecordings();
        handleRefreshRecordingJobs();
        handleRefreshRecordingSummary();
        handleRefreshGeoLocation();
        handleRefreshRadiometry();
        handleRefreshPalettes();
        handleRefreshMeasurements();
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

void OnvifCameraTab::handleRefreshGeoLocation()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshGeoLocation();
    }
}

void OnvifCameraTab::handleSaveGeoLocation()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    Onvif::LocationEntity loc {};
    loc.entity = "Device";
    loc.token = "Location_1";
    loc.fixed = true;
    loc.location.latitude = spinCameraLat->value();
    loc.location.longitude = spinCameraLon->value();
    loc.location.elevation = spinCameraElev->value();
    loc.orientation.yaw = spinCameraYaw->value();
    loc.orientation.pitch = spinCameraPitch->value();
    loc.orientation.roll = 0.0;
    m_onvifDevice->updateGeoLocation(loc);
}

void OnvifCameraTab::handleExecuteGeoMove()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const double lat = spinTargetLat->value();
    const double lon = spinTargetLon->value();
    const double elev = spinTargetElev->value();
    const double w = spinTargetWidth->value();
    const double h = spinTargetHeight->value();
    m_onvifDevice->geoMove(lat, lon, elev, 1.0, w, h);
}

void OnvifCameraTab::handleExecuteAbsoluteSpherical()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const double az = spinSphericalAzimuth->value();
    const double el = spinSphericalElevation->value();
    const double zoom = spinSphericalZoom->value();
    m_onvifDevice->absoluteMoveSpherical(az, el, zoom);
}

void OnvifCameraTab::handleUpdateLiveGeoTargetReadout()
{
    if (!spinCameraLat || !spinTargetLat || !lblComputedGeoBearing) {
        return;
    }
    Onvif::GeoLocation camLoc { spinCameraLat->value(), spinCameraLon->value(), spinCameraElev->value() };
    Onvif::GeoOrientation camOri { spinCameraYaw->value(), spinCameraPitch->value(), 0.0 };
    Onvif::GeoLocation tgtLoc { spinTargetLat->value(), spinTargetLon->value(), spinTargetElev->value() };

    double pan = 0.0;
    double tilt = 0.0;
    double dist = 0.0;
    Onvif::Geodesy::computeTargetAzimuthElevation(camLoc, camOri, tgtLoc, pan, tilt, dist);

    lblComputedGeoBearing->setText(tr("Bearing: %1°").arg(QString::number(pan, 'f', 2)));
    lblComputedGeoTilt->setText(tr("Tilt: %1°").arg(QString::number(tilt, 'f', 2)));
    lblComputedGeoDistance->setText(tr("Slant Dist: %1 m").arg(QString::number(dist, 'f', 1)));
}

void OnvifCameraTab::handleGeoLocationUpdated(const Onvif::LocationEntity& location)
{
    if (spinCameraLat && spinCameraLon && spinCameraElev && spinCameraYaw && spinCameraPitch) {
        const QSignalBlocker b1(spinCameraLat);
        const QSignalBlocker b2(spinCameraLon);
        const QSignalBlocker b3(spinCameraElev);
        const QSignalBlocker b4(spinCameraYaw);
        const QSignalBlocker b5(spinCameraPitch);

        spinCameraLat->setValue(location.location.latitude);
        spinCameraLon->setValue(location.location.longitude);
        spinCameraElev->setValue(location.location.elevation);
        spinCameraYaw->setValue(location.orientation.yaw);
        spinCameraPitch->setValue(location.orientation.pitch);

        handleUpdateLiveGeoTargetReadout();
    }
}

void OnvifCameraTab::handleGeoMoveCompleted(bool success)
{
    if (!success) {
        QMessageBox::warning(this, tr("GeoMove"), tr("Failed to execute ONVIF GeoMove request."));
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

void OnvifCameraTab::handlePresetsUpdated(const std::vector<Onvif::PtzPreset>& presets)
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
            if (tour.status == Onvif::PresetTourState::Touring) {
                statusStr = tr("Touring");
                colorStr = "#7ee787";
            } else if (tour.status == Onvif::PresetTourState::Paused) {
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
        m_onvifDevice->operatePresetTour(token, Onvif::PresetTourOperation::Start);
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
        m_onvifDevice->operatePresetTour(token, Onvif::PresetTourOperation::Pause);
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
        m_onvifDevice->operatePresetTour(token, Onvif::PresetTourOperation::Stop);
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

    Onvif::PresetTour tour;
    tour.token = token.toStdString();
    tour.name = cmbPresetTours->currentText().toStdString();

    for (int r = 0; r < tableTourSpots->rowCount(); ++r) {
        Onvif::PresetTourSpot spot;
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

void OnvifCameraTab::handleToursUpdated(const std::vector<Onvif::PresetTour>& tours)
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

void OnvifCameraTab::handleStatusUpdated(const Onvif::PtzStatus& status)
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

    Onvif::ImagingSettings settings {};
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

void OnvifCameraTab::handleImagingSettingsUpdated(const Onvif::ImagingSettings& settings)
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

void OnvifCameraTab::handleEventReceived(const Onvif::OnvifEvent& event)
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
    Onvif::OsdConfig osd;
    osd.plainText = editOsdText->text().toStdString();
    osd.fontSize = static_cast<std::uint32_t>(spinOsdFontSize->value());
    osd.isDateAndTime = chkOsdDateTime->isChecked();

    osd.position = osdPositionFromIndex(cmbOsdPosition->currentIndex());

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
    Onvif::OsdConfig osd;
    osd.token = token.toStdString();
    osd.plainText = editOsdText->text().toStdString();
    osd.fontSize = static_cast<std::uint32_t>(spinOsdFontSize->value());
    osd.isDateAndTime = chkOsdDateTime->isChecked();

    osd.position = osdPositionFromIndex(cmbOsdPosition->currentIndex());

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

void OnvifCameraTab::handleOsdsUpdated(const std::vector<Onvif::OsdConfig>& osds)
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
        if (osd.position == Onvif::OsdPositionType::UpperRight) {
            posStr = tr("UpperRight");
        } else if (osd.position == Onvif::OsdPositionType::LowerLeft) {
            posStr = tr("LowerLeft");
        } else if (osd.position == Onvif::OsdPositionType::LowerRight) {
            posStr = tr("LowerRight");
        } else if (osd.position == Onvif::OsdPositionType::Custom) {
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

void OnvifCameraTab::handleRefreshMasks()
{
    if (m_onvifDevice) {
        m_onvifDevice->refreshMasks();
    }
}

void OnvifCameraTab::handleAddMask()
{
    if (!m_onvifDevice) {
        return;
    }
    Onvif::PrivacyMask mask;
    mask.token = editMaskToken->text().trimmed().toStdString();
    mask.configurationToken = "VideoSourceConfig_1";

    const int typeIdx = cmbMaskType->currentIndex();
    if (typeIdx == 1) {
        mask.type = Onvif::MaskType::Pixelated;
    } else if (typeIdx == 2) {
        mask.type = Onvif::MaskType::Blurred;
    } else {
        mask.type = Onvif::MaskType::Color;
    }

    mask.color.x = spinMaskColorR->value();
    mask.color.y = spinMaskColorG->value();
    mask.color.z = spinMaskColorB->value();
    mask.color.colorspace = "RGB";
    mask.enabled = chkMaskEnabled->isChecked();

    // Default normalized rectangle polygon
    mask.polygon = { { 0.1f, 0.1f }, { 0.3f, 0.1f }, { 0.3f, 0.3f }, { 0.1f, 0.3f } };

    m_onvifDevice->createMask(mask);
}

void OnvifCameraTab::handleUpdateMask()
{
    if (!m_onvifDevice || tableMasks == nullptr) {
        return;
    }
    const int row = tableMasks->currentRow();
    if (row < 0 || row >= tableMasks->rowCount()) {
        return;
    }
    const QString token = tableMasks->item(row, 0)->text();
    Onvif::PrivacyMask mask;
    mask.token = token.toStdString();
    mask.configurationToken
        = tableMasks->item(row, 1) ? tableMasks->item(row, 1)->text().toStdString() : "VideoSourceConfig_1";

    const int typeIdx = cmbMaskType->currentIndex();
    if (typeIdx == 1) {
        mask.type = Onvif::MaskType::Pixelated;
    } else if (typeIdx == 2) {
        mask.type = Onvif::MaskType::Blurred;
    } else {
        mask.type = Onvif::MaskType::Color;
    }

    mask.color.x = spinMaskColorR->value();
    mask.color.y = spinMaskColorG->value();
    mask.color.z = spinMaskColorB->value();
    mask.color.colorspace = "RGB";
    mask.enabled = chkMaskEnabled->isChecked();
    mask.polygon = { { 0.1f, 0.1f }, { 0.3f, 0.1f }, { 0.3f, 0.3f }, { 0.1f, 0.3f } };

    m_onvifDevice->setMask(mask);
}

void OnvifCameraTab::handleDeleteMask()
{
    if (!m_onvifDevice || tableMasks == nullptr) {
        return;
    }
    const int row = tableMasks->currentRow();
    if (row < 0 || row >= tableMasks->rowCount()) {
        return;
    }
    const QString token = tableMasks->item(row, 0)->text();
    m_onvifDevice->deleteMask(token);
}

void OnvifCameraTab::handleMasksUpdated(const std::vector<Onvif::PrivacyMask>& masks)
{
    if (tableMasks == nullptr) {
        return;
    }
    tableMasks->setRowCount(0);
    for (const auto& mask : masks) {
        const int row = tableMasks->rowCount();
        tableMasks->insertRow(row);
        tableMasks->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(mask.token)));
        tableMasks->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(mask.configurationToken)));
        tableMasks->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(Onvif::maskTypeToString(mask.type))));
        tableMasks->setItem(row, 3,
            new QTableWidgetItem(QString("R:%1 G:%2 B:%3").arg(mask.color.x).arg(mask.color.y).arg(mask.color.z)));
        tableMasks->setItem(row, 4, new QTableWidgetItem(mask.enabled ? tr("Enabled") : tr("Disabled")));
    }
}

void OnvifCameraTab::handleMaskSelectionChanged()
{
    if (tableMasks == nullptr) {
        return;
    }
    const int row = tableMasks->currentRow();
    if (row < 0 || row >= tableMasks->rowCount()) {
        return;
    }
    if (tableMasks->item(row, 0)) {
        editMaskToken->setText(tableMasks->item(row, 0)->text());
    }
    if (tableMasks->item(row, 2)) {
        const QString typeStr = tableMasks->item(row, 2)->text();
        const int idx = cmbMaskType->findText(typeStr);
        if (idx >= 0) {
            cmbMaskType->setCurrentIndex(idx);
        }
    }
    if (tableMasks->item(row, 4)) {
        chkMaskEnabled->setChecked(tableMasks->item(row, 4)->text() == tr("Enabled"));
    }
}

void OnvifCameraTab::handleRefreshVideoSourceModes()
{
    if (m_onvifDevice) {
        m_onvifDevice->refreshVideoSourceModes();
    }
}

void OnvifCameraTab::handleApplyVideoSourceMode()
{
    if (!m_onvifDevice || cmbVideoSourceModes == nullptr) {
        return;
    }
    const QString modeToken = cmbVideoSourceModes->currentData().toString();
    if (modeToken.isEmpty()) {
        return;
    }
    m_onvifDevice->setVideoSourceMode("VideoSource_1", modeToken);
}

void OnvifCameraTab::handleVideoSourceModesUpdated(const std::vector<Onvif::VideoSourceMode>& modes)
{
    if (cmbVideoSourceModes == nullptr) {
        return;
    }
    const QString previousToken = cmbVideoSourceModes->currentData().toString();
    const QSignalBlocker blocker(cmbVideoSourceModes);
    cmbVideoSourceModes->clear();

    for (const auto& mode : modes) {
        QString label = QString("%1 (%2x%3 @ %4 fps%5)")
                            .arg(QString::fromStdString(mode.token))
                            .arg(mode.width)
                            .arg(mode.height)
                            .arg(mode.maxFramerate, 0, 'f', 0)
                            .arg(mode.reboot ? tr(", Reboot") : "");
        if (mode.enabled) {
            label += tr(" [ACTIVE]");
        }
        cmbVideoSourceModes->addItem(label, QString::fromStdString(mode.token));
    }

    int selectIndex = cmbVideoSourceModes->findData(previousToken);
    if (selectIndex < 0 && cmbVideoSourceModes->count() > 0) {
        selectIndex = 0;
    }
    if (selectIndex >= 0) {
        cmbVideoSourceModes->setCurrentIndex(selectIndex);
    }
}

void OnvifCameraTab::handleVideoSourceModeChanged(const QString& modeToken, bool rebootRequired)
{
    if (rebootRequired) {
        QMessageBox::warning(this, tr("Camera Mode Changed"),
            tr("Mode '%1' applied successfully.\nA camera reboot is required to activate this sensor mode.")
                .arg(modeToken));
    } else {
        QMessageBox::information(this, tr("Camera Mode Changed"), tr("Mode '%1' applied successfully.").arg(modeToken));
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

    Onvif::OnvifUser user {};
    user.username = username.toStdString();
    user.password = password.toStdString();
    user.level = Onvif::userLevelFromString(cmbUserLevel->currentText().toStdString());

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

    Onvif::OnvifUser user {};
    user.username = username.toStdString();
    user.password = password.toStdString();
    user.level = Onvif::userLevelFromString(cmbUserLevel->currentText().toStdString());

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

void OnvifCameraTab::handleUsersUpdated(const std::vector<Onvif::OnvifUser>& users)
{
    tableUsers->setRowCount(0);
    for (const auto& u : users) {
        const int row = tableUsers->rowCount();
        tableUsers->insertRow(row);
        tableUsers->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(u.username)));
        tableUsers->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(Onvif::userLevelToString(u.level))));
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
    Onvif::NetworkInterfaceConfig cfg {};
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
    Onvif::DnsConfig dns {};
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
    Onvif::NtpConfig ntp {};
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

    Onvif::SystemDateTimeConfig cfg {};
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

void OnvifCameraTab::handleNetworkUpdated(const std::vector<Onvif::NetworkInterfaceConfig>& ifaces)
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

void OnvifCameraTab::handleDnsUpdated(const Onvif::DnsConfig& dns)
{
    chkDnsDhcp->setChecked(dns.fromDhcp);
    QStringList servers {};
    for (const auto& s : dns.dnsServers) {
        servers << QString::fromStdString(s);
    }
    editDnsServers->setText(servers.join(QStringLiteral(", ")));
}

void OnvifCameraTab::handleNtpUpdated(const Onvif::NtpConfig& ntp)
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

void OnvifCameraTab::handleFocusStatusUpdated(const Onvif::FocusStatus20& status)
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

void OnvifCameraTab::handleImagingPresetsUpdated(const std::vector<Onvif::ImagingPreset>& presets)
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
    Onvif::RelayOutputConfig cfg {};
    cfg.token = token.toStdString();
    cfg.mode = Onvif::relayModeFromString(cmbRelayMode->currentText().toStdString());
    cfg.delayTimeSeconds = static_cast<float>(spinRelayDelay->value());
    cfg.idleState = Onvif::relayIdleStateFromString(cmbRelayIdleState->currentText().toStdString());

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

void OnvifCameraTab::handleRelaysUpdated(const std::vector<Onvif::RelayOutputConfig>& relays)
{
    if (tableRelays == nullptr) {
        return;
    }
    tableRelays->setRowCount(0);
    for (const auto& r : relays) {
        const int row = tableRelays->rowCount();
        tableRelays->insertRow(row);
        tableRelays->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.token)));
        tableRelays->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(Onvif::relayModeToString(r.mode))));
        tableRelays->setItem(row, 2, new QTableWidgetItem(QString::number(r.delayTimeSeconds, 'f', 1)));
        tableRelays->setItem(
            row, 3, new QTableWidgetItem(QString::fromStdString(Onvif::relayIdleStateToString(r.idleState))));
        const QString stateStr = QString::fromStdString(Onvif::relayLogicalStateToString(r.logicalState));
        auto* itemState = new QTableWidgetItem(stateStr);
        if (r.logicalState == Onvif::RelayLogicalState::Active) {
            itemState->setForeground(QBrush(QColor("#7ee787")));
        } else {
            itemState->setForeground(QBrush(QColor("#8b949e")));
        }
        tableRelays->setItem(row, 4, itemState);
    }
}

void OnvifCameraTab::handleDigitalInputsUpdated(const std::vector<Onvif::DigitalInputConfig>& inputs)
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
            row, 1, new QTableWidgetItem(QString::fromStdString(Onvif::relayIdleStateToString(in.idleState))));
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

void OnvifCameraTab::handleRefreshMetadataConfigs()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshMetadataConfigurations();
    }
}

void OnvifCameraTab::handleApplyMetadataConfig()
{
    if (m_onvifDevice == nullptr || cmbMetaConfigs == nullptr) {
        return;
    }
    const QString token = cmbMetaConfigs->currentData().toString();
    if (token.isEmpty()) {
        return;
    }

    Onvif::MetadataConfiguration cfg {};
    cfg.token = token.toStdString();
    cfg.name = cmbMetaConfigs->currentText().toStdString();
    cfg.ptzStatusEnabled = chkMetaPtzStatus != nullptr && chkMetaPtzStatus->isChecked();
    cfg.analyticsEnabled = chkMetaAnalytics != nullptr && chkMetaAnalytics->isChecked();
    cfg.eventsEnabled = chkMetaEvents != nullptr && chkMetaEvents->isChecked();
    cfg.geoOrientationEnabled = chkMetaGeo != nullptr && chkMetaGeo->isChecked();

    m_onvifDevice->setMetadataConfiguration(cfg);
}

void OnvifCameraTab::handleToggleMetadataStream(bool start)
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    if (start) {
        if (btnToggleMetaStream != nullptr) {
            btnToggleMetaStream->setText(tr("⏹ Stop Metadata Streaming"));
        }
        m_onvifDevice->startMetadataStreaming(200);
    } else {
        if (btnToggleMetaStream != nullptr) {
            btnToggleMetaStream->setText(tr("▶ Start Metadata Streaming (5 Hz)"));
        }
        m_onvifDevice->stopMetadataStreaming();
    }
}

void OnvifCameraTab::handlePollMetadataOnce()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->pollCurrentMetadata();
    }
}

void OnvifCameraTab::handleMetadataConfigsUpdated(const std::vector<Onvif::MetadataConfiguration>& configs)
{
    if (cmbMetaConfigs == nullptr) {
        return;
    }
    cmbMetaConfigs->clear();
    for (const auto& cfg : configs) {
        cmbMetaConfigs->addItem(QString::fromStdString(cfg.name), QString::fromStdString(cfg.token));
    }
    if (!configs.empty()) {
        const auto& first = configs.front();
        if (chkMetaPtzStatus != nullptr) {
            chkMetaPtzStatus->setChecked(first.ptzStatusEnabled);
        }
        if (chkMetaAnalytics != nullptr) {
            chkMetaAnalytics->setChecked(first.analyticsEnabled);
        }
        if (chkMetaEvents != nullptr) {
            chkMetaEvents->setChecked(first.eventsEnabled);
        }
        if (chkMetaGeo != nullptr) {
            chkMetaGeo->setChecked(first.geoOrientationEnabled);
        }
    }
}

void OnvifCameraTab::handleMetadataReceived(const Onvif::MetadataStreamPayload& payload)
{
    if (lblMetaTelemetry != nullptr) {
        QString text = QStringLiteral("PTZ: ");
        if (payload.ptzStatus.has_value()) {
            text += QStringLiteral("Pan=%1 Tilt=%2 Zoom=%3 | Moving: %4")
                        .arg(payload.ptzStatus->pan, 0, 'f', 3)
                        .arg(payload.ptzStatus->tilt, 0, 'f', 3)
                        .arg(payload.ptzStatus->zoom, 0, 'f', 3)
                        .arg(payload.ptzStatus->isMoving ? tr("Yes") : tr("No"));
        } else {
            text += QStringLiteral("Pan=N/A Tilt=N/A Zoom=N/A");
        }

        if (payload.analyticsFrame.has_value()) {
            text += QStringLiteral(" | Objs: %1 (%2x%3)")
                        .arg(payload.analyticsFrame->objects.size())
                        .arg(payload.analyticsFrame->frameWidth)
                        .arg(payload.analyticsFrame->frameHeight);
        }
        lblMetaTelemetry->setText(text);
    }

    if (tableMetaObjects == nullptr || !payload.analyticsFrame.has_value()) {
        return;
    }

    const auto& frame = *payload.analyticsFrame;
    const QString ts = QString::fromStdString(frame.utcTime);
    tableMetaObjects->setRowCount(0);
    for (const auto& obj : frame.objects) {
        const int row = tableMetaObjects->rowCount();
        tableMetaObjects->insertRow(row);
        tableMetaObjects->setItem(row, 0, new QTableWidgetItem(QString::number(obj.objectId)));
        tableMetaObjects->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(obj.className)));
        tableMetaObjects->setItem(row, 2, new QTableWidgetItem(QString::number(obj.confidence, 'f', 2)));
        const QString bbox = QStringLiteral("[%1, %2, %3, %4]")
                                 .arg(obj.boundingBox.left, 0, 'f', 2)
                                 .arg(obj.boundingBox.top, 0, 'f', 2)
                                 .arg(obj.boundingBox.right, 0, 'f', 2)
                                 .arg(obj.boundingBox.bottom, 0, 'f', 2);
        tableMetaObjects->setItem(row, 3, new QTableWidgetItem(bbox));
        const QString geo = QStringLiteral("Lat: %1, Lon: %2")
                                .arg(obj.geoLocation.latitude, 0, 'f', 4)
                                .arg(obj.geoLocation.longitude, 0, 'f', 4);
        tableMetaObjects->setItem(row, 4, new QTableWidgetItem(geo));
        tableMetaObjects->setItem(row, 5, new QTableWidgetItem(ts));
    }
}

// =========================================================================
// Profile M & T: Video Analytics Rule Engine & Modules Handlers
// =========================================================================

void OnvifCameraTab::handleRefreshRules()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRules();
        m_onvifDevice->refreshSupportedRules();
    }
}

void OnvifCameraTab::handleAddRule()
{
    if (m_onvifDevice == nullptr || editRuleName == nullptr || cmbRuleType == nullptr) {
        return;
    }
    const QString name = editRuleName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Add Rule"), tr("Please enter a rule name."));
        return;
    }

    Onvif::AnalyticsRule rule;
    rule.name = name.toStdString();
    rule.type = cmbRuleType->currentData().toString().toStdString();
    rule.enabled = true;

    if (editRuleClasses != nullptr) {
        const QString classesStr = editRuleClasses->text().trimmed();
        if (!classesStr.isEmpty()) {
            const auto list = classesStr.split(',', Qt::SkipEmptyParts);
            for (const auto& c : list) {
                rule.objectClasses.push_back(c.trimmed().toStdString());
            }
        }
    }

    if (spinRuleMinConf != nullptr) {
        rule.minConfidence = static_cast<float>(spinRuleMinConf->value());
    }

    if (spinRuleDwellTime != nullptr) {
        rule.dwellTimeSeconds = spinRuleDwellTime->value();
    }

    // Default geometries for quick evaluation setup
    if (rule.type.find("Line") != std::string::npos) {
        rule.lineStart = { 0.2f, 0.5f };
        rule.lineEnd = { 0.8f, 0.5f };
        rule.direction = "Any";
    } else {
        rule.polygon = { { 0.2f, 0.2f }, { 0.8f, 0.2f }, { 0.8f, 0.8f }, { 0.2f, 0.8f } };
    }

    if (m_onvifDevice->createRules({ rule })) {
        editRuleName->clear();
    } else {
        QMessageBox::warning(this, tr("Add Rule"), tr("Failed to create rule on camera."));
    }
}

void OnvifCameraTab::handleDeleteRule()
{
    if (m_onvifDevice == nullptr || tableRules == nullptr) {
        return;
    }
    const int row = tableRules->currentRow();
    if (row < 0 || row >= tableRules->rowCount()) {
        QMessageBox::warning(this, tr("Delete Rule"), tr("Please select a rule to delete."));
        return;
    }
    const auto* item = tableRules->item(row, 0);
    if (item == nullptr) {
        return;
    }
    m_onvifDevice->deleteRules({ item->text() });
}

void OnvifCameraTab::handleRulesUpdated(const std::vector<Onvif::AnalyticsRule>& rules)
{
    if (tableRules == nullptr) {
        return;
    }
    tableRules->setRowCount(0);
    for (const auto& r : rules) {
        const int row = tableRules->rowCount();
        tableRules->insertRow(row);
        tableRules->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.name)));
        tableRules->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.type)));

        QString classesStr;
        for (size_t i = 0; i < r.objectClasses.size(); ++i) {
            if (i > 0)
                classesStr += QStringLiteral(", ");
            classesStr += QString::fromStdString(r.objectClasses[i]);
        }
        tableRules->setItem(row, 2, new QTableWidgetItem(classesStr.isEmpty() ? tr("All") : classesStr));

        QString paramStr;
        if (r.type.find("Loitering") != std::string::npos) {
            paramStr = QStringLiteral("Dwell: %1s | Polygon: %2 pts").arg(r.dwellTimeSeconds).arg(r.polygon.size());
        } else if (r.type.find("Line") != std::string::npos) {
            paramStr = QStringLiteral("Dir: %1 | Line: (%2,%3)->(%4,%5)")
                           .arg(QString::fromStdString(r.direction))
                           .arg(r.lineStart.x, 0, 'f', 2)
                           .arg(r.lineStart.y, 0, 'f', 2)
                           .arg(r.lineEnd.x, 0, 'f', 2)
                           .arg(r.lineEnd.y, 0, 'f', 2);
        } else if (r.type.find("Field") != std::string::npos) {
            paramStr = QStringLiteral("Polygon: %1 pts").arg(r.polygon.size());
        } else {
            paramStr = QStringLiteral("Sensitivity: %1").arg(r.sensitivity);
        }
        tableRules->setItem(row, 3, new QTableWidgetItem(paramStr));

        auto* statusItem = new QTableWidgetItem(r.enabled ? tr("Active") : tr("Disabled"));
        statusItem->setForeground(QBrush(r.enabled ? QColor("#7ee787") : QColor("#8b949e")));
        tableRules->setItem(row, 4, statusItem);
    }
}

void OnvifCameraTab::handleSupportedRulesUpdated(const std::vector<Onvif::AnalyticsRuleDescription>& /*rules*/)
{
}

void OnvifCameraTab::handleRefreshAnalyticsModules()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshAnalyticsModules();
        m_onvifDevice->refreshSupportedAnalyticsModules();
    }
}

void OnvifCameraTab::handleAnalyticsModulesUpdated(const std::vector<Onvif::AnalyticsModule>& /*modules*/)
{
}

void OnvifCameraTab::handleFetchSystemLog()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->fetchSystemLog(Onvif::SystemLogType::System);
    }
}

void OnvifCameraTab::handleFetchAccessLog()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->fetchSystemLog(Onvif::SystemLogType::Access);
    }
}

void OnvifCameraTab::handleSystemLogReceived(Onvif::SystemLogType logType, const QString& logData)
{
    if (txtSystemLogs == nullptr) {
        return;
    }
    const QString typeName = (logType == Onvif::SystemLogType::System) ? QStringLiteral("=== SYSTEM LOG ===")
                                                                       : QStringLiteral("=== ACCESS LOG ===");
    txtSystemLogs->append(typeName);
    txtSystemLogs->append(logData);
    txtSystemLogs->append(QStringLiteral(""));
}

void OnvifCameraTab::handleFetchSupportInfo()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->fetchSystemSupportInformation();
    }
}

void OnvifCameraTab::handleSystemSupportInfoReceived(const Onvif::SystemSupportInfo& info)
{
    if (txtSystemLogs == nullptr) {
        return;
    }
    txtSystemLogs->append(QStringLiteral("=== SYSTEM SUPPORT INFORMATION ==="));
    txtSystemLogs->append(QString::fromStdString(info.rawDiagnostics));
    txtSystemLogs->append(QStringLiteral(""));
}

void OnvifCameraTab::handleDownloadBackup()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->downloadSystemBackup();
    }
}

void OnvifCameraTab::handleSystemBackupReceived(const QString& backupData)
{
    if (editBackupPayload != nullptr) {
        editBackupPayload->setText(backupData);
    }
    QMessageBox::information(this, tr("Backup Downloaded"),
        tr("Camera configuration backup retrieved successfully (%1 characters).").arg(backupData.size()));
}

void OnvifCameraTab::handleRestoreBackup()
{
    if (m_onvifDevice == nullptr || editBackupPayload == nullptr) {
        return;
    }
    const QString payload = editBackupPayload->text().trimmed();
    if (payload.isEmpty()) {
        QMessageBox::warning(this, tr("Restore System"), tr("Backup payload cannot be empty."));
        return;
    }

    const auto answer = QMessageBox::warning(this, tr("Restore System"),
        tr("Are you sure you want to restore this configuration backup?\nThe camera may reboot."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_onvifDevice->restoreSystem(payload);
}

void OnvifCameraTab::handleSystemRestoreCompleted(bool success)
{
    if (success) {
        QMessageBox::information(this, tr("System Restore"), tr("System configuration restored successfully!"));
    } else {
        QMessageBox::critical(this, tr("System Restore"), tr("Failed to restore system configuration."));
    }
}

void OnvifCameraTab::handleFetchEndpointReference()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->fetchEndpointReference();
    }
}

void OnvifCameraTab::handleEndpointReferenceReceived(const QString& endpointReference)
{
    if (lblEndpointRef != nullptr) {
        lblEndpointRef->setText(QStringLiteral("UUID: %1").arg(endpointReference));
    }
}

// =========================================================================
// Profile G: Recordings & Replay
// =========================================================================

void OnvifCameraTab::handleRefreshRecordings()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRecordings();
    }
}

void OnvifCameraTab::handleCreateRecording()
{
    if (m_onvifDevice == nullptr)
        return;
    Onvif::RecordingConfig cfg;
    cfg.sourceToken = editNewRecordingSource->text().trimmed().toStdString();
    if (cfg.sourceToken.empty())
        cfg.sourceToken = "VideoSource_1";
    cfg.content = editNewRecordingContent->text().trimmed().toStdString();
    if (cfg.content.empty())
        cfg.content = "MainStream";
    cfg.maximumRetentionTime = "P30D";

    const QString token = m_onvifDevice->createRecording(cfg);
    if (!token.isEmpty()) {
        editNewRecordingSource->clear();
        editNewRecordingContent->clear();
        QMessageBox::information(this, tr("Create Recording"), tr("Created recording with token: %1").arg(token));
    } else {
        QMessageBox::critical(this, tr("Create Recording"), tr("Failed to create recording."));
    }
}

void OnvifCameraTab::handleDeleteRecording()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableRecordings->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Delete Recording"), tr("Please select a recording to delete."));
        return;
    }
    const QString token = tableRecordings->item(row, 0)->text();
    if (m_onvifDevice->deleteRecording(token)) {
        QMessageBox::information(this, tr("Delete Recording"), tr("Recording %1 deleted.").arg(token));
    } else {
        QMessageBox::critical(this, tr("Delete Recording"), tr("Failed to delete recording %1.").arg(token));
    }
}

void OnvifCameraTab::handleCreateTrack()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableRecordings->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Add Track"), tr("Please select a parent recording in the table."));
        return;
    }
    const QString recToken = tableRecordings->item(row, 0)->text();
    Onvif::RecordingTrack trk;
    trk.trackType = static_cast<Onvif::RecordingTrackType>(cmbTrackType->currentData().toInt());
    trk.description = editTrackDesc->text().trimmed().toStdString();
    if (trk.description.empty())
        trk.description = "PrimaryTrack";

    const QString trkToken = m_onvifDevice->createTrack(recToken, trk);
    if (!trkToken.isEmpty()) {
        editTrackDesc->clear();
        QMessageBox::information(this, tr("Add Track"), tr("Created track %1 in recording %2").arg(trkToken, recToken));
    } else {
        QMessageBox::critical(this, tr("Add Track"), tr("Failed to create track."));
    }
}

void OnvifCameraTab::handleDeleteTrack()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableRecordings->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Delete Track"), tr("Please select a recording in the table."));
        return;
    }
    const QString recToken = tableRecordings->item(row, 0)->text();
    const QString trackStr = tableRecordings->item(row, 4)->text();
    if (trackStr.isEmpty()) {
        QMessageBox::warning(this, tr("Delete Track"), tr("Selected recording has no tracks."));
        return;
    }
    const QString trackToken = trackStr.split(',').first().trimmed();
    if (m_onvifDevice->deleteTrack(recToken, trackToken)) {
        QMessageBox::information(this, tr("Delete Track"), tr("Deleted track %1.").arg(trackToken));
    } else {
        QMessageBox::critical(this, tr("Delete Track"), tr("Failed to delete track %1.").arg(trackToken));
    }
}

void OnvifCameraTab::handleRefreshRecordingJobs()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRecordingJobs();
    }
}

void OnvifCameraTab::handleCreateRecordingJob()
{
    if (m_onvifDevice == nullptr)
        return;
    Onvif::RecordingJob job;
    job.recordingToken = editJobRecordingToken->text().trimmed().toStdString();
    job.sourceToken = editJobSourceToken->text().trimmed().toStdString();
    if (job.recordingToken.empty() || job.sourceToken.empty()) {
        QMessageBox::warning(this, tr("Create Job"), tr("Recording Token and Source Token are required."));
        return;
    }
    job.priority = spinJobPriority->value();
    job.mode = static_cast<Onvif::RecordingJobMode>(cmbJobMode->currentData().toInt());

    const QString jobToken = m_onvifDevice->createRecordingJob(job);
    if (!jobToken.isEmpty()) {
        editJobRecordingToken->clear();
        editJobSourceToken->clear();
        QMessageBox::information(this, tr("Create Job"), tr("Created recording job: %1").arg(jobToken));
    } else {
        QMessageBox::critical(this, tr("Create Job"), tr("Failed to create recording job."));
    }
}

void OnvifCameraTab::handleToggleJobMode()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableRecordingJobs->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Toggle Job"), tr("Please select a recording job in the table."));
        return;
    }
    const QString jobToken = tableRecordingJobs->item(row, 0)->text();
    const QString curMode = tableRecordingJobs->item(row, 4)->text();
    const auto newMode = (curMode == "Active") ? Onvif::RecordingJobMode::Idle : Onvif::RecordingJobMode::Active;
    if (m_onvifDevice->setRecordingJobMode(jobToken, newMode)) {
        QMessageBox::information(this, tr("Toggle Job"), tr("Job %1 mode changed.").arg(jobToken));
    }
}

void OnvifCameraTab::handleDeleteRecordingJob()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableRecordingJobs->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Delete Job"), tr("Please select a recording job in the table."));
        return;
    }
    const QString jobToken = tableRecordingJobs->item(row, 0)->text();
    if (m_onvifDevice->deleteRecordingJob(jobToken)) {
        QMessageBox::information(this, tr("Delete Job"), tr("Deleted job %1.").arg(jobToken));
    }
}

void OnvifCameraTab::handleRefreshRecordingSummary()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRecordingSummary();
    }
}

void OnvifCameraTab::handleFindRecordings()
{
    if (m_onvifDevice == nullptr)
        return;
    const QString scope = editSearchScope->text().trimmed();
    const QString token = m_onvifDevice->findRecordings(scope, 20);
    if (!token.isEmpty()) {
        m_onvifDevice->refreshRecordingSearchResults(token);
    }
}

void OnvifCameraTab::handleFindEvents()
{
    if (m_onvifDevice == nullptr)
        return;
    const QString startUtc = editEventStartUtc->text().trimmed();
    const QString endUtc = editEventEndUtc->text().trimmed();
    const QString token = m_onvifDevice->findEvents(startUtc, endUtc, 20);
    if (!token.isEmpty()) {
        m_onvifDevice->refreshEventSearchResults(token);
    }
}

void OnvifCameraTab::handleResolveReplayUri()
{
    if (m_onvifDevice == nullptr)
        return;
    QString recToken;
    const int row = tableRecordings->currentRow();
    if (row >= 0) {
        recToken = tableRecordings->item(row, 0)->text();
    } else if (tableSearchResults->currentRow() >= 0) {
        recToken = tableSearchResults->item(tableSearchResults->currentRow(), 0)->text();
    } else if (!editNewRecordingSource->text().isEmpty()) {
        recToken = editNewRecordingSource->text();
    } else {
        recToken = "Rec_Main";
    }
    m_onvifDevice->resolveReplayUri(recToken);
}

void OnvifCameraTab::handlePlayInVideoStreamTab()
{
    const QString uri = editReplayUri->text().trimmed();
    if (!uri.isEmpty()) {
        emit streamUriSelected(uri);
    } else {
        QMessageBox::warning(this, tr("Play Replay"), tr("Please resolve or enter a replay RTSP URI first."));
    }
}

void OnvifCameraTab::handleRecordingsUpdated(const std::vector<Onvif::RecordingConfig>& recordings)
{
    tableRecordings->setRowCount(0);
    for (const auto& r : recordings) {
        const int row = tableRecordings->rowCount();
        tableRecordings->insertRow(row);
        tableRecordings->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.recordingToken)));
        tableRecordings->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.sourceToken)));
        tableRecordings->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.content)));
        tableRecordings->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(r.maximumRetentionTime)));

        QStringList trkTokens;
        for (const auto& t : r.tracks) {
            trkTokens << QString::fromStdString(t.trackToken);
        }
        tableRecordings->setItem(row, 4, new QTableWidgetItem(trkTokens.join(QStringLiteral(", "))));
    }
}

void OnvifCameraTab::handleRecordingJobsUpdated(const std::vector<Onvif::RecordingJob>& jobs)
{
    tableRecordingJobs->setRowCount(0);
    for (const auto& j : jobs) {
        const int row = tableRecordingJobs->rowCount();
        tableRecordingJobs->insertRow(row);
        tableRecordingJobs->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(j.jobToken)));
        tableRecordingJobs->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(j.recordingToken)));
        tableRecordingJobs->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(j.sourceToken)));
        tableRecordingJobs->setItem(row, 3, new QTableWidgetItem(QString::number(j.priority)));
        tableRecordingJobs->setItem(
            row, 4, new QTableWidgetItem(QString::fromStdString(Onvif::recordingJobModeToString(j.mode))));
    }
}

void OnvifCameraTab::handleRecordingSummaryUpdated(const Onvif::RecordingSummary& summary)
{
    lblRecordingSummary->setText(tr("Storage Summary: %1 recordings | Earliest: %2 | Latest: %3 | Total Size: %4 MB")
                                     .arg(summary.numberRecordings)
                                     .arg(QString::fromStdString(summary.dataFrom))
                                     .arg(QString::fromStdString(summary.dataUntil))
                                     .arg(summary.totalStorageBytes / (1024ULL * 1024ULL)));
}

void OnvifCameraTab::handleRecordingSearchResultsReceived(
    const QString& /*searchToken*/, const std::vector<Onvif::RecordingSearchResult>& results)
{
    tableSearchResults->setRowCount(0);
    for (const auto& r : results) {
        const int row = tableSearchResults->rowCount();
        tableSearchResults->insertRow(row);
        tableSearchResults->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.recordingToken)));
        tableSearchResults->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(r.trackToken)));
        tableSearchResults->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.earliestTime)));
        tableSearchResults->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(r.latestTime)));
        tableSearchResults->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(r.searchState)));
    }
}

void OnvifCameraTab::handleEventSearchResultsReceived(
    const QString& /*searchToken*/, const std::vector<Onvif::RecordedEventResult>& results)
{
    tableEventSearchResults->setRowCount(0);
    for (const auto& e : results) {
        const int row = tableEventSearchResults->rowCount();
        tableEventSearchResults->insertRow(row);
        tableEventSearchResults->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(e.recordingToken)));
        tableEventSearchResults->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(e.eventTime)));
        tableEventSearchResults->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(e.topic)));
        tableEventSearchResults->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(e.source)));
        tableEventSearchResults->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(e.data)));
    }
}

void OnvifCameraTab::handleReplayUriResolved(const QString& recordingToken, const QString& uri)
{
    editReplayUri->setText(uri);
    QMessageBox::information(
        this, tr("Replay URI Resolved"), tr("Resolved replay URI for %1:\n%2").arg(recordingToken, uri));
}

// =========================================================================
// PKI Certificates & HTTPS/TLS Security
// =========================================================================

void OnvifCameraTab::handleRefreshCertificates()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshCertificates();
        m_onvifDevice->refreshClientCertificateMode();
    }
}

void OnvifCameraTab::handleCreateSelfSignedCert()
{
    if (m_onvifDevice == nullptr)
        return;
    const QString id = editNewCertId->text().trimmed();
    const QString subject = editNewCertSubject->text().trimmed();
    const int days = spinNewCertDays->value();
    if (id.isEmpty() || subject.isEmpty()) {
        QMessageBox::warning(this, tr("Create Certificate"), tr("Certificate ID and Subject DN cannot be empty."));
        return;
    }
    if (m_onvifDevice->createCertificate(id, subject, days)) {
        editNewCertId->clear();
        editNewCertSubject->clear();
        QMessageBox::information(
            this, tr("Create Certificate"), tr("Self-signed certificate %1 created successfully.").arg(id));
    } else {
        QMessageBox::critical(this, tr("Create Certificate"), tr("Failed to create certificate %1.").arg(id));
    }
}

void OnvifCameraTab::handleGenerateCsr()
{
    if (m_onvifDevice == nullptr)
        return;
    const QString id = editNewCertId->text().trimmed();
    const QString subject = editNewCertSubject->text().trimmed();
    if (id.isEmpty() || subject.isEmpty()) {
        QMessageBox::warning(this, tr("Generate CSR"), tr("Certificate ID and Subject DN cannot be empty."));
        return;
    }
    if (!m_onvifDevice->createPkcs10Csr(id, subject)) {
        QMessageBox::critical(this, tr("Generate CSR"), tr("Failed to generate PKCS#10 CSR for %1.").arg(id));
    }
}

void OnvifCameraTab::handleDeleteCertificate()
{
    if (m_onvifDevice == nullptr)
        return;
    const int row = tableCertificates->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("Delete Certificate"), tr("Please select a certificate to delete."));
        return;
    }
    const QString id = tableCertificates->item(row, 0)->text();
    if (m_onvifDevice->deleteCertificates({ id })) {
        QMessageBox::information(this, tr("Delete Certificate"), tr("Certificate %1 deleted.").arg(id));
    } else {
        QMessageBox::critical(this, tr("Delete Certificate"), tr("Failed to delete certificate %1.").arg(id));
    }
}

void OnvifCameraTab::handleApplyClientCertMode()
{
    if (m_onvifDevice == nullptr)
        return;
    const auto mode = static_cast<Onvif::ClientCertificateMode>(cmbClientCertMode->currentData().toInt());
    if (m_onvifDevice->setClientCertificateMode(mode)) {
        QMessageBox::information(
            this, tr("Client Certificate Mode"), tr("Client certificate authentication mode applied."));
    } else {
        QMessageBox::critical(this, tr("Client Certificate Mode"), tr("Failed to update client certificate mode."));
    }
}

void OnvifCameraTab::handleCertificatesUpdated(const std::vector<Onvif::OnvifCertificate>& certs)
{
    tableCertificates->setRowCount(0);
    for (const auto& c : certs) {
        const int row = tableCertificates->rowCount();
        tableCertificates->insertRow(row);
        tableCertificates->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(c.certificateId)));
        tableCertificates->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(c.info.subject)));
        tableCertificates->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(c.info.issuer)));
        tableCertificates->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(c.info.validNotBefore)));
        tableCertificates->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(c.info.validNotAfter)));
        tableCertificates->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(c.info.keyAlgorithm)));
    }
}

void OnvifCameraTab::handleCertificateInfoReceived(const Onvif::CertificateInformation& info)
{
    QMessageBox::information(this, tr("Certificate Information"),
        tr("Certificate ID: %1\nSubject: %2\nIssuer: %3\nValid From: %4\nValid Until: %5\nKey Usage: %6")
            .arg(QString::fromStdString(info.certificateId))
            .arg(QString::fromStdString(info.subject))
            .arg(QString::fromStdString(info.issuer))
            .arg(QString::fromStdString(info.validNotBefore))
            .arg(QString::fromStdString(info.validNotAfter))
            .arg(QString::fromStdString(info.keyAlgorithm)));
}

void OnvifCameraTab::handlePkcs10CsrReceived(const Onvif::Pkcs10Request& csr)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("PKCS#10 CSR Generated"));
    msgBox.setText(tr("Generated CSR for certificate '%1'").arg(QString::fromStdString(csr.certificateId)));
    msgBox.setDetailedText(QString::fromStdString(csr.csrBase64));
    msgBox.exec();
}

void OnvifCameraTab::handleClientCertModeUpdated(Onvif::ClientCertificateMode mode)
{
    const int idx = cmbClientCertMode->findData(static_cast<int>(mode));
    if (idx >= 0) {
        cmbClientCertMode->setCurrentIndex(idx);
    }
}

void OnvifCameraTab::handleRefreshRadiometry()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRadiometryConfiguration();
    }
}

void OnvifCameraTab::handleApplyRadiometry()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    Onvif::RadiometryConfig cfg;
    cfg.emissivity = static_cast<float>(spinEmissivity->value());
    cfg.distance = static_cast<float>(spinTargetDistance->value());
    cfg.reflectedTemperature = static_cast<float>(spinReflectedTemp->value());
    cfg.atmosphericTemperature = static_cast<float>(spinAtmosphericTemp->value());
    cfg.relativeHumidity = static_cast<float>(spinRelativeHumidity->value());
    cfg.windowTransmission = static_cast<float>(spinWindowTransmission->value());
    m_onvifDevice->setRadiometryConfiguration(cfg);
}

void OnvifCameraTab::handleRefreshPalettes()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshColorPalettes();
    }
}

void OnvifCameraTab::handleSetPalette()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString palette = cmbThermalPalettes->currentText().trimmed();
    if (!palette.isEmpty()) {
        m_onvifDevice->setColorPalette(palette);
    }
}

void OnvifCameraTab::handleTriggerNuc()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    lblNucStatus->setText(tr("NUC: Calibrating..."));
    lblNucStatus->setStyleSheet(QStringLiteral("color: #d29922; font-weight: bold;"));
    m_onvifDevice->triggerNuc();
}

void OnvifCameraTab::handleRefreshMeasurements()
{
    if (m_onvifDevice != nullptr) {
        m_onvifDevice->refreshRadiometryMeasurements();
    }
}

void OnvifCameraTab::handleAddMeasurement()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const QString type = cmbRadType->currentData().toString();
    const QString token = editRadToken->text().trimmed().isEmpty()
        ? QString("Rad_%1").arg(QDateTime::currentMSecsSinceEpoch() % 10000)
        : editRadToken->text().trimmed();
    const QString label = editRadLabel->text().trimmed();

    if (type == QStringLiteral("Spot")) {
        Onvif::RadiometrySpot spot;
        spot.token = token.toStdString();
        spot.label = label.toStdString();
        spot.position.x = static_cast<float>(spinRadX1->value());
        spot.position.y = static_cast<float>(spinRadY1->value());
        spot.temperature = 36.5f;

        auto spots = m_onvifDevice->radiometrySpots();
        bool found = false;
        for (auto& s : spots) {
            if (s.token == spot.token) {
                s = spot;
                found = true;
                break;
            }
        }
        if (!found) {
            spots.push_back(spot);
        }
        m_onvifDevice->setRadiometrySpots(spots);
    } else {
        Onvif::RadiometryBox box;
        box.token = token.toStdString();
        box.label = label.toStdString();
        box.topLeft.x = static_cast<float>(spinRadX1->value());
        box.topLeft.y = static_cast<float>(spinRadY1->value());
        box.bottomRight.x = static_cast<float>(spinRadX2->value());
        box.bottomRight.y = static_cast<float>(spinRadY2->value());
        box.avgTemperature = 42.0f;
        box.maxTemperature = 55.0f;
        box.minTemperature = 30.0f;

        auto boxes = m_onvifDevice->radiometryBoxes();
        bool found = false;
        for (auto& b : boxes) {
            if (b.token == box.token) {
                b = box;
                found = true;
                break;
            }
        }
        if (!found) {
            boxes.push_back(box);
        }
        m_onvifDevice->setRadiometryBoxes(boxes);
    }
}

void OnvifCameraTab::handleDeleteMeasurement()
{
    if (m_onvifDevice == nullptr) {
        return;
    }
    const int row = tableRadiometry->currentRow();
    if (row < 0) {
        return;
    }
    const QString token = tableRadiometry->item(row, 0)->text();
    const QString type = tableRadiometry->item(row, 1)->text();

    if (type.contains(QStringLiteral("Spot"), Qt::CaseInsensitive)) {
        auto spots = m_onvifDevice->radiometrySpots();
        spots.erase(std::remove_if(spots.begin(), spots.end(),
                        [&](const Onvif::RadiometrySpot& s) { return s.token == token.toStdString(); }),
            spots.end());
        m_onvifDevice->setRadiometrySpots(spots);
    } else {
        auto boxes = m_onvifDevice->radiometryBoxes();
        boxes.erase(std::remove_if(boxes.begin(), boxes.end(),
                        [&](const Onvif::RadiometryBox& b) { return b.token == token.toStdString(); }),
            boxes.end());
        m_onvifDevice->setRadiometryBoxes(boxes);
    }
}

void OnvifCameraTab::handleRadiometryConfigUpdated(const Onvif::RadiometryConfig& config)
{
    QSignalBlocker b1(spinEmissivity);
    QSignalBlocker b2(spinTargetDistance);
    QSignalBlocker b3(spinReflectedTemp);
    QSignalBlocker b4(spinAtmosphericTemp);
    QSignalBlocker b5(spinRelativeHumidity);
    QSignalBlocker b6(spinWindowTransmission);

    spinEmissivity->setValue(config.emissivity);
    spinTargetDistance->setValue(config.distance);
    spinReflectedTemp->setValue(config.reflectedTemperature);
    spinAtmosphericTemp->setValue(config.atmosphericTemperature);
    spinRelativeHumidity->setValue(config.relativeHumidity);
    spinWindowTransmission->setValue(config.windowTransmission);
}

void OnvifCameraTab::handleRadiometrySpotsUpdated(const std::vector<Onvif::RadiometrySpot>& spots)
{
    tableRadiometry->setRowCount(0);
    for (const auto& s : spots) {
        const int row = tableRadiometry->rowCount();
        tableRadiometry->insertRow(row);
        tableRadiometry->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(s.token)));
        tableRadiometry->setItem(row, 1, new QTableWidgetItem(tr("Spotmeter")));
        tableRadiometry->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(s.label)));
        tableRadiometry->setItem(row, 3,
            new QTableWidgetItem(QString("(%1, %2)").arg(s.position.x, 0, 'f', 2).arg(s.position.y, 0, 'f', 2)));
        tableRadiometry->setItem(row, 4, new QTableWidgetItem(QString("%1 °C").arg(s.temperature, 0, 'f', 1)));
        tableRadiometry->setItem(
            row, 5, new QTableWidgetItem(QString("%1 °F").arg(s.temperature * 1.8f + 32.0f, 0, 'f', 1)));
        tableRadiometry->setItem(row, 6, new QTableWidgetItem(tr("N/A")));
    }
    if (m_onvifDevice != nullptr) {
        const float thresh = static_cast<float>(spinAlarmThreshold->value());
        for (const auto& b : m_onvifDevice->radiometryBoxes()) {
            const int row = tableRadiometry->rowCount();
            tableRadiometry->insertRow(row);
            tableRadiometry->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(b.token)));
            tableRadiometry->setItem(row, 1, new QTableWidgetItem(tr("Box ROI")));
            tableRadiometry->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(b.label)));
            tableRadiometry->setItem(row, 3,
                new QTableWidgetItem(QString("[%1, %2, %3, %4]")
                                         .arg(b.topLeft.x, 0, 'f', 2)
                                         .arg(b.topLeft.y, 0, 'f', 2)
                                         .arg(b.bottomRight.x, 0, 'f', 2)
                                         .arg(b.bottomRight.y, 0, 'f', 2)));
            tableRadiometry->setItem(row, 4,
                new QTableWidgetItem(
                    QString("Avg %1 °C (Max %2)").arg(b.avgTemperature, 0, 'f', 1).arg(b.maxTemperature, 0, 'f', 1)));
            tableRadiometry->setItem(
                row, 5, new QTableWidgetItem(QString("Avg %1 °F").arg(b.avgTemperature * 1.8f + 32.0f, 0, 'f', 1)));
            const bool alarm = (b.maxTemperature >= thresh);
            auto* itemAlarm = new QTableWidgetItem(alarm ? tr("🚨 HIGH TEMP") : tr("Normal"));
            if (alarm) {
                itemAlarm->setForeground(QBrush(QColor(230, 50, 50)));
            }
            tableRadiometry->setItem(row, 6, itemAlarm);
        }
    }
}

void OnvifCameraTab::handleRadiometryBoxesUpdated(const std::vector<Onvif::RadiometryBox>& boxes)
{
    tableRadiometry->setRowCount(0);
    if (m_onvifDevice != nullptr) {
        for (const auto& s : m_onvifDevice->radiometrySpots()) {
            const int row = tableRadiometry->rowCount();
            tableRadiometry->insertRow(row);
            tableRadiometry->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(s.token)));
            tableRadiometry->setItem(row, 1, new QTableWidgetItem(tr("Spotmeter")));
            tableRadiometry->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(s.label)));
            tableRadiometry->setItem(row, 3,
                new QTableWidgetItem(QString("(%1, %2)").arg(s.position.x, 0, 'f', 2).arg(s.position.y, 0, 'f', 2)));
            tableRadiometry->setItem(row, 4, new QTableWidgetItem(QString("%1 °C").arg(s.temperature, 0, 'f', 1)));
            tableRadiometry->setItem(
                row, 5, new QTableWidgetItem(QString("%1 °F").arg(s.temperature * 1.8f + 32.0f, 0, 'f', 1)));
            tableRadiometry->setItem(row, 6, new QTableWidgetItem(tr("N/A")));
        }
    }
    bool anyAlarm = false;
    const float thresh = static_cast<float>(spinAlarmThreshold->value());
    for (const auto& b : boxes) {
        const int row = tableRadiometry->rowCount();
        tableRadiometry->insertRow(row);
        tableRadiometry->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(b.token)));
        tableRadiometry->setItem(row, 1, new QTableWidgetItem(tr("Box ROI")));
        tableRadiometry->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(b.label)));
        tableRadiometry->setItem(row, 3,
            new QTableWidgetItem(QString("[%1, %2, %3, %4]")
                                     .arg(b.topLeft.x, 0, 'f', 2)
                                     .arg(b.topLeft.y, 0, 'f', 2)
                                     .arg(b.bottomRight.x, 0, 'f', 2)
                                     .arg(b.bottomRight.y, 0, 'f', 2)));
        tableRadiometry->setItem(row, 4,
            new QTableWidgetItem(
                QString("Avg %1 °C (Max %2)").arg(b.avgTemperature, 0, 'f', 1).arg(b.maxTemperature, 0, 'f', 1)));
        tableRadiometry->setItem(
            row, 5, new QTableWidgetItem(QString("Avg %1 °F").arg(b.avgTemperature * 1.8f + 32.0f, 0, 'f', 1)));
        const bool alarm = (b.maxTemperature >= thresh);
        if (alarm) {
            anyAlarm = true;
        }
        auto* itemAlarm = new QTableWidgetItem(alarm ? tr("🚨 HIGH TEMP") : tr("Normal"));
        if (alarm) {
            itemAlarm->setForeground(QBrush(QColor(230, 50, 50)));
        }
        tableRadiometry->setItem(row, 6, itemAlarm);
    }
    if (anyAlarm) {
        lblThermalAlarmStatus->setText(tr("🚨 HIGH TEMP ALARM!"));
        lblThermalAlarmStatus->setStyleSheet(QStringLiteral("font-weight: bold; color: #da3633; padding: 2px 8px;"));
    } else {
        lblThermalAlarmStatus->setText(tr("Alarm: Normal"));
        lblThermalAlarmStatus->setStyleSheet(QStringLiteral("font-weight: bold; color: #238636; padding: 2px 8px;"));
    }
}

void OnvifCameraTab::handleColorPalettesUpdated(const std::vector<Onvif::ColorPalette>& palettes)
{
    cmbThermalPalettes->clear();
    for (const auto& p : palettes) {
        cmbThermalPalettes->addItem(QString::fromStdString(p.name), QString::fromStdString(p.token));
    }
}

void OnvifCameraTab::handleNucTriggered(bool success)
{
    if (success) {
        lblNucStatus->setText(tr("NUC: Calibrated"));
        lblNucStatus->setStyleSheet(QStringLiteral("color: #7ee787; font-weight: bold;"));
    } else {
        lblNucStatus->setText(tr("NUC: Failed"));
        lblNucStatus->setStyleSheet(QStringLiteral("color: #da3633; font-weight: bold;"));
    }
}

} // namespace PelcoDApp
